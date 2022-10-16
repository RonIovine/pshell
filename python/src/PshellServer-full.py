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
This is the Python implementation, there are also corresponding C/C++ and Golang
implementations.

The functions are similar to the prototype of the 'main' in 'python', i.e.
'def myFunc(string)', there are several ways to invoke these embedded
functions based on how the pshell server is configured, which is described
in documentation further down in this file.

A complete example of the usage of the API can be found in the included
demo program pshellServerDemo.py.

Functions:

The main API calls to register PSHELL commands and start the server

  addCommand()       -- register a pshell command with the server
  startServer()      -- start the pshell server
  cleanupResources() -- release all resources claimed by the server

Function to allow a parent application to call it's own PSHELL function

  runCommand() -- run a registered command from the parent (i.e. registering) program

Functions to allow extraction of internal log messages from parent application

  setLogLevel()    -- set the internal log level for this module
  setLogFunction() -- register a user function to receive all logs

Functions to help in argument parsing/extraction, even though many of these
operations can easily be done with native Python constructs, they are provided
here for consistency of the API across all language implementations

  tokenize()              -- parse a string based on token delimeters
  getLength()             -- return the string length
  isEqual()               -- compare two strings for equality, case sensitive
  isEqualNoCase()         -- compare two strings for equality, case insensitive
  isSubString()           -- checks for string1 substring of string2 at position 0, case sensitive
  isSubStringNoCase()     -- checks for string1 substring of string2 at position 0, case insensitive
  isFloat()               -- returns True if string is floating point
  isDec()                 -- returns True if string is dec
  isHex()                 -- returns True if string is hex, with or without the preceeding 0x
  isAlpha()               -- returns True if string is alphabetic
  isNumeric()             -- returns True if string isDec or isHex
  isAlphaNumeric()        -- returns True if string is alpha-numeric
  isIpv4Addr()            -- returns True if string is valid ipv4 address format
  isIpv4AddrWithNetmask() -- returns True is string is valid ipv4 address/netmask format
  isMacAddr()             -- returns True if string is valid MAC address format
  getBool()               -- returns True if string is 'true', 'yes', 'on'
  getInt()                -- return the integer value from the string
  getFloat()              -- return the float value from the string

The following commands should only be called from within the context of a
PSHELL callback function

  printf()    -- display a message from a pshell callback function to the client
  flush()     -- flush the transfer buffer to the client (UDP/UNIX servers only)
  wheel()     -- spinning ascii wheel to keep UDP/UNIX client alive or show progress
  march()     -- marching ascii character to keep UDP/UNIX client alive or show progress
  showUsage() -- show the usage the command is registered with
  isHelp()    -- checks if the user has requested help on this command
  getOption() -- parses arg of format -<key><value> or <key>=<value>

Integer constants:

These are the identifiers for the serverMode.  BLOCKING will never return
control to the caller of startServer, NON_BLOCKING will spawn a thread to
run the server and will return control to the caller of startServer

  BLOCKING
  NON_BLOCKING

Constants to let the host program set the internal debug log level,
if the user of this API does not want to see any internal message
printed out, set the debug log level to LOG_LEVEL_NONE, the default
log level is LOG_LEVEL_ALL

  LOG_LEVEL_NONE
  LOG_LEVEL_ERROR
  LOG_LEVEL_WARNING
  LOG_LEVEL_INFO
  LOG_LEVEL_ALL
  LOG_LEVEL_DEFAULT

Used to specify the radix extraction format for the getInt() function

  RADIX_DEC
  RADIX_HEX
  RADIX_ANY

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
of the startServer call.  ANYHOST will bind the server socket to all interfaces
of a multi-homed host, ANYBCAST will bind to 255.255.255.255, LOCALHOST will
bind the server socket to the local loopback address (i.e. 127.0.0.1), note that
subnet broadcast it also supported, e.g. x.y.z.255

  ANYHOST
  ANYBCAST
  LOCALHOST

Typedefs:

These typedefs/prototypes are used for the main pshell function callback
function and the log function callback functions respectively

  def pshellFunction func(args)
  def logFunction func(string)
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
import fcntl
import fnmatch
import commands
from collections import OrderedDict
from collections import namedtuple
import PshellReadline

#################################################################################
#
# "public" API functions and data
#
# Users of this module should only interface via the "public" API
#
#################################################################################

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

# These are the identifiers for the serverMode.  BLOCKING will never return
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

# constants to let the host program set the internal debug log level,
# if the user of this API does not want to see any internal message
# printed out, set the debug log level to LOG_LEVEL_NONE (0)
LOG_LEVEL_NONE = 0
LOG_LEVEL_ERROR = 1
LOG_LEVEL_WARNING = 2
LOG_LEVEL_INFO = 3
LOG_LEVEL_ALL = LOG_LEVEL_INFO
LOG_LEVEL_DEFAULT = LOG_LEVEL_ALL

# Used to specify the radix extraction format for the getInt function
RADIX_DEC = 0
RADIX_HEX = 1
RADIX_ANY = 2

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
#################################################################################
def setLogLevel(level):
  """
  Set the internal log level, valid levels are:

  LOG_LEVEL_ERROR
  LOG_LEVEL_WARNING
  LOG_LEVEL_INFO
  LOG_LEVEL_ALL
  LOG_LEVEL_DEFAULT

  Where the default is LOG_LEVEL_ALL

    Args:
        level (int) : The desired log level to set

    Returns:
        None
  """
  _setLogLevel(level)

#################################################################################
#################################################################################
def setLogFunction(function):
  """
  Provide a user callback function to send the logs to, this allows an
  application to extract all the logs issued by this module to put in it's
  own logfile.  If a log function is not set, all internal logs are just
  sent to the 'print' function.

    Args:
        function (ptr) : Log callback function

    Returns:
        None
  """
  _setLogFunction(function)

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
  Spinning ascii wheel keep alive, user string is optional

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
def tokenize(string, delimiter):
  """
  This will parse a string based on a delimiter and return the parsed
  string as a list

    Args:
        string (str)    : string to tokenize
        delemiter (str) : the delimiter to parse the string

    Returns:
        int  : Number of tokens parsed
        list : The parsed tokens list
  """
  return (_tokenize(string, delimiter))

#################################################################################
#################################################################################
def getLength(string):
  """
  This will return the length of the passed in string

    Args:
        string (str) : string to return length on

    Returns:
        int : the length of the string
  """
  return (_getLength(string))

#################################################################################
#################################################################################
def isEqual(string1, string2):
  """
  This will do a case sensitive compare for string equality

    Args:
        string1 (str) : The source string
        string2 (str) : The target string

    Returns:
        int : True if strings are equal, False otherwise
  """
  return (_isEqual(string1, string2))

#################################################################################
#################################################################################
def isEqualNoCase(string1, string2):
  """
  This will do a case insensitive compare for string equality

    Args:
        string1 (str) : The source string
        string2 (str) : The target string

    Returns:
        int : True if strings are equal, False otherwise
  """
  return (_isEqualNoCase(string1, string2))

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
def isSubStringNoCase(string1, string2, minMatchLength = 0):
  """
  This function will return True if string1 is a substring of string2 at
  position 0.  If the minMatchLength is 0, then it will compare up to the
  length of string1.  If the minMatchLength > 0, it will require a minimum
  of that many characters to match.  A string that is longer than the min
  match length must still match for the remaining charactes, e.g. with a
  minMatchLength of 2, 'q' will not match 'quit', but 'qu', 'qui' or 'quit'
  will match, 'quix' will not match.  This function is useful for wildcard
  matching.  This does a case insensitive match

    Args:
        string1 (str)        : The substring
        string2 (str)        : The target string
        minMatchLength (int) : The minimum required characters match

    Returns:
        bool : True if substring matches, False otherwise
  """
  return (_isSubStringNoCase(string1, string2, minMatchLength))

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
#################################################################################
def isFloat(string):
  """
  This function will parse a string to see if it is in valid floating point format

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if valid floating point format
  """
  return _isFloat(string)

#################################################################################
#################################################################################
def isDec(string):
  """
  This function will parse a string to see if it is in valid decimal format

    Args:
        string (str) : The string string to parse

    Returns:
        bool : True if valid decimal format
  """
  return _isDec(string)

#################################################################################
#################################################################################
def isHex(string, needHexPrefix = True):
  """
  This function will parse a string to see if it is in valid hexidecimal format,
  with or without the preceeding 0x

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if valid hexidecimal format
  """
  return _isHex(string, needHexPrefix)

#################################################################################
#################################################################################
def isAlpha(string):
  """
  This function will parse a string to see if it is in valid alphatebetic format

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if valid alphabetic format
  """
  return _isAlpha(string)

#################################################################################
#################################################################################
def isNumeric(string, needHexPrefix = True):
  """
  This function will parse a string to see if it is in valid numerc format,
  hex or decimal

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if valid numeric format
  """
  return _isNumeric(string, needHexPrefix)

#################################################################################
#################################################################################
def isAlphaNumeric(string):
  """
  This function will parse a string to see if it is in valid alphanumeric format

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if valid alphanumeric format
  """
  return _isAlphaNumeric(string)

#################################################################################
#################################################################################
def isIpv4Addr(string):
  """
  This function will parse a string to see if it is in valid ipv4 address format

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if valid ipv4 address format
  """
  return _isIpv4Addr(string)

#################################################################################
#################################################################################
def isIpv4AddrWithNetmask(string):
  """
  This function will parse a string to see if it is in valid ipv4
  address/netmask format

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if valid ipv4 address/netmask format
  """
  return _isIpv4AddrWithNetmask(string)

#################################################################################
#################################################################################
def isMacAddr(string):
  """
  This function will parse a string to see if it is in valid MAC address format

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if valid MAC address format
  """
  return _isMacAddr(string)

#################################################################################
#################################################################################
def getBool(string):
  """
  This function will parse a string to see if it 'true', 'yes', or 'on'

    Args:
        string (str) : The string to parse

    Returns:
        bool : True if 'true', 'yes', 'on'
  """
  return _getBool(string)

#################################################################################
#################################################################################
def getInt(string, radix = RADIX_ANY, needHexPrefix = True):
  """
  Returns the integer value of the corresponding string

    Args:
        string : string to convert to integer

    Returns:
        int  : Integer value of corresponding string
  """
  return (_getInt(string, radix, needHexPrefix))

#################################################################################
#################################################################################
def getFloat(string):
  """
  Returns the float value of the corresponding string

    Args:
        string : string to convert to float

    Returns:
        int  : Float value of corresponding string
  """
  return (_getFloat(string))

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
#
# global "private" data
#
#################################################################################

_gCommandHelp = ('?', '-h', '--h', '-help', '--help')
_gListHelp = ('?', 'help')
_gCommandList = []
_gMaxLength = len("history")

_gServerVersion = "1"
_gServerName = None
_gServerType = None
_gServerMode = None
_gHostnameOrIpAddr = None
_gPort = None
_gPrompt = "PSHELL> "
_gTitle = "PSHELL"
_gBanner = "PSHELL: Process Specific Embedded Command Line Shell"
_gSocketFd = None
_gConnectFd = None
_gFromAddr = None
_gFileSystemPath = "/tmp/.pshell/"
_gArgs = None
_gFoundCommand = None
_gUnixSourceAddress = None
_gLockFile = None
_gLockFileExtension = ".lock"
_gUnixLockFileId = "unix"+_gLockFileExtension
_gLockFd = None
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
_gPshellMsgPayloadLength = 1024*64

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

_gQuitLocal = False
_gQuitTcp = False
_gIdleSessionTimeout = 10  # PshellReadline.IDLE_TIMEOUT_NONE
_gPshellClientTimeout = 5  # seconds
_gTcpConnectSockName = None
_gTcpPrompt = None
_gTcpTitle = None
# flag to indicate thespecial pshell.py client
_gPshellClient = False
_gClientTimeoutOverride = None

# log level and log print function
_gLogLevel = LOG_LEVEL_DEFAULT
_gLogFunction = None

_MAX_BIND_ATTEMPTS = 1000

_gBatchFiles = {"maxFilenameLength":8, "maxDirectoryLength":9, "files":[]}

#################################################################################
#
# global "private" functions
#
#################################################################################

#################################################################################
#################################################################################
def _tokenize(string_, delimiter_):
  return (len(str(string_).split(delimiter_)), str(string_).split(delimiter_))

#################################################################################
#################################################################################
def _getLength(string_):
  return (len(str(string_)))

#################################################################################
#################################################################################
def _isEqual(string1_, string2_):
  return (str(string1_) == str(string2_))

#################################################################################
#################################################################################
def _isEqualNoCase(string1_, string2_):
  return (isEqual(str(string1_).lower(), str(string2_).lower()))

#################################################################################
#################################################################################
def _isSubString(string1_, string2_, minMatchLength_):
  return (PshellReadline.isSubString(str(string1_), str(string2_), minMatchLength_))

#################################################################################
#################################################################################
def _isSubStringNoCase(string1_, string2_, minMatchLength_ = 0):
  return (_isSubString(str(string1_).lower(), str(string2_).lower(), minMatchLength))

#################################################################################
#################################################################################
def _getOption(arg_):
  if (len(arg_) < 3):
    return (False, "", "")
  elif (arg_[0] == "-"):
    return (True, arg_[:2], arg_[2:])
  else:
    value = arg_.split("=")
    if (len(value) >= 2):
      return (True, value[0], '='.join(value[1:]))
    else:
      return (False, "", "")

#################################################################################
#################################################################################
def _isFloat(string_):
  split = str(string_).split(".")
  if len(split) != 2:
    return False
  else:
    return split[1][0] != "-" and isDec(split[0]) and isDec(split[1])

#################################################################################
#################################################################################
def _isDec(string_):
  try:
    int(str(string_), 10)
    return True
  except ValueError:
    return False

#################################################################################
#################################################################################
def _isHex(string_, needHexPrefix_):
  string = str(string_)
  if needHexPrefix_:
    # hex prefix requested, make sure they provided one
    if len(string) < 3 or string[0:2].lower() != "0x":
      return False
  else:
    # no prefix requested, make sure the did not provide one, since the
    # following 'int' call for base 16 will work either way
    if "0x" in string:
      return False
  try:
    int(string, 16)
    return True
  except ValueError:
    return False

#################################################################################
#################################################################################
def _isAlpha(string_):
  return str(string_).isalpha()

#################################################################################
#################################################################################
def _isNumeric(string_, needHexPrefix_):
  return _isDec(string_) or _isHex(string_, needHexPrefix_)

#################################################################################
#################################################################################
def _isAlphaNumeric(string_):
  return str(string_).isalnum()

#################################################################################
#################################################################################
def _isIpv4Addr(string_):
  addr = string_.split(".")
  return (len(addr) == 4 and
          isDec(addr[0]) and
          getInt(addr[0]) >= 0 and
          getInt(addr[0]) <= 255 and
          isDec(addr[1]) and
          getInt(addr[1]) >= 0 and
          getInt(addr[1]) <= 255 and
          isDec(addr[2]) and
          getInt(addr[2]) >= 0 and
          getInt(addr[2]) <= 255 and
          isDec(addr[3]) and
          getInt(addr[3]) >= 0 and
          getInt(addr[3]) <= 255)

#################################################################################
#################################################################################
def _isIpv4AddrWithNetmask(string_):
  addr = string_.split("/")
  return (len(addr) == 2 and
          isIpv4Addr(addr[0]) and
          isDec(addr[1]) and
          getInt(addr[1]) >= 0 and
          getInt(addr[1]) <= 32)

#################################################################################
#################################################################################
def _isMacAddr(string_):
  addr = string_.split(":")
  return (len(addr) == 6 and
          isHex(addr[0], False) and len(addr[0]) == 2 and
          isHex(addr[1], False) and len(addr[1]) == 2 and
          isHex(addr[2], False) and len(addr[2]) == 2 and
          isHex(addr[3], False) and len(addr[3]) == 2 and
          isHex(addr[4], False) and len(addr[4]) == 2 and
          isHex(addr[5], False) and len(addr[5]) == 2)

#################################################################################
#################################################################################
def _getBool(string_):
  string = str(string_)
  return ((string.lower() == "true") or
          (string.lower() == "yes") or
          (string.lower() == "on"))

#################################################################################
#################################################################################
def _getInt(string_, radix_, needHexPrefix_):
  string = str(string_)
  if radix_ == RADIX_ANY:
    if _isDec(string):
      return (int(string, 10))
    elif _isHex(string, needHexPrefix_):
      return (int(string, 16))
    else:
      _printError("Could not extract numeric value from string: '%s', consider checking format with PshellServer.isNumeric()" % string)
      return (0)
  elif radix_ == RADIX_DEC and _isDec(string):
    return (int(string, 10))
  elif radix_ == RADIX_HEX and _isHex(string, needHexPrefix_):
    return (int(string, 16))
  else:
    _printError("Could not extract numeric value from string: '%s', consider checking format with PshellServer.isNumeric()" % string)
    return (0)

#################################################################################
#################################################################################
def _getFloat(string_):
  string = str(string_)
  if _isFloat(string):
    return (float(string))
  else:
    _printError("Could not extract floating point value from string: '%s', consider checking format with PshellServer.isFloat()" % string)
    return (0.0)

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
    _printError("NULL command name, command not added")
    return

  # see if we have a NULL description
  if ((description_ == None) or (len(description_) == 0)):
    _printError("NULL description, command: '%s' not added" % command_)
    return

  # see if we have a NULL function
  if (function_ == None):
    _printError("NULL function, command: '%s' not added" % command_)
    return

  # if they provided no usage for a function with arguments
  if (((maxArgs_ > 0) or (minArgs_ > 0)) and (command_ != "quit") and (command_ != "history") and ((usage_ == None) or (len(usage_) == 0))):
    _printError("NULL usage for command that takes arguments, command: '%s' not added" % command_)
    return

  # see if their minArgs is greater than their maxArgs, we ignore if maxArgs is 0
  # because that is the default value and we will set maxArgs to minArgs if that
  # case later on in this function
  if ((minArgs_ > maxArgs_) and (maxArgs_ > 0)):
    _printError("minArgs: %d is greater than maxArgs: %d, command: '%s' not added" % (minArgs_, maxArgs_, command_))
    return

  # see if it is a duplicate command
  for command in _gCommandList:
    if (command["name"] == command_):
      # command name already exists, don't add it again
      _printError("Command: %s already exists, not adding command" % command_)
      return

  if len(command_.split()) > 1:
    # we do not allow any commands with whitespace, single keyword commands only
    _printError("Whitespace found, command: '%s' not added" % command_)
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
                             "showUsage":showUsage_,
                             "length":len(command_)})
  else:
    _gCommandList.append({"function":function_,
                          "name":command_,
                          "description":description_,
                          "usage":usage_,
                          "minArgs":minArgs_,
                          "maxArgs":maxArgs,
                          "showUsage":showUsage_,
                          "length":len(command_)})

#################################################################################
#################################################################################
def _startServer(serverName_, serverType_, serverMode_, hostnameOrIpAddr_, port_):
  global _gServerName
  global _gServerType
  global _gServerMode
  global _gHostnameOrIpAddr
  global _gPort
  global _gRunning
  global _gPrompt
  global _gIdleSessionTimeout
  _cleanupFileSystemResources()
  if (_gRunning == False):
    _gServerName = serverName_
    _gServerType = serverType_
    _gServerMode = serverMode_
    _gHostnameOrIpAddr = hostnameOrIpAddr_
    _gPort = port_
    if _gServerType == TCP:
      _gIdleSessionTimeout = 10  # minutes
    _loadConfigFile()
    _loadStartupFile()
    if _gPrompt[-1] != " ":
      _gPrompt = _gPrompt + " "
    _gRunning = True
    if (_gServerMode == BLOCKING):
      _runServer()
    else:
      # spawn thread
      thread.start_new_thread(_serverThread, ())
  else:
    _printError("PSHELL server: %s is already running" % serverName_)

#################################################################################
#################################################################################
def _serverThread():
  _runServer()

#################################################################################
#################################################################################
def _cleanupFileSystemResources():
  global _gFileSystemPath
  global _gLockFileExtension
  global _gUnixLockFileId
  if not os.path.isdir(_gFileSystemPath):
    os.system("mkdir %s" % _gFileSystemPath)
    os.system("chmod 777 %s" % _gFileSystemPath)
  lockFiles = fnmatch.filter(os.listdir(_gFileSystemPath), "*"+_gLockFileExtension)
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
        None
    except:
      None

#################################################################################
#################################################################################
def _bindSocket(address_):
  global _gSocketFd
  global _gPort
  global _gHostnameOrIpAddr
  global _gServerType
  global _gUnixSourceAddress
  global _gServerName
  global _gLockFd
  global _gLockFile
  global _gLockFileExtension
  global _gFileSystemPath
  global _MAX_BIND_ATTEMPTS
  if _gServerType == UNIX:
    # Unix domain socket
    _gLockFile = _gUnixSourceAddress+"-unix"+_gLockFileExtension
    for attempt in range(1,_MAX_BIND_ATTEMPTS+1):
      try:
        _gLockFd = open((_gLockFile), "w+")
        fcntl.flock(_gLockFd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        _gSocketFd.bind((_gUnixSourceAddress))
        if attempt > 1:
          _gServerName = _gServerName + str(attempt-1)
        return
      except Exception as error:
        if attempt == 1:
          # only print message on first attemps
          _printWarning("Could not bind to UNIX address: {}, looking for first available address".format(_gServerName))
        _gUnixSourceAddress = address_ + str(attempt)
        _gLockFile = _gUnixSourceAddress+"-unix"+_gLockFileExtension
    _printError("Could not find available address after {} attempts".format(_MAX_BIND_ATTEMPTS))
  else:
    # IP domain socket
    port = _gPort
    for attempt in range(1,_MAX_BIND_ATTEMPTS+1):
      try:
        _gSocketFd.bind((address_, port))
        _gLockFile = _gFileSystemPath + _gServerName + "-" + _gServerType + "-" + _gHostnameOrIpAddr + "-" + str(port) + _gLockFileExtension
        _gLockFd = open((_gLockFile), "w+")
        fcntl.flock(_gLockFd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return
      except Exception as error:
        if attempt == 1:
          # only print message on first attemps
          _printWarning("Could not bind to requested port: {}, looking for first available port".format(_gPort))
        port = _gPort + attempt
        _gPort = port
    _printError("Could not find available port after {} attempts".format(_MAX_BIND_ATTEMPTS))
  raise Exception(error)

#################################################################################
#################################################################################
def _createSocket():
  global _gServerName
  global _gServerType
  global _gHostnameOrIpAddr
  global _gPort
  global _gSocketFd
  global _gFileSystemPath
  global _gUnixSourceAddress
  try:
    if (_gServerType == UDP):
      # IP domain socket (UDP)
      _gSocketFd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      ipAddrOctets = _gHostnameOrIpAddr.split(".")
      if (_gHostnameOrIpAddr == ANYHOST):
        _bindSocket("")
        #_gSocketFd.bind(("", _gPort))
      elif (_gHostnameOrIpAddr == LOCALHOST):
        _bindSocket("127.0.0.1")
      elif (_gHostnameOrIpAddr == ANYBCAST):
        # global broadcast address
        _gSocketFd.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        _bindSocket("255.255.255.255")
      elif ((len(ipAddrOctets) == 4) and (ipAddrOctets[3] == "255")):
        # subnet broadcast address
        _gSocketFd.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        _bindSocket(_gHostnameOrIpAddr)
      else:
        _bindSocket(_gHostnameOrIpAddr)
    elif (_gServerType == TCP):
      # IP domain socket (TCP)
      _gSocketFd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      _gSocketFd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
      # Bind the socket to the port
      if (_gHostnameOrIpAddr == ANYHOST):
        _bindSocket("")
      elif (_gHostnameOrIpAddr == LOCALHOST):
        _bindSocket("127.0.0.1")
      else:
        _bindSocket(_gHostnameOrIpAddr)
      # Listen for incoming connections
      _gSocketFd.listen(1)
    elif (_gServerType == UNIX):
      _gSocketFd = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
      _gUnixSourceAddress = _gFileSystemPath+_gServerName
      _bindSocket(_gUnixSourceAddress)
    return (True)
  except Exception as error:
    _printError("{}".format(error))
    return (False)

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
  if (_createSocket()):
    _printInfo("UDP Server: %s Started On Host: %s, Port: %d" %
          (_gServerName, _gHostnameOrIpAddr, _gPort))
    _runDGRAMServer()
  else:
    _printError("Cannot create socket for UDP Server: %s On Host: %s, Port: %d" %
          (_gServerName, _gHostnameOrIpAddr, _gPort))

#################################################################################
#################################################################################
def _runUNIXServer():
  global _gServerName
  if (_createSocket()):
    _printInfo("UNIX Server: %s Started" % _gServerName)
    _runDGRAMServer()
  else:
    _printError("Cannot create socket for UNIX Server: %s" % _gServerName)

#################################################################################
#################################################################################
def _runDGRAMServer():
  _addNativeCommands()
  while (True):
    _receiveDGRAM()

#################################################################################
#################################################################################
def _acceptConnection():
  global _gSocketFd
  global _gConnectFd
  global _gTcpConnectSockName
  try:
    (_gConnectFd, clientAddr) = _gSocketFd.accept()
    _gTcpConnectSockName = clientAddr[0]
    return (True)
  except Exception as error:
    _printError("{}".format(error))
    return (False)

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
  global _gIdleSessionTimeout
  socketCreated = True
  connectionAccepted = True
  initialStartup = True
  while socketCreated and connectionAccepted:
    # startup our TCP server and accept new connections
    socketCreated = _createSocket()
    if socketCreated:
      if initialStartup:
        _printInfo("TCP Server: %s Started On Host: %s, Port: %d" %
              (_gServerName, _gHostnameOrIpAddr, _gPort))
        _addNativeCommands()
        initialStartup = False
      connectionAccepted = _acceptConnection()
      if connectionAccepted:
        # shutdown original socket to not allow any new connections
        # until we are done with this one
        _gTcpPrompt = _gServerName + "[" + _gTcpConnectSockName + ":" + str(_gPort) + "]:" + _gPrompt
        _gTcpTitle = _gTitle + ": " + _gServerName + "[" + \
                     _gTcpConnectSockName + ":" + str(_gPort) + "], Mode: INTERACTIVE"
        PshellReadline.setFileDescriptors(_gConnectFd,
                                          _gConnectFd,
                                          PshellReadline.SOCKET,
                                          PshellReadline.ONE_MINUTE*_gIdleSessionTimeout)
        try:
          _gSocketFd.shutdown(socket.SHUT_RDWR)
        except:
          None
        _receiveTCP()
        _gConnectFd.shutdown(socket.SHUT_RDWR)
        _gConnectFd.close()
        _gSocketFd.close()
  if not socketCreated or not connectionAccepted:
    _printError("Cannot create socket for TCP Server: %s On Host: %s, Port: %d" %
          (_gServerName, _gHostnameOrIpAddr, _gPort))

#################################################################################
#################################################################################
def _runLocalServer():
  global _gPrompt
  global _gTitle
  global _gQuitLocal
  _gPrompt = _getDisplayServerName() + "[" + \
             _getDisplayServerType() + "]:" + _getDisplayPrompt()
  _gTitle = _getDisplayTitle() + ": " + _getDisplayServerName() + \
            "[" + _getDisplayServerType() + "], Mode: INTERACTIVE"
  _addNativeCommands()
  _showWelcome()
  _gQuitLocal = False
  command = ""
  PshellReadline.setIdleTimeout(PshellReadline.ONE_MINUTE*_gIdleSessionTimeout)
  while (not _gQuitLocal):
    (command, _gQuitLocal) = PshellReadline.getInput(_gPrompt)
    if (not _gQuitLocal):
      _processCommand(command)

#################################################################################
#################################################################################
def _addNativeCommands():
  global _gPshellClient
  global _gServerType
  if ((_gServerType == TCP) or (_gServerType == LOCAL)):
    _addCommand(_batch,
                "batch",
                "run commands from a batch file",
                "{{<filename> | <index>} [-show]} | -list",
                1,
                3,
                False,
                True)
    _addCommand(_history,
                "history",
                "show history list of all entered commands",
                "",
                0,
                1,
                True,
                True)
    _addCommand(_help,
                "help",
                "show all available commands",
                "",
                0,
                0,
                True,
                True)
    _addCommand(_quit,
                "quit",
                "exit interactive mode",
                "",
                0,
                1,
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
  global _gPshellClient
  global _gClientTimeoutOverride
  global _gPshellClientTimeout

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
    _gClientTimeoutOverride = None
    if _gPshellClient and "-t" in command_.split()[0]:
      _gClientTimeoutOverride = command_.split()[0]
      command_ = ' '.join(command_.split()[1:])
      if len(command_) == 0:
        if len(_gClientTimeoutOverride) > 2:
          _gPshellClientTimeout = int(_gClientTimeoutOverride[2:])
          printf("PSHELL_INFO: Setting server response timeout to: %d seconds" % _gPshellClientTimeout)
        else:
          printf("PSHELL_INFO: Current server response timeout: %d seconds" % _gPshellClientTimeout)
        return
    _gArgs = command_.split()[_gFirstArgPos:]
    command_ = command_.split()[0]
    numMatches = 0
    if ((command_ == "?") or (command_ == "help")):
      _help(_gArgs)
      _gCommandDispatched = False
      return
    else:
      length = len(command_)
      for command in _gCommandList:
        # do an initial substring match for command abbreviation support
        if (isSubString(command_, command["name"], len(command_))):
          _gFoundCommand = command
          numMatches += 1
          # if we have an exact match, take it and break out
          if _gFoundCommand["length"] == length:
            numMatches = 1
            break
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
  printf("")
  printf("***********************************************")
  printf("*            AVAILABLE BATCH FILES            *")
  printf("***********************************************")
  printf("")
  printf("%s   %-*s   %-*s" % ("Index", _gBatchFiles["maxFilenameLength"], "Filename", _gBatchFiles["maxDirectoryLength"], "Directory"))
  printf("%s   %s   %s" % ("=====", "="*_gBatchFiles["maxFilenameLength"], "="*_gBatchFiles["maxDirectoryLength"]))
  for index, entry in enumerate(_gBatchFiles["files"]):
    printf("%-5d   %-*s   %-*s" % (index+1, _gBatchFiles["maxFilenameLength"], entry["filename"], _gBatchFiles["maxDirectoryLength"], entry["directory"]))
  printf("")

#################################################################################
#################################################################################
def getBatchFile(filename_):
  global _gBatchFiles

  batchFile = None

  _gBatchFiles["files"] = []
  _gBatchFiles["maxDirectoryLength"] = 9
  _gBatchFiles["maxFilenameLength"] = 8

  findBatchFiles(os.getcwd())
  findBatchFiles(os.getenv('PSHELL_BATCH_DIR'))
  findBatchFiles(_PSHELL_BATCH_DIR)

  if filename_ == "?" or filename_ == "-h":
    printf("")
    _showUsage()
    printf("")
    printf("  where:")
    printf("    filename  - Filename of the batch file to execute")
    printf("    index     - Index of the batch file to execute (from the -list option)")
    printf("    -list     - List all the available batch files")
    printf("    -show     - Show the contents of the batch file without executing")
    printf("")
    printf("  NOTE: Batch files must have a .psh or .batch extension.  Batch")
    printf("        files will be searched in the following directory order:")
    printf("")
    printf("        current directory - {}".format(os.getcwd()))
    printf("        $PSHELL_BATCH_DIR - {}".format(os.getenv('PSHELL_BATCH_DIR')))
    printf("        default directory - {}".format(_PSHELL_BATCH_DIR))
    printf("")
  elif ((_gFirstArgPos == 0) and (filename_ in "batch")):
    _showUsage()
  elif (isSubString(filename_, "-list", 2)):
    showBatchFiles()
  elif (isDec(filename_)):
    if (int(filename_) > 0 and int(filename_) <= len(_gBatchFiles["files"])):
      batchFile = _gBatchFiles["files"][int(filename_)-1]["directory"] + "/" + _gBatchFiles["files"][int(filename_)-1]["filename"]
    else:
      printf("ERROR: Invalid batch file index: {}, valid values 1-{}".format(filename_, len(_gBatchFiles["files"])))
  else:
    numMatches = 0
    for index, entry in enumerate(_gBatchFiles["files"]):
      if (isSubString(filename_, entry["filename"], len(filename_))):
        batchFile = entry["directory"] + "/" + entry["filename"]
        numMatches += 1
    if (numMatches == 0):
      printf("PSHELL_ERROR: Could not find batch file: '{}', use -list option to see available files".format(filename_))
    elif (numMatches > 1):
      printf("PSHELL_ERROR: Ambiguous file: '{}', use -list option to see available files or <index> to select specific file".format(filename_))
      return (None)
  return (batchFile)

#################################################################################
#################################################################################
def _batch(command_):
  global _gFirstArgPos
  if command_[0] == "batch":
    command_ = command_[1:]
  if len(command_) > 0:
    batchFile = command_[0]
  else:
    showUsage()
    return
  if isSubString(command_[-1], "-show", 2):
    showOnly = True
  else:
    showOnly = False
  batchFile = getBatchFile(batchFile)
  if (batchFile != None and os.path.isfile(batchFile)):
    file = open(batchFile, 'r')
    for line in file:
      # skip comments
      line = line.strip()
      if ((len(line) > 0) and (line[0] != "#")):
        if showOnly:
          printf(line)
        else:
          _processCommand(line)
    file.close()

#################################################################################
#################################################################################
def _history(command_):
  PshellReadline._showHistory()

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
def _quit(command_):
  global _gQuitTcp
  global _gQuitLocal
  global _gServerType
  if (_gServerType == TCP):
    # TCP server, signal receiveTCP function to quit
    _gQuitTcp = True
  else:
    # local server, exit the process
    _gQuitLocal = True

#################################################################################
#################################################################################
def _showWelcome():
  global _gTitle
  global _gServerType
  global _gHostnameOrIpAddr
  global _gIdleSessionTimeout
  global _gServerName
  global _gTcpConnectSockName
  global _gTcpTitle
  global _gPshellClient
  global _gPshellClientTimeout
  global _gPort
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
    server = "#  Single session TCP server: %s[%s:%d]" % (_gServerName,
                                                          _gTcpConnectSockName,
                                                          _gPort)
  maxBorderWidth = max(58, len(banner),len(server))+2
  printf()
  printf("#"*maxBorderWidth)
  printf("#")
  printf(banner)
  printf("#")
  printf(server)
  printf("#")
  if (_gIdleSessionTimeout == PshellReadline.IDLE_TIMEOUT_NONE):
    printf("#  Idle session timeout: NONE")
  else:
    printf("#  Idle session timeout: %d minutes" % _gIdleSessionTimeout)
  printf("#")
  if (_gPshellClient == True):
    if _gPshellClientTimeout > 0:
      printf("#  Command response timeout: {} seconds".format(_gPshellClientTimeout))
    else:
      printf("#  Command response timeout: NONE")
      printf("#")
      printf("#  WARNING: Interactive client started with no command")
      printf("#           response timeout.  All commands will be")
      printf("#           sent as 'fire-and-forget', no results will")
      printf("#           be extracted or displayed")
    printf("#")
    printf("#  The default response timeout can be changed on a")
    printf("#  per-command basis by preceeding the command with")
    printf("#  option -t<timeout> (use -t0 for no response)")
    printf("#")
    printf("#  e.g. -t10 command")
    printf("#")
    printf("#  The default timeout for all commands can be changed")
    printf("#  by using the -t<timeout> option with no command, to")
    printf("#  display the current default timeout, just use -t")
    printf("#")
  printf("#  Type '?' or 'help' at prompt for command summary")
  printf("#  Type '?' or '-h' after command for command usage")
  printf("#")
  printf("#  Full <TAB> completion, command history, command")
  printf("#  line editing, and command abbreviation supported")
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
      message_ = str(message_)+"\n"
    if ((_gServerType == LOCAL) or (_gServerType == TCP)):
      PshellReadline.writeOutput(str(message_))
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
    except Exception as error:
      _printError("{}".format(error))

#################################################################################
#################################################################################
def _cleanupResources():
  global _gUnixSourceAddress
  global _gLockFile
  global _gSocketFd
  if (_gUnixSourceAddress != None):
    try:
      os.unlink(_gUnixSourceAddress)
    except:
      None
  try:
    os.unlink(_gLockFile)
  except:
    None
  _cleanupFileSystemResources()
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
  global _gIdleSessionTimeout
  configFile1 = ""
  configPath = os.getenv('PSHELL_CONFIG_DIR')
  if (configPath != None):
    configFile1 = configPath+"/"+_PSHELL_CONFIG_FILE
  configFile2 = _PSHELL_CONFIG_DIR+"/"+_PSHELL_CONFIG_FILE
  if (os.path.isfile(configFile1)):
    file = open(configFile1, 'r')
  elif (os.path.isfile(configFile2)):
    file = open(configFile2, 'r')
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
          elif (option[1].lower() == "timeout"):
            if (value[1].isdigit()):
              _gIdleSessionTimeout = int(value[1])
            elif (value[1].lower() == "none"):
              _gIdleSessionTimeout = PshellReadline.IDLE_TIMEOUT_NONE
  file.close()
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
  file.close()

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
    length = len(command_)
    for command in _gCommandList:
      # do an initial substring match for command abbreviation support
      if (isSubString(command_, command["name"], len(command_))):
        _gFoundCommand = command
        numMatches += 1
        # if we have an exact match, take it and break out
        if _gFoundCommand["length"] == length:
          numMatches = 1
          break
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
  global _gMsgTypes
  if ((_gCommandInteractive == True) and
      (_gPshellMsg["msgType"] != _gMsgTypes["controlCommand"]) and
      ((_gServerType == UDP) or (_gServerType == UNIX))):
    _reply()
    _gPshellMsg["payload"] = ""

#################################################################################
#################################################################################
def _setLogLevel(level_):
  global _gLogLevel
  _gLogLevel = level_

#################################################################################
#################################################################################
def _setLogFunction(function_):
  global _gLogFunction
  _gLogFunction = function_

#################################################################################
#################################################################################
def _printError(message_):
  if _gLogLevel >= LOG_LEVEL_ERROR:
    _printLog("PSHELL_ERROR: {}".format(message_))

#################################################################################
#################################################################################
def _printWarning(message_):
  if _gLogLevel >= LOG_LEVEL_WARNING:
    _printLog("PSHELL_WARNING: {}".format(message_))

#################################################################################
#################################################################################
def _printInfo(message_):
  if _gLogLevel >= LOG_LEVEL_INFO:
    _printLog("PSHELL_INFO: {}".format(message_))

#################################################################################
#################################################################################
def _printLog(message_):
  if _gLogFunction is not None:
    _gLogFunction(message_)
  else:
    print(message_)

#################################################################################
#
# start of main program
#
#################################################################################
if (__name__ == '__main__'):
  # just print out a message identifying this as the 'full' module
  print("PSHELL_INFO: PshellServer FULL Module")
