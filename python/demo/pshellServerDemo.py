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
# This is an example demo program that uses all the basic features of the PSHELL 
# server python module.  This program can be run as either a UDP, TCP, UNIX, or 
# local server based on the command line options.  If it is run as a UDP or UNIX 
# based server, you must use the provided stand-alone UDP client program 'pshell' 
# or 'pshell.py' to connect to it, if it is run as a TCP server, you must use
# 'telnet, if it is run as a local server, user command line input is solicited 
# directly from this program, no external client is needed.
#
#################################################################################

# import all our necessary modules
import sys
import signal
import time
import PshellServer

#################################################################################
#
# PSHELL user callback functions, the interface is similar to the "main" in C
# and Python, with the argv argument being the argument list and the len and
# contents of argv are the user entered arguments, excluding the actual command
# itself (arguments only).
#
# Use the special 'PshellServer.printf' function call to display data back to 
# the remote client.  The interface to this function is similar to the standard
# 'print' function in Python.
#
#################################################################################
 
#################################################################################
#################################################################################
def hello(argv):
  PshellServer.printf("hello command dispatched:\n")
  for index, arg in enumerate(argv):
    PshellServer.printf("  argv[%d]: '%s'\n" % (index, arg))

#################################################################################
#################################################################################
def world(argv):
  PshellServer.printf("world command dispatched:\n")

#################################################################################
#################################################################################
def enhancedUsage(argv):
  # see if the user asked for help
  if (PshellServer.isHelp()):
    # show standard usage 
    PshellServer.showUsage()
    # give some enhanced usage 
    PshellServer.printf("Enhanced usage here...\n")
  else:
    # do normal function processing 
    PshellServer.printf("enhancedUsage command dispatched:\n")
    for index, arg in enumerate(argv):
      PshellServer.printf("  argv[%d]: '%s'\n" % (index, arg))

#################################################################################
#################################################################################
def keepAlive(argv):
  if (argv[0] == "dots"):
    PshellServer.printf("marching dots keep alive:\n")
    for i in range(1,10):
      PshellServer.march(".")
      time.sleep(1)
  elif (argv[0] == "bang"):
    PshellServer.printf("marching 'bang' keep alive:\n");
    for  i in range(1,10):
      PshellServer.march("!")
      time.sleep(1)
  elif (argv[0] == "pound"):
    PshellServer.printf("marching pound keep alive:\n");
    for  i in range(1,10):
      PshellServer.march("#")
      time.sleep(1)
  elif (argv[0] == "wheel"):
    PshellServer.printf("spinning wheel keep alive:\n")
    for  i in range(1,10):
      # string is optional, use NULL to omit
      PshellServer.wheel("optional string: ")
      time.sleep(1)
  else:
    PshellServer.showUsage()
    return
  PshellServer.printf()

#################################################################################
#################################################################################
def showUsage():
  print("Usage: pshellServerDemo.py -udp | -tcp | -unix | -local")
  sys.exit()

#################################################################################
#################################################################################
def signalHandler(signal, frame):
  PshellServer.cleanupResources()
  print("")
  sys.exit()

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

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  if (len(sys.argv) != 2):
    showUsage()
  elif (sys.argv[1] == "-udp"):
    serverType = PshellServer.UDP
  elif (sys.argv[1] == "-unix"):
    serverType = PshellServer.UNIX
  elif (sys.argv[1] == "-tcp"):
    serverType = PshellServer.TCP
  elif (sys.argv[1] == "-local"):
    serverType = PshellServer.LOCAL
  else:
    showUsage()

  registerSignalHandlers()

  # register our callback commands
  PshellServer.addCommand(hello, "hello", "hello command description", "[<arg1> ... <arg20>]", 0, 20)
  PshellServer.addCommand(world, "world", "world command description")
  PshellServer.addCommand(enhancedUsage, "enhancedUsage", "command with enhanced usage", "<arg1>", 1, 1, False)
  if ((serverType == PshellServer.UDP) or (serverType == PshellServer.UNIX)):
    PshellServer.addCommand(keepAlive, "keepAlive", "command to show client keep-alive", "dots | bang | pound | wheel", 1, 1)

  # run a registered command from within it's parent process
  PshellServer.runCommand("hello 1 2 3")

  # start our pshell server
  PshellServer.startServer("pshellServerDemo", serverType, PshellServer.BLOCKING, PshellServer.ANYHOST, 9001)

  PshellServer.cleanupResources()
