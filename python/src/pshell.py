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
import time
import PshellControl
import PshellServer
import PshellReadline

_gSid = None
_gHelp = ('?', '-h', '--h', '-help', '--help', 'help')

#################################################################################
#################################################################################
def _showWelcome():
  global _gServerName
  global _gRemoteServer
  # show our welcome screen
  # put up our window title banner
  sys.stdout.write("\033]0;PSHELL: %s[%s], Mode: INTERACTIVE\007" % (_gServerName, _gRemoteServer))
  sys.stdout.flush()    
  banner = "#  PSHELL: Process Specific Embedded Command Line Shell"
  server = "#  Multi-session BROADCAST server: %s[%s]" % (_gServerName, _gRemoteServer)
  maxBorderWidth = max(58, len(banner),len(server))+2
  print("")
  print("#"*maxBorderWidth)
  print("#")
  print(banner)
  print("#")
  print(server)
  print("#")
  print("#  Idle session timeout: NONE")
  print("#")
  print("#  Type '?' or 'help' at prompt for command summary")
  print("#  Type '?' or '-h' after command for command usage")
  print("#")
  print("#  Full <TAB> completion, up-arrow recall, command")
  print("#  line editing and command abbreviation supported")
  print("#")
  print("#  NOTE: Connected to a broadcast address, all commands")
  print("#        are single-shot, 'fire-and'forget', with no")
  print("#        response requested or expected, and no results")
  print("#        displayed.  All commands are 'invisible' since")
  print("#        no remote command query is requested.")
  print("#")
  print("#"*maxBorderWidth)
  print("")

#################################################################################
#################################################################################
def _showHelp():
  print("")
  print("****************************************")
  print("*             COMMAND LIST             *")
  print("****************************************")
  print("")
  print("quit   -  exit interactive mode")
  print("help   -  show all available commands")
  print("")
  print("NOTE: Connected to a broadcast address, all remote server")
  print("      commands are 'invisible' to this client application")
  print("      and are single-shot, 'fire-and-forget', with no response")
  print("      requested or expected, and no results displayed")
  print("")

#################################################################################
#################################################################################
def _processCommand(command_):
  global _gSid
  if ((command_.split()[0] == "?") or (PshellReadline.isSubString(command_.split()[0], "help"))):
    _showHelp()
  else:
    PshellControl.sendCommand1(_gSid, command_)

#################################################################################
#################################################################################
def _comandDispatcher(args_):
  global _gSid
  global _gHelp
  global _gInteractive
  if args_[0] in _gHelp:
    results = PshellControl.extractCommands(_gSid)
  else:
    (results, retCode) = PshellControl.sendCommand3(_gSid, ' '.join(args_))
  if (_gInteractive == True):
    PshellServer.printf(results, newline=False)
  else:
    # command line mode
    sys.stdout.write(results)

#################################################################################
#################################################################################
def _getIpAddress():
  global _gRemoteServer
  global _gPort
  if (_gPort == "unix"):
    return (_gPort)
  elif (_gRemoteServer == "localhost"):
    return ("127.0.0.1")
  else:
    return (_gRemoteServer)
    
#################################################################################
#################################################################################
def _processCommandLine():
  global _gRate
  global _gClear
  global _gCommand
  global _gServerName
  global _gTitle
  command = _gCommand.split()
  sys.stdout.write("\033]0;%s: %s[%s], Mode: COMMAND LINE[%s], Rate: %d SEC\007" % (_gTitle, _gServerName, _getIpAddress(), _gCommand, _gRate))
  while (True):
    if (_gClear != False):
      sys.stdout.write(_gClear)
    _comandDispatcher(command)
    if (_gRate > 0):
      time.sleep(_gRate)
    else:
      break
  
#################################################################################
#################################################################################
def _processBatchFile():
  global _gRate
  global _gClear
  global _gFilename
  global _gServerName
  global _gTitle
  if (os.path.isfile(_gFilename)):
    file = open(_gFilename, 'r')
    sys.stdout.write("\033]0;%s: %s[%s], Mode: BATCH[%s], Rate: %d SEC\007" % (_gTitle, _gServerName, _getIpAddress(), _gFilename, _gRate))
    while (True):
      if (_gClear != False):
        sys.stdout.write(_gClear)
      file.seek(0, 0)
      for line in file:
        # skip comments
        line = line.strip()
        if ((len(line) > 0) and (line[0] != "#")):
          command = line.split()
          _comandDispatcher(command)
      if (_gRate > 0):
        time.sleep(_gRate) 
      else:
        break
  else:
    print("ERROR: Could not open file: '%s'" % _gFilename)
  
#################################################################################
#################################################################################
def _configureLocalServer():
  global _gSid
  global _gRemoteServer
  global _gTimeout
  global _gPort

  # need to set the first arg position to 0 so we can pass
  # through the exact command to our remote server for dispatching
  PshellServer._gFirstArgPos = 0
  # we tell the local server we are the special UDP/UNIX command 
  # line client so it can process commands correctly and display
  # the correct banner  information
  PshellServer._gPshellClient = True
  # supress the automatic invalid arg count message from the PshellControl.py
  # module so we can display the returned usage
  PshellControl._gSupressInvalidArgCountMessage = True
  # extract information from our remote server via the special
  # "private" control API so we can feed the info to our local
  # pshell server to make it look like a remote server
  prompt = PshellControl._extractPrompt(_gSid)
  if (len(prompt) > 0):
    PshellServer._gPromptOverride = prompt
    
  title = PshellControl._extractTitle(_gSid)
  if (len(title) > 0):
    PshellServer._gTitleOverride = title
    
  serverName = PshellControl._extractName(_gSid)
  if (len(serverName) > 0):
    PshellServer._gServerNameOverride = serverName
    
  banner = PshellControl._extractBanner(_gSid)
  if (len(banner) > 0):
    PshellServer._gBannerOverride = banner

  if (_gPort == PshellControl.UNIX):
    PshellServer._gServerTypeOverride = PshellServer.UNIX
  elif (_gRemoteServer == "localhost"):
    PshellServer._gServerTypeOverride = "127.0.0.1"
  else:
    PshellServer._gServerTypeOverride = _gRemoteServer

  PshellServer._gTcpTimeout = _gTimeout

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

#####################################################
#####################################################
def _showUsage():
  print("")
  print("Usage: %s {{<hostNameOrIpAddr> <port>} | <unixServerName>} [-t<timeout>]" % os.path.basename(sys.argv[0]))
  print("                 [{<command> [<rate> [clear]]} | {-f <fileName> [<rate> [clear]]}]")
  print("")
  print("  where:")
  print("")
  print("    -f               - run commands from a batch file")
  print("    -t               - change the default server response timeout")
  print("    hostNameOrIpAddr - hostname or IP address of UDP server")
  print("    port             - port number of UDP server")
  print("    unixServerName   - name of UNIX server")
  print("    timeout          - response wait timeout in sec (default=5)")
  print("    command          - optional command to execute")
  print("    fileName         - optional batch file to execute")
  print("    rate             - optional rate to repeat command or batch file (in seconds)")
  print("    clear            - clear screen between commands or batch file passes")
  print("")
  print("    NOTE: If no <command> is given, pshell will be started")
  print("          up in interactive mode, commands issued in command")
  print("          line mode that require arguments must be enclosed")
  print("          in double quotes, commands issued in interactive")
  print("          mode that require arguments do not require double")
  print("          quotes.")
  print("")
  print("          To get help on a command in command line mode, type")
  print("          \"<command> ?\" or \"<command> -h\".  To get help in")
  print("          interactive mode type 'help' or '?' at the prompt to")
  print("          see all available commands, to get help on a single")
  print("          command, type '<command> {? | -h}'.  Use TAB completion")
  print("          to fill out partial commands and up-arrow to recall")
  print("          for command history.")
  print("")
  exit(0)

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):
  
  if ((len(sys.argv) < 2) or 
      ((len(sys.argv)) > 8) or 
      ((len(sys.argv) > 1) and (sys.argv[1] == "-h"))):
    _showUsage()

  _gTimeout = 5

  _gPort = PshellServer.UNIX
  
  _gRate = 0
  _gClear = False
  _gFilename = None
  _gCommand = None
  _gInteractive = False
  
  needFile = False
  
  for index, arg in enumerate(sys.argv[2:]):
    if ("-t" in arg):
      _gTimeout = int(arg[2:])
    elif (arg.isdigit()):
      if (index == 0):
        _gPort = arg
      else:
        _gRate = int(arg)
    elif (arg == "clear"):
      _gClear = "\033[H\033[J"
    elif (arg == "-f"):
      needFile = True
    elif (needFile == True):
      if (not arg.isdigit()):
        _gFilename = arg
        needFile = False
      else:
        _showUsage()
    else:
      _gCommand = arg
  
  # make sure we cleanup any system resorces on an abnormal termination
  _registerSignalHandlers()
  
  _gRemoteServer = sys.argv[1]

  # see if we are requesting a server sitting on a subnet broadcast address,
  # if so, we configure for one-way, fire-and-forget messaging since there may
  # be many hosts/servers on our subnet which might all be listening on the
  # same broadcast address/port
  _gIsBroadcastAddr = ((len(_gRemoteServer.split(".")) == 4) and (_gRemoteServer.split(".")[3] == "255"))

  # connect to our remote server via the control client
  _gSid = PshellControl.connectServer("pshellClient", _gRemoteServer, _gPort, PshellControl.ONE_SEC*_gTimeout)
  _gServerName = PshellControl._extractName(_gSid)
  _gTitle = PshellControl._extractTitle(_gSid)

  if (_gCommand != None):
    # command line mode, execute command
    _processCommandLine()
  elif (_gFilename != None):
    # command line mode, using batch file containing commands
    _processBatchFile()
  else:
    
    # interactive mode, setup local server and interact with user
    _gInteractive = True
  
    if (_gIsBroadcastAddr == False):
    
      # if not a broadcast server address, extract all the commands from
      # our unicast remote server and add them to our local server
      commandList = PshellControl.extractCommands(_gSid)
      if (len(commandList) > 0):
        commandList = commandList.split("\n")
        for command in commandList:
          splitCommand = command.split("-")
          if (len(splitCommand ) >= 2):
            commandName = splitCommand[0].strip()
            description = splitCommand[1].strip()
            PshellServer.addCommand(_comandDispatcher, commandName, description, "[<arg1> ... <arg20>]", 0, 20)

        # configure our local server to interact with a remote server, we override the display settings
        # (i.e. prompt, server name, banner, title etc), to make it appear that our local server is really
        # a remote server
        _configureLocalServer()

        # now start our local server which will interact with a remote server via the pshell control machanism
        PshellServer.startServer("pshellServer", PshellServer.LOCAL, PshellServer.BLOCKING)
      else:
        print("PSHELL_ERROR: Could not connect to server: '%s:%s'" % (_gRemoteServer, _gPort))
  
    else:

      _gServerName = "broadcastAddr"

      # add some TAB completors
      PshellReadline.addTabCompletion("quit")
      PshellReadline.addTabCompletion("help")

      _showWelcome()

      command = ""
      while (not PshellReadline.isSubString(command, "quit")):
        (command, idleSession) = PshellReadline.getInput("%s[%s]:PSHELL> " % (_gServerName, _gRemoteServer))
        if (not PshellReadline.isSubString(command, "quit")):
          _processCommand(command)

  _cleanupAndExit()
  

