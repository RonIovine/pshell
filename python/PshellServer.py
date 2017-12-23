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
A Python module to provide remote command line access to external processes

This API provides the Process Specific Embedded Command Line Shell (PSHELL)
user API functionality.  It provides the ability for a client program to
register functions that can be invoked via a command line user interface.
There are several ways to invoke these embedded functions based on how the 
pshell server is configured, which is described in documentation further down 
in this file.
 
This module provides the same functionality as the PshellServer.h API and
the libpshell-server linkable 'C' library, but implemented as a Python module.  
It supports all the server types as the corresponding 'C' impelmentation, TCP, 
UDP, UNIX and LOCAL server types are all supported.  Applications using this 
module can be controlled via the pshell UDP/UNIX stand-alone 'C' client program 
or via the PshellControl.h/libpshell-control or PshellControl.py 
control mechanism.

Functions:

addCommand()       -- register a pshell command with the server
startServer()      -- start the pshell server
cleanupResources() -- release all resources claimed by the server
runCommand()       -- run a registered command from the parent (i.e. registering) program

The following commands can only be called from within the context of 
a pshell callback function

printf()      -- display a message from a pshell callback function to the client
flush()       -- flush the transfer buffer to the client (UDP/UNIX servers only)
wheel()       -- spinning ascii wheel to keep UDP/UNIX client alive
march()       -- marching ascii character to keep UDP/UNIX client alive
showUsage()   -- show the usage the command is registered with
isHelp()      -- checks if the user has requested help on this command
isSubString() -- checks for string1 substring of string2 at position 0

Integer constants:

These are the identifiers for the serverMode.  BLOCKING wil never return 
control to the caller of startServer, NON_BLOCKING will spawn a thread to 
run the server and will return control to the caller of startServer

BLOCKING
NON_BLOCKING

String constants: 
 
Valid server types, UDP/UNIX servers require the 'pshell' or 'pshell.py'
client programs, TCP servers require a 'telnet' client, local servers
require no client (all user interaction done directly with server running
in the parent host program)

UDP
TCP
UNIX
LOCAL

These two identifiers that can be used for the hostnameOrIpAddr argument 
of the startServer call.  PshellServer.ANYHOST will bind the server socket
to all interfaces of a multi-homed host, PshellServer.LOCALHOST will bind 
the server socket to the local loopback address (i.e. 127.0.0.1)

ANYHOST
LOCALHOST

A complete example of the usage of the API can be found in the included 
demo program pshellServerDemo.py.
"""

# import all our necessary modules
import sys
import os
import time
import select
import socket
import struct
import random
import thread
import PshellReadline
from collections import OrderedDict
from collections import namedtuple

# dummy variables so we can create pseudo end block indicators, add these 
# identifiers to your list of python keywords in your editor to get syntax 
# highlighting on these identifiers, sorry Guido
_enddef = _endif = _endwhile = _endfor = None

#################################################################################
#
# global "public" data, these are used for various parts of the public API
#
#################################################################################

# Valid server types, UDP/UNIX servers require the 'pshell' or 'pshell.py'
# client programs, TCP servers require a 'telnet' client, local servers
# require no client (all user interaction done directly with server running
# in the parent host program)
UDP = "udp"
TCP = "tcp"
UNIX = "unix"
LOCAL = "local"

# These are the identifiers for the serverMode.  BLOCKING wil never return 
# control to the caller of startServer, NON_BLOCKING will spawn a thread to 
# run the server and will return control to the caller of startServer
BLOCKING = 0
NON_BLOCKING = 1

# These two identifiers that can be used for the hostnameOrIpAddr argument 
# of the startServer call.  PshellServer.ANYHOST will bind the server socket
# to all interfaces of a multi-homed host, PshellServer.LOCALHOST will bind 
# the server socket to the local loopback address (i.e. 127.0.0.1)
ANYHOST = "anyhost"
LOCALHOST = "localhost"

#################################################################################
#
# "public" API functions
#
# Users of this module should only access functionality via these "public"
# methods.  This is broken up into "public" and "private" sections for 
# readability and to not expose the implementation in the API definition
#
#################################################################################

#################################################################################
#################################################################################
def addCommand(function, command, description, usage = None, minArgs = 0, maxArgs = 0, showUsage = True):
  """
  Register callback commands to our PSHELL server.  If the command takes no
  arguments, the default parameters can be provided.  If the command takes
  an exact number of parameters, set minArgs and maxArgs to be the same.  If
  the user wants the callback function to handle all help initiated usage,
  set the showUsage parameter to False.

    Args:
        function (ptr)    : User callback function
        command (str)     : Command to dispatch the function (single keyword only)
        description (str) : One line description of command
        usage (str)       : One line command usage (Unix style preferred)
        minArgs (int)     : Minimum number of required arguments
        maxArgs (int)     : Maximum number of required arguments
        showUsage (bool)  : Show registered usage on a '?' or '-h'

    Returns:
        none
  """
  _addCommand(function, command, description, usage, minArgs,  maxArgs,  showUsage)
_enddef

#################################################################################
#################################################################################
def startServer(serverName, serverType, serverMode, hostnameOrIpAddr = None, port = 0):
  """
  Start our PSHELL server, if serverType is UNIX or LOCAL, the default
  parameters can be used, and will be ignored if provided.  All of these
  parameters except serverMode can be overridden on a per serverName
  basis via the pshell-server.conf config file.  All commands in the
  <serverName>.startup file will be executed in this function at server
  startup time.

    Args:
        serverName (str)       : Logical name of the Pshell server
        serverType (str)       : Desired server type (UDP, UNIX, TCP, LOCAL)
        serverMode (str)       : Desired server mode (BLOCKING, NON_BLOCKING)
        hostnameOrIpAddr (str) : Hostname or IP address to run server on
        port (int)             : Port number to run server on (UDP or TCP only)

    Returns:
        none
  """
  _startServer(serverName, serverType, serverMode, hostnameOrIpAddr, port)
_enddef

#################################################################################
#################################################################################
def cleanupResources():
  """
  Cleanup and release any system resources claimed by this module.  This includes
  any open socked handles, file descriptors, or system 'tmp' files.  This should
  be called at program exit time as well as any signal exception handler that
  results in a program termination.

    Args:
        none

    Returns:
        none
  """
  _cleanupResources()
_enddef

#################################################################################
#################################################################################
def runCommand(command):
  """
  Run a registered command from within its parent process

    Args:
        command (str) : Registered callback command to run

    Returns:
        none
  """
  _runCommand(command)
_enddef

#################################################################################
#
# The following public functions should only be called from within a 
# registered callback function
#
#################################################################################

#################################################################################
#################################################################################
def printf(message = "\n"):
  """
  Display data back to the remote client

    Args:
        string (str) : Message to display to the client user

    Returns:
        none
  """
  _printf(message)
_enddef

#################################################################################
#################################################################################
def flush():
  """
  Flush the reply (i.e. display) buffer back to the remote client

    Args:
        none

    Returns:
        none
  """
  _flush()
_enddef

#################################################################################
#################################################################################

#################################################################################
#################################################################################
def wheel(string = None):
  """
  Spinning ascii wheel keep alive, user string string is optional

    Args:
        string (str) : String to display before the spinning wheel

    Returns:
        none
  """
  _wheel(string)
_enddef

#################################################################################
#################################################################################
def march(string):
  """
  March a string or character keep alive across the screen

    Args:
        string (str) : String or char to march across the screen

    Returns:
        none
  """
  _march(string)
_enddef

#################################################################################
#################################################################################
def showUsage():
  """
  Show the command's registered usage

    Args:
        none

    Returns:
        none
  """
  _showUsage()
_enddef

#################################################################################
#################################################################################
def isHelp():
  """
  Check if the user has asked for help on this command.  Command must be 
  registered with the showUsage = False option.

    Args:
        none

    Returns:
        bool : True if user is requesting command help, False otherwise
  """
  return (_isHelp())
_enddef

#################################################################################
#################################################################################
def isSubString(string1, string2, minMatchLength = 0):
  """
  This function will return True if string1 is a substring of string2 at 
  position 0.  If the minMatchLength is 0, then it will compare up to the
  length of string1.  If the minMatchLength > 0, it will require a minimum
  of that many characters to match.  A string that is longer than the min
  match length must still match for the remaining charactes, e.g. with a
  minMatchLength of 2, 'q' will not match 'quit', but 'qu', 'qui' or 'quit'
  will match, 'quix' will not match.  This function is useful for wildcard
  matching.

    Args:
        string1 (str)        : The substring
        string2 (str)        : The target string
        minMatchLength (int) : The minimum required characters match

    Returns:
        bool : True is substring matches, False otherwise
  """
  return (_isSubString(string1, string2, minMatchLength))
_enddef

#################################################################################
#
# "private" functions and data
#
# Users of this module should never access any of these "private" items directly,
# these are meant to hide the implementation from the presentation of the public
# API above
#
#################################################################################
 
#################################################################################
#################################################################################
def _isSubString(string1_, string2_, minMatchLength_):
  return (PshellReadline.isSubString(string1_, string2_, minMatchLength_))
_enddef

#################################################################################
#################################################################################
def _addCommand(function_, command_, description_, usage_, minArgs_,  maxArgs_, showUsage_, prepend_ = False):
  global _gCommandList
  global _gMaxLength
  for command in _gCommandList:
    if (command["name"] == command_):
      # command name already exists, don't add it again
      print "PSHELL_ERROR: Command: %s already exists, not adding command" % command_
      return
    _endif
  _endfor
  if (len(command_) > _gMaxLength):
    _gMaxLength = len(command_)
  _endif
  if (prepend_ == True):
    _gCommandList.insert(0, {"function":function_, "name":command_, "description":description_, "usage":usage_, "minArgs":minArgs_, "maxArgs":maxArgs_, "showUsage":showUsage_})
  else:
    _gCommandList.append({"function":function_, "name":command_, "description":description_, "usage":usage_, "minArgs":minArgs_, "maxArgs":maxArgs_, "showUsage":showUsage_})
  _endif
_enddef

#################################################################################
#################################################################################
def _startServer(serverName_, serverType_, serverMode_, hostnameOrIpAddr_, port_):
  global _gServerName
  global _gServerType
  global _gServerMode
  global _gHostnameOrIpAddr
  global _gPort
  global _gPrompt
  global _gTitle
  global _gBanner
  global gRunning
  global _gTcpTimeout
  
  if (_gRunninig == False):
    gRunning = True
    _gServerName = serverName_
    _gServerMode = serverMode_  
    (_gTitle, _gBanner, _gPrompt, _gServerType, _gHostnameOrIpAddr, _gPort, _gTcpTimeout) = _loadConfigFile(_gServerName, _gTitle, _gBanner, _gPrompt, serverType_, hostnameOrIpAddr_, port_, _gTcpTimeout)
    _loadStartupFile()  
    if (_gServerMode == BLOCKING):
      _runServer()
    else:
      # spawn thread
      thread.start_new_thread(_serverThread, ())
    _endif
  _endif
_enddef

#################################################################################
#################################################################################
def _serverThread():
  _runServer()
_enddef
  
#################################################################################
#################################################################################
def _createSocket():
  global _gServerName
  global _gServerType
  global _gHostnameOrIpAddr
  global _gPort
  global _gSocketFd
  global _gUnixSocketPath
  global _gUnixSourceAddress
  if (_gServerType == UDP):
    # IP domain socket (UDP)
    _gSocketFd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if (_gHostnameOrIpAddr == ANYHOST):
      _gSocketFd.bind((_NULL, _gPort))
    elif (_gHostnameOrIpAddr == LOCALHOST):
      _gSocketFd.bind(("127.0.0.1", _gPort))
    else:
      _gSocketFd.bind((_gHostnameOrIpAddr, _gPort))
    _endif
  elif (_gServerType == TCP):
    # IP domain socket (TCP)
    _gSocketFd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    _gSocketFd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    # Bind the socket to the port
    if (_gHostnameOrIpAddr == ANYHOST):
      _gSocketFd.bind((_NULL, _gPort))
    elif (_gHostnameOrIpAddr == LOCALHOST):
      _gSocketFd.bind(("127.0.0.1", _gPort))
    else:
      _gSocketFd.bind((_gHostnameOrIpAddr, _gPort))
    _endif  
    # Listen for incoming connections
    _gSocketFd.listen(1)
  elif (_gServerType == UNIX):
    # UNIX domain socket
    _gSocketFd = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    _gUnixSourceAddress = _gUnixSocketPath+_gServerName
    # cleanup any old handle that might be hanging around
    if (os.path.isfile(_gUnixSourceAddress)):
      os.unlink(_gUnixSourceAddress)
    _endif
    _gSocketFd.bind(_gUnixSourceAddress)
  _endif
  return (True)
_enddef

#################################################################################
#################################################################################
def _runServer():
  global _gServerType
  if (_gServerType == UDP):
    _runUDPServer()
  elif (_gServerType == TCP):
    _runTCPServer()
  elif (_gServerType == UNIX):
    _runUNIXServer()
  else:  # local server 
    _runLocalServer()
  _endif
_enddef

#################################################################################
#################################################################################
def _runUDPServer():
  global _gServerName
  global _gHostnameOrIpAddr
  global _gPort
  print "PSHELL_INFO: UDP Server: %s Started On Host: %s, Port: %d" % (_gServerName, _gHostnameOrIpAddr, _gPort)
  # startup our UDP server
  _addCommand(_batch, "batch", "run commands from a batch file", "<filename>", 1, 1, True, True)
  if (_createSocket()):
    while (True):
      _receiveDGRAM()
    _endwhile
  _endif
_enddef

#################################################################################
#################################################################################
def _runUNIXServer():
  global _gServerName
  print "PSHELL_INFO: UNIX Server: %s Started" % _gServerName
  # startup our UNIX server
  _addCommand(_batch, "batch", "run commands from a batch file", "<filename>", 1, 1, True, True)
  if (_createSocket()):
    while (True):
      _receiveDGRAM()
    _endwhile
  _endif
_enddef

#################################################################################
#################################################################################
def _acceptConnection():
  global _gSocketFd
  global _gConnectFd
  global _gTcpConnectSockName
  (_gConnectFd, clientAddr) = _gSocketFd.accept()
  _gTcpConnectSockName = clientAddr[0]
  return (True)
_enddef

#################################################################################
#################################################################################
def _runTCPServer():
  global _gSocketFd
  global _gConnectFd
  global _gServerName
  global _gHostnameOrIpAddr
  global _gTitle
  global _gPort
  global _gPrompt
  global _gTcpPrompt
  global _gTcpTitle
  global _gTcpInteractivePrompt
  global _gTcpConnectSockName
  global _gTcpTimeout
  print "PSHELL_INFO: TCP Server: %s Started On Host: %s, Port: %d" % (_gServerName, _gHostnameOrIpAddr, _gPort)
  _addCommand(_batch, "batch", "run commands from a batch file", "<filename>", 1, 1, True, True)
  _addCommand(_help, "help", "show all available commands", "", 0, 0, True, True)
  _addCommand(_exit, "quit", "exit interactive mode", "", 0, 0, True, True)
  _addTabCompletions()
  # startup our TCP server and accept new connections
  while (_createSocket() and _acceptConnection()):
    # shutdown original socket to not allow any new connections until we are done with this one
    _gTcpPrompt = _gServerName + "[" + _gTcpConnectSockName + "]:" + _gPrompt
    _gTcpTitle = _gTitle + ": " + _gServerName + "[" + _gTcpConnectSockName + "], Mode: INTERACTIVE"
    PshellReadline.setFileDescriptors(_gConnectFd, _gConnectFd, PshellReadline.SOCKET, PshellReadline.ONE_MINUTE*_gTcpTimeout)
    _gSocketFd.shutdown(socket.SHUT_RDWR)
    _receiveTCP()
    _gConnectFd.shutdown(socket.SHUT_RDWR)
  _endwhile
_enddef

#################################################################################
#################################################################################
def _runLocalServer():
  global _gPrompt
  global _gTitle
  global _gAddBatch
  _gPrompt = _getDisplayServerName() + "[" + _getDisplayServerType() + "]:" + _getDisplayPrompt()
  _gTitle = _getDisplayTitle() + ": " + _getDisplayServerName() + "[" + _getDisplayServerType() + "], Mode: INTERACTIVE"
  if (_gAddBatch):
    _addCommand(_batch, "batch", "run commands from a batch file", "<filename>", 1, 2, True, True)
  _endif
  _addCommand(_help, "help", "show all available commands", "", 0, 0, True, True)
  _addCommand(_exit, "quit", "exit interactive mode", "", 0, 0, True, True)
  _addTabCompletions()
  _showWelcome()
  command = _NULL
  while (not isSubString(command, "quit")):
    (command, idleSession) = PshellReadline.getInput(_gPrompt)
    if (not isSubString(command, "quit")):
      _processCommand(command)
    _endif
  _endwhile
_enddef

#################################################################################
#################################################################################
def _getDisplayServerType():
  global _gServerType
  global _gServerTypeOverride
  serverType = _gServerType
  if (_gServerTypeOverride != None):
    serverType = _gServerTypeOverride
  _enddef
  return (serverType)
_enddef

#################################################################################
#################################################################################
def _getDisplayPrompt():
  global _gPrompt
  global _gPromptOverride
  prompt = _gPrompt
  if (_gPromptOverride != None):
    prompt = _gPromptOverride
  _enddef
  return (prompt)
_enddef

#################################################################################
#################################################################################
def _getDisplayTitle():
  global _gTitle
  global _gTitleOverride
  title = _gTitle
  if (_gTitleOverride != None):
    title = _gTitleOverride
  _enddef
  return (title)
_enddef

#################################################################################
#################################################################################
def _getDisplayServerName():
  global _gServerName
  global _gServerNameOverride
  serverName = _gServerName
  if (_gServerNameOverride != None):
    serverName = _gServerNameOverride
  _enddef
  return (serverName)
_enddef

#################################################################################
#################################################################################
def _getDisplayBanner():
  global _gBanner
  global _gBannerOverride
  banner = _gBanner
  if (_gBannerOverride != None):
    banner = _gBannerOverride
  _enddef
  return (banner)
_enddef

#################################################################################
#################################################################################
def _addTabCompletions():
  global _gCommandList
  global _gServerType
  if ((_gServerType == LOCAL) or (_gServerType == TCP)):
    for command in _gCommandList:
      PshellReadline.addTabCompletion(command["name"])
    _endfor
  _endif
_enddef

#################################################################################
#################################################################################
def _receiveDGRAM():
  global _gPshellMsg
  global _gSocketFd
  global _gPshellMsgPayloadLength
  global _gPshellMsgHeaderFormat
  global _gFromAddr
  (_gPshellMsg, _gFromAddr) = _gSocketFd.recvfrom(_gPshellMsgPayloadLength)
  _gPshellMsg = _PshellMsg._asdict(_PshellMsg._make(struct.unpack(_gPshellMsgHeaderFormat+str(len(_gPshellMsg)-struct.calcsize(_gPshellMsgHeaderFormat))+"s", _gPshellMsg)))
  _processCommand(_gPshellMsg["payload"])
_enddef

#################################################################################
#################################################################################
def _receiveTCP():
  global _gConnectFd
  global _gQuitTcp
  global _gTcpPrompt
  global _gPshellMsg
  global _gMsgTypes
  _showWelcome()
  _gPshellMsg["msgType"] = _gMsgTypes["userCommand"]
  _gQuitTcp = False
  while (not _gQuitTcp):
    (command, _gQuitTcp) = PshellReadline.getInput(_gTcpPrompt)
    if (not _gQuitTcp):
      _processCommand(command)
    _endif
  _endwhile
_enddef

#################################################################################
#################################################################################
def _processCommand(command_):
  global _gCommandList
  global _gMaxLength
  global _gCommandHelp
  global _gListHelp
  global _gMsgTypes
  global _gPshellMsg
  global _gServerType
  global _gArgs
  global _gFirstArgPos
  global _gFoundCommand
  global _gCommandDispatched

  if (_gPshellMsg["msgType"] == _gMsgTypes["queryVersion"]):
    _processQueryVersion()
  elif (_gPshellMsg["msgType"] == _gMsgTypes["queryPayloadSize"]):
    _processQueryPayloadSize()
  elif (_gPshellMsg["msgType"] == _gMsgTypes["queryName"]):
    _processQueryName()
  elif (_gPshellMsg["msgType"] == _gMsgTypes["queryTitle"]):
    _processQueryTitle()
  elif (_gPshellMsg["msgType"] == _gMsgTypes["queryBanner"]):
    _processQueryBanner()
  elif (_gPshellMsg["msgType"] == _gMsgTypes["queryPrompt"]):
    _processQueryPrompt()
  elif (_gPshellMsg["msgType"] == _gMsgTypes["queryCommands1"]):
    _processQueryCommands1()
  elif (_gPshellMsg["msgType"] == _gMsgTypes["queryCommands2"]):
    _processQueryCommands2()
  else:
    _gCommandDispatched = True
    _gPshellMsg["payload"] = _NULL
    _gArgs = command_.split()[_gFirstArgPos:]
    command_ = command_.split()[0]
    numMatches = 0
    if ((command_ == "?") or (command_ in "help")):
      _help(_gArgs)
      _gCommandDispatched = False
      return
    else:
      for command in _gCommandList:
        if (command_ in command["name"]):
          _gFoundCommand = command
          numMatches += 1
        _endif
      _endfor
    _endif
    if (numMatches == 0):
      printf("PSHELL_ERROR: Command: '%s' not found\n" % command_)
    elif (numMatches > 1):
      printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n" % command_)
    else:
      if (not _isValidArgCount()):
        showUsage()
      elif (isHelp() and (_gFoundCommand["showUsage"] == True)):
        showUsage()
      else:
        _gFoundCommand["function"](_gArgs)
      _endif
    _endif
  _endif
  _gCommandDispatched = False
  _gPshellMsg["msgType"] = _gMsgTypes["commandComplete"]
  _reply()
_enddef

#################################################################################
#################################################################################
def _isValidArgCount():
  global _gArgs
  global _gFoundCommand
  return ((len(_gArgs) >= _gFoundCommand["minArgs"]) and (len(_gArgs) <= _gFoundCommand["maxArgs"]))
_enddef

#################################################################################
#################################################################################
def _processQueryVersion():
  global _gServerVersion
  printf(_gServerVersion)
_enddef

#################################################################################
#################################################################################
def _processQueryPayloadSize():
  global _gPshellMsgPayloadLength
  printf(str(_gPshellMsgPayloadLength))
_enddef

#################################################################################
#################################################################################
def _processQueryName():
  global _gServerName
  printf(_gServerName)
_enddef

#################################################################################
#################################################################################
def _processQueryTitle():
  global _gTitle
  printf(_gTitle)
_enddef

#################################################################################
#################################################################################
def _processQueryBanner():
  global _gBanner
  printf(_gBanner)
_enddef

#################################################################################
#################################################################################
def _processQueryPrompt():
  global _gPrompt
  printf(_gPrompt)
_enddef

#################################################################################
#################################################################################
def _processQueryCommands1():
  global _gCommandList
  global _gMaxLength
  global _gPshellMsg
  _gPshellMsg["payload"] = _NULL
  for command in _gCommandList:
    printf("%-*s  -  %s\n" % (_gMaxLength, command["name"], command["description"]))
  _endif
  printf()
_enddef

#################################################################################
#################################################################################
def _processQueryCommands2():
  global _gCommandList
  for command in _gCommandList:
    printf("%s%s" % (command["name"], "/"))
  _endif
_enddef

#################################################################################
#################################################################################
def _batch(command_):
  global _gFirstArgPos
  if (len(command_) == 1):
    batchFile = command_[0]
  else:
    batchFile = command_[1]
  _endif
  batchFile1 = _NULL
  batchPath = os.getenv('_PSHELL_BATCH_DIR')
  if (batchPath != None):
    batchFile1 = batchPath+"/"+batchFile+".batch"
  _endif
  batchFile2 = _PSHELL_BATCH_DIR+"/"+batchFile+".batch"
  batchFile3 = os.getcwd()+"/"+batchFile+".batch"
  batchFile4 = batchFile
  if (os.path.isfile(batchFile1)):
    file = open(batchFile1, 'r')
  elif (os.path.isfile(batchFile2)):
    file = open(batchFile2, 'r')
  elif (os.path.isfile(batchFile3)):
    file = open(batchFile3, 'r')
  elif (os.path.isfile(batchFile4)):
    file = open(batchFile4, 'r')
  elif ((_gFirstArgPos == 0) and (batchFile in "batch")):
    _showUsage()
    return
  else:
    printf("ERROR: Could not find batch file: '%s'\n" % batchFile)
    return
  _endif
  # found a config file, process it
  for line in file:
    # skip comments
    line = line.strip()
    if ((len(line) > 0) and (line[0] != "#")):
      _processCommand(line)
    _endif
  _endfor
_enddef

#################################################################################
#################################################################################
def _help(command_):
    printf()
    printf("****************************************\n")
    printf("*             COMMAND LIST             *\n")
    printf("****************************************\n")
    printf()
    _processQueryCommands1()
_enddef

#################################################################################
#################################################################################
def _exit(command_):
  global _gQuitTcp
  global _gServerType
  if (_gServerType == TCP):
    # TCP server, signal receiveTCP function to quit
    _gQuitTcp = True
  else:
    # local server, exit the process
    sys.exit()
  _endif
_enddef

#################################################################################
#################################################################################
def _showWelcome():
  global _gTitle
  global _gServerType
  global _gHostnameOrIpAddr
  global _gTcpTimeout
  global _gServerName
  global _gTcpConnectSockName
  global _gTcpTitle
  # show our welcome screen
  banner = "#  %s" % _getDisplayBanner()
  if (_gServerType == LOCAL):
    # put up our window title banner
    printf("\033]0;" + _gTitle + "\007")
    server = "#  Single session LOCAL server: %s[%s]" % (_getDisplayServerName(), _getDisplayServerType())
  else:
    # put up our window title banner
    printf("\033]0;" + _gTcpTitle + "\007")
    server = "#  Single session TCP server: %s[%s]" % (_gServerName, _gTcpConnectSockName)
  _endif
  maxBorderWidth = max(58, len(banner),len(server))+2
  printf()
  printf("#"*maxBorderWidth+"\n")
  printf("#\n")
  printf(banner+"\n")
  printf("#\n")
  printf(server+"\n")
  printf("#\n")
  if (_gServerType == LOCAL):
    printf("#  Idle session timeout: NONE\n")
  else:
    printf("#  Idle session timeout: %d minutes\n" % _gTcpTimeout)
  _endif
  printf("#\n")
  printf("#  Type '?' or 'help' at prompt for command summary\n")
  printf("#  Type '?' or '-h' after command for command usage\n")
  printf("#\n")
  printf("#  Full <TAB> completion, up-arrow recall, command\n")
  printf("#  line editing and command abbreviation supported\n")
  printf("#\n")
  printf("#"*maxBorderWidth+"\n")
  printf()
_enddef

#################################################################################
#################################################################################
def _printf(message_):
  global _gServerType
  global _gPshellMsg
  global _gCommandInteractive
  if (_gCommandInteractive == True):
    if (_gServerType == LOCAL):
      sys.stdout.write(message_)
      sys.stdout.flush()
    elif (_gServerType == TCP):
      PshellReadline.write(message_)
    else:
      # remote UDP/UNIX server
      _gPshellMsg["payload"] += message_
    _endif
  _endif
_enddef

#################################################################################
#################################################################################
def _showUsage():
  global _gFoundCommand
  if (_gFoundCommand["usage"] != None):
    printf("Usage: %s %s\n" % (_gFoundCommand["name"], _gFoundCommand["usage"]))
  else:
    printf("Usage: %s\n" % _gFoundCommand["name"])
  _endif
_enddef

#################################################################################
#################################################################################
def _isHelp():
  global _gArgs
  global _gCommandHelp
  return ((len(_gArgs) == 1) and (_gArgs[0] in _gCommandHelp))
_enddef

#################################################################################
#################################################################################
def _reply():
  global _gFromAddr
  global _gSocketFd
  global _gPshellMsg
  global _gServerType
  global _gPshellMsgHeaderFormat  
  # only issue a reply for a 'datagram' oriented remote server, TCP
  # uses a character stream and is not message based and LOCAL uses
  # no client app
  if ((_gServerType == UDP) or (_gServerType == UNIX)):
    _gSocketFd.sendto(struct.pack(_gPshellMsgHeaderFormat+str(len(_gPshellMsg["payload"]))+"s", *_gPshellMsg.values()), _gFromAddr)
  _endif
_enddef

#################################################################################
#################################################################################
def _cleanupResources():
  global _gUnixSourceAddress
  global _gServerType
  global _gSocketFd
  if (_gUnixSourceAddress != None):
    os.unlink(_gUnixSourceAddress)
  _endif
  if (_gServerType != LOCAL):
    _gSocketFd.close()
  _endif
_enddef

#################################################################################
#################################################################################
def _loadConfigFile(name_, title_, banner_, prompt_, type_, host_, port_, tcpTimeout_):  
  configFile1 = _NULL
  configPath = os.getenv('_PSHELL_CONFIG_DIR')
  if (configPath != None):
    configFile1 = configPath+"/"+_PSHELL_CONFIG_FILE
  _endif
  configFile2 = _PSHELL_CONFIG_DIR+"/"+_PSHELL_CONFIG_FILE
  configFile3 = os.getcwd()+"/"+_PSHELL_CONFIG_FILE
  if (os.path.isfile(configFile1)):
    file = open(configFile1, 'r')
  elif (os.path.isfile(configFile2)):
    file = open(configFile2, 'r')
  elif (os.path.isfile(configFile3)):
    file = open(configFile3, 'r')
  else:
    return (title_, banner_, prompt_, type_, host_, port_, tcpTimeout_)
  _endif
  # found a config file, process it
  for line in file:
    # skip comments
    if (line[0] != "#"):
      value = line.strip().split("=");
      if (len(value) == 2):
        option = value[0].split(".")
        value[1] = value[1].strip()
        if ((len(option) == 2) and  (name_ == option[0])):
          if (option[1].lower() == "title"):
            title_ = value[1]
          elif (option[1].lower() == "banner"):
            banner_ = value[1]
          elif (option[1].lower() == "prompt"):
            prompt_ = value[1]+" "
          elif (option[1].lower() == "host"):
            host_ = value[1].lower()
          elif ((option[1].lower() == "port") and (value[1].isdigit())):
            port_ = int(value[1])
          elif (option[1].lower() == "type"):
            if ((value[1].lower() == UDP) or 
                (value[1].lower() == TCP) or 
                (value[1].lower() == UNIX) or 
                (value[1].lower() == LOCAL)):
              type_ = value[1].lower()
            _endif
          elif ((option[1].lower() == "timeout") and (value[1].isdigit())):
            tcpTimeout_ = int(value[1])
          _endif
        _endif
      _endif
    _endif
  _endfor
  return (title_, banner_, prompt_, type_, host_, port_, tcpTimeout_)
_enddef

#################################################################################
#################################################################################
def _loadStartupFile():
  global _gServerName
  startupFile1 = _NULL
  startupPath = os.getenv('_PSHELL_STARTUP_DIR')
  if (startupPath != None):
    startupFile1 = startupPath+"/"+_gServerName+".startup"
  _endif
  startupFile2 = _PSHELL_STARTUP_DIR+"/"+_gServerName+".startup"
  startupFile3 = os.getcwd()+"/"+_gServerName+".startup"
  if (os.path.isfile(startupFile1)):
    file = open(startupFile1, 'r')
  elif (os.path.isfile(startupFile2)):
    file = open(startupFile2, 'r')
  elif (os.path.isfile(startupFile3)):
    file = open(startupFile3, 'r')
  else:
    return
  _endif
  # found a config file, process it
  for line in file:
    # skip comments
    line = line.strip()
    if ((len(line) > 0) and (line[0] != "#")):
      _runCommand(line)
    _endif
  _endfor
_enddef

#################################################################################
#################################################################################
def _runCommand(command_):
  global _gCommandList
  global _gCommandInteractive
  global _gCommandDispatched
  global _gFoundCommand
  global _gFirstArgPos
  global _gArgs
  if (_gCommandDispatched == False):
    _gCommandDispatched = True
    _gCommandInteractive = False
    numMatches = 0
    _gArgs = command_.split()[_gFirstArgPos:]
    command_ = command_.split()[0]
    for command in _gCommandList:
      if (command_ in command["name"]):
        _gFoundCommand = command
        numMatches += 1
      _endif
    _endfor
    if ((numMatches == 1) and _isValidArgCount() and not isHelp()):
      _gFoundCommand["function"](_gArgs)
    _endif
    _gCommandDispatched = False
    _gCommandInteractive = True
  _endif
_enddef

#################################################################################
#################################################################################
def _wheel(string_):
  global _gWheel
  global _gWheelPos
  _gWheelPos += 1
  if (string_ != _NULL):
    _printf("\r%s%c" % (string_, _gWheel[(_gWheelPos)%4]))
  else:
    _printf("\r%c" % _gWheel[(_gWheelPos)%4])
  _endif
  _flush()
_enddef

#################################################################################
#################################################################################
def _march(string_):
  _printf(string_)
  _flush()
_enddef

#################################################################################
#################################################################################
def _flush():
  global _gCommandInteractive
  global _gServerType
  global _gPshellMsg
  if ((_gCommandInteractive == True) and ((_gServerType == UDP) or (_gServerType == UNIX))):
    _reply()
    _gPshellMsg["payload"] = _NULL
  _endif
_enddef

#################################################################################
#
# global "private" data
#
#################################################################################

# python does not have a native null string identifier, so create one
_NULL = ""

_gCommandHelp = ('?', '-h', '--h', '-help', '--help')
_gListHelp = ('?', 'help')
_gCommandList = []
_gMaxLength = 0

_gServerVersion = "1"
_gServerName = None
_gServerType = None
_gServerMode = None
_gHostnameOrIpAddr = None
_gPort = None
_gPrompt = "PSHELL> "
_gTitle = "PSHELL: "
_gBanner = "PSHELL: Process Specific Embedded Command Line Shell"
_gSocketFd = None 
_gConnectFd = None 
_gFromAddr = None
_gUnixSocketPath = "/tmp/"
_gArgs = None
_gFoundCommand = None
_gUnixSourceAddress = None
_gRunninig = False
_gCommandDispatched = False
_gCommandInteractive = True

# dislay override setting used by the pshell.py client program
_gPromptOverride = None
_gTitleOverride = None
_gServerNameOverride = None
_gServerTypeOverride = None
_gBannerOverride = None

# these are the valid types we recognize in the msgType field of the pshellMsg structure,
# that structure is the message passed between the pshell client and server, these values
# must match their corresponding #define definitions in the C file PshellCommon.h
_gMsgTypes = {"commandSuccess": 0, 
              "queryVersion":1, 
              "commandNotFound":1, 
              "queryPayloadSize":2, 
              "invalidArgCount":2, 
              "queryName":3, 
              "queryCommands1":4, 
              "queryCommands2":5, 
              "updatePayloadSize":6, 
              "userCommand":7, 
              "commandComplete":8, 
              "queryBanner":9, 
              "queryTitle":10, 
              "queryPrompt":11, 
              "controlCommand":12}

# fields of PshellMsg, we use this definition to unpack the received PshellMsg response
# from the server into a corresponding OrderedDict in the PshellControl entry
_PshellMsg = namedtuple('PshellMsg', 'msgType respNeeded dataNeeded pad seqNum payload')

# format of PshellMsg header, 4 bytes and 1 (4 byte) integer, we use this for packing/unpacking
# the PshellMessage to/from an OrderedDict into a packed binary structure that can be transmitted 
# over-the-wire via a socket
_gPshellMsgHeaderFormat = "4BI"

# default PshellMsg payload length, used to receive responses
_gPshellMsgPayloadLength = 4096

_gPshellMsg =  OrderedDict([("msgType",0),
                            ("respNeeded",True),
                            ("dataNeeded",True),
                            ("pad",0),
                            ("seqNum",0),
                            ("payload",_NULL)])

_PSHELL_CONFIG_DIR = "/etc/pshell/config"
_PSHELL_STARTUP_DIR = "/etc/pshell/startup"
_PSHELL_BATCH_DIR = "/etc/pshell/batch"
_PSHELL_CONFIG_FILE = "pshell-server.conf"

_gFirstArgPos = 1

_gWheelPos = 0
_gWheel = "|/-\\"

_gQuitTcp = False 
_gTcpTimeout = 10  # minutes
_gTcpConnectSockName = None 
_gTcpInteractivePrompt = None
_gTcpPrompt = None
_gTcpTitle = None
# used so the pshell.py UDP/UNIX client program
# can supress the adding of the batch command in
# its local server, since they already exist in
# the remote servers
_gAddBatch = True
