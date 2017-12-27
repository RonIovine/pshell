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
A Python program to provide remote client access to UDP/UNIX PshellServers

This stand-alone client process will provide remote user interaction to 
any process that is running a UDP or UNIX PshellServer (TCP PshellServers 
are controlled via a standard 'telnet' client).  It can control processes 
that are running either the 'C' version of the PshellServer, which is 
provided via the PshellServer.h API and the libpshell-server link libaray, 
or the Python version that is provided via the PshellServer.py module.

Modules:

The following Python modules are provided for programs to embed their own
PshellServer functionality and to programatically control PshellServers 
running in other processes.

PshellServer.py   -- provide PshellServer capability in a given process
PshellControl.py  -- invoke PshellServer callback functions in another process
PshellReadline.py -- provide a readline like capability to solicit user input

See the respective 'pydoc' pages (text or HTML) for more detailed documentation
on the above modules.
"""

# import all our necessary module
import os
import sys
import signal
import PshellControl
import PshellServer
import PshellReadline

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
_enddef = _endif = _endwhile = _endfor = None

_gSid = None
_gBroadcastServer = False
_NULL = ""

#################################################################################
#################################################################################
def _showWelcome():
  global _gClientName
  global _gRemoteServer
  # show our welcome screen
  # put up our window title banner
  sys.stdout.write("\033]0;PSHELL: %s[%s], Mode: INTERACTIVE\007" % (_gClientName, _gRemoteServer))
  sys.stdout.flush()    
  banner = "#  PSHELL: Process Specific Embedded Command Line Shell"
  server = "#  Broadcast server: %s[%s]" % (_gClientName, _gRemoteServer)
  maxBorderWidth = max(58, len(banner),len(server))+2
  print
  print "#"*maxBorderWidth
  print "#"
  print banner
  print "#"
  print server
  print "#"
  print "#  Idle session timeout: NONE"
  print "#"
  print "#  Type '?' or 'help' at prompt for command summary"
  print "#  Type '?' or '-h' after command for command usage"
  print "#"
  print "#  Full <TAB> completion, up-arrow recall, command"
  print "#  line editing and command abbreviation supported"
  print "#"
  print "#  NOTE: Connected to a broadcast address, all commands"
  print "#        are single-shot, 'fire-and'forget', with no"
  print "#        response requested or expected and no results"
  print "#        displayed.  All commands are 'invisible' since"
  print "#        since no remote command query is requested."
  print "#"
  print "#"*maxBorderWidth
  print
_enddef

#################################################################################
#################################################################################
def _processCommand(command_):
  global _gSid
  if ((command_.split()[0] == "?") or (PshellReadline.isSubString(command_.split()[0], "help"))):
    print
    print "****************************************"
    print "*             COMMAND LIST             *"
    print "****************************************"
    print
    print "quit   -  exit interactive mode"
    print "help   -  show all available commands"
    print "batch  -  run commands from a batch file"
    print
    print "NOTE: Connected to a broadcast address, all remote server"
    print "      commands are 'invisible' to this client application"
    print "      and are single-shot, 'fire-and-forget', with no response"
    print "      requested or expected, and no results displayed"
    print
  elif (PshellReadline.isSubString(command_.split()[0], "batch")):
    print "processing batch file"
  else:
    PshellControl.sendCommand1(_gSid, command_)
  _endif
_enddef

#################################################################################
#################################################################################
def _comandDispatcher(args_):
  global _gSid
  (results, retCode) = PshellControl.sendCommand3(_gSid, ' '.join(args_))
  PshellServer.printf(results)
_enddef

#################################################################################
#################################################################################
def _configureLocalServer():
  global _gSid
  global _gRemoteServer
  global _gPort

  # need to set the first arg position to 0 so we can pass
  # through the exact command to our remote server for dispatching
  PshellServer._gFirstArgPos = 0
  # we tell the local server not to add the 'batch' command because
  # they are already added in the remote UDP/UNIX servers
  PshellServer._gAddBatch = False
  # supress the automatic invalid arg count messag from the PshellControl.py
  # module so we can display the returned usage
  PshellControl._gSupressInvalidArgCountMessage = True
  prompt = PshellControl._extractPrompt(_gSid)
  if (len(prompt) > 0):
    PshellServer._gPromptOverride = prompt
  _endif
  title = PshellControl._extractTitle(_gSid)
  if (len(title) > 0):
    PshellServer._gTitleOverride = title
  _endif
  serverName = PshellControl._extractName(_gSid)
  if (len(serverName) > 0):
    PshellServer._gServerNameOverride = serverName
  _endif
  banner = PshellControl._extractBanner(_gSid)
  if (len(banner) > 0):
    PshellServer._gBannerOverride = banner
  _endif
  if (_gPort == PshellControl.UNIX):
    PshellServer._gServerTypeOverride = PshellControl.UNIX
  elif (_gRemoteServer == "localhost"):
    PshellServer._gServerTypeOverride = "127.0.0.1"
  else:
    PshellServer._gServerTypeOverride = _gRemoteServer
  _endif
_enddef

#################################################################################
#################################################################################
def _cleanupAndExit():
  PshellServer.cleanupResources()
  PshellControl.disconnectAllServers()
  sys.exit()
_enddef

#################################################################################
#################################################################################
def _signalHandler(signal, frame):
  _cleanupAndExit()
_enddef

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
  signal.signal(signal.SIGPWR, _signalHandler)      # 30 Power failure restart (System V)
  signal.signal(signal.SIGSYS, _signalHandler)      # 31 Bad system call
_enddef

#####################################################
#####################################################
def _showUsage():
  print
  print "Usage: %s {<hostname> | <ipAddress> | <unixServerName>} {<port> | unix} [-t<timeout>]" % os.path.basename(sys.argv[0])
  print
  print "  where:"
  print "    <hostname>       - hostname of UDP server"
  print "    <ipAddress>      - IP address of UDP server"
  print "    <unixServerName> - name of UNIX server"
  print "    unix             - specifies a UNIX server"
  print "    <port>           - port number of UDP server"
  print "    <timeout>        - wait timeout for response in sec (default=5)"
  print
  exit(0)
_enddef

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):
  
  if ((len(sys.argv) < 3) or ((len(sys.argv)) > 4)):
    _showUsage()
  _endif

  timeout = 5

  for arg in sys.argv[3:]:
    if ("-t" in arg):
      timeout = int(arg[2:])
    else:
      _showUsage()
    _endif
  _endfor

  _gRemoteServer = sys.argv[1]
  _gPort = sys.argv[2]

  # connect to our remote server via the control client
  _gSid = PshellControl.connectServer("pshellClient", _gRemoteServer, _gPort, PshellControl.ONE_SEC*timeout)

  if (PshellControl._gBroadcastServer == False):
    # if not a broadcast server address, extract all the commands from
    # our unicast remote server and add then to our local server
    commandList = PshellControl.extractCommands(_gSid)
    commandList = commandList.split("\n")
    for command in commandList:
      splitCommand = command.split("-")
      if (len(splitCommand ) >= 2):
        commandName = splitCommand[0].strip()
        description = splitCommand[1].strip()
        PshellServer.addCommand(_comandDispatcher, commandName, description, "[<arg1> ... <arg20>]", 0, 20)
      _endif
    _endif

    # configure our local server to interact with a remote server, we override the display settings
    # (i.e. prompt, server name, banner, title etc, to make it appear that our local server is really
    # a remote server
    _configureLocalServer()

    # now start our local server which will interact with a remote server via the pshell control machanism
    PshellServer.startServer("pshellClient", PshellServer.LOCAL, PshellServer.BLOCKING)
  
  else:

    _gClientName = "pshellClient"

    # add some TAB completors
    PshellReadline.addTabCompletion("quit")
    PshellReadline.addTabCompletion("help")
    PshellReadline.addTabCompletion("batch")

    _showWelcome()

    command = _NULL
    while (not PshellReadline.isSubString(command, "quit")):
      (command, idleSession) = PshellReadline.getInput("%s[%s]:PSHELL> " % (_gClientName, _gRemoteServer))
      if (not PshellReadline.isSubString(command, "quit")):
        _processCommand(command)
      _endif
    _endwhile

  _endif

  _cleanupAndExit()
  
_endif

