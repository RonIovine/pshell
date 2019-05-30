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

This is the STUB version of the PshellServer module, to use the actual version,
set the PshellServer.py softlink to PshellServer-full.py, or use the provided
utility shell script 'setPshellLib' to set the desired softlink and 'showPshellLib'
to display the current softlink settings.
"""

# import all our necessary modules
import time

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

# constants to let the host program set the internal debug log level,
# if the user of this API does not want to see any internal message
# printed out, set the debug log level to LOG_LEVEL_NONE (0)
LOG_LEVEL_NONE = 0
LOG_LEVEL_ERROR = 1
LOG_LEVEL_WARNING = 2
LOG_LEVEL_INFO = 3
LOG_LEVEL_ALL = LOG_LEVEL_INFO
LOG_LEVEL_DEFAULT = LOG_LEVEL_ALL

#################################################################################
#
# "public" API functions
#
# This module implements the public API of the PshellServer-full.py module
# but with all the underlying functionality stubbed out.
#
#################################################################################

#################################################################################
#################################################################################
def setLogLevel(level):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def setLogFunction(function):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def addCommand(function, command, description, usage = None, minArgs = 0, maxArgs = 0, showUsage = True):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def startServer(serverName, serverType, serverMode, hostnameOrIpAddr = None, port = 0):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  print("PSHELL_INFO: STUB Server: %s Started" % serverName)
  if (serverMode == BLOCKING):
    while (True):
      time.sleep(100000)

#################################################################################
#################################################################################
def cleanupResources():
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def runCommand(command):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def printf(message = "", newline = True):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def flush():
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def wheel(string = None):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def march(string):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def showUsage():
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  None

#################################################################################
#################################################################################
def isHelp():
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  return (True)

#################################################################################
#################################################################################
def getOption(arg):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  return (False, "", "")

#################################################################################
#################################################################################
def isSubString(string1, string2, minMatchLength = 0):
  """
  Stub function, set PshellServer.py softlink to PshellServer-full.py for full functionality
  """
  return (True)

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):
  # just print out a message identifying this as the 'stub' module
  print("PSHELL_INFO: PshellServer STUB Module")

