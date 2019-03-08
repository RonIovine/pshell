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
  maxBorderWidth = max(58, len(banner), len(server))+2
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
  print("#  Full <TAB> completion, command history, command")
  print("#  line editing, and command abbreviation supported")
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
    results = PshellControl.extractCommands(_gSid, includeName=False)
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
  global _gRepeat
  global _gIteration
  command = _gCommand.split()
  if _gRate > 0 and _gRepeat == 0:
    sys.stdout.write("\033]0;%s: %s[%s], Mode: COMMAND LINE[%s], Rate: %d SEC\007" % (_gTitle, _gServerName, _getIpAddress(), _gCommand, _gRate))
  while (True):
    if (_gRepeat > 0):
      _gIteration += 1
      sys.stdout.write("\033]0;%s: %s[%s], Mode: COMMAND LINE[%s], Rate: %d SEC, Iteration: %d of %d\007" %
                      (_gTitle,
                       _gServerName,
                       _getIpAddress(),
                       _gCommand,
                       _gRate,
                       _gIteration,
                       _gRepeat))
    if (_gClear != False):
      sys.stdout.write(_gClear)
    _comandDispatcher(command)
    if _gRepeat > 0 and _gIteration == _gRepeat:
      break
    elif (_gRate > 0):
      time.sleep(_gRate)
    elif (_gRepeat == 0):
      break
  
#################################################################################
#################################################################################
def _processBatchFile():
  global _gRate
  global _gClear
  global _gFilename
  global _gServerName
  global _gTitle
  global _gRepeat
  global _gIteration
  global _gDefaultBatchDir
  batchFile1 = ""
  batchPath = os.getenv('PSHELL_BATCH_DIR')
  if (batchPath != None):
    batchFile1 = batchPath+"/"+_gFilename
  batchFile2 = _gDefaultBatchDir+"/"+_gFilename
  batchFile3 = os.getcwd()+"/"+_gFilename
  batchFile4 = _gFilename
  if (os.path.isfile(batchFile1)):
    file = open(batchFile1, 'r')
  elif (os.path.isfile(batchFile2)):
    file = open(batchFile2, 'r')
  elif (os.path.isfile(batchFile3)):
    file = open(batchFile3, 'r')
  elif (os.path.isfile(batchFile4)):
    file = open(batchFile4, 'r')
  else:
    print("PSHELL_ERROR: Could not open file: '%s'" % _gFilename)
    return
  # we found a batch file, process it
  if _gRate > 0 and _gRepeat == 0:
    sys.stdout.write("\033]0;%s: %s[%s], Mode: BATCH[%s], Rate: %d SEC\007" % (_gTitle, _gServerName, _getIpAddress(), _gFilename, _gRate))
  while (True):
    if (_gClear != False):
      sys.stdout.write(_gClear)
    file.seek(0, 0)
    if (_gRepeat > 0):
      _gIteration += 1
      sys.stdout.write("\033]0;%s: %s[%s], Mode: BATCH[%s], Rate: %d SEC, Iteration: %d of %d\007" %
                      (_gTitle,
                       _gServerName,
                       _getIpAddress(),
                       _gFilename,
                       _gRate,
                       _gIteration,
                       _gRepeat))
    for line in file:
      # skip comments
      line = line.strip()
      if ((len(line) > 0) and (line[0] != "#")):
        command = line.split()
        _comandDispatcher(command)
    if _gRepeat > 0 and _gIteration == _gRepeat:
      break
    elif (_gRate > 0):
      time.sleep(_gRate)
    elif (_gRepeat == 0):
      break
  file.close()
  
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

#####################################################
#####################################################
def _getServer(arg):
  global _gServerList
  global _gPort
  global _gTimeout
  for server in _gServerList:
    if server["server"] == arg:
      _gPort = server["port"]
      _gTimeout = server["timeout"]
      return True
  return False

#####################################################
#####################################################
def _loadServers():
  global _gDefaultConfigDir
  global _gMaxServerNameLength
  global _gServerList
  global _gClientConfigFile
  _gServerList = []
  _gMaxServerNameLength = 11
  configFile1 = ""
  configPath = os.getenv('PSHELL_CONFIG_DIR')
  if (configPath != None):
    configFile1 = configPath+"/"+_gClientConfigFile
  configFile2 = _gDefaultConfigDir+"/"+_gClientConfigFile
  configFile3 = os.getcwd()+"/"+_gClientConfigFile
  configFile4 = _gClientConfigFile
  if (os.path.isfile(configFile1)):
    file = open(configFile1, 'r')
  elif (os.path.isfile(configFile2)):
    file = open(configFile2, 'r')
  elif (os.path.isfile(configFile3)):
    file = open(configFile3, 'r')
  elif (os.path.isfile(configFile4)):
    file = open(configFile4, 'r')
  else:
    return
  # opened config file, process it
  for line in file:
    # skip comments
    line = line.strip()
    if ((len(line) > 0) and (line[0] != "#")):
      server = line.split(":")
      if len(server) == 2:
        _gMaxServerNameLength = max(_gMaxServerNameLength, len(server[0]))
        _gServerList.append({"server":server[0], "port":server[1], "timeout":5})
      elif len(server) == 3:
        _gMaxServerNameLength = max(_gMaxServerNameLength, len(server[0]))
        _gServerList.append({"server":server[0], "port":server[1], "timeout":int(server[2])})
  file.close()

#####################################################
#####################################################
def _showServers():
  global _gServerList
  global _gMaxServerNameLength
  print("")
  print("**************************************")
  print("*      Available PSHELL Servers      *")
  print("**************************************")
  print("")
  print("%s  Port Number  Response Timeout" % "Server Name".ljust(_gMaxServerNameLength))
  print("%s  ===========  ================" % ("="*_gMaxServerNameLength))
  for server in _gServerList:
    print("%s  %s  %d seconds" % (server["server"].ljust(_gMaxServerNameLength), server["port"].ljust(11), server["timeout"]))
  print("")
  exit(0)

#####################################################
#####################################################
def _showUsage():
  print("")
  print("Usage: %s -s | {{{<hostName> | <ipAddr>} {<portNum> | <serverName>}} | <unixServerName>} [-t<timeout>]" % os.path.basename(sys.argv[0]))
  print("                      [{{-c <command> | -f <filename>} [rate=<seconds>] [repeat=<count>] [clear]}]")
  print("")
  print("  where:")
  print("")
  print("    -s             - show named servers in pshell-client.conf file")
  print("    -c             - run command from command line")
  print("    -f             - run commands from a batch file")
  print("    -t             - change the default server response timeout")
  print("    hostName       - hostname of UDP server")
  print("    ipAddr         - IP address of UDP server")
  print("    portNum        - port number of UDP server")
  print("    serverName     - name of UDP server from pshell-client.conf file")
  print("    unixServerName - name of UNIX server")
  print("    timeout        - response wait timeout in sec (default=5)")
  print("    command        - optional command to execute (in double quotes, ex. -c \"myCommand arg1 arg2\")")
  print("    fileName       - optional batch file to execute")
  print("    rate           - optional rate to repeat command or batch file (in seconds)")
  print("    repeat         - optional repeat count for command or batch file (default=forever)")
  print("    clear          - optional clear screen between commands or batch file passes")
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
  
  if ((len(sys.argv) < 2) or 
      ((len(sys.argv)) > 8) or 
      ((len(sys.argv) > 1) and (sys.argv[1] == "-h"))):
    _showUsage()

  # make sure we cleanup any system resorces on an abnormal termination
  _registerSignalHandlers()
  
  _gTimeout = 5

  _gPort = PshellServer.UNIX
  
  _gRate = 0
  _gRepeat = 0
  _gIteration = 0
  _gClear = False
  _gFilename = None
  _gCommand = None
  _gInteractive = False
  
  _gDefaultInstallDir = "/etc/pshell"
  _gDefaultConfigDir = _gDefaultInstallDir+"/config"
  _gDefaultBatchDir = _gDefaultInstallDir+"/batch"
  _gClientConfigFile = "pshell-client.conf"
  _gMaxServerNameLength = 11
  _gServerList = []

  _loadServers()

  if sys.argv[1] == "-s":
    _showServers()
  else:
    _gRemoteServer = sys.argv[1]

  needFile = False
  needCommand = False

  for index, arg in enumerate(sys.argv[2:]):
    if "-t" in arg:
      if len(arg) > 2 and arg[2:].isdigit():
        _gTimeout = int(arg[2:])
      else:
        _showUsage()
    elif (arg == "-f"):
      needFile = True
    elif (arg == "-c"):
      needCommand = True
    elif index == 0:
      if (arg.isdigit()):
        _gPort = arg
      elif not _getServer(arg):
        print("")
        print("PSHELL_ERROR: Could not find server: '%s' in file: '%s'" % (arg, _gClientConfigFile))
        _showServers()
    elif "=" in arg:
      rateOrRepeat = arg.split("=")
      if len(rateOrRepeat) == 2 and rateOrRepeat[0] == "rate" and rateOrRepeat[1].isdigit():
        _gRate = int(rateOrRepeat[1])
      elif len(rateOrRepeat) == 2 and rateOrRepeat[0] == "repeat" and rateOrRepeat[1].isdigit():
        _gRepeat = int(rateOrRepeat[1])
      else:
        _showUsage()
    elif (arg == "clear"):
      _gClear = "\033[H\033[J"
    elif (needFile == True):
      if (not arg.isdigit()):
        _gFilename = arg
        needFile = False
      else:
        _showUsage()
    elif (needCommand == True):
      if (not arg.isdigit()):
        _gCommand = arg
        needCommand = False
      else:
        _showUsage()
    else:
      _showUsage()

  # needed filename but it was not fulfilled
  if needFile or needCommand:
    _showUsage()
  
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
  
