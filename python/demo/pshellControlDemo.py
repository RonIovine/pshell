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
import PshellReadline
import PshellControl

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

#####################################################
#####################################################
def showUsage():
  print
  print "Usage: pshellControlDemo.py {<hostname> | <ipAddress> | <unixServerName>} {<port> | unix}"
  print "                            [-t<timeout>] [-extract]"
  print
  print "  where:"
  print "    <hostname>       - hostname of UDP server"
  print "    <ipAddress>      - IP address of UDP server"
  print "    <unixServerName> - name of UNIX server"
  print "    unix             - specifies a UNIX server"
  print "    <port>           - port number of UDP server"
  print "    <timeout>        - wait timeout for response in mSec (default=100)"
  print "    extract          - extract data contents of response (must have non-0 wait timeout)"
  print
  exit(0)
enddef

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  if ((len(sys.argv) < 3) or ((len(sys.argv)) > 5)):
    showUsage()
  endif

  extract = False
  timeout = 1000
  command = NULL

  for arg in sys.argv[3:]:
    if ("-t" in arg):
      timeout = int(arg[2:])
    elif (arg == "-extract"):
      extract = True
    else:
      showUsage()
    endif
  endfor

  #print "server: %s, port: %s, timeout: %s, extract: %d" % (sys.argv[1], sys.argv[2], timeout, extract)

  sid = PshellControl.connectServer("pshellControlDemo", sys.argv[1], sys.argv[2], PshellControl.ONE_MSEC*timeout)

  command = NULL
  while (not PshellReadline.isSubString(command, "quit")):
    (command, idleSession) = PshellReadline.getInput("pshellControlCmd> ")
    if (not PshellReadline.isSubString(command, "quit")):
      if ((command.split()[0] == "?") or (command.split()[0] == "help")):
        sys.stdout.write(PshellControl.extractCommands(sid))
      elif (extract):
        (results, retCode) = PshellControl.sendCommand3(sid, command)
        print "%d bytes extracted, results:" % len(results)
        sys.stdout.write("%s" % results)
      else:
        PshellControl.sendCommand1(sid, command)
      endif
    endif
  endwhile

  PshellControl.disconnectServer(sid)

endif 
