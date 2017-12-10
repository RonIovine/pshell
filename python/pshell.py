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
# This is an example demo program that shows the use of multiple pshell control
# interfaces to aggregate all of the remote commands from all of the connected
# servers into a single local pshell server.  This can be useful in presenting
# a consolidated user shell who's functionality spans several discrete pshell
# servers.  Since this uses the PshellControl API, the external servers must
# all be either UDP or Unix servers.  The consolidation point in this example 
# is a local pshell server.
#
#################################################################################

# import all our necessary modules
import sys
import signal
import PshellControl
import PshellServer

#################################################################################
#
# This is a python version of the PshellClient.cc UDP/UNIX client program.
#
#################################################################################

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

gSid = None

#################################################################################
#################################################################################
def comandDispatcher(args_):
  global gSid
  sys.stdout.write(PshellControl.sendCommand3(gSid, ' '.join(args_)))
enddef

#################################################################################
#################################################################################
def configureLocalServer():
  global gSid
  global gRemoteServer
  global gPort

  PshellServer.gFirstArgPos = 0
  prompt = PshellControl.__extractPrompt(gSid)
  if (len(prompt) > 0):
    PshellServer.gPromptOverride = prompt
  endif
  title = PshellControl.__extractTitle(gSid)
  if (len(title) > 0):
    PshellServer.gTitleOverride = title
  endif
  serverName = PshellControl.__extractName(gSid)
  if (len(serverName) > 0):
    PshellServer.gServerNameOverride = serverName
  endif
  banner = PshellControl.__extractBanner(gSid)
  if (len(banner) > 0):
    PshellServer.gBannerOverride = banner
  endif
  if (gPort == PshellControl.UNIX):
    PshellServer.gServerTypeOverride = PshellControl.UNIX
  elif (gRemoteServer == "localhost"):
    PshellServer.gServerTypeOverride = "127.0.0.1"
  else:
    PshellServer.gServerTypeOverride = gRemoteServer
  endif
enddef

#################################################################################
#################################################################################
def cleanupAndExit():
  PshellServer.cleanupResources()
  PshellControl.disconnectAllServers()
  sys.exit()
enddef

#################################################################################
#################################################################################
def signalHandler(signal, frame):
  cleanupAndExit()
enddef

#################################################################################
#################################################################################
def registerSignalHandlers():
  # register a signal handlers so we can cleanup our
  # system resources upon abnormal termination
  signal.signal(signal.SIGHUP, signalHandler)      # 1  Hangup (POSIX)
  signal.signal(signal.SIGINT, signalHandler)      # 2  Interrupt (ANSI)
  signal.signal(signal.SIGQUIT, signalHandler)     # 3  Quit (POSIX)
  signal.signal(signal.SIGILL, signalHandler)      # 4  Illegal instruction (ANSI)
  signal.signal(signal.SIGABRT, signalHandler)     # 6  Abort (ANSI)
  signal.signal(signal.SIGBUS, signalHandler)      # 7  BUS error (4.2 BSD)
  signal.signal(signal.SIGFPE, signalHandler)      # 8  Floating-point exception (ANSI)
  signal.signal(signal.SIGSEGV, signalHandler)     # 11 Segmentation violation (ANSI)
  signal.signal(signal.SIGPIPE, signalHandler)     # 13 Broken pipe (POSIX)
  signal.signal(signal.SIGALRM, signalHandler)     # 14 Alarm clock (POSIX)
  signal.signal(signal.SIGTERM, signalHandler)     # 15 Termination (ANSI)
  signal.signal(signal.SIGXCPU, signalHandler)     # 24 CPU limit exceeded (4.2 BSD)
  signal.signal(signal.SIGXFSZ, signalHandler)     # 25 File size limit exceeded (4.2 BSD)
  signal.signal(signal.SIGPWR, signalHandler)      # 30 Power failure restart (System V)
  signal.signal(signal.SIGSYS, signalHandler)      # 31 Bad system call
enddef

#####################################################
#####################################################
def showUsage():
  print
  print "Usage: pshell.py {<hostname> | <ipAddress> | <unixServerName>} {<port> | unix} [-t<timeout>]"
  print
  print "  where:"
  print "    <hostname>       - hostname of UDP server"
  print "    <ipAddress>      - IP address of UDP server"
  print "    <unixServerName> - name of UNIX server"
  print "    unix             - specifies a UNIX server"
  print "    <port>           - port number of UDP server"
  print "    <timeout>        - wait timeout for response in sec (default=5)"
  print
  exit(0)
enddef

##############################
#
# start of main program
#
##############################

if ((len(sys.argv) < 3) or ((len(sys.argv)) > 4)):
  showUsage()
endif

timeout = 5

for arg in sys.argv[3:]:
  if ("-t" in arg):
    timeout = int(arg[2:])
  else:
    showUsage()
  endif
endfor

gRemoteServer = sys.argv[1]
gPort = sys.argv[2]

gSid = PshellControl.connectServer("pshellClient", gRemoteServer, gPort, PshellControl.ONE_SEC*timeout)

commandList = PshellControl.extractCommands(gSid)
commandList = commandList.split("\n")
for command in commandList:
  splitCommand = command.split("-")
  if (len(splitCommand ) >= 2):
    commandName = splitCommand[0].strip()
    description = splitCommand[1].strip()
    PshellServer.addCommand(comandDispatcher, commandName, description, "[<arg1> ... <arg20>]", 0, 20)
  endif
endif

# configure our local server to interact with a remote server
configureLocalServer()

# now start our local server which will interact with a remote server 
# via the pshell control machanism
PshellServer.startServer("pshellClient", PshellServer.LOCAL_SERVER, PshellServer.BLOCKING_MODE)

cleanupAndExit()
