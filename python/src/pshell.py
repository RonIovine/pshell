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
the Python version that is provided via the PshellServer.py module, or the
Go (Golang) version that is provided by the PshellServer.go module and API.

Modules:

The following Python modules are provided for programs to embed their own
PshellServer functionality and to programatically control PshellServers
running in other processes.

PshellServer.py   -- provide PshellServer capability in a given process
PshellControl.py  -- invoke PshellServer callback functions in another process
PshellReadline.py -- provide a readline like capability to solicit user input

See the respective 'pydoc' pages (text or HTML) for more detailed documentation
on the above modules.

Usage:

The following shows the usage of the UDP/UNIX pshell interactive client program:

Usage: pshell -s | -n | {{{<hostName | ipAddr>} {<portNum> | <udpServerName>}} | <unixServerName> | <serverIndex} [-t<timeout>]
                        [{{-c <command> | -f <filename>} [rate=<seconds>] [repeat=<count>] [clear]}]

  where:
    -s              - show all servers running on the local host
    -n              - show named IP server/port mappings in pshell-client.conf file
    -c              - run command from command line
    -f              - run commands from a batch file
    -t              - change the default server response timeout
    hostName        - hostname of UDP server
    ipAddr          - IP addr of UDP server
    portNum         - port number of UDP server
    udpServerName   - name of UDP server from pshell-client.conf file
    unixServerName  - name of UNIX server (use '-s' option to list servers)
    serverIndex     - index of local UNIX or UDP server (use '-s' option to list servers)
    timeout         - response wait timeout in sec (default=5)
    command         - optional command to execute (in double quotes, ex. -c "myCommand arg1 arg2")
    fileName        - optional batch file to execute
    rate            - optional rate to repeat command or batch file (in seconds)
    repeat          - optional repeat count for command or batch file (default=forever)
    clear           - optional clear screen between commands or batch file passes

    NOTE: If no <command> is given, pshell will be started
          up in interactive mode, commands issued in command
          line mode that require arguments must be enclosed
          in double quotes, commands issued in interactive
          mode that require arguments do not require double
          quotes.

          To get help on a command in command line mode, type
          "<command> ?" or "<command> -h".  To get help in
          interactive mode type 'help' or '?' at the prompt to
          see all available commands, to get help on a single
          command, type '<command> {? | -h}'.  Use TAB completion
          to fill out partial commands and up-arrow to recall
          for command history.

"""

# import all our necessary module
import os
import fnmatch
import fcntl
import sys
import signal
import time
import commands
import PshellControl
import PshellServer
import PshellReadline

_gControlName = "pshellClient"
_gHelp = ('?', '-h', '--h', '-help', '--help', 'help')

_gFileSystemPath = "/tmp/.pshell/"
_gLockFileExtension = ".lock"
_gUnixLockFileId = "unix"+_gLockFileExtension
_gActiveServers = []
_gBatchFiles = {"maxFilenameLength":8, "maxDirectoryLength":9, "files":[]}

#################################################################################
#################################################################################
def _isFloat(string):
  try:
    float(string)
    return True
  except:
    return False

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
  print("#  Command response timeout: NONE")
  print("#")
  print("#  Type '?' or 'help' at prompt for command summary")
  print("#  Type '?' or '-h' after command for command usage")
  print("#")
  print("#  Full <TAB> completion, command history, command")
  print("#  line editing, and command abbreviation supported")
  print("#")
  print("#  NOTE: Connected to a broadcast address, all commands")
  print("#        are single-shot, 'fire-and-forget', with no")
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
  print("quit     -  exit interactive mode")
  print("help     -  show all available commands")
  print("history  -  show history list of all entered commands")
  print("")
  print("NOTE: Connected to a broadcast address, all remote server")
  print("      commands are 'invisible' to this client application")
  print("      and are single-shot, 'fire-and-forget', with no response")
  print("      requested or expected, and no results displayed")
  print("")

#################################################################################
#################################################################################
def _processCommand(command_):
  global _gControlName
  if ((command_.split()[0] == "?") or (PshellReadline.isSubString(command_.split()[0], "help", 2))):
    _showHelp()
  elif PshellReadline.isSubString(command_.split()[0], "history", 2):
    PshellReadline._showHistory()
  else:
    PshellControl.sendCommand1(_gControlName, command_)

#################################################################################
#################################################################################
def _comandDispatcher(args_):
  global _gControlName
  global _gHelp
  global _gInteractive
  global _gTimeout
  results = None
  timeout = PshellServer._gPshellClientTimeout
  if PshellServer._gClientTimeoutOverride:
    if len(PshellServer._gClientTimeoutOverride) > 2:
      timeout = int(PshellServer._gClientTimeoutOverride[2:])
  command = ' '.join(args_)
  if args_[0] in _gHelp:
    results = PshellControl.extractCommands(_gControlName, includeName=False)
  elif timeout == PshellControl.NO_WAIT:
    # if they asked for command help, go ahead and dispatch the command and
    # extract the results, otherwise, just send command with no extraction
    # or display
    if len(args_) == 2 and args_[1] in _gHelp:
      retCode = PshellControl._sendUserCommand(_gControlName, PshellControl.ONE_SEC*5, command)
    else:
      print("PSHELL_INFO: Command sent fire-and-forget, no response requested")
      PshellControl.sendCommand2(_gControlName, PshellControl.NO_WAIT, command)
      return
  else:
    # normal user command
    retCode = PshellControl._sendUserCommand(_gControlName, PshellControl.ONE_SEC*timeout, command)
  while retCode != PshellControl._COMMAND_COMPLETE and retCode != PshellControl.SOCKET_TIMEOUT:
    if (timeout > PshellControl.NO_WAIT):
      results, retCode = PshellControl._receiveUserCommand(_gControlName, PshellControl.ONE_SEC*timeout)
    else:
      results, retCode = PshellControl._receiveUserCommand(_gControlName, PshellControl.WAIT_FOREVER)
    if results != None:
      if _gInteractive == True:
        PshellServer.printf(results, newline=False)
      else:
        # command line mode
        sys.stdout.write(results)
  if retCode == PshellControl.SOCKET_TIMEOUT:
    PshellServer.printf("PSHELL_ERROR: Response timeout from remote pshellServer")

#################################################################################
#################################################################################
def _getIpAddress():
  global _gRemoteServer
  global _gPort
  if (_gPort == PshellControl.UNIX):
    return (_gPort)
  elif (_gRemoteServer == "localhost"):
    return ("127.0.0.1"+":"+str(_gPort))
  else:
    return (_gRemoteServer+":"+str(_gPort))

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
  global _gTimeout
  PshellServer._gPshellClientTimeout = _gTimeout
  command = _gCommand.split()
  if _gRate >= 0 and _gRepeat == 0:
    sys.stdout.write("\033]0;{}: {}[{}], Mode: COMMAND LINE[{}], Rate: {} SEC\007".format(_gTitle, _gServerName, _getIpAddress(), _gCommand, _gRate))
  while (True):
    if (_gRepeat > 0):
      _gIteration += 1
      if _gRate >= 0:
        sys.stdout.write("\033]0;{}: {}[{}], Mode: COMMAND LINE[{}], Rate: {} SEC, Iteration: {} of {}\007".format(_gTitle,
                                                                                                                   _gServerName,
                                                                                                                   _getIpAddress(),
                                                                                                                   _gCommand,
                                                                                                                   _gRate,
                                                                                                                   _gIteration,
                                                                                                                   _gRepeat))
      else:
        sys.stdout.write("\033]0;{}: {}[{}], Mode: COMMAND LINE[{}], Iteration: {} of {}\007".format(_gTitle,
                                                                                                     _gServerName,
                                                                                                     _getIpAddress(),
                                                                                                     _gCommand,
                                                                                                     _gIteration,
                                                                                                     _gRepeat))
    if (_gClear != False):
      sys.stdout.write(_gClear)
    _comandDispatcher(command)
    if _gRepeat > 0 and _gIteration == _gRepeat:
      break
    elif _gRate >= 0:
      time.sleep(_gRate)
    elif _gRepeat == 0:
      break

#################################################################################
#################################################################################
def findBatchFiles(directory_):
  global _gBatchFiles
  if directory_ != None and os.path.isdir(directory_):
    fileList = commands.getoutput("ls {}".format(directory_)).split()
    for filename in fileList:
      if ".psh" in filename or ".batch" in filename:
        _gBatchFiles["files"].append({"filename":filename, "directory":directory_})
        _gBatchFiles["maxFilenameLength"] = max(_gBatchFiles["maxFilenameLength"], len(filename))
        _gBatchFiles["maxDirectoryLength"] = max(_gBatchFiles["maxDirectoryLength"], len(directory_))

#################################################################################
#################################################################################
def showBatchFiles():
  global _gBatchFiles
  print("")
  print("***********************************************")
  print("*            AVAILABLE BATCH FILES            *")
  print("***********************************************")
  print("")
  print("%s   %-*s   %-*s" % ("Index", _gBatchFiles["maxFilenameLength"], "Filename", _gBatchFiles["maxDirectoryLength"], "Directory"))
  print("%s   %s   %s" % ("=====", "="*_gBatchFiles["maxFilenameLength"], "="*_gBatchFiles["maxDirectoryLength"]))
  for index, entry in enumerate(_gBatchFiles["files"]):
    print("%-5d   %-*s   %-*s" % (index+1, _gBatchFiles["maxFilenameLength"], entry["filename"], _gBatchFiles["maxDirectoryLength"], entry["directory"]))
  print("")

#################################################################################
#################################################################################
def getBatchFile(filename_):
  global _gBatchFiles
  global _gDefaultBatchDir

  batchFile = None

  _gBatchFiles["files"] = []
  _gBatchFiles["maxDirectoryLength"] = 9
  _gBatchFiles["maxFilenameLength"] = 8

  findBatchFiles(os.getcwd())
  findBatchFiles(os.getenv('PSHELL_BATCH_DIR'))
  findBatchFiles(_gDefaultBatchDir)

  if filename_ == "?" or filename_ == "-h":
    print("")
    _showUsage()
    print("")
    print("  where:")
    print("    filename  - Filename of the batch file to execute")
    print("    index     - Index of the batch file to execute (from the -list option)")
    print("    -list     - List all the available batch files")
    print("    -show     - Show the contents of the batch file without executing")
    print("")
    print("  NOTE: Batch files must have a .psh or .batch extension.  Batch")
    print("        files will be searched in the following directory order:")
    print("")
    print("        current directory - {}".format(os.getcwd()))
    print("        $PSHELL_BATCH_DIR - {}".format(os.getenv('PSHELL_BATCH_DIR')))
    print("        default directory - {}".format(_gDefaultBatchDir))
    print("")
  elif (filename_ in "batch"):
    _showUsage()
  elif (PshellServer.isSubString(filename_, "-list", 2)):
    showBatchFiles()
  elif (PshellServer.isDec(filename_)):
    if (int(filename_) > 0 and int(filename_) <= len(_gBatchFiles["files"])):
      batchFile = _gBatchFiles["files"][int(filename_)-1]["directory"] + "/" + _gBatchFiles["files"][int(filename_)-1]["filename"]
    else:
      print("ERROR: Invalid batch file index: {}, valid values 1-{}".format(filename_, len(_gBatchFiles["files"])))
  else:
    numMatches = 0
    for index, entry in enumerate(_gBatchFiles["files"]):
      if (PshellServer.isSubString(filename_, entry["filename"], len(filename_))):
        batchFile = entry["directory"] + "/" + entry["filename"]
        numMatches += 1
    if (numMatches == 0):
      print("PSHELL_ERROR: Could not find batch file: '{}', use -list option to see available files".format(filename_))
    elif (numMatches > 1):
      print("PSHELL_ERROR: Ambiguous file: '{}', use -list option to see available files or <index> to select specific file".format(filename_))
      return (None)
  return (batchFile)

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
  global _gTimeout
  PshellServer._gPshellClientTimeout = _gTimeout
  batchFile = getBatchFile(_gFilename)
  if (batchFile != None and os.path.isfile(batchFile)):
    # we found a batch file, process it
    file = open(batchFile, 'r')
    if _gRate >= 0 and _gRepeat == 0:
      sys.stdout.write("\033]0;{}: {}[{}], Mode: BATCH[{}], Rate: {} SEC\007".format(_gTitle, _gServerName, _getIpAddress(), _gFilename, _gRate))
    while (True):
      if (_gClear != False):
        sys.stdout.write(_gClear)
      file.seek(0, 0)
      if (_gRepeat > 0):
        _gIteration += 1
        if _gRate >= 0:
          sys.stdout.write("\033]0;{}: {}[{}], Mode: BATCH[{}], Rate: {} SEC, Iteration: {} of {}\007".format(_gTitle,
                                                                                                              _gServerName,
                                                                                                              _getIpAddress(),
                                                                                                              _gFilename,
                                                                                                              _gRate,
                                                                                                              _gIteration,
                                                                                                              _gRepeat))
        else:
          sys.stdout.write("\033]0;{}: {}[{}], Mode: BATCH[{}], Iteration: {} of {}\007".format(_gTitle,
                                                                                                _gServerName,
                                                                                                _getIpAddress(),
                                                                                                _gFilename,
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
      elif _gRate >= 0:
        time.sleep(_gRate)
      elif _gRepeat == 0:
        break
    file.close()

#################################################################################
#################################################################################
def _configureLocalServer():
  global _gControlName
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
  # set the client timeout so we can show a warning message in the
  # startup banner
  PshellServer._gPshellClientTimeout = _gTimeout
  # supress the automatic invalid arg count message from the PshellControl.py
  # module so we can display the returned usage
  PshellControl._gSupressInvalidArgCountMessage = True
  # extract information from our remote server via the special
  # "private" control API so we can feed the info to our local
  # pshell server to make it look like a remote server

  prompt = PshellControl._extractPrompt(_gControlName)
  if (len(prompt) > 0):
    PshellServer._gPromptOverride = prompt

  title = PshellControl._extractTitle(_gControlName)
  if (len(title) > 0):
    PshellServer._gTitleOverride = title

  serverName = PshellControl._extractName(_gControlName)
  if (len(serverName) > 0):
    PshellServer._gServerNameOverride = serverName

  banner = PshellControl._extractBanner(_gControlName)
  if (len(banner) > 0):
    PshellServer._gBannerOverride = banner

  if (_gPort == PshellControl.UNIX):
    PshellServer._gServerTypeOverride = PshellServer.UNIX
  elif (_gRemoteServer == "localhost"):
    PshellServer._gServerTypeOverride = "127.0.0.1"
  else:
    PshellServer._gServerTypeOverride = _gRemoteServer

  if _gPort != PshellServer.UNIX:
     PshellServer._gServerTypeOverride += ":{}".format(_gPort)

#####################################################
#####################################################
def _getNamedServer(arg):
  global _gServerList
  global _gPort
  global _gTimeout
  numFound = 0
  foundIndex = 0
  for index, server in enumerate(_gServerList):
    #print("arg: {}, server: {}".format(arg, server["server"][:len(arg)]))
    if len(server["server"]) == len(arg) and server["server"] == arg:
      # exact match found, assumes no duplicates in list
      numFound = 1
      foundIndex = index
      break
    elif arg == server["server"][:len(arg)]:
      numFound += 1
      foundIndex = index
  if numFound == 1:
    _gPort = _gServerList[foundIndex]["port"]
    _gTimeout = _gServerList[foundIndex]["timeout"]
    return True
  elif numFound == 0:
    print("")
    print("PSHELL_ERROR: Could not find server: '%s' in file: '%s'" % (arg, _gClientConfigFile))
  elif numFound > 1:
    print("")
    print("PSHELL_ERROR: Ambiguous server name: '%s' found in file: '%s'" % (arg, _gClientConfigFile))
  return False

#####################################################
#####################################################
def _loadServers():
  global _gDefaultConfigDir
  global _gMaxNamedServerLength
  global _gServerList
  global _gClientConfigFile
  _gServerList = []
  _gMaxNamedServerLength = 11
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
        _gMaxNamedServerLength = max(_gMaxNamedServerLength, len(server[0]))
        _gServerList.append({"server":server[0], "port":server[1], "timeout":5})
      elif len(server) == 3:
        _gMaxNamedServerLength = max(_gMaxNamedServerLength, len(server[0]))
        _gServerList.append({"server":server[0], "port":server[1], "timeout":int(server[2])})
  file.close()

#####################################################
#####################################################
def _cleanupFileSystemResources():
  global _gFileSystemPath
  global _gLockFileExtension
  global _gActiveServers
  global _gMaxHostnameLength
  global _gMaxActiveServerLength
  global _gUnixLockFileId
  if not os.path.isdir(_gFileSystemPath):
    os.system("mkdir %s" % _gFileSystemPath)
    os.system("chmod 777 %s" % _gFileSystemPath)
  lockFiles = fnmatch.filter(os.listdir(_gFileSystemPath), "*"+_gLockFileExtension)
  lockFiles.sort()
  for file in lockFiles:
    try:
      fd = open(_gFileSystemPath+file, "r")
      try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        # we got the lock, delete any socket file and lock file and don't print anything
        try:
          # unix temp socket file
          if _gUnixLockFileId in file:
            os.unlink(_gFileSystemPath+file.split("-")[0])
        except:
          None
        try:
          # lockfile
          os.unlink(_gFileSystemPath+file)
        except:
          None
      except:
        # file handle is in use and locked by another process, print it
        if "-control" not in file:
          server = file.split(".")[0]
          server = server.split("-")
          _gMaxActiveServerLength = max(len(server[0]), _gMaxActiveServerLength)
          if len(server) == 2:
            _gActiveServers.append({"name":server[0], "type":server[1], "host":"N/A", "port":"N/A"})
          elif len(server) == 4:
            _gActiveServers.append({"name":server[0], "type":server[1], "host":server[2], "port":server[3]})
            _gMaxHostnameLength = max(len(server[2]), _gMaxHostnameLength)
    except:
      None

#####################################################
#####################################################
def _getActiveServer(arg):
  global _gActiveServers
  global _gRemoteServer
  global _gMaxHostnameLength
  global _gPort
  try:
    int(arg, 10)
    isInt = True
  except:
    isInt = False
  if isInt:
    index = int(arg)
    if index-1 >= 0 and index-1 < len(_gActiveServers):
      if _gActiveServers[index-1]["type"] == "unix":
        _gRemoteServer = _gActiveServers[index-1]["name"]
        return True
      elif _gActiveServers[index-1]["type"] == "udp":
        if _gActiveServers[index-1]["host"] == "anyhost":
          _gRemoteServer = "localhost"
        elif _gActiveServers[index-1]["host"] == "anybcast":
          _gRemoteServer = "255.255.255.255"
        else:
          _gRemoteServer = _gActiveServers[index-1]["host"]
        _gPort = _gActiveServers[index-1]["port"]
        return True
      else:
        print("")
        print("PSHELL_ERROR: Cannot use 'pshell' client for TCP server, use 'telnet' instead")
        _showActiveServers()
    else:
      print("")
      print("PSHELL_ERROR: Index: %d out of range for server, valid range: 1-%d" % (index, len(_gActiveServers)))
      _showActiveServers()
  else:
    # they did not enter an integer value, look for a matching server name, we only do this
    # for UNIX servers sinced they are guaranteed to be unique, whereas an IP based server
    # can have the same name running different instances on different ports
    numFound = 0
    for server in _gActiveServers:
      if len(server["name"]) == len(arg) and server["name"] == arg and server["type"] == "unix":
        # exact match found, assumes no duplicates in list
        numFound = 1
        break
      elif arg == server["name"][:len(arg)] and server["type"] == "unix":
        numFound += 1
    if numFound == 1:
      _gRemoteServer = server["name"]
      return True
    else:
      return False

#####################################################
#####################################################
def _showActiveServers():
  global _gActiveServers
  global _gMaxHostnameLength
  global _gMaxActiveServerLength
  tcpFound = False
  udpFound = False
  unixFound = False
  print("")
  print("***************************************************")
  print("*   Active PSHELL Servers Running On Local Host   *")
  print("***************************************************")
  print("")
  if len(_gActiveServers) > 0:
    print("Index   %s   Type   Host%s   Port" % ("Server Name".ljust(_gMaxActiveServerLength), " "*(_gMaxHostnameLength-4)))
    print("=====   %s   ====   %s   =====" % ("="*_gMaxActiveServerLength, "="*_gMaxHostnameLength))
  for index, server in enumerate(_gActiveServers):
    if server["type"] == "tcp":
      tcpFound = True
    if server["type"] == "udp":
      udpFound = True
    if server["type"] == "unix":
      unixFound = True
    print("%-5d   %s   %-4s   %s   %-4s" % (index+1, server["name"].ljust(_gMaxActiveServerLength), server["type"], server["host"].ljust(_gMaxHostnameLength), server["port"]))
  if len(_gActiveServers) > 0:
    print("")
    if tcpFound:
      print("Connect to TCP server with: telnet <host> <port>")
    if udpFound:
      print("Connect to UDP server with: pshell {<host> <port>} | <index>")
    if unixFound:
      print("Connect to UNIX server with: pshell <name> | <index>")
    print("")
  exit(0)

#####################################################
#####################################################
def _showNamedServers():
  global _gServerList
  global _gMaxNamedServerLength
  print("")
  print("******************************************")
  print("*     Available Named PSHELL Servers     *")
  print("******************************************")
  print("")
  print("%s  Port Number  Response Timeout" % "Server Name".ljust(_gMaxNamedServerLength))
  print("%s  ===========  ================" % ("="*_gMaxNamedServerLength))
  for server in _gServerList:
    print("%s  %s  %d seconds" % (server["server"].ljust(_gMaxNamedServerLength), server["port"].ljust(11), server["timeout"]))
  print("")
  exit(0)

#####################################################
#####################################################
def _showUsage():
  print("")
  print("Usage: %s -s | -n | {{{<hostName> | <ipAddr>} {<portNum> | <udpServerName>}} | <unixServerName> | <serverIndex>} [-t<timeout>]" % os.path.basename(sys.argv[0]))
  print("                           [{{-c <command> | -f <filename>} [rate=<seconds>] [repeat=<count>] [clear]}]")
  print("")
  print("  where:")
  print("    -s              - show all servers running on the local host")
  print("    -n              - show named IP server/port mappings in pshell-client.conf file")
  print("    -c              - run command from command line")
  print("    -f              - run commands from a batch file")
  print("    -t              - change the default server response timeout")
  print("    hostName        - hostname of UDP server")
  print("    ipAddr          - IP address of UDP server")
  print("    portNum         - port number of UDP server")
  print("    udpServerName   - name of UDP server from pshell-client.conf file")
  print("    unixServerName  - name of UNIX server (use '-s' option to list servers)")
  print("    serverIndex     - index of local UNIX or UDP server (use '-s' option to list servers)")
  print("    timeout         - response wait timeout in sec (default=5)")
  print("    command         - optional command to execute (in double quotes, ex. -c \"myCommand arg1 arg2\")")
  print("    fileName        - optional batch file to execute")
  print("    rate            - optional rate to repeat command or batch file (in seconds)")
  print("    repeat          - optional repeat count for command or batch file (default=forever)")
  print("    clear           - optional clear screen between commands or batch file passes")
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

  _gRate = -1
  _gRepeat = 0
  _gIteration = 0
  _gClear = False
  _gFilename = None
  _gCommand = None
  _gInteractive = False
  _gMaxHostnameLength = 4

  _gDefaultInstallDir = "/etc/pshell"
  _gDefaultConfigDir = _gDefaultInstallDir+"/config"
  _gDefaultBatchDir = _gDefaultInstallDir+"/batch"
  _gClientConfigFile = "pshell-client.conf"
  _gMaxNamedServerLength = 11
  _gMaxActiveServerLength = 11
  _gServerList = []

  _loadServers()
  _cleanupFileSystemResources()

  if sys.argv[1] == "-n":
    _showNamedServers()
  elif sys.argv[1] == "-s":
    _showActiveServers()
  elif not _getActiveServer(sys.argv[1]):
    _gRemoteServer = sys.argv[1]

  needFile = False
  needCommand = False

  for index, arg in enumerate(sys.argv[2:]):
    if "-t" in arg:
      if len(arg) > 2 and (arg[2:].isdigit() or arg[2:] == "-1"):
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
      elif not _getNamedServer(arg):
        _showNamedServers()
    elif "=" in arg:
      rateOrRepeat = arg.split("=")
      if len(rateOrRepeat) == 2 and rateOrRepeat[0] == "rate" and _isFloat(rateOrRepeat[1]):
        _gRate = float(rateOrRepeat[1])
      elif len(rateOrRepeat) == 2 and rateOrRepeat[0] == "repeat" and rateOrRepeat[1].isdigit():
        _gRepeat = int(rateOrRepeat[1])
      else:
        _showUsage()
    elif (arg == "clear"):
      _gClear = "\033[H\033[J"
    elif (needFile == True):
      _gFilename = arg
      needFile = False
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
  PshellControl.connectServer(_gControlName, _gRemoteServer, _gPort, PshellControl.ONE_SEC*_gTimeout)
  _gServerName = PshellControl._extractName(_gControlName)
  _gTitle = PshellControl._extractTitle(_gControlName)

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
      commandList = PshellControl.extractCommands(_gControlName)
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

      _gServerName = "broadcastServer"

      # add some TAB completors
      PshellReadline.addTabCompletion("quit")
      PshellReadline.addTabCompletion("help")
      PshellReadline.addTabCompletion("history")

      _showWelcome()

      command = ""
      prompt = "%s[%s:%s]:PSHELL> " % (_gServerName, _gRemoteServer, str(_gPort))
      while (not PshellReadline.isSubString(command, "quit")):
        (command, idleSession) = PshellReadline.getInput(prompt)
        if (not PshellReadline.isSubString(command, "quit")):
          _processCommand(command)

  _cleanupAndExit()
