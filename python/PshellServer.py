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

#################################################################################
#
# This API provides the Process Specific Embedded Command Line Shell (PSHELL)
# user API functionality.  It provides the ability for a client program to
# register functions that can be invoked via a command line user interface.
# There are several ways to invoke these embedded functions based on how the 
# pshell server is configured, which is described in documentation further down 
# in this file.
# 
# This module provides the same functionality as the PshellServer.h API and
# the libpshell-server linkable 'C' library, but implemented as a Python
# module.  Currently only the server types UDP, UNIX and LOCAL are supported.
# TCP server functionality will be added at a future date.  Applications using
# this module can be controlled via the pshell UDP/UNIX stand-alone 'C' client
# program or via the PshellControl.h/libpshell-control or PshellControl.py 
# control mechanism.
# 
# A complete example of the usage of the API can be found in the included 
# demo program pshellServerDemo.py.
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
import thread
from collections import OrderedDict
from collections import namedtuple

# dummy variables so we can create pseudo end block indicators, add these 
# identifiers to your list of python keywords in your editor to get syntax 
# highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

#################################################################################
#
# global "public" data, these are used for various parts of the public API
#
#################################################################################

# No TCP server available for now, to be added at a future date
UDP_SERVER = "udp"
UNIX_SERVER = "unix"
LOCAL_SERVER = "local"

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
# addCommand:
#
# Register callback commands to our PSHELL server.  If the command takes no
# arguments, the default parameters can be provided.  If the command takes
# an exact number of parameters, set minArgs and maxArgs to be the same.  If
# the user wants the callback function to handle all help initiated usage,
# set the showUsage parameter to False.
#
#################################################################################
def addCommand(function_, command_, description_, usage_ = None, minArgs_ = 0, maxArgs_ = 0, showUsage_ = True):
  __addCommand(function_, command_, description_, usage_, minArgs_,  maxArgs_,  showUsage_)
enddef

#################################################################################
#
# startServer:
#
# Start our PSHELL server, if serverType is UNIX or LOCAL, the default
# parameters can be used, and will be ignored if provided.  All of these
# parameters except serverMode can be overridden on a per serverName
# basis via the pshell-server.conf config file.  All commands in the
# <serverName>.startup file will be executed in this function at server
# startup time.
#
#################################################################################
def startServer(serverName_, serverType_, serverMode_, hostnameOrIpAddr_ = None, port_ = 0):
  __startServer(serverName_, serverType_, serverMode_, hostnameOrIpAddr_, port_)
enddef

#################################################################################
#
# cleanupResources
#
# Cleanup and release any system resources claimed by this module.  This includes
# any open socked handles, file descriptors, or system 'tmp' files.  This should
# be called at program exit time as well as any signal exception handler that
# results in a program termination.
#
#################################################################################
def cleanupResources():
  __cleanupResources()
enddef

#################################################################################
#
# runCommand:
#
# Run a registered command from within the registering program
#
#################################################################################
def runCommand(command_):
  __runCommand(command_)
enddef

#################################################################################
#
# The following public functions should only be called from within a 
# registered callback function
#
#################################################################################

#################################################################################
#
# printf:
#
# Display data back to the remote client
#
#################################################################################
def printf(message_ = "\n"):
  __printf(message_)
enddef

#################################################################################
#
# flush:
#
# Flush the reply (i.e. display) buffer back to the remote client
#
#################################################################################
def flush():
  __flush()
enddef

#################################################################################
#
# Commands to keep the remote server from timing out commands that take a 
# long time to run (i.e. longer than the pshell client timeout alue, which
# defaults to 5 seconds).
#
#################################################################################

#################################################################################
#
# wheel:
#
# spinning ascii wheel keep alive, user string string is optional
#
#################################################################################
def wheel(string_ = None):
  __wheel(string_)
enddef

#################################################################################
#
# march:
#
# march a string or character keep alive across the screen
#
#################################################################################
def march(string_):
  __march(string_)
enddef

#################################################################################
#
# showUsage:
#
# Show the command's registered usage
#
#################################################################################
def showUsage():
  __showUsage()
enddef

#################################################################################
#
# isHelp:
#
# Check if the user has asked for help on this command.  Command must be 
# registered with the showUsage = False option.
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
  global gBanner
  global gRunning
  
  if (gRunninig == False):
    gRunning = True
    gServerName = serverName_
    gServerMode = serverMode_
  
    (gTitle, gBanner, gPrompt, gServerType, gHostnameOrIpAddr, gPort) = __loadConfigFile(gServerName, gTitle, gBanner, gPrompt, serverType_, hostnameOrIpAddr_, port_)
    __loadStartupFile()  
    if (gServerMode == BLOCKING_MODE):
      __runServer()
    else:
      # spawn thread
      thread.start_new_thread(__serverThread, ())
    endif
  endif
enddef

#################################################################################
#################################################################################
def __serverThread():
  __runServer()
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
  global gTitle
  gPrompt = gServerName + "[local]:" + gPrompt
  gTitle = gTitle + gServerName + "[local], Mode: INTERACTIVE"
  __addCommand(__help, "help", "show all available commands", "", 0, 0, True, True)
  __addCommand(__exit, "quit", "exit interactive mode", "", 0, 0, True, True)
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
  global gCommandDispatched
  
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
    gCommandDispatched = True
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
  gCommandDispatched = False
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
  global gServerVersion
  printf(gServerVersion)
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
  printf()
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
    printf()
    printf("****************************************\n")
    printf("*             COMMAND LIST             *\n")
    printf("****************************************\n")
    printf()
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
  global gCommandInteractive
  if (gCommandInteractive == True):
    if (gServerType == LOCAL_SERVER):
      sys.stdout.write(message_)
    else:
      # remote server
      gPshellMsg["payload"] += message_
    endif
  endif
enddef

#################################################################################
#################################################################################
def __showUsage():
  global gFoundCommand
  if (gFoundCommand["usage"] != None):
    printf("Usage: %s %s\n" % (gFoundCommand["name"], gFoundCommand["usage"]))
  else:
    printf("Usage: %s\n" % gFoundCommand["name"])
  endif
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
#################################################################################
def __loadConfigFile(name_, title_, banner_, prompt_, type_, host_, port_):  
  configFile1 = NULL
  configPath = os.getenv('PSHELL_CONFIG_DIR')
  if (configPath != None):
    configFile1 = configPath+"/"+PSHELL_CONFIG_FILE
  endif
  configFile2 = PSHELL_CONFIG_DIR+"/"+PSHELL_CONFIG_FILE
  configFile3 = os.getcwd()+"/"+PSHELL_CONFIG_FILE
  if (os.path.isfile(configFile1)):
    file = open(configFile1, 'r')
  elif (os.path.isfile(configFile2)):
    file = open(configFile2, 'r')
  elif (os.path.isfile(configFile3)):
    file = open(configFile3, 'r')
  else:
    return (title_, banner_, prompt_, type_, host_, port_)
  endif
  # found a config file, process it
  for line in file:
    # skip comments
    if (line[0] != "#"):
      value = line.split("=");
      if (len(value) == 2):
        option = value[0].split(".")
        if ((len(option) == 2) and  (name_ == option[0])):
          if (option[1].lower() == "title"):
            title_ = value[1].strip()
          elif (option[1].lower() == "banner"):
            banner_ = value[1].strip()
          elif (option[1].lower() == "prompt"):
            prompt__ = value[1].strip()
          elif (option[1].lower() == "host"):
            host_ = value[1].strip()
          elif (option[1].lower() == "port"):
            port_ = int(value[1].strip())
          elif (option[1].lower() == "type"):
            type_ = value[1].strip()
          endif
        endif
      endif
    endif
  endfor
  return (title_, banner_, prompt_, type_, host_, port_)
enddef

#################################################################################
#################################################################################
def __loadStartupFile():
  global gServerName
  startupFile1 = NULL
  startupPath = os.getenv('PSHELL_STARTUP_DIR')
  if (startupPath != None):
    startupFile1 = startupPath+"/"+gServerName+".startup"
  endif
  startupFile2 = PSHELL_STARTUP_DIR+"/"+gServerName+".startup"
  startupFile3 = os.getcwd()+"/"+gServerName+".startup"
  if (os.path.isfile(startupFile1)):
    file = open(startupFile1, 'r')
  elif (os.path.isfile(startupFile2)):
    file = open(startupFile2, 'r')
  elif (os.path.isfile(startupFile3)):
    file = open(startupFile3, 'r')
  else:
    return
  endif
  # found a config file, process it
  for line in file:
    # skip comments
    line = line.strip()
    if ((len(line) > 0) and (line[0] != "#")):
      __runCommand(line)
    endif
  endfor
enddef

#################################################################################
#################################################################################
def __runCommand(command_):
  global gCommandList
  global gCommandInteractive
  global gCommandDispatched
  global gFoundCommand
  global gArgs
  if (gCommandDispatched == False):
    gCommandDispatched = True
    gCommandInteractive = False
    numMatches = 0
    gArgs = command_.lower().split()[1:]
    command_ = command_.lower().split()[0]
    for command in gCommandList:
      if (command_ in command["name"]):
        gFoundCommand = command
        numMatches += 1
      endif
    endfor
    if ((numMatches == 1) and __isValidArgCount() and not isHelp()):
      gFoundCommand["function"](gArgs)
    endif
    gCommandDispatched = False
    gCommandInteractive = True
  endif
enddef

#################################################################################
#################################################################################
def __wheel(string_):
  global gWheel
  global gWheelPos
  gWheelPos += 1
  if (string_ != NULL):
    __printf("\r%s%c" % (string_, gWheel[(gWheelPos)%4]))
  else:
    __printf("\r%c" % gWheel[(gWheelPos)%4])
  endif
  __flush()
enddef

#################################################################################
#################################################################################
def __march(string_):
  __printf(string_)
  __flush()
enddef

#################################################################################
#################################################################################
def __flush():
  global gCommandInteractive
  global gServerType
  global gPshellMsg
  if ((gCommandInteractive == True) and (gServerType != LOCAL_SERVER)):
    __reply()
    gPshellMsg["payload"] = NULL
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

gServerVersion = "1"
gServerName = None
gServerType = None
gServerMode = None
gHostnameOrIpAddr = None
gPort = None
gPrompt = "PSHELL> "
gTitle = "PSHELL: "
gBanner = "PSHELL: Process Specific Embedded Command Line Shell"
gSocketFd = None 
gFromAddr = None
gUnixSocketPath = "/tmp/"
gArgs = None
gFoundCommand = None
gUnixSourceAddress = None
gRunninig = False
gCommandDispatched = False
gCommandInteractive = True

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

PSHELL_CONFIG_DIR = "/etc/pshell/config"
PSHELL_STARTUP_DIR = "/etc/pshell/startup"
PSHELL_CONFIG_FILE = "pshell-server.conf"

gWheelPos = 0
gWheel = "|/-\\"
