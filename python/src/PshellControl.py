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
A Python module to invoke pshell commands in another process

This API provides the ability for a client program to invoke pshell commands
that are registered in a remote program that is running a pshell UDP or UNIX
server.  The format of the command string that is sent to the pshell should
be in the same usage format that the given command is expecting.  This
provides a very lightweight way to provide a control mechanism into a program
that is running a PshellServer, this is analagous to a remote procedure call (rpc).

This module provides the same functionality as the PshellControl.h API and
the libpshell-control linkable 'C' library, but implemented as a Python
module.

Functions:

connectServer()        -- connect to a remote pshell server
disconnectServer()     -- disconnect from a remote pshell server
disconnectAllServers() -- disconnect from all connected remote pshell servers
setDefaultTimeout()    -- set the default server response timeout
extractCommands()      -- extract all commands from remote server
addMulticast()         -- add a command keyword to a multicast group
sendMulticast()        -- send a command to a multicast group
sendCommand1()         -- send command to server using default timeout, no results extracted
sendCommand2()         -- send command to server using timeout override, no results extracted
sendCommand3()         -- send command to server using default timeout, results extracted
sendCommand4()         -- send command to server using timeout override, results extracted
getResponseString()    -- return the human readable form of one of the command response return codes
setLogLevel()          -- set the internal log level for this module
setLogFunction()       -- register a user function to receive all logs

Integer constants:

Helpful items used for the server response timeout values

NO_WAIT
ONE_MSEC
ONE_SEC
ONE_MINUTE
ONE_HOUR

These are returned from the sendCommandN functions

COMMAND_SUCCESS
COMMAND_NOT_FOUND
COMMAND_INVALID_ARG_COUNT
SOCKET_SEND_FAILURE
SOCKET_SELECT_FAILURE
SOCKET_RECEIVE_FAILURE
SOCKET_TIMEOUT
SOCKET_NOT_CONNECTED

Used if we cannot connect to the local source socket

INVALID_SID

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

String constants:

This is used for the host when connecting to a server running
at the loopback localhost address

LOCALHOST

Use this as the "port" identifier for the connectServer
call when using a UNIX domain server

UNIX

Specifies if the addMulticast should add the given command to all specified
multicast receivers or if all control destinations should receive the given
multicast command

MULTICAST_ALL

A complete example of the usage of the API can be found in the included
demo programs pshellControlDemo.py and pshellAggregatorDemo.py
"""

# import all our necessary modules
import os
import sys
import time
import select
import socket
import struct
import random
import fcntl
import fnmatch
from collections import OrderedDict
from collections import namedtuple

#################################################################################
#
# global "public" data, these are used for various parts of the public API
#
#################################################################################

# helpful items used for the timeout values
NO_WAIT = 0
ONE_MSEC = 1
ONE_SEC = ONE_MSEC*1000
ONE_MINUTE = ONE_SEC*60
ONE_HOUR = ONE_MINUTE*60

# use this when connecting to server running at loopback address
LOCALHOST = "localhost"

# use this as the "port" identifier for the connectServer call when
# using a UNIX domain server
UNIX = "unix"

# the following enum values are returned by the non-extraction
# based sendCommand1 and sendCommand2 functions
#
# the following "COMMAND" enums are returned by the remote pshell server
# and must match their corresponding values in PshellServer.cc
COMMAND_SUCCESS = 0
COMMAND_NOT_FOUND = 1
COMMAND_INVALID_ARG_COUNT = 2
# the following "SOCKET" enums are generated internally by the sendCommandN functions
SOCKET_SEND_FAILURE = 3
SOCKET_SELECT_FAILURE = 4
SOCKET_RECEIVE_FAILURE = 5
SOCKET_TIMEOUT = 6
SOCKET_NOT_CONNECTED = 7

# specifies if the addMulticast should add the given command to all
# destinations or all commands to the specified destinations
MULTICAST_ALL = "__multicast_all__"

# used if we cannot connect to a local UNIX socket
INVALID_SID = -1

# constants to let the host program set the internal debug log level,
# if the user of this API does not want to see any internal message
# printed out, set the debug log level to LOG_LEVEL_NONE (0)
LOG_LEVEL_NONE = 0      # No debug logs
LOG_LEVEL_ERROR = 1     # PSHELL_ERROR
LOG_LEVEL_WARNING = 2   # PSHELL_ERROR, PSHELL_WARNING
LOG_LEVEL_INFO = 3      # PSHELL_ERROR, PSHELL_WARNING, PSHELL_INFO
LOG_LEVEL_ALL = LOG_LEVEL_INFO
LOG_LEVEL_DEFAULT = LOG_LEVEL_WARNING

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
def connectServer(controlName, remoteServer, port, defaultTimeout):
  """
  Connect to a pshell server in another process, note that this does
  not do any handshaking to the remote pshell or maintain a connection
  state, it meerly sets the internal destination remote pshell server
  information to use when sending commands via the sendCommandN
  functions and sets up any resources necessary for the source socket,
  the timeout value is the number of milliseconds to wait for a response
  from the remote command in the sendCommandN functions, a timeout
  value of 0 will not wait for a response, in which case the function
  will return either SOCKET_NOT_CONNECTED, SOCKET_SEND_FAILURE, or
  COMMAND_SUCCESS, the timeout value entered in this function will
  be used as the default timeout for all calls to sendCommandN that
  do not provide an override timeout value, for a UDP server, the
  remoteServer must be either a valid hostname or IP address and a
  valid destination port must be provided, for a UNIX server, only
  a valid server name must be provided along with the identifier
  PshellControl.UNIX for the 'port' parameter

  This function returns a Server ID (sid) handle which must be saved and
  used for all subsequent calls into this module

    Args:
        controlName (str)    : The logical name of the control server
        remoteServer (str)   : The server name (UNIX) or hostname/IP address (UDP) of the remote server
        port (str)           : The UDP port of the remote server
        defaultTimeout (int) : The default timeout (in msec) for the remote server response

    Returns:
        int: The ServerId (sid) handle of the connected server or INVALID_SID on failure
  """
  return (_connectServer(controlName, remoteServer, port, defaultTimeout))

#################################################################################
#################################################################################
def disconnectServer(sid):
  """
  Cleanup any resources associated with the server connection, including
  releasing any temp file handles, closing any local socket handles etc.

    Args:
        sid (int) : The ServerId as returned from the connectServer call

    Returns:
        none
  """
  _disconnectServer(sid)

#################################################################################
#################################################################################
def disconnectAllServers():
  """
  Use this function to cleanup any resources for all connected servers, this
  function should be called upon program termination, either in a graceful
  termination or within an exception signal handler, it is especially important
  that this be called when a unix server is used since there are associated file
  handles that need to be cleaned up

    Args:
        none

    Returns:
        none
  """
  _disconnectAllServers()

#################################################################################
#################################################################################
def setDefaultTimeout(sid, defaultTimeout):
  """
  Set the default server response timeout that is used in the 'send' commands
  that don't take a timeout override

    Args:
        sid (int)            : The ServerId as returned from the connectServer call
        defaultTimeout (int) : The default timeout (in msec) for the remote server response

    Returns:
        none
  """
  _setDefaultTimeout(sid, defaultTimeout)

#################################################################################
#################################################################################
def extractCommands(sid, includeName = True):
  """
  This function will extract all the commands of a remote pshell server and
  present them in a human readable form, this is useful when writing a multi
  server control aggregator, see the demo program pshellAggregatorDemo.py in
  the demo directory for examples

    Args:
        sid (int)          : The ServerId as returned from the connectServer call
        includeName (bool) : Include server name in banner

    Returns:
        str : The remote server's command list in human readable form
  """
  return (_extractCommands(sid, includeName))

#################################################################################
#################################################################################
def addMulticast(command, controlList):
  """
  This command will add a controlList of multicast receivers to a multicast
  group, multicast groups are based either on the command, or if the special
  argument PSHELL_MULTICAST_ALL is used, the given controlList will receive
  all multicast commands, the format of the controlList is a CSV formatted list
  of all the desired controlNames (as provided in the first argument of the
  pshell_connectServer command) that will receive this multicast command or
  if the PSHELL_MULTICAST_ALL is used then all control destinations will
  receive the given multicast command, see examples below

  ex 1: multicast command sent to receivers in CSV controlList

    PshellControl.addMulticast("command", "control1,control2,control3")

  ex 2: all multicast commands sent to receivers in CSV controlList

    PshellControl.addMulticast(PshellControl.MULTICAST_ALL, "control1,control2,control3")

  ex 3: multicast command sent to all receivers

    PshellControl.addMulticast("command", PshellControl.MULTICAST_ALL)

  ex 4: all multicast commands sent to all receivers

    PshellControl.addMulticast(PshellControl.MULTICAST_ALL, PshellControl.MULTICAST_ALL)

    Args:
        command (str)     : The multicast command that will be distributed to the
                            following controlList, if the special MULTICAST_ALL
                            identifier is used, then the controlList will receive
                            all multicast initiated commands
        controlList (str) : A CSV formatted list of all the desired controlNames
                            (as provided in the first argument of the connectServer
                            command) that will receive this multicast command, if
                            the special MULTICAST_ALL identifier is used, then
                            all control destinations will receive the command

    Returns:
        none
  """
  _addMulticast(command, controlList)

##################################################################################
#################################################################################
def sendMulticast(command):
  """
  This command will send a given command to all the registered multicast
  receivers (i.e. sids) for this multicast group, multicast groups are
  based on the command's keyword, this function will issue the command as
  a best effort fire-and-forget command to each receiver in the multicast
  group, no results will be requested or expected, and no response will be
  requested or expected

    Args:
        command (str) : The command to send to the remote server

    Returns:
        none
  """
  _sendMulticast(command)

#################################################################################
#################################################################################
def sendCommand1(sid, command):
  """
  Send a command using the default timeout setup in the connectServer call,
  if the default timeout is 0, the server will not reply with a response and
  this function will not wait for one

    Args:
        sid (int)     : The ServerId as returned from the connectServer call
        command (str) : The command to send to the remote server

    Returns:
        int: Return code result of the command:
               COMMAND_SUCCESS
               COMMAND_NOT_FOUND
               COMMAND_INVALID_ARG_COUNT
               SOCKET_SEND_FAILURE
               SOCKET_SELECT_FAILURE
               SOCKET_RECEIVE_FAILURE
               SOCKET_TIMEOUT
               SOCKET_NOT_CONNECTED
  """
  return (_sendCommand1(sid, command))

#################################################################################
#################################################################################
def sendCommand2(sid, timeoutOverride, command):
  """
  Send a command overriding the default timeout, if the override timeout is 0,
  the server will not reply with a response and this function will not wait for
  one

    Args:
        sid (int)             : The ServerId as returned from the connectServer call
        timeoutOverride (int) : The server timeout override (in msec) for this command
        command (str)         : The command to send to the remote server

    Returns:
        int: Return code result of the command:
               COMMAND_SUCCESS
               COMMAND_NOT_FOUND
               COMMAND_INVALID_ARG_COUNT
               SOCKET_SEND_FAILURE
               SOCKET_SELECT_FAILURE
               SOCKET_RECEIVE_FAILURE
               SOCKET_TIMEOUT
               SOCKET_NOT_CONNECTED
  """
  return (_sendCommand2(sid, timeoutOverride, command))

#################################################################################
#################################################################################
def sendCommand3(sid, command):
  """
  Send a command using the default timeout setup in the connectServer call and
  return any results received in the payload, if the default timeout is 0, the
  server will not reply with a response and this function will not wait for one,
  and no results will be extracted

    Args:
        sid (int)     : The ServerId as returned from the connectServer call
        command (str) : The command to send to the remote server

    Returns:
        str: The human readable results of the command response or NULL
             if no results or command failure
        int: Return code result of the command:
               COMMAND_SUCCESS
               COMMAND_NOT_FOUND
               COMMAND_INVALID_ARG_COUNT
               SOCKET_SEND_FAILURE
               SOCKET_SELECT_FAILURE
               SOCKET_RECEIVE_FAILURE
               SOCKET_TIMEOUT
               SOCKET_NOT_CONNECTED
  """
  return (_sendCommand3(sid, command))

#################################################################################
#################################################################################
def sendCommand4(sid, timeoutOverride, command):
  """
  Send a command overriding the default timeout and return any results received
  in the payload, if the timeout override default timeout is 0, the server will
  not reply with a response and this function will not wait for one, and no
  results will be extracted

    Args:
        sid (int)             : The ServerId as returned from the connectServer call
        timeoutOverride (int) : The server timeout override (in msec) for this command
        command (str)         : The command to send to the remote server

    Returns:
        str: The human readable results of the command response or NULL
             if no results or command failure
        int: Return code result of the command:
               COMMAND_SUCCESS
               COMMAND_NOT_FOUND
               COMMAND_INVALID_ARG_COUNT
               SOCKET_SEND_FAILURE
               SOCKET_SELECT_FAILURE
               SOCKET_RECEIVE_FAILURE
               SOCKET_TIMEOUT
               SOCKET_NOT_CONNECTED
  """
  return (_sendCommand4(sid, timeoutOverride, command))

#################################################################################
#################################################################################
def getResponseString(retCode):
  """
  Return the human readable form of one of the command response return codes

    Args:
        retCode (int)  :  One of the return codes from the above sendCommand
                          functions

    Returns:
        str: The human readable results of the command response
  """
  return (_getResponseString(retCode))

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
  application to get all the logs issued by this module to put in it's
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
# "private" functions and data
#
# Users of this module should never access any of these "private" items directly,
# these are meant to hide the implementation from the presentation of the public
# API above
#
#################################################################################

#################################################################################
#################################################################################
def _connectServer(controlName_, remoteServer_, port_, defaultTimeout_):
  global _gPshellControl
  global _gUnixSocketPath
  global _gLockFileExtension
  global _gPshellMsgPayloadLength
  isBroadcastAddress = False
  _cleanupUnixResources()
  sid = _getSid(controlName_)
  if sid == INVALID_SID:
    (remoteServer_, port_, defaultTimeout_) = _loadConfigFile(controlName_, remoteServer_, port_, defaultTimeout_)
    if (port_.lower() == "unix"):
      # UNIX domain socket
      socketFd = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
      # bind our source socket so we can get replies
      sourceAddress = _gUnixSocketPath+remoteServer_+"-control"+str(random.randrange(1000))
      lockFile = sourceAddress+_gLockFileExtension
      bound = False
      while (not bound):
        try:
          lockFd = open((lockFile), "w+")
          fcntl.flock(lockFd, fcntl.LOCK_EX | fcntl.LOCK_NB)
          socketFd.bind(sourceAddress)
          bound = True
        except Exception as e:
          sourceAddress = _gUnixSocketPath+remoteServer_+"-control"+str(random.randrange(1000))
          lockFile = sourceAddress+_gLockFileExtension
      _gPshellControl.append({"socket":socketFd,
                              "timeout":defaultTimeout_,
                              "serverType":"unix",
                              "isBroadcastAddress":isBroadcastAddress,
                              "lockFd":lockFd,
                              "sourceAddress":sourceAddress,
                              "destAddress":_gUnixSocketPath+remoteServer_,
                              "controlName":controlName_,
                              "remoteServer":controlName_+"[unix]",
                              "pshellMsg":OrderedDict([("msgType",0),
                                                       ("respNeeded",True),
                                                       ("dataNeeded",True),
                                                       ("pad",0),
                                                       ("seqNum",0),
                                                       ("payload","")])})
      # return the newly appended list entry as the SID
      sid = len(_gPshellControl)-1
    else:
      # IP domain socket
      socketFd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      ipAddrOctets = remoteServer_.split(".")
      # if we are trying to use a subnet broadcast address, set our socket option
      if ((len(ipAddrOctets) == 4) and (ipAddrOctets[3] == "255")):
        # subnet broadcast address
        isBroadcastAddress = True
        defaultTimeout_ = NO_WAIT
        socketFd.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
      # bind our source socket so we can get replies
      socketFd.bind(("", 0))
      _gPshellControl.append({"socket":socketFd,
                              "timeout":defaultTimeout_,
                              "serverType":"udp",
                              "isBroadcastAddress":isBroadcastAddress,
                              "lockFd":None,
                              "sourceAddress":None,
                              "destAddress":(remoteServer_, int(port_)),
                              "controlName":controlName_,
                              "remoteServer":controlName_+"["+remoteServer_+"]",
                              "pshellMsg":OrderedDict([("msgType",0),
                                                       ("respNeeded",True),
                                                       ("dataNeeded",True),
                                                       ("pad",0),
                                                       ("seqNum",0),
                                                       ("payload","")])})
      # return the newly appended list entry as the SID
      sid = len(_gPshellControl)-1
  else:
    _printWarning("Control name: '{}' already exists, must use unique control name".format(controlName_))
  return (sid)

#################################################################################
#################################################################################
def _disconnectServer(sid_):
  control = _getControl(sid_)
  if (control != None):
    _removeControl(control)

#################################################################################
#################################################################################
def _disconnectAllServers():
  global _gPshellControl
  for control in _gPshellControl:
    _removeControl(control)
  _gPshellControl = []

#################################################################################
#################################################################################
def _setDefaultTimeout(sid_, defaultTimeout_):
  control = _getControl(sid_)
  if (control != None):
    control["timeout"] = defaultTimeout_

#################################################################################
#################################################################################
def _extractCommands(sid_, includeName_):
  global _gMsgTypes
  results = ""
  control = _getControl(sid_)
  if (control != None):
    control["pshellMsg"]["dataNeeded"] = True
    if (_sendCommand(control, _gMsgTypes["queryCommands"], "query commands", ONE_SEC*5) == COMMAND_SUCCESS):
      results += "\n"
      if includeName_:
        results += (len(control["remoteServer"])+22)*"*"
        results += "\n"
        results += "*   COMMAND LIST: %s   *" % control["remoteServer"]
        results += "\n"
        results += (len(control["remoteServer"])+22)*"*"
        results += "\n"
      else:
        results += "****************************************\n"
        results += "*             COMMAND LIST             *\n"
        results += "****************************************\n"
      results += "\n"
      results += control["pshellMsg"]["payload"]
  return (results)

#################################################################################
#################################################################################
def _extractName(sid_):
  global _gMsgTypes
  results = ""
  control = _getControl(sid_)
  if (control != None):
    control["pshellMsg"]["dataNeeded"] = True
    if (_sendCommand(control, _gMsgTypes["queryName"], "query name", ONE_SEC*5) == COMMAND_SUCCESS):
      results += control["pshellMsg"]["payload"]
  return (results)

#################################################################################
#################################################################################
def _extractTitle(sid_):
  global _gMsgTypes
  results = ""
  control = _getControl(sid_)
  if (control != None):
    control["pshellMsg"]["dataNeeded"] = True
    if (_sendCommand(control, _gMsgTypes["queryTitle"], "query title", ONE_SEC*5) == COMMAND_SUCCESS):
      results = control["pshellMsg"]["payload"]
  return (results)

#################################################################################
#################################################################################
def _extractBanner(sid_):
  global _gMsgTypes
  results = ""
  control = _getControl(sid_)
  if (control != None):
    control["pshellMsg"]["dataNeeded"] = True
    if (_sendCommand(control, _gMsgTypes["queryBanner"], "query banner", ONE_SEC*5) == COMMAND_SUCCESS):
      results = control["pshellMsg"]["payload"]
  return (results)

#################################################################################
#################################################################################
def _extractPrompt(sid_):
  global _gMsgTypes
  results = ""
  control = _getControl(sid_)
  if (control != None):
    control["pshellMsg"]["dataNeeded"] = True
    if (_sendCommand(control, _gMsgTypes["queryPrompt"], "query prompt", ONE_SEC*5) == COMMAND_SUCCESS):
      results = control["pshellMsg"]["payload"]
  return (results)

#################################################################################
#################################################################################
def _addMulticast(command_, controlList_):
  global _gPshellControl
  if controlList_ == MULTICAST_ALL:
    # add the multicast command to all control destinations
    for sid, control in enumerate(_gPshellControl):
      _addMulticastControlName(command_, sid)
  else:
    # add the multicast command to the specified control destinations
    controlNames = controlList_.split(",")
    for controlName in controlNames:
      sid = _getSid(controlName)
      if sid != INVALID_SID:
        _addMulticastSid(command_, sid)
      else:
        _printWarning("Control name: '{}' not found".format(controlName))

#################################################################################
#################################################################################
def _getSid(controlName_):
  global _gPshellControl
  global _gPshellMulticast
  for sid, control in enumerate(_gPshellControl):
    if control["controlName"] == controlName_:
      return sid
  return INVALID_SID

#################################################################################
#################################################################################
def _addMulticastSid(command_, sid_):
  multicastFound = False
  for multicast in _gPshellMulticast:
    if (multicast["command"] == command_):
      multicastFound = True
      sidList = multicast["sidList"]
      break
  if not multicastFound:
    # multicast entry not found for this keyword, add a new one
    _gPshellMulticast.append({"command":command_, "sidList":[]})
    sidList = _gPshellMulticast[-1]["sidList"]
  sidFound = False
  for sid in sidList:
    if sid == sid_:
      # make sure we don't add the same sid twice
      sidFound = True
      break
  if not sidFound:
    # sid not found for this multicast group, add it for this group
    sidList.append(sid_)

#################################################################################
#################################################################################
def _sendMulticast(command_):
  global _gPshellMulticast
  global NO_WAIT
  keywordFound = False
  for multicast in _gPshellMulticast:
    command = command_.split()[0]
    if ((multicast["command"] == MULTICAST_ALL) or (command == multicast["command"][:len(command)])):
      keywordFound = True
      for sid in multicast["sidList"]:
        control = _getControl(sid)
        if (control != None):
          control["dataNeeded"] = False
          _sendCommand(control, _gMsgTypes["controlCommand"], command_, NO_WAIT)
  if not keywordFound:
    _printError("Multicast command: '%s', not found" % command)

#################################################################################
#################################################################################
def _sendCommand1(sid_, command_):
  global _gMsgTypes
  retCode = SOCKET_NOT_CONNECTED
  control = _getControl(sid_)
  if (control != None):
    control["pshellMsg"]["dataNeeded"] = False
    retCode = _sendCommand(control, _gMsgTypes["controlCommand"], command_, control["timeout"])
  return (retCode)

#################################################################################
#################################################################################
def _sendCommand2(sid_, timeoutOverride_, command_):
  global _gMsgTypes
  retCode = SOCKET_NOT_CONNECTED
  control = _getControl(sid_)
  if (control != None):
    control["pshellMsg"]["dataNeeded"] = False
    retCode = _sendCommand(control, _gMsgTypes["controlCommand"], command_, timeoutOverride_)
  return (retCode)

#################################################################################
#################################################################################
def _sendCommand3(sid_, command_):
  global NO_WAIT
  results = ""
  retCode = SOCKET_NOT_CONNECTED
  control = _getControl(sid_)
  if (control != None):
    # for a broadcast server,our default timeout will beforced to NO_WAIT,
    # so no need to force it here
    control["pshellMsg"]["dataNeeded"] = (control["timeout"] > NO_WAIT)
    retCode = _sendCommand(control, _gMsgTypes["controlCommand"], command_, control["timeout"])
    # only try to extract data if not talking to a broadcast address
    if (control["isBroadcastAddress"] == False):
      if (not control["pshellMsg"]["dataNeeded"]):
        _printWarning("Trying to extract data with a 0 wait timeout, no data will be extracted")
      elif (retCode == COMMAND_SUCCESS):
        results = control["pshellMsg"]["payload"]
  return (results, retCode)

#################################################################################
#################################################################################
def _sendCommand4(sid_, timeoutOverride_, command_):
  global NO_WAIT
  results = ""
  retCode = SOCKET_NOT_CONNECTED
  control = _getControl(sid_)
  if (control != None):
    if (control["isBroadcastAddress"] == True):
      # if talking to a broadcast address, force our wait time to 0
      # because we do not request or expecet a response
      timeoutOverride_ = NO_WAIT
    control["pshellMsg"]["dataNeeded"] = (timeoutOverride_ > NO_WAIT)
    retCode = _sendCommand(control, _gMsgTypes["controlCommand"], command_, timeoutOverride_)
    # only try to extract data if not talking to a broadcast address
    if (control["isBroadcastAddress"] == False):
      if (not control["pshellMsg"]["dataNeeded"]):
        _printWarning("Trying to extract data with a 0 wait timeout, no data will be extracted")
      elif (retCode == COMMAND_SUCCESS):
        results = control["pshellMsg"]["payload"]
  return (results, retCode)

#################################################################################
#################################################################################
def _sendCommand(control_, commandType_, command_, timeout_):
  global _gMsgTypes
  global _gPshellControlResponse
  global _gSupressInvalidArgCountMessage
  global NO_WAIT
  retCode = COMMAND_SUCCESS
  if (control_ != None):
    if (control_["isBroadcastAddress"] == True):
      # if talking to a broadcast address, force our wait time to 0
      # because we do not request or expecet a response
      timeout_ = NO_WAIT
    control_["pshellMsg"]["msgType"] = commandType_
    control_["pshellMsg"]["respNeeded"] = (timeout_ > NO_WAIT)
    control_["pshellMsg"]["seqNum"] += 1
    seqNum = control_["pshellMsg"]["seqNum"]
    control_["pshellMsg"]["payload"] = str(command_)
    try:
      sentSize = control_["socket"].sendto(struct.pack(_gPshellMsgHeaderFormat+str(len(control_["pshellMsg"]["payload"]))+"s",
                                           *control_["pshellMsg"].values()),
                                           control_["destAddress"])
    except:
      sentSize = 0
    if (sentSize == 0):
      retCode = SOCKET_SEND_FAILURE
    elif (timeout_ > NO_WAIT):
      while (True):
        try:
          inputready, outputready, exceptready = select.select([control_["socket"]], [], [], float(timeout_)/float(1000.0))
        except:
          inputready = []
        if (len(inputready) > 0):
          control_["pshellMsg"], addr = control_["socket"].recvfrom(_gPshellMsgPayloadLength)
          control_["pshellMsg"] = _PshellMsg._asdict(_PshellMsg._make(struct.unpack(_gPshellMsgHeaderFormat+str(len(control_["pshellMsg"])-struct.calcsize(_gPshellMsgHeaderFormat))+"s", control_["pshellMsg"])))
          if (seqNum > control_["pshellMsg"]["seqNum"]):
            # make sure we have the correct response, this condition can happen if we had
            # a very short timeout for the previous call and missed the response, in which
            # case the response to the previous call will be queued in the socket ahead of
            # our current expected response, when we detect that condition, we read the
            # socket until we either find the correct response or timeout, we toss any previous
            # unmatched responses
            _printWarning("Received seqNum: %d, does not match sent seqNum: %d" % (control_["pshellMsg"]["seqNum"], seqNum))
          else:
            retCode = control_["pshellMsg"]["msgType"]
            break
        else:
          retCode = SOCKET_TIMEOUT
          break
      control_["pshellMsg"]["seqNum"] = seqNum
  else:
    retCode = SOCKET_NOT_CONNECTED
  # the suppress flag is used as a backdoor for the pshell.py client to allow
  # a remote server to pass the command usage back to the local server that
  # is run by the client
  if ((_gSupressInvalidArgCountMessage == True) and (retCode == COMMAND_INVALID_ARG_COUNT)):
    retCode = COMMAND_SUCCESS
  elif ((len(control_["pshellMsg"]["payload"]) > 0) and (retCode > COMMAND_SUCCESS) and (retCode < SOCKET_SEND_FAILURE)):
    _printError("Remote pshell command: '%s', server: %s, %s" % (command_, control_["remoteServer"], _getResponseString(retCode)))
  elif ((retCode != COMMAND_SUCCESS) and (retCode != _gMsgTypes["commandComplete"])):
    _printError("Remote pshell command: '%s', server: %s, %s" % (command_, control_["remoteServer"], _getResponseString(retCode)))
  else:
    retCode = COMMAND_SUCCESS
  return (retCode)

#################################################################################
#################################################################################
def _getResponseString(retCode):
  global _gPshellControlResponse
  if (retCode < len(_gPshellControlResponse)):
    return (_gPshellControlResponse[retCode])
  else:
    return ("PSHELL_UNKNOWN_RESPONSE: %d" % retCode)

#################################################################################
#################################################################################
def _cleanupUnixResources():
  global _gUnixSocketPath
  global _gLockFileExtension
  if not os.path.isdir(_gUnixSocketPath):
    os.system("mkdir %s" % _gFileSystemPath)
    os.system("chmod 777 %s" % _gFileSystemPath)
  lockFiles = fnmatch.filter(os.listdir(_gUnixSocketPath), "*"+_gLockFileExtension)
  for file in lockFiles:
    try:
      fd = open(_gUnixSocketPath+file, "r")
      try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        # we got the lock, delete any socket file and lock file and don't print anything
        try:
          os.unlink(_gUnixSocketPath+file.split(".")[0])
        except:
          None
        try:
          os.unlink(_gUnixSocketPath+file)
        except:
          None
      except:
        None
    except:
      None

#################################################################################
#################################################################################
def _removeControl(control_):
  global _gPshellControl
  global _gLockFileExtension
  if (control_["serverType"] == "unix"):
    try:
      os.unlink(control_["sourceAddress"])
    except:
      None
    try:
      os.unlink(control_["sourceAddress"]+_gLockFileExtension)
    except:
      None
  _cleanupUnixResources()
  control_["socket"].close()

#################################################################################
#################################################################################
def _getControl(sid_):
  global _gPshellControl
  if ((sid_ >= 0) and (sid_ < len(_gPshellControl))):
    return (_gPshellControl[sid_])
  else:
    _printError("No control defined for sid: %d" % sid_)
    return (None)

#################################################################################
#################################################################################
def _loadConfigFile(controlName_, remoteServer_, port_, defaultTimeout_):
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
    return (remoteServer_, port_, defaultTimeout_)
  # found a config file, process it
  isUnix = False
  for line in file:
    # skip comments
    if (line[0] != "#"):
      option = line.split("=")
      if (len(option) == 2):
        control = option[0].split(".")
        if ((len(control) == 2) and (controlName_ == control[0])):
          if (control[1].lower() == "udp"):
            remoteServer_ = option[1].strip()
          elif (control[1].lower() == "unix"):
            remoteServer_ = option[1].strip()
            port_ = "unix"
            isUnix = True
          elif (control[1].lower() == "port"):
            port_ = option[1].strip()
          elif (control[1].lower() == "timeout"):
            if (option[1].lower().strip() == "none"):
              defaultTimeout_ = 0
            else:
              defaultTimeout_ = int(option[1].strip())
  file.close()
  # make this check in case they changed the server
  # from udp to unix and forgot to comment out the
  # port
  if (isUnix):
    port_ = "unix"
  return (remoteServer_, port_, defaultTimeout_)

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
# global "private" data
#
#################################################################################

# list of dictionaries that contains a control structure for each control client
_gPshellControl = []

# list of dictionaries that contains multicast group information
_gPshellMulticast = []

# path of unix domain socket handle for client sockets
_gUnixSocketPath = "/tmp/.pshell/"
_gLockFileExtension = ".lock"
_PSHELL_CONFIG_DIR = "/etc/pshell/config"
_PSHELL_CONFIG_FILE = "pshell-control.conf"

# these are the valid types we recognize in the msgType field of the pshellMsg structure,
# that structure is the message passed between the pshell client and server, these values
# must match their corresponding #define definitions in the C file PshellCommon.h
_gMsgTypes = {"queryName":3, "queryCommands":4, "commandComplete":8, "queryBanner":9, "queryTitle":10, "queryPrompt":11, "controlCommand":12}

# fields of PshellMsg, we use this definition to unpack the received PshellMsg response
# from the server into a corresponding OrderedDict in the PshellControl entry
_PshellMsg = namedtuple('PshellMsg', 'msgType respNeeded dataNeeded pad seqNum payload')

# format of PshellMsg header, 4 bytes and 1 (4 byte) integer, we use this for packing/unpacking
# the PshellMessage to/from an OrderedDict into a packed binary structure that can be transmitted
# over-the-wire via a socket
_gPshellMsgHeaderFormat = "4BI"

# default PshellMsg payload length, used to receive responses,
# set to the max UDP datagram size, 64k
_gPshellMsgPayloadLength = 1024*64

# mapping of above definitions to strings so we can display text in error messages
_gPshellControlResponse = {COMMAND_SUCCESS:"PSHELL_COMMAND_SUCCESS",
                           COMMAND_NOT_FOUND:"PSHELL_COMMAND_NOT_FOUND",
                           COMMAND_INVALID_ARG_COUNT:"PSHELL_COMMAND_INVALID_ARG_COUNT",
                           SOCKET_SEND_FAILURE:"PSHELL_SOCKET_SEND_FAILURE",
                           SOCKET_SELECT_FAILURE:"PSHELL_SOCKET_SELECT_FAILURE",
                           SOCKET_RECEIVE_FAILURE:"PSHELL_SOCKET_RECEIVE_FAILURE",
                           SOCKET_TIMEOUT:"PSHELL_SOCKET_TIMEOUT",
                           SOCKET_NOT_CONNECTED:"PSHELL_SOCKET_NOT_CONNECTED"}

# the suppress flag is used as a backdoor for the pshell.py client to allow
# a remote server to pass the command usage back to the local server that
# is run by the client
_gSupressInvalidArgCountMessage = False

# log level and log print function
_gLogLevel = LOG_LEVEL_DEFAULT
_gLogFunction = None
