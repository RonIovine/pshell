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

# import all our necessary modules
import sys
import os
import time
import select
import socket
import struct
import random
from collections import OrderedDict
from collections import namedtuple

#################################################################################
#
# 
#################################################################################

# dummy variables so we can create pseudo end block indicators, add these 
# identifiers to your list of python keywords in your editor to get syntax 
# highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

#################################################################################
#
# global "public" data, these are used for various parts of the public API
#
#################################################################################

UDP_SERVER = 0
UNIX_SERVER = 1
LOCAL_SERVER = 2

BLOCKING_MODE = 0
NON_BLOCKING_MODE = 1

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
#
#################################################################################
def addCommand(function_, command_, description_, usage_ = "", minArgs_ = 0, maxArgs_ = 0, showUsage_ = True):
  __addCommand(function_, command_, description_, usage_, minArgs_,  maxArgs_,  showUsage_)
enddef

#################################################################################
#
#################################################################################
def startServer(serverName_, serverType_, serverMode_, hostnameOrIpAddr_ = None, port_ = 0):
  __startServer(serverName_, serverType_, serverMode_, hostnameOrIpAddr_, port_)
enddef

#################################################################################
#
#################################################################################
def cleanupResources():
  __cleanupResources()
enddef

#################################################################################
#
# This function should only be called from a registered callback function
#
#################################################################################
def printf(message_):
  __printf(message_)
enddef

#################################################################################
#
# This function should only be called from a registered callback function
#
#################################################################################
def showUsage():
  __showUsage()
enddef

#################################################################################
#
# This function should only be called from a registered callback function
#
#################################################################################
def isHelp():
  return (__isHelp())
enddef

#################################################################################
#
# "private" functions and data
#
# Users of this module should never access any of these "private" items directly,
# these are meant to hide the implementation from the presentation of the public
# API
#
#################################################################################

#################################################################################
#################################################################################
def __addCommand(function_, command_, description_, usage_, minArgs_,  maxArgs_, showUsage_, prepend_ = False):
  global gCommandList
  global gMaxLength
  for command in gCommandList:
    if (command["name"] == command_):
      # command name already exists, don't add it again
      print "PSHELL_ERROR: Command: %s already exists, not adding command" % command_
      return
    endif
  endfor
  if (len(command_) > gMaxLength):
    gMaxLength = len(command_)
  endif
  if (prepend_ == True):
    gCommandList.insert(0, {"function":function_, "name":command_, "description":description_, "usage":usage_, "minArgs":minArgs_, "maxArgs":maxArgs_, "showUsage":showUsage_})
  else:
    gCommandList.append({"function":function_, "name":command_, "description":description_, "usage":usage_, "minArgs":minArgs_, "maxArgs":maxArgs_, "showUsage":showUsage_})
  endif
enddef

#################################################################################
#################################################################################
def __startServer(serverName_, serverType_, serverMode_, hostnameOrIpAddr_, port_):
  global gServerName
  global gServerType
  global gServerMode
  global gHostnameOrIpAddr
  global gPort
  global gPrompt
  global gTitle
  gServerName = serverName_
  gServerType = serverType_
  gServerMode = serverMode_
  gHostnameOrIpAddr = hostnameOrIpAddr_
  gPort = port_
  if (gServerType == LOCAL_SERVER):
    gPrompt = gServerName + "[local]:PSHELL> "
    gTitle = "PSHELL: " + gServerName + "[local], Mode: INTERACTIVE"
    __addCommand(__help, "help", "show all available commands", "", 0, 0, True, True)
    __addCommand(__exit, "quit", "exit interactive mode", "", 0, 0, True, True)
  else:
    gPrompt = "PSHELL> "
    gTitle = "PSHELL"
  endif
  if (gServerMode == BLOCKING_MODE):
    __runServer()
  else:
    # spawn thread
    None
  endif
enddef
  
#################################################################################
#################################################################################
def __runServer():
  global gServerType
  if (gServerType == UDP_SERVER):
    __runUDPServer()
  elif (gServerType == UNIX_SERVER):
    __runUNIXServer()
  else:  # local server 
    __runLocalServer()
  endif
enddef

#################################################################################
#################################################################################
def __runUDPServer():
  global gServerName
  global gHostnameOrIpAddr
  global gPort
  print "PSHELL_INFO: UDP Server: %s Started On Host: %s, Port: %d" % (gServerName, gHostnameOrIpAddr, gPort)
  # startup our UDP server
  if (__createSocket()):
    while (True):
      __receive()
    endwhile
  endif
enddef

#################################################################################
#################################################################################
def __runUNIXServer():
  global gServerName
  print "PSHELL_INFO: UNIX Server: %s Started" % gServerName
  # startup our UNIX server
  if (__createSocket()):
    while (True):
      __receive()
    endwhile
  endif
enddef

#################################################################################
#################################################################################
def __runLocalServer():
  global gPrompt
  __showWelcome()
  command = NULL
  while (command.lower() != "q"):
    command = raw_input(gPrompt)
    if ((len(command) > 0) and (command.lower() != "q")):
      __processCommand(command)
    endif
  endwhile
enddef

#################################################################################
#################################################################################
def __createSocket():
  global gServerName
  global gServerType
  global gHostnameOrIpAddr
  global gPort
  global gSocketFd
  global gUnixSocketPath
  global gUnixSourceAddress
  if (gServerType == UDP_SERVER):
    # IP domain socket
    gSocketFd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if (gHostnameOrIpAddr == "anyhost"):
      gSocketFd.bind((NULL, gPort))
    elif (gHostnameOrIpAddr == "localhost"):
      gSocketFd.bind(("127.0.0.1", gPort))
    else:
      gSocketFd.bind((gHostnameOrIpAddr, gPort))
    endif
  elif (gServerType == UNIX_SERVER):
    # UNIX domain socket
    gSocketFd = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    gUnixSourceAddress = gUnixSocketPath+gServerName
    # cleanup any old handle that might be hanging around
    if (os.path.isfile(gUnixSourceAddress)):
      os.unlink(gUnixSourceAddress)
    endif
    gSocketFd.bind(gUnixSourceAddress)
  endif
  return (True)
enddef

#################################################################################
#################################################################################
def __receive():
  global gPshellMsg
  global gSocketFd
  global gPshellMsgPayloadLength
  global gPshellMsgHeaderFormat
  global gFromAddr
  gPshellMsg, gFromAddr = gSocketFd.recvfrom(gPshellMsgPayloadLength)
  gPshellMsg = PshellMsg._asdict(PshellMsg._make(struct.unpack(gPshellMsgHeaderFormat+str(len(gPshellMsg)-struct.calcsize(gPshellMsgHeaderFormat))+"s", gPshellMsg)))
  __processCommand(gPshellMsg["payload"])
enddef

#################################################################################
#################################################################################
def __processCommand(command_):
  global gCommandList
  global gMaxLength
  global gCommandHelp
  global gListHelp
  global gMsgTypes
  global gPshellMsg
  global gServerType
  global gArgs
  global gFoundCommand
  if (gPshellMsg["msgType"] == gMsgTypes["queryVersion"]):
    __processQueryVersion()
  elif (gPshellMsg["msgType"] == gMsgTypes["queryPayloadSize"]):
    __processQueryPayloadSize()
  elif (gPshellMsg["msgType"] == gMsgTypes["queryName"]):
    __processQueryName()
  elif (gPshellMsg["msgType"] == gMsgTypes["queryTitle"]):
    __processQueryTitle()
  elif (gPshellMsg["msgType"] == gMsgTypes["queryBanner"]):
    __processQueryBanner()
  elif (gPshellMsg["msgType"] == gMsgTypes["queryPrompt"]):
    __processQueryPrompt()
  elif (gPshellMsg["msgType"] == gMsgTypes["queryCommands1"]):
    __processQueryCommands1()
  elif (gPshellMsg["msgType"] == gMsgTypes["queryCommands2"]):
    __processQueryCommands2()
  else:
    gPshellMsg["payload"] = NULL
    gArgs = command_.lower().split()[1:]
    command_ = command_.lower().split()[0]
    numMatches = 0
    if (command_ == "?"):
      __help(gArgs)
      return
    else:
      for command in gCommandList:
        if (command_ in command["name"]):
          gFoundCommand = command
          numMatches += 1
        endif
      endfor
    endif
    if (numMatches == 0):
      printf("PSHELL_ERROR: Command: '%s' not found\n" % command_)
    elif (numMatches > 1):
      printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n" % command_)
    else:
      if (not __isValidArgCount()):
        showUsage()
      elif (isHelp() and (gFoundCommand["showUsage"] == True)):
        showUsage()
      else:
        gFoundCommand["function"](gArgs)
      endif
    endif
  endif
  gPshellMsg["msgType"] = gMsgTypes["commandComplete"]
  __reply()
enddef

#################################################################################
#################################################################################
def __isValidArgCount():
  global gArgs
  global gFoundCommand
  return ((len(gArgs) >= gFoundCommand["minArgs"]) and (len(gArgs) <= gFoundCommand["maxArgs"]))
enddef

#################################################################################
#################################################################################
def __processQueryVersion():
  printf("1")
enddef

#################################################################################
#################################################################################
def __processQueryPayloadSize():
  global gPshellMsgPayloadLength
  printf(str(gPshellMsgPayloadLength))
enddef

#################################################################################
#################################################################################
def __processQueryName():
  global gServerName
  printf(gServerName)
enddef

#################################################################################
#################################################################################
def __processQueryTitle():
  global gTitle
  printf(gTitle)
enddef

#################################################################################
#################################################################################
def __processQueryBanner():
  global gBanner
  printf(gBanner)
enddef

#################################################################################
#################################################################################
def __processQueryPrompt():
  global gPrompt
  printf(gPrompt)
enddef

#################################################################################
#################################################################################
def __processQueryCommands1():
  global gCommandList
  global gMaxLength
  global gPshellMsg
  gPshellMsg["payload"] = NULL
  for command in gCommandList:
    printf("%-*s  -  %s\n" % (gMaxLength, command["name"], command["description"]))
  endif
  printf("\n")
enddef

#################################################################################
#################################################################################
def __processQueryCommands2():
  global gCommandList
  for command in gCommandList:
    printf("%s%s" % (command["name"], "/"))
  endif
enddef

#################################################################################
#################################################################################
def __help(command_):
    printf("\n")
    printf("****************************************\n")
    printf("*             COMMAND LIST             *\n")
    printf("****************************************\n")
    printf("\n")
    __processQueryCommands1()
enddef

#################################################################################
#################################################################################
def __exit(command_):
  sys.exit()
enddef

#################################################################################
#################################################################################
def __showWelcome():
  global gServerName
  global gTitle
  global gBanner
  # put up our window title banner
  sys.stdout.write("\033]0;" + gTitle + "\007")
  sys.stdout.flush()
  # show our welcome screen
  print
  print "#########################################################"
  print "#"
  print "#  %s" % gBanner
  print "#"
  print "#  Single session LOCAL server: %s[local]" % gServerName
  print "#"
  print "#  Idle session timeout: NONE"
  print "#"
  print "#  Type '?' or 'help' at prompt for command summary"
  print "#  Type '?' or '-h' after command for command usage"
  print "#"
  print "#  Command abbreviation supported"
  print "#"
  print "#########################################################"
  print
enddef

#################################################################################
#################################################################################
def __printf(message_):
  global gServerType
  global gPshellMsg
  if (gServerType == LOCAL_SERVER):
    sys.stdout.write(message_)
  else:
    # remote server
    gPshellMsg["payload"] += message_
  endif
enddef

#################################################################################
#################################################################################
def __showUsage():
  global gFoundCommand
  printf("Usage: %s %s\n" % (gFoundCommand["name"], gFoundCommand["usage"]))
enddef

#################################################################################
#################################################################################
def __isHelp():
  global gArgs
  global gCommandHelp
  return ((len(gArgs) == 1) and (gArgs[0] in gCommandHelp))
enddef

#################################################################################
#################################################################################
def __reply():
  global gFromAddr
  global gSocketFd
  global gPshellMsg
  global gServerType
  global gPshellMsgHeaderFormat  
  if (gServerType != LOCAL_SERVER):
    gSocketFd.sendto(struct.pack(gPshellMsgHeaderFormat+str(len(gPshellMsg["payload"]))+"s", *gPshellMsg.values()), gFromAddr)
  endif
enddef

#################################################################################
#################################################################################
def __cleanupResources():
  global gUnixSourceAddress
  global gServerType
  global gSocketFd
  if (gUnixSourceAddress != None):
    os.unlink(gUnixSourceAddress)
  endif
  if (gServerType != LOCAL_SERVER):
    gSocketFd.close()
  endif
enddef

#################################################################################
#
# global "private" data
#
#################################################################################

# python does not have a native null string identifier, so create one
NULL = ""

gCommandHelp = ('?', '-h', '--h', '-help', '--help')
gListHelp = ('?', 'help')
gCommandList = []
gMaxLength = 0

gServerName = None
gServerType = None
gServerMode = None
gHostnameOrIpAddr = None
gPort = None
gPrompt = None
gTitle = None
gBanner = "PSHELL: Process Specific Embedded Command Line Shell"
gSocketFd = None 
gFromAddr = None
gUnixSocketPath = "/tmp/"
gArgs = None
gFoundCommand = None
gUnixSourceAddress = None

# these are the valid types we recognize in the msgType field of the pshellMsg structure,
# that structure is the message passed between the pshell client and server, these values
# must match their corresponding #define definitions in the C file PshellCommon.h
gMsgTypes = {"commandSuccess": 0, 
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
PshellMsg = namedtuple('PshellMsg', 'msgType respNeeded dataNeeded pad seqNum payload')

# format of PshellMsg header, 4 bytes and 1 (4 byte) integer, we use this for packing/unpacking
# the PshellMessage to/from an OrderedDict into a packed binary structure that can be transmitted 
# over-the-wire via a socket
gPshellMsgHeaderFormat = "4BI"

# default PshellMsg payload length, used to receive responses
gPshellMsgPayloadLength = 4096

gPshellMsg =  OrderedDict([("msgType",0),
                           ("respNeeded",True),
                           ("dataNeeded",True),
                           ("pad",0),
                           ("seqNum",0),
                           ("payload",NULL)])

