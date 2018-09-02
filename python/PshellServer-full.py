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
A Lightweight, Process-Specific, Embedded Command Line Shell

This API provides the Process Specific Embedded Command Line Shell (PSHELL)
user API functionality.  It provides the ability for a program to register 
functions that can be invoked via a command line user interface.  There are 
several ways to invoke these embedded functions based on how the pshell server 
is configured, which is described in documentation further down in this file.
 
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
getOption()   -- parses arg of format -<key><value> or <key>=<value>

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

These three identifiers that can be used for the hostnameOrIpAddr argument 
of the startServer call.  PshellServer.ANYHOST will bind the server socket
to all interfaces of a multi-homed host, PshellServer.ANYBCAST will bind to
255.255.255.255, PshellServer.LOCALHOST will bind the server socket to 
the local loopback address (i.e. 127.0.0.1), note that subnet broadcast 
it also supported, e.g. x.y.z.255

ANYHOST
ANYBCAST
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
from collections import OrderedDict
from collections import namedtuple
import PshellReadline

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

# These three identifiers that can be used for the hostnameOrIpAddr argument 
# of the startServer call.  PshellServer.ANYHOST will bind the server socket
# to all interfaces of a multi-homed host, PSHELL_ANYBCAST will bind to
# 255.255.255.255, PshellServer.LOCALHOST will bind the server socket to 
# the local loopback address (i.e. 127.0.0.1), note that subnet broadcast 
# it also supported, e.g. x.y.z.255
ANYHOST = "anyhost"
ANYBCAST = "anybcast"
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

#################################################################################
#
# The following public functions should only be called from within a 
# registered callback function
#
#################################################################################

#################################################################################
#################################################################################
def printf(message = "", newline = True):
  """
  Display data back to the remote client

    Args:
        string (str)   : Message to display to the client user
        newline (bool) : Automatically add a newline to message

    Returns:
        none
  """
  _printf(message, newline)

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
        bool : True if substring matches, False otherwise
  """
  return (_isSubString(string1, string2, minMatchLength))

#################################################################################
#################################################################################
def getOption(arg):
  """
  This function will parse an argument string of the formats -<key><value> where
  key is one letter only, i.e. '-t', or <key>=<value> where key can be any length
  word, i.e. 'timeout', and return a 3-tuple indicating if the arg was parsed
  correctly, along with the associated key and corresponding value.  An example 
  of the two valid formats are -t10, timeout=10.

    Args:
        arg (str) : The argument string to parse

    Returns:
        bool : True if string parses correctly, i.e. -<key><value> or <key>=<value>
        str  : The key value found
        str  : The value associated with the key
  """
  return (_getOption(arg))

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

#################################################################################
#################################################################################
def _getOption(arg):
  if (len(arg) < 3):
    return (False, "", "")
  elif (arg[0] == "-"):
    return (True, arg[:2], arg[2:])
  else:
    value = arg.split("=")
    if (len(value) != 2):
      return (False, "", "")
    else:
      return (True, value[0], value[1])

#################################################################################
#################################################################################
def _addCommand(function_,
                command_,
                description_,
                usage_,
                minArgs_,
                maxArgs_,
                showUsage_,
                prepend_ = False):
  global _gCommandList
  global _gMaxLength
  global _gServerType
  global _gPshellClient
  
  # see if we have a NULL command name 
  if ((command_ == None) or (len(command_) == 0)):
    print("PSHELL_ERROR: NULL command name, command not added")
    return

  # see if we have a NULL description 
  if ((description_ == None) or (len(description_) == 0)):
    print("PSHELL_ERROR: NULL description, command: '%s' not added" % command_)
    return

  # see if we have a NULL function
  if (function_ == None):
    print("PSHELL_ERROR: NULL function, command: '%s' not added" % command_)
    return

  # if they provided no usage for a function with arguments
  if (((maxArgs_ > 0) or (minArgs_ > 0)) and ((usage_ == None) or (len(usage_) == 0))):
    print("PSHELL_ERROR: NULL usage for command that takes arguments, command: '%s' not added" % command_)
    return

  # see if their minArgs is greater than their maxArgs, we ignore if maxArgs is 0
  # because that is the default value and we will set maxArgs to minArgs if that
  # case later on in this function
  if ((minArgs_ > maxArgs_) and (maxArgs_ > 0)):
    print("PSHELL_ERROR: minArgs: %d is greater than maxArgs: %d, command: '%s' not added" % (minArgs_, maxArgs_, command_))
    return
    
  # see if it is a duplicate command
  for command in _gCommandList:
    if (command["name"] == command_):
      # command name already exists, don't add it again
      print("PSHELL_ERROR: Command: %s already exists, not adding command" % command_)
      return
      
  # everything ok, good to add command
  
  # see if they gave the default for maxArgs, if so, set maxArgs to minArgs 
  if (maxArgs_ == 0):
    maxArgs = minArgs_
  else:
    maxArgs = maxArgs_
    
  if (len(command_) > _gMaxLength):
    _gMaxLength = len(command_)
    
  if (prepend_ == True):
    _gCommandList.insert(0, {"function":function_,
                             "name":command_,
                             "description":description_,
                             "usage":usage_,
                             "minArgs":minArgs_,
                             "maxArgs":maxArgs,
                             "showUsage":showUsage_})
  else:
    _gCommandList.append({"function":function_,
                          "name":command_,
                          "description":description_,
                          "usage":usage_,
                          "minArgs":minArgs_,
                          "maxArgs":maxArgs,
                          "showUsage":showUsage_})

#################################################################################
#################################################################################
def _startServer(serverName_, serverType_, serverMode_, hostnameOrIpAddr_, port_):
  global _gServerName
  global _gServerType
  global _gServerMode
  global _gHostnameOrIpAddr
  global _gPort
  global _gRunning
  if (_gRunning == False):
    _gServerName = serverName_
    _gServerType = serverType_
    _gServerMode = serverMode_
    _gHostnameOrIpAddr = hostnameOrIpAddr_
    _gPort = port_
    _loadConfigFile()
    _loadStartupFile()  
    _gRunning = True
    if (_gServerMode == BLOCKING):
      _runServer()
    else:
      # spawn thread
      thread.start_new_thread(_serverThread, ())

#################################################################################
#################################################################################
def _serverThread():
  _runServer()
  
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
    ipAddrOctets = _gHostnameOrIpAddr.split(".")
    if (_gHostnameOrIpAddr == ANYHOST):
      _gSocketFd.bind(("", _gPort))
    elif (_gHostnameOrIpAddr == LOCALHOST):
      _gSocketFd.bind(("127.0.0.1", _gPort))
    elif (_gHostnameOrIpAddr == ANYBCAST):
      # global broadcast address
      _gSocketFd.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
      _gSocketFd.bind(("255.255.255.255", _gPort))
    elif ((len(ipAddrOctets) == 4) and (ipAddrOctets[3] == "255")):
      # subnet broadcast address
      _gSocketFd.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
      _gSocketFd.bind((_gHostnameOrIpAddr, _gPort))
    else:
      _gSocketFd.bind((_gHostnameOrIpAddr, _gPort))
  elif (_gServerType == TCP):
    # IP domain socket (TCP)
    _gSocketFd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    _gSocketFd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    # Bind the socket to the port
    if (_gHostnameOrIpAddr == ANYHOST):
      _gSocketFd.bind(("", _gPort))
    elif (_gHostnameOrIpAddr == LOCALHOST):
      _gSocketFd.bind(("127.0.0.1", _gPort))
    else:
      _gSocketFd.bind((_gHostnameOrIpAddr, _gPort))
    # Listen for incoming connections
    _gSocketFd.listen(1)
  elif (_gServerType == UNIX):
    # UNIX domain socket
    _gSocketFd = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    _gUnixSourceAddress = _gUnixSocketPath+_gServerName
    # cleanup any old handle that might be hanging around
    try:
      os.unlink(_gUnixSourceAddress)
    except:
      None
    _gSocketFd.bind(_gUnixSourceAddress)
  return (True)

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

#################################################################################
#################################################################################
def _runUDPServer():
  global _gServerName
  global _gHostnameOrIpAddr
  global _gPort
  print("PSHELL_INFO: UDP Server: %s Started On Host: %s, Port: %d" %
        (_gServerName, _gHostnameOrIpAddr, _gPort))
  _runDGRAMServer()

#################################################################################
#################################################################################
def _runUNIXServer():
  global _gServerName
  print("PSHELL_INFO: UNIX Server: %s Started" % _gServerName)
  _runDGRAMServer()

#################################################################################
#################################################################################
def _runDGRAMServer():
  _addNativeCommands()
  if (_createSocket()):
    while (True):
      _receiveDGRAM()

#################################################################################
#################################################################################
def _acceptConnection():
  global _gSocketFd
  global _gConnectFd
  global _gTcpConnectSockName
  (_gConnectFd, clientAddr) = _gSocketFd.accept()
  _gTcpConnectSockName = clientAddr[0]
  return (True)

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
  global _gTcpConnectSockName
  global _gTcpTimeout
  print("PSHELL_INFO: TCP Server: %s Started On Host: %s, Port: %d" %
        (_gServerName, _gHostnameOrIpAddr, _gPort))
  _addNativeCommands()
  # startup our TCP server and accept new connections
  while (_createSocket() and _acceptConnection()):
    # shutdown original socket to not allow any new connections
    # until we are done with this one
    _gTcpPrompt = _gServerName + "[" + _gTcpConnectSockName + "]:" + _gPrompt
    _gTcpTitle = _gTitle + ": " + _gServerName + "[" + \
                 _gTcpConnectSockName + "], Mode: INTERACTIVE"
    PshellReadline.setFileDescriptors(_gConnectFd,
                                      _gConnectFd,
                                      PshellReadline.SOCKET,
                                      PshellReadline.ONE_MINUTE*_gTcpTimeout)
    try:
      _gSocketFd.shutdown(socket.SHUT_RDWR)
    except:
      None
    _receiveTCP()
    _gConnectFd.shutdown(socket.SHUT_RDWR)
    _gConnectFd.close()
    _gSocketFd.close()

#################################################################################
#################################################################################
def _runLocalServer():
  global _gPrompt
  global _gTitle
  _gPrompt = _getDisplayServerName() + "[" + \
             _getDisplayServerType() + "]:" + _getDisplayPrompt()
  _gTitle = _getDisplayTitle() + ": " + _getDisplayServerName() + \
            "[" + _getDisplayServerType() + "], Mode: INTERACTIVE"
  _addNativeCommands()
  _showWelcome()
  command = ""
  while (not isSubString(command, "quit")):
    (command, idleSession) = PshellReadline.getInput(_gPrompt)
    if (not isSubString(command, "quit")):
      _processCommand(command)

#################################################################################
#################################################################################
def _addNativeCommands():
  global _gPshellClient
  global _gServerType
  if (not _gPshellClient):
    _addCommand(_batch,
                "batch",
                "run commands from a batch file",
                "<filename>",
                1,
                2,
                True,
                True)
  if ((_gServerType == TCP) or (_gServerType == LOCAL)):
    _addCommand(_help,
                "help",
                "show all available commands",
                "",
                0,
                0,
                True,
                True)
    _addCommand(_exit,
                "quit",
                "exit interactive mode",
                "",
                0,
                0,
                True,
                True)
  _addTabCompletions()

#################################################################################
#################################################################################
def _getDisplayServerType():
  global _gServerType
  global _gServerTypeOverride
  serverType = _gServerType
  if (_gServerTypeOverride != None):
    serverType = _gServerTypeOverride
  return (serverType.strip())

#################################################################################
#################################################################################
def _getDisplayPrompt():
  global _gPrompt
  global _gPromptOverride
  prompt = _gPrompt
  if (_gPromptOverride != None):
    prompt = _gPromptOverride
  return (prompt.rstrip(' \t\r\n\0') + " ")

#################################################################################
#################################################################################
def _getDisplayTitle():
  global _gTitle
  global _gTitleOverride
  title = _gTitle
  if (_gTitleOverride != None):
    title = _gTitleOverride
  return (title.strip())

#################################################################################
#################################################################################
def _getDisplayServerName():
  global _gServerName
  global _gServerNameOverride
  serverName = _gServerName
  if (_gServerNameOverride != None):
    serverName = _gServerNameOverride
  return (serverName.strip())

#################################################################################
#################################################################################
def _getDisplayBanner():
  global _gBanner
  global _gBannerOverride
  banner = _gBanner
  if (_gBannerOverride != None):
    banner = _gBannerOverride
  return (banner.strip())

#################################################################################
#################################################################################
def _addTabCompletions():
  global _gCommandList
  global _gServerType
  global _gRunning
  if (((_gServerType == LOCAL) or (_gServerType == TCP)) and (_gRunning  == True)):
    for command in _gCommandList:
      PshellReadline.addTabCompletion(command["name"])

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

  _gPshellMsg["payload"] = ""
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
    _gArgs = command_.split()[_gFirstArgPos:]
    command_ = command_.split()[0]
    numMatches = 0
    if ((command_ == "?") or (command_ in "help")):
      _help(_gArgs)
      _gCommandDispatched = False
      return
    else:
      for command in _gCommandList:
        if (isSubString(command_, command["name"], len(command_))):
          _gFoundCommand = command
          numMatches += 1
    if (numMatches == 0):
      printf("PSHELL_ERROR: Command: '%s' not found" % command_)
    elif (numMatches > 1):
      printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'" % command_)
    else:
      if (isHelp()):
        if (_gFoundCommand["showUsage"] == True):
          showUsage()          
        else:
          _gFoundCommand["function"](_gArgs)
      elif (not _isValidArgCount()):
        showUsage()
      else:
        _gFoundCommand["function"](_gArgs)
  _gCommandDispatched = False
  _gPshellMsg["msgType"] = _gMsgTypes["commandComplete"]
  _reply()

#################################################################################
#################################################################################
def _isValidArgCount():
  global _gArgs
  global _gFoundCommand
  return ((len(_gArgs) >= _gFoundCommand["minArgs"]) and
          (len(_gArgs) <= _gFoundCommand["maxArgs"]))

#################################################################################
#################################################################################
def _processQueryVersion():
  global _gServerVersion
  printf(_gServerVersion, newline=False)

#################################################################################
#################################################################################
def _processQueryPayloadSize():
  global _gPshellMsgPayloadLength
  printf(str(_gPshellMsgPayloadLength), newline=False)

#################################################################################
#################################################################################
def _processQueryName():
  global _gServerName
  printf(_gServerName, newline=False)

#################################################################################
#################################################################################
def _processQueryTitle():
  global _gTitle
  printf(_gTitle, newline=False)

#################################################################################
#################################################################################
def _processQueryBanner():
  global _gBanner
  printf(_gBanner, newline=False)

#################################################################################
#################################################################################
def _processQueryPrompt():
  global _gPrompt
  printf(_gPrompt, newline=False)

#################################################################################
#################################################################################
def _processQueryCommands1():
  global _gCommandList
  global _gMaxLength
  global _gPshellMsg
  _gPshellMsg["payload"] = ""
  for command in _gCommandList:
    printf("%-*s  -  %s" % (_gMaxLength, command["name"], command["description"]))
  printf()

#################################################################################
#################################################################################
def _processQueryCommands2():
  global _gCommandList
  for command in _gCommandList:
    printf("%s%s" % (command["name"], "/"), newline=False)

#################################################################################
#################################################################################
def _batch(command_):
  global _gFirstArgPos
  if (len(command_) == 1):
    batchFile = command_[0]
  else:
    batchFile = command_[1]
  batchFile1 = ""
  batchPath = os.getenv('PSHELL_BATCH_DIR')
  if (batchPath != None):
    batchFile1 = batchPath+"/"+batchFile+".batch"
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
    printf("ERROR: Could not find batch file: '%s'" % batchFile)
    return
  # found a config file, process it
  for line in file:
    # skip comments
    line = line.strip()
    if ((len(line) > 0) and (line[0] != "#")):
      _processCommand(line)

#################################################################################
#################################################################################
def _help(command_):
  printf()
  printf("****************************************")
  printf("*             COMMAND LIST             *")
  printf("****************************************")
  printf()
  _processQueryCommands1()

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
  global  _gPshellClient
  # show our welcome screen
  banner = "#  %s" % _getDisplayBanner()
  if (_gPshellClient == True):
    # put up our window title banner
    printf("\033]0;" + _gTitle + "\007", newline=False)
    if (_getDisplayServerType() == UNIX):
      server = "#  Multi-session UNIX server: %s[%s]" % (_getDisplayServerName(),
                                                         _getDisplayServerType())
    else:
      server = "#  Multi-session UDP server: %s[%s]" % (_getDisplayServerName(),
                                                        _getDisplayServerType())
  elif (_gServerType == LOCAL):
    # put up our window title banner
    printf("\033]0;" + _gTitle + "\007", newline=False)
    server = "#  Single session LOCAL server: %s[%s]" % (_getDisplayServerName(),
                                                         _getDisplayServerType())
  else:
    # put up our window title banner
    printf("\033]0;" + _gTcpTitle + "\007", newline=False)
    server = "#  Single session TCP server: %s[%s]" % (_gServerName,
                                                       _gTcpConnectSockName)
  maxBorderWidth = max(58, len(banner),len(server))+2
  printf()
  printf("#"*maxBorderWidth)
  printf("#")
  printf(banner)
  printf("#")
  printf(server)
  printf("#")
  if (_gServerType == LOCAL):
    printf("#  Idle session timeout: NONE")
  else:
    printf("#  Idle session timeout: %d minutes" % _gTcpTimeout)
  printf("#")
  if (_gPshellClient == True):
    printf("#  Command response timeout: %d seconds" % _gTcpTimeout)
    printf("#")
  printf("#  Type '?' or 'help' at prompt for command summary")
  printf("#  Type '?' or '-h' after command for command usage")
  printf("#")
  printf("#  Full <TAB> completion, up-arrow recall, command")
  printf("#  line editing and command abbreviation supported")
  printf("#")
  printf("#"*maxBorderWidth+"")
  printf()

#################################################################################
#################################################################################
def _printf(message_, newline_):
  global _gServerType
  global _gPshellMsg
  global _gCommandInteractive
  if (_gCommandInteractive == True):
    if (newline_ == True):
      message_ += "\n"
    if ((_gServerType == LOCAL) or (_gServerType == TCP)):
      PshellReadline.write(str(message_))
    else:   # UDP/UNIX server
      _gPshellMsg["payload"] += str(message_)

#################################################################################
#################################################################################
def _showUsage():
  global _gFoundCommand
  if (_gFoundCommand["usage"] != None):
    printf("Usage: %s %s" % (_gFoundCommand["name"], _gFoundCommand["usage"]))
  else:
    printf("Usage: %s" % _gFoundCommand["name"])

#################################################################################
#################################################################################
def _setFirstArgPos(position):
  global _gFirstArgPos
  global _gHelpPos
  global _gHelpLength
  if (position == 0):
    _gFirstArgPos = 0
    _gHelpLength = 2
    _gHelpPos = 1
  elif (position == 1):
    _gFirstArgPos = 1
    _gHelpLength = 1
    _gHelpPos = 0
    
#################################################################################
#################################################################################
def _isHelp():
  global _gArgs
  global _gCommandHelp
  global _gHelpPos
  global _gHelpLength
  return ((len(_gArgs) == _gHelpLength) and (_gArgs[_gHelpPos] in _gCommandHelp))

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
    try:
      _gSocketFd.sendto(struct.pack(_gPshellMsgHeaderFormat+str(len(_gPshellMsg["payload"]))+"s", *_gPshellMsg.values()), _gFromAddr)
    except:
      None

#################################################################################
#################################################################################
def _cleanupResources():
  global _gUnixSourceAddress
  global _gSocketFd
  if (_gUnixSourceAddress != None):
    try:
      os.unlink(_gUnixSourceAddress)
    except:
      None
  if (_gSocketFd != None):
    try:
      _gSocketFd.close()
    except:
      None

#################################################################################
#################################################################################
def _loadConfigFile():  
  global _gServerName
  global _gServerType
  global _gHostnameOrIpAddr
  global _gPort
  global _gPrompt
  global _gTitle
  global _gBanner
  global _gTcpTimeout
  configFile1 = ""
  configPath = os.getenv('PSHELL_CONFIG_DIR')
  if (configPath != None):
    configFile1 = configPath+"/"+_PSHELL_CONFIG_FILE
  configFile2 = _PSHELL_CONFIG_DIR+"/"+_PSHELL_CONFIG_FILE
  configFile3 = os.getcwd()+"/"+_PSHELL_CONFIG_FILE
  if (os.path.isfile(configFile1)):
    file = open(configFile1, 'r')
  elif (os.path.isfile(configFile2)):
    file = open(configFile2, 'r')
  elif (os.path.isfile(configFile3)):
    file = open(configFile3, 'r')
  else:
    return
  # found a config file, process it
  for line in file:
    # skip comments
    if (line[0] != "#"):
      value = line.strip().split("=");
      if (len(value) == 2):
        option = value[0].split(".")
        value[1] = value[1].strip()
        if ((len(option) == 2) and  (_gServerName == option[0])):
          if (option[1].lower() == "title"):
            _gTitle = value[1]
          elif (option[1].lower() == "banner"):
            _gBanner = value[1]
          elif (option[1].lower() == "prompt"):
            _gPrompt = value[1]+" "
          elif (option[1].lower() == "host"):
            _gHostnameOrIpAddr = value[1].lower()
          elif ((option[1].lower() == "port") and (value[1].isdigit())):
            _gPort = int(value[1])
          elif (option[1].lower() == "type"):
            if ((value[1].lower() == UDP) or 
                (value[1].lower() == TCP) or 
                (value[1].lower() == UNIX) or 
                (value[1].lower() == LOCAL)):
              _gServerType = value[1].lower()
          elif ((option[1].lower() == "timeout") and (value[1].isdigit())):
            _gTcpTimeout = int(value[1])
  return

#################################################################################
#################################################################################
def _loadStartupFile():
  global _gServerName
  startupFile1 = ""
  startupPath = os.getenv('PSHELL_STARTUP_DIR')
  if (startupPath != None):
    startupFile1 = startupPath+"/"+_gServerName+".startup"
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
  # found a config file, process it
  for line in file:
    # skip comments
    line = line.strip()
    if ((len(line) > 0) and (line[0] != "#")):
      _runCommand(line)

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
    if ((numMatches == 1) and _isValidArgCount() and not isHelp()):
      _gFoundCommand["function"](_gArgs)
    _gCommandDispatched = False
    _gCommandInteractive = True

#################################################################################
#################################################################################
def _wheel(string_):
  global _gWheel
  global _gWheelPos
  _gWheelPos += 1
  if (string_ != ""):
    _printf("\r%s%c" % (string_, _gWheel[(_gWheelPos)%4]), newline_=False)
  else:
    _printf("\r%c" % _gWheel[(_gWheelPos)%4], newline_=False)
  _flush()

#################################################################################
#################################################################################
def _march(string_):
  _printf(string_, newline_=False)
  _flush()

#################################################################################
#################################################################################
def _flush():
  global _gCommandInteractive
  global _gServerType
  global _gPshellMsg
  if ((_gCommandInteractive == True) and
      ((_gServerType == UDP) or (_gServerType == UNIX))):
    _reply()
    _gPshellMsg["payload"] = ""

#################################################################################
#
# global "private" data
#
#################################################################################

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
_gRunning = False
_gCommandDispatched = False
_gCommandInteractive = True

# dislay override setting used by the pshell.py client program
_gPromptOverride = None
_gTitleOverride = None
_gServerNameOverride = None
_gServerTypeOverride = None
_gBannerOverride = None

# these are the valid types we recognize in the msgType field of the pshellMsg
#  structure,that structure is the message passed between the pshell client and
# server, these values must match their corresponding #define definitions in
# the C file PshellCommon.h
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

# fields of PshellMsg, we use this definition to unpack the received PshellMsg
# response from the server into a corresponding OrderedDict in the PshellControl
# entry
_PshellMsg = namedtuple('PshellMsg', 'msgType respNeeded dataNeeded pad seqNum payload')

# format of PshellMsg header, 4 bytes and 1 (4 byte) integer, we use this for
# packing/unpacking the PshellMessage to/from an OrderedDict into a packed binary
# structure that can be transmitted over-the-wire via a socket
_gPshellMsgHeaderFormat = "4BI"

# default PshellMsg payload length, used to receive responses
_gPshellMsgPayloadLength = 4096

_gPshellMsg =  OrderedDict([("msgType",0),
                            ("respNeeded",True),
                            ("dataNeeded",True),
                            ("pad",0),
                            ("seqNum",0),
                            ("payload","")])

_PSHELL_CONFIG_DIR = "/etc/pshell/config"
_PSHELL_STARTUP_DIR = "/etc/pshell/startup"
_PSHELL_BATCH_DIR = "/etc/pshell/batch"
_PSHELL_CONFIG_FILE = "pshell-server.conf"

_gFirstArgPos = 1
_gHelpPos = 0
_gHelpLength = 1

_gWheelPos = 0
_gWheel = "|/-\\"

_gQuitTcp = False 
_gTcpTimeout = 10  # minutes
_gTcpConnectSockName = None 
_gTcpPrompt = None
_gTcpTitle = None
# flag to indicate the special pshell.py client
_gPshellClient = False 

##############################
#
# start of main program
#
##############################

if (__name__ == '__main__'):
  # just print out a message identifying this as the 'full' module
  print("PSHELL_INFO: PshellServer FULL Module")
