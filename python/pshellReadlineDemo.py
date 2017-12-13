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

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

#####################################################
#####################################################
def showUsage():
  print
  print "Usage: pshellReadlineDemo.py {-tty | -socket} [<idleTimeout>]"
  print
  print "  where:"
  print "    -tty          - serial terminal using stdin and stdout"
  print "    -socket       - TCP socket using telnet client"
  print "    <idleTimeout> - the idle session timeout in minutes"
  print
  sys.exit(0)
enddef

##############################
#
# start of main program
#
##############################

idleTimeout = 0
if ((len(sys.argv) < 2) or (len(sys.argv) > 3)):
  showUsage()
elif (sys.argv[1] == "-tty"):
  serialType = PshellReadline.TTY
elif (sys.argv[1] == "-socket"):
  serialType = PshellReadline.SOCKET
else:
  showUsage()
endif

if (len(sys.argv) == 3):
  idleTimeout = PshellReadline.ONE_MINUTE*int(sys.argv[2])
endif

# add some keywords for TAB completion, the completions
# only apply to the first keyword of a a given command
PshellReadline.addTabCompletion("quit")
PshellReadline.addTabCompletion("help")
PshellReadline.addTabCompletion("hello")
PshellReadline.addTabCompletion("world")
PshellReadline.addTabCompletion("enhancedUsage")
PshellReadline.addTabCompletion("keepAlive")

if (serialType == PshellReadline.SOCKET):
  
  # Create a TCP/IP socket
  sockFd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  sockFd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  
  # Bind the socket to the port
  sockFd.bind((NULL, 9005))
  
  # Listen for incoming connections
  sockFd.listen(1)
  
  # Wait for a connection
  print "waiting for a connection on port 9005, use 'telnet localhost 9005' to connect"
  connectFd, clientAddr = sockFd.accept()
  print "connection accepted"

  # set our connected file descriptors
  PshellReadline.setFileDescriptors(connectFd, connectFd, PshellReadline.SOCKET)
  
  sockFd.shutdown(socket.SHUT_RDWR);

endif

PshellReadline.setIdleTimeout(idleTimeout)

command = "xxx"
while (not PshellReadline.IDLE_SESSION and command.lower() not in "quit"):
  command = PshellReadline.getInput("prompt> ")
  if (not PshellReadline.IDLE_SESSION and command not in "quit"):
    PshellReadline.write("\ncommand: '%s'\n" % command)
  endif
endwhile
print 
