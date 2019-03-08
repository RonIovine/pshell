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
# This demo programshows the usage of the PshellReadline module.  This module
# provides functionality similar to the readline library.  This will work with
# any character based input stream, i.e.  terminal, serial, TCP etc.
#
#################################################################################

import sys
import select
import socket
import PshellReadline

#####################################################
#####################################################
def showUsage():
  print("")
  print("Usage: pshellReadlineDemo.py {-tty | -socket} [-bash | -fast] [<idleTimeout>]")
  print("")
  print("  where:")
  print("    -tty          - serial terminal using stdin and stdout (default)")
  print("    -socket       - TCP socket terminal using telnet client")
  print("    -bash         - Use bash/readline style tabbing")
  print("    -fast         - Use \"fast\" style tabbing (default)")
  print("    <idleTimeout> - the idle session timeout in minutes (default=none)")
  print("")
  sys.exit(0)

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  if (len(sys.argv) > 4):
    showUsage()

  serialType = PshellReadline.TTY
  idleTimeout = PshellReadline.IDLE_TIMEOUT_NONE

  for arg in sys.argv[1:]:
    if (arg == "-bash"):
      PshellReadline.setTabStyle(PshellReadline.BASH_TAB)
    elif (arg == "-fast"):
      PshellReadline.setTabStyle(PshellReadline.FAST_TAB)
    elif (arg == "-tty"):
      serialType = PshellReadline.TTY
    elif (arg == "-socket"):
      serialType = PshellReadline.SOCKET
    elif (arg == "-h"):
      showUsage()
    elif (arg.isdigit()):
      idleTimeout = PshellReadline.ONE_MINUTE*int(arg)
    else:
      showUsage()

  # add some keywords for TAB completion, the completions
  # only apply to the first keyword (i.e. up to the first
  # whitespace) of a a given command
  PshellReadline.addTabCompletion("quit")
  PshellReadline.addTabCompletion("help")
  PshellReadline.addTabCompletion("hello")
  PshellReadline.addTabCompletion("world")
  PshellReadline.addTabCompletion("enhancedUsage")
  PshellReadline.addTabCompletion("keepAlive")
  PshellReadline.addTabCompletion("pshellAggregatorDemo")
  PshellReadline.addTabCompletion("pshellControlDemo")
  PshellReadline.addTabCompletion("pshellReadlineDemo")
  PshellReadline.addTabCompletion("pshellServerDemo")
  PshellReadline.addTabCompletion("myComm")
  PshellReadline.addTabCompletion("myCommand123")
  PshellReadline.addTabCompletion("myCommand456")
  PshellReadline.addTabCompletion("myCommand789")

  # socket serial type requested, setup our TCP server socket
  # and pass the connected file descriptors to our PshellReadline
  # module
  if (serialType == PshellReadline.SOCKET):
  
    # Create a TCP/IP socket
    sockFd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockFd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  
    # Bind the socket to the port
    port = 9005
    sockFd.bind(("", port))
  
    # Listen for incoming connections
    sockFd.listen(1)
  
    # Wait for a connection
    print("waiting for a connection on port %d, use 'telnet localhost %d' to connect" % (port, port))
    connectFd, clientAddr = sockFd.accept()
    print("connection accepted")

    # set our connected file descriptors and serial type
    PshellReadline.setFileDescriptors(connectFd, connectFd, PshellReadline.SOCKET)
  
    # shutdown our original listening socket
    sockFd.shutdown(socket.SHUT_RDWR);

  PshellReadline.setIdleTimeout(idleTimeout)

  command = ""
  idleSession = False
  while (not idleSession and not PshellReadline.isSubString(command, "quit")):
    (command, idleSession) = PshellReadline.getInput("prompt> ")
    if (not idleSession and not PshellReadline.isSubString(command, "quit")):
      PshellReadline.writeOutput("command: '%s'\n" % command)
