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
# This is an example demo program that shows how to use the pshell control
# interface.  This API will allow any external client program to invoke
# functions that are registered within a remote program running a pshell
# server.  This will only control programs that are running either a UDP
# or UNIX pshell server.
#
#################################################################################

# import all our necessary modules
import sys
import signal
import PshellReadline
import PshellControl

#####################################################
#####################################################
def showUsage():
  print("")
  print("Usage: pshellControlDemo.py {<hostname> | <ipAddress> | <unixServerName>} {<port> | unix}")
  print("                            [-t<timeout>] [-extract]")
  print("")
  print("  where:")
  print("    <hostname>       - hostname of UDP server")
  print("    <ipAddress>      - IP address of UDP server")
  print("    <unixServerName> - name of UNIX server")
  print("    unix             - specifies a UNIX server")
  print("    <port>           - port number of UDP server")
  print("    <timeout>        - wait timeout for response in mSec (default=100)")
  print("    extract          - extract data contents of response (must have non-0 wait timeout)")
  print("")
  exit(0)

#################################################################################
#################################################################################
def signalHandler(signal, frame):
  PshellControl.disconnectAllServers()
  print("")
  exit(0)

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
  signal.signal(signal.SIGSYS, signalHandler)      # 31 Bad system call

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  if ((len(sys.argv) < 3) or ((len(sys.argv)) > 5)):
    showUsage()

  extract = False
  timeout = 1000

  for arg in sys.argv[3:]:
    if ("-t" in arg):
      timeout = int(arg[2:])
    elif (arg == "-extract"):
      extract = True
    else:
      showUsage()

  # register signal handlers so we can do a graceful termination and cleanup any system resources
  registerSignalHandlers()

  sid = PshellControl.connectServer("pshellControlDemo", sys.argv[1], sys.argv[2], PshellControl.ONE_MSEC*timeout)

  if (sid != PshellControl.INVALID_SID):
    command = ""
    print("Enter command or 'q' to quit");
    while (not PshellReadline.isSubString(command, "quit")):
      (command, idleSession) = PshellReadline.getInput("pshellControlCmd> ")
      if (not PshellReadline.isSubString(command, "quit")):
        if ((command.split()[0] == "?") or (command.split()[0] == "help")):
          PshellReadline.write(PshellControl.extractCommands(sid))
        elif (extract):
          (results, retCode) = PshellControl.sendCommand3(sid, command)
          PshellReadline.write("%d bytes extracted, results:\n" % len(results))
          PshellReadline.write("%s" % results)
          PshellReadline.write("retCode: %s\n" % PshellControl.getResponseString(retCode))
        else:
          retCode = PshellControl.sendCommand1(sid, command)
          PshellReadline.write("retCode: %s\n" % PshellControl.getResponseString(retCode))

    PshellControl.disconnectServer(sid)
