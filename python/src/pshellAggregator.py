#!/usr/bin/python2

#################################################################################
#
# Copyright (c) 2009, Ron Iovine, All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Ron Iovine nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY Ron Iovine ''AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL Ron Iovine BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#################################################################################

"""
Remote pshell server aggregator

This is a pshell client program that aggregates several remote pshell servers
into single local pshell server that serves as an interactive client to the
remote servers.  This can be useful in presenting a consolidated user shell
who's functionality spans several discrete pshell servers.  Since this uses
the PshellControl API, the external servers must all be either UDP or Unix
servers.  The consolidation point a local pshell server.

This is a generic dynamic aggregator, i.e. it is server agnostic.  Servers can
be added to the aggregation via the 'add server' command either at startup via
the pshellAggregator.startup file or interactively via the interactive command
line.

This program can also create multicast groups commands via the 'add multicast'
command (also at startup or interactively).  The multicast commands can then be
distributed to multiple aggregated servers.

The aggregation and multicast functionality can be useful to manually drive a set
of processes that use the pshell control mechanism as a control plane IPC.
"""

# import all our necessary modules
import sys
import os
import signal
import PshellServer
import PshellControl

# list of dictionaries that contains a control structure for each control client
_gPshellServers = []

_gMulticast = []

_gControlNameLabel = "Control Name"
_gServerNameLabel = "Remote Server"
_gCommandLabel = "Command"
_gMaxControlName = len(_gControlNameLabel)
_gMaxServerName = len(_gServerNameLabel)
_gMaxMulticastCommand = len(_gCommandLabel)

#################################################################################
#################################################################################
def _getMulticast(command):
  global _gMulticast
  for multicast in _gMulticast:
    if (command == multicast["command"]):
      return (multicast)
  return (None)

#################################################################################
#################################################################################
def _getServer(controlName):
  global _gPshellServers
  for server in _gPshellServers:
    if (PshellServer.isSubString(controlName, server["controlName"])):
      return (server)
  return (None)

#################################################################################
#################################################################################
def _controlServer(argv):
  timeout = PshellServer._gPshellClientTimeout
  if PshellServer._gClientTimeoutOverride:
    if len(PshellServer._gClientTimeoutOverride) > 2:
      timeout = int(PshellServer._gClientTimeoutOverride[2:])
  server = _getServer(argv[0])
  if (server != None):
    # see if they asked for help
    if ((len(argv) == 1) or
        (argv[1] == "help") or
        (argv[1] == "-h") or
        (argv[1] == "-help") or
        (argv[1] == "--h") or
        (argv[1] == "--help") or
        (argv[1] == "?")):
      # user asked for help, display all the registered commands of the remote server
      PshellServer.printf(PshellControl.extractCommands(server["controlName"]), newline=False)
    elif timeout == PshellControl.NO_WAIT:
      print("PSHELL_INFO: Command sent fire-and-forget, no response requested")
      PshellControl.sendCommand2(server["controlName"], PshellControl.NO_WAIT, ' '.join(argv[1:]))
    else:
      # reconstitute and dispatch original command to remote server minus the first keyword
      retCode = PshellControl._sendUserCommand(server["controlName"], PshellControl.ONE_SEC*timeout, ' '.join(argv[1:]))
      # good return, display results back to user
      while retCode != PshellControl._COMMAND_COMPLETE and retCode != PshellControl.SOCKET_TIMEOUT:
        if (timeout > PshellControl.NO_WAIT):
          results, retCode = PshellControl._receiveUserCommand(server["controlName"], PshellControl.ONE_SEC*timeout)
        else:
          results, retCode = PshellControl._receiveUserCommand(server["controlName"], PshellControl.WAIT_FOREVER)
        if results != None:
          PshellServer.printf(results, newline=False)
      if retCode == PshellControl.SOCKET_TIMEOUT:
        PshellServer.printf("PSHELL_ERROR: Response timeout from remote pshellServer")

#################################################################################
#################################################################################
def _isDuplicateServer(remoteServer_, port_):
  global _gPshellServers
  for server in _gPshellServers:
    if remoteServer_ == server["remoteServer"] and port_ == server["port"]:
      return (True)
  return (False)

#################################################################################
#################################################################################
def _isDuplicateControl(controlName_):
  global _gPshellServers
  for server in _gPshellServers:
    if controlName_ == server["controlName"]:
      return (True)
  return (False)

#################################################################################
#################################################################################
def _isDuplicateMulticast(multicast, controlName_):
  for server in multicast["servers"]:
    if (controlName_ == server["controlName"]):
      return (True)
  return (False)

#################################################################################
#################################################################################
def _add(argv):
  global _gPshellServers
  global _gMaxControlName
  global _gMaxServerName
  global _gMulticast
  global _gMaxMulticastCommand
  global _gServerNameLabel
  global _gControlNameLabel
  global _gCommandLabel
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where:")
    PshellServer.printf("    <controlName>  - Local logical control name of the server, must be unique")
    PshellServer.printf("    <remoteServer> - Hostname or IP address of UDP server or name of UNIX server")
    PshellServer.printf("    <port>         - UDP port number or 'unix' for UNIX server (can be omitted for UNIX)")
    PshellServer.printf("    <command>      - Multicast group command, must be valid registered remote command")
    PshellServer.printf("    <controlList>  - CSV formatted list or space separated list of remote controlNames")
    PshellServer.printf("    all            - Add all multicast commands to the controlList, or add the given")
    PshellServer.printf("                     command to all control destination servers, or both")
    PshellServer.printf()
  elif (PshellServer.isSubString(argv[1], "server")):
    # default port
    port = PshellServer.UNIX
    if (len(argv) == 5):
      port = argv[4]
    if _isDuplicateServer(argv[3], port):
      PshellServer.printf("ERROR: Remote server: %s, port: %s already exists" % (argv[3], port))
    elif _isDuplicateControl(argv[2]):
      PshellServer.printf("ERROR: Control name: %s already exists" % argv[2])
    elif PshellControl.connectServer(argv[2], argv[3], port, PshellControl.ONE_SEC*5):
      if (len(argv[2]) > _gMaxControlName):
        _gMaxControlName = max(len(argv[2]), len(_gControlNameLabel))
      if (len(argv[3]) > _gMaxServerName):
        _gMaxServerName = max(len(argv[3]), len(_gServerNameLabel))
      _gPshellServers.append({"controlName":argv[2],
                              "remoteServer":argv[3],
                              "port":port})
      PshellServer.addCommand(_controlServer,
                              argv[2],
                              "control the remote " + argv[2] + " process",
                              "[<command> | ? | -h]",
                              0,
                              60,
                              False)
      PshellServer._addTabCompletions()
  elif (PshellServer.isSubString(argv[1], "multicast")):
    multicast = _getMulticast(argv[2])
    if (multicast == None):
      # new command
      if (len(argv[2]) > _gMaxMulticastCommand):
        _gMaxMulticastCommand = max(len(argv[2]), len(_gCommandLabel))
      _gMulticast.append({"command":argv[2], "servers":[]})
      multicast = _gMulticast[-1]
    # add remote servers to this command
    if len(argv) == 4 and "," in argv[3]:
      controlNames = argv[3].split(",")
    elif argv[3] == "all":
      controlNames = PshellControl._extractControlNames()
    else:
      controlNames = argv[3:]
    for controlName in controlNames:
      server = _getServer(controlName)
      if server != None and not _isDuplicateMulticast(multicast, controlName):
        if argv[2] == "all":
          PshellControl.addMulticast(PshellControl.MULTICAST_ALL, server["controlName"])
        else:
          PshellControl.addMulticast(argv[2], server["controlName"])
        multicast["servers"].append(server)
  else:
    PshellServer.showUsage()

#################################################################################
#################################################################################
def _show(argv):
  global _gPshellServers
  global _gMaxControlName
  global _gMaxServerName
  global _gMulticast
  global _gMaxMulticastCommand
  global _gServerNameLabel
  global _gControlNameLabel
  global _gCommandLabel
  if (PshellServer.isSubString(argv[1], "servers")):
    PshellServer.printf()
    PshellServer.printf("*************************************************")
    PshellServer.printf("*           AGGREGATED REMOTE SERVERS           *")
    PshellServer.printf("*************************************************")
    PshellServer.printf()
    PshellServer.printf("%s    %s    Port" % (_gControlNameLabel.ljust(_gMaxControlName),
                                              _gServerNameLabel.ljust(_gMaxServerName)))
    PshellServer.printf("%s    %s    ======" % ("=".ljust(_gMaxControlName, "="),
                                                "=".ljust(_gMaxServerName, "=")))
    for server in _gPshellServers:
      PshellServer.printf("%s    %s    %s" % (server["controlName"].ljust(_gMaxControlName), server["remoteServer"].ljust(_gMaxServerName), server["port"]))
    PshellServer.printf()
  elif (PshellServer.isSubString(argv[1], "multicast")):
    PshellServer.printf()
    PshellServer.printf("*****************************************************")
    PshellServer.printf("*            REGISTERED MULTICAST GROUPS            *")
    PshellServer.printf("*****************************************************")
    PshellServer.printf()
    PshellServer.printf("%s    %s    %s    Port" % (_gCommandLabel.ljust(_gMaxMulticastCommand),
                                                    _gControlNameLabel.ljust(_gMaxControlName),
                                                    _gServerNameLabel.ljust(_gMaxServerName)))
    PshellServer.printf("%s    %s    %s    ======" % ("=".ljust(_gMaxMulticastCommand, "="),
                                                      "=".ljust(_gMaxControlName, "="),
                                                      "=".ljust(_gMaxServerName, "=")))
    for multicast in _gMulticast:
      PshellServer.printf("%s    " % multicast["command"].ljust(_gMaxMulticastCommand), newline=False)
      for index, server in enumerate(multicast["servers"]):
        if (index > 0):
          PshellServer.printf("%s    " % " ".ljust(_gMaxMulticastCommand, " "), newline=False)
        PshellServer.printf("%s    %s    %s" % (server["controlName"].ljust(_gMaxControlName),
                                                server["remoteServer"].ljust(_gMaxServerName),
                                                server["port"]))
    PshellServer.printf()
  else:
    PshellServer.showUsage()

#################################################################################
#################################################################################
def _multicast(argv):
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("Send a registered multicast command to the associated multicast remote server group")
    _show(('show', 'multicast'))
  else:
    # reconstitute the original command
    PshellControl.sendMulticast(' '.join(argv[1:]))

#################################################################################
#################################################################################
def _cleanupAndExit():
  PshellServer.cleanupResources()
  PshellControl.disconnectAllServers()
  sys.exit()

#################################################################################
#################################################################################
def _signalHandler(signal, frame):
  print("")
  _cleanupAndExit()

#################################################################################
#################################################################################
def _registerSignalHandlers():
  # register a signal handlers so we can cleanup our
  # system resources upon abnormal termination
  signal.signal(signal.SIGHUP, _signalHandler)      # 1  Hangup (POSIX)
  signal.signal(signal.SIGINT, _signalHandler)      # 2  Interrupt (ANSI)
  signal.signal(signal.SIGQUIT, _signalHandler)     # 3  Quit (POSIX)
  signal.signal(signal.SIGILL, _signalHandler)      # 4  Illegal instruction (ANSI)
  signal.signal(signal.SIGABRT, _signalHandler)     # 6  Abort (ANSI)
  signal.signal(signal.SIGBUS, _signalHandler)      # 7  BUS error (4.2 BSD)
  signal.signal(signal.SIGFPE, _signalHandler)      # 8  Floating-point exception (ANSI)
  signal.signal(signal.SIGSEGV, _signalHandler)     # 11 Segmentation violation (ANSI)
  signal.signal(signal.SIGPIPE, _signalHandler)     # 13 Broken pipe (POSIX)
  signal.signal(signal.SIGALRM, _signalHandler)     # 14 Alarm clock (POSIX)
  signal.signal(signal.SIGTERM, _signalHandler)     # 15 Termination (ANSI)
  signal.signal(signal.SIGXCPU, _signalHandler)     # 24 CPU limit exceeded (4.2 BSD)
  signal.signal(signal.SIGXFSZ, _signalHandler)     # 25 File size limit exceeded (4.2 BSD)
  signal.signal(signal.SIGSYS, _signalHandler)      # 31 Bad system call

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  # verify usage
  if (len(sys.argv) != 1):
    print("")
    print("Usage: %s" % os.path.basename(sys.argv[0]))
    print("")
    print("  Client program that will allow for the aggregation of multiple remote")
    print("  UDP/UNIX pshell servers into one consolidated client shell.  This program")
    print("  can also create multicast groups for sets of remote servers.  The remote")
    print("  servers and multicast groups can be added interactively via the 'add'")
    print("  command or at startup via the 'pshellAggregator.startup' file.")
    print("")
    exit (0)

  # make sure we cleanup any system resorces on an abnormal termination
  _registerSignalHandlers()

  # need to set the first arg position to 0 so we can pass
  # through the exact command to our remote server for dispatching
  PshellServer._setFirstArgPos(0)

  # we tell the local server we are the special UDP/UNIX command
  # line client so it can process commands correctly and display
  # the correct banner information
  PshellServer._gPshellClient = True

  # supress the automatic invalid arg count message from the PshellControl.py
  # module so we can display the returned usage
  PshellControl._gSupressInvalidArgCountMessage = True

  # register our callback commands
  PshellServer.addCommand(_add,
                          "add",
                          "add a new remote server or multicast group entry",
                          "{server <controlName> <remoteServer> [<port>]} |\n           {multicast {<command> | all} {<controlList> | all}}",
                          4,
                          60,
                          False)

  PshellServer.addCommand(_show,
                          "show",
                          "show aggregated servers or multicast group info",
                          "servers | multicast",
                          2,
                          2,
                          True)

  PshellServer.addCommand(_multicast,
                          "multicast",
                          "send multicast command to registered server group",
                          "<command>",
                          2,
                          60,
                          False)

  # start our local pshell server
  PshellServer.startServer("pshellAggregator", PshellServer.LOCAL, PshellServer.BLOCKING)

  # disconnect all our remote control servers
  PshellControl.disconnectAllServers()

  # cleanup our local server's resources
  PshellServer.cleanupResources()

