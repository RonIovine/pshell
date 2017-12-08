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
import signal
import PshellServer

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

#################################################################################
#################################################################################
def hello(args_):
  PshellServer.printf("hello command dispatched:\n")
  for index, arg in enumerate(args_):
    PshellServer.printf("  argv[%d]: '%s'\n" % (index, arg))
  endfor
enddef

#################################################################################
#################################################################################
def world(args_):
  PshellServer.printf("world command dispatched:\n")
enddef

#################################################################################
#################################################################################
def enhancedUsage(args_):

  # see if the user asked for help
  if (PshellServer.isHelp()):
    # show standard usage 
    PshellServer.showUsage()
    # give some enhanced usage 
    PshellServer.printf("Enhanced usage here...\n")
  else:
    # do normal function processing 
    PshellServer.printf("enhancedUsage command dispatched:\n")
    for index, arg in enumerate(args_):
      PshellServer.printf("  argv[%d]: '%s'\n" % (index, arg))
    endfor
  endif
  
enddef

#################################################################################
#################################################################################
def showUsage():
  print "Usage: pshellServerDemo.py -udp | -unix | -local"
  sys.exit()
enddef

#################################################################################
#################################################################################
def signalHandler(signal, frame):
  PshellServer.cleanupResources()
  sys.exit()
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

##############################
#
# start of main program
#
##############################

if (len(sys.argv) != 2):
  showUsage()
elif (sys.argv[1] == "-udp"):
  serverType = PshellServer.UDP_SERVER
elif (sys.argv[1] == "-unix"):
  serverType = PshellServer.UNIX_SERVER
elif (sys.argv[1] == "-local"):
  serverType = PshellServer.LOCAL_SERVER
else:
  showUsage()
endif 

registerSignalHandlers()

PshellServer.addCommand(hello, "hello", "hello command description", "[<arg1> ... <arg20>]", 0, 20, True)
PshellServer.addCommand(world, "world", "world command description")
PshellServer.addCommand(enhancedUsage, "enhancedUsage", "command with enhanced usage", "<arg1>", 1, 1, False)

PshellServer.startServer("pshellServerDemo", serverType, PshellServer.BLOCKING_MODE, "anyhost", 9001)

PshellServer.cleanupResources()
