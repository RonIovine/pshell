#!/usr/bin/python

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

_gLocalNameLabel = "Local Server Name"
_gRemoteNameLabel = "Remote Server"
_gKeywordLabel = "Keyword"
_gMaxLocalName = len(_gLocalNameLabel)
_gMaxRemoteName = len(_gRemoteNameLabel)
_gMaxMulticastKeyword = len(_gKeywordLabel)

#################################################################################
#################################################################################
def _getMulticast(keyword):
  global _gMulticast
  for multicast in _gMulticast:
    if (keyword == multicast["keyword"]):
      return (multicast)
  return (None)

#################################################################################
#################################################################################
def _getServer(localName):
  global _gPshellServers
  for server in _gPshellServers:
    if (PshellServer.isSubString(localName, server["localName"])):
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
      PshellServer.printf(PshellControl.extractCommands(server["sid"]), newline=False)
    elif timeout == 0:
      print("PSHELL_INFO: Command sent fire-and-forget")
      PshellControl.sendCommand1(server["sid"], ' '.join(argv[1:]))
    else:
      # reconstitute and dispatch original command to remote server minus the first keyword
      (results, retCode) = PshellControl.sendCommand4(server["sid"], timeout, ' '.join(argv[1:]))
      # good return, display results back to user
      if (retCode == PshellControl.COMMAND_SUCCESS):
        PshellServer.printf(results, newline=False)

#################################################################################
#################################################################################
def _isDuplicate(localName_, remoteServer_, port_):
  global _gPshellServers
  for server in _gPshellServers:
    if ((localName_ == server["localName"]) or ((remoteServer_ == server["remoteServer"]) and (port_ == server["port"]))):
      return (True)
  return (False)

#################################################################################
#################################################################################
def _add(argv):
  global _gPshellServers
  global _gMaxLocalName
  global _gMaxRemoteName
  global _gMulticast
  global _gMaxMulticastKeyword
  global _gRemoteNameLabel
  global _gLocalNameLabel
  global _gKeywordLabel
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where:")
    PshellServer.printf()
    PshellServer.printf("    <localName>    - Local logical name of the server, must be unique")
    PshellServer.printf("    <remoteServer> - Hostname or IP address of UDP server or name of UNIX server")
    PshellServer.printf("    <port>         - UDP port number or 'unix' for UNIX server (can be omitted for UNIX)")
    PshellServer.printf("    <keyword>      - Multicast group keyword, must be valid registered remote command")
    PshellServer.printf()
  elif (PshellServer.isSubString(argv[1], "server")):
    # default port
    port = PshellServer.UNIX
    if (len(argv) == 5):
      port = argv[4]
    if (not _isDuplicate(argv[2], argv[3], port)):
      if (len(argv[2]) > _gMaxLocalName):
        _gMaxLocalName = max(len(argv[2]), len(_gLocalNameLabel))
      if (len(argv[3]) > _gMaxRemoteName):
        _gMaxRemoteName = max(len(argv[3]), len(_gRemoteNameLabel))
      _gPshellServers.append({"localName":argv[2],
                              "remoteServer":argv[3],
                              "port":port,
                              "sid":PshellControl.connectServer(argv[2],
                                                                argv[3],
                                                                port,
                                                                PshellControl.ONE_SEC*5)})
      PshellServer.addCommand(_controlServer,
                              argv[2],
                              "control the remote " + argv[2] + " process",
                              "[<command> | ? | -h]",
                              0,
                              30,
                              False)
      PshellServer._addTabCompletions()
    else:
      PshellServer.printf("ERROR: Local name: %s, remote server: %s, port: %s already exists" % (argv[2], argv[3], argv[4]))
  elif (PshellServer.isSubString(argv[1], "multicast")):
    multicast = _getMulticast(argv[2])
    if (multicast == None):
      # new keyword
      if (len(argv[2]) > _gMaxMulticastKeyword):
        _gMaxMulticastKeyword = max(len(argv[2]), len(_gKeywordLabel))
      _gMulticast.append({"keyword":argv[2], "servers":[]})
      multicast = _gMulticast[-1]
    # add servers to this keyword
    for localName in argv[3:]:
      server = _getServer(localName)
      if (server != None):
        PshellControl.addMulticast(server["sid"], argv[2])
        multicast["servers"].append(server)
  else:
    PshellServer.showUsage()

#################################################################################
#################################################################################
def _show(argv):
  global _gPshellServers
  global _gMaxLocalName
  global _gMaxRemoteName
  global _gMulticast
  global _gMaxMulticastKeyword
  global _gRemoteNameLabel
  global _gLocalNameLabel
  global _gKeywordLabel
  if (PshellServer.isSubString(argv[1], "server")):
    PshellServer.printf()
    PshellServer.printf("*************************************************")
    PshellServer.printf("*           AGGREGATED REMOTE SERVERS           *")
    PshellServer.printf("*************************************************")
    PshellServer.printf()
    PshellServer.printf("%s    %s    Port" % (_gLocalNameLabel.ljust(_gMaxLocalName),
                                                _gRemoteNameLabel.ljust(_gMaxRemoteName)))
    PshellServer.printf("%s    %s    ======" % ("=".ljust(_gMaxLocalName, "="),
                                                  "=".ljust(_gMaxRemoteName, "=")))
    for server in _gPshellServers:
      PshellServer.printf("%s    %s    %s" % (server["localName"].ljust(_gMaxLocalName), server["remoteServer"].ljust(_gMaxRemoteName), server["port"]))
    PshellServer.printf()
  elif (PshellServer.isSubString(argv[1], "multicast")):
    PshellServer.printf()
    PshellServer.printf("*****************************************************")
    PshellServer.printf("*            REGISTERED MULTICAST GROUPS            *")
    PshellServer.printf("*****************************************************")
    PshellServer.printf()
    PshellServer.printf("%s    %s    %s    Port" % (_gKeywordLabel.ljust(_gMaxMulticastKeyword),
                                                      _gLocalNameLabel.ljust(_gMaxLocalName),
                                                      _gRemoteNameLabel.ljust(_gMaxRemoteName)))
    PshellServer.printf("%s    %s    %s    ======" % ("=".ljust(_gMaxMulticastKeyword, "="),
                                                        "=".ljust(_gMaxLocalName, "="),
                                                        "=".ljust(_gMaxRemoteName, "=")))
    for multicast in _gMulticast:
      PshellServer.printf("%s    " % multicast["keyword"].ljust(_gMaxMulticastKeyword), newline=False)
      for index, server in enumerate(multicast["servers"]):
        if (index > 0):
          PshellServer.printf("%s    " % " ".ljust(_gMaxMulticastKeyword, " "), newline=False)
        PshellServer.printf("%s    %s    %s" % (server["localName"].ljust(_gMaxLocalName),
                                                      server["remoteServer"].ljust(_gMaxRemoteName),
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
    PshellServer.printf("  Send a registered multicast command to the associated")
    PshellServer.printf("  multicast remote server group")
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
  # the correct banner  information
  PshellServer._gPshellClient = True

  # supress the automatic invalid arg count message from the PshellControl.py
  # module so we can display the returned usage
  PshellControl._gSupressInvalidArgCountMessage = True

  # register our callback commands
  PshellServer.addCommand(_add,
                          "add",
                          "add a new remote server or multicast group entry",
                          "{server <localName> <remoteServer> [<port>]} | {multicast <keyword> <localName1> [<localName2>...<localNameN>]}",
                          4,
                          30,
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
                          30,
                          False)

  # start our local pshell server
  PshellServer.startServer("pshellAggregator", PshellServer.LOCAL, PshellServer.BLOCKING)

  # disconnect all our remote control servers
  PshellControl.disconnectAllServers()

  # cleanup our local server's resources
  PshellServer.cleanupResources()

