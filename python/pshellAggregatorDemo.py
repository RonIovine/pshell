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
import PshellControl

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

# PshellControl aggregator list
gControlList = []

#################################################################################
#
# addControl:
#
# add a PshellControl entry to our PshellControl aggregator list
#
#################################################################################
def addControl(controlName_, description_, remoteServer_, port_, defaultTimeout_):
  global gControlList
  for control in gControlList:
    if (control["name"] == controlName_):
      # control name already exists, don't add it again
      return
    endif
  endfor
  sid = PshellControl.connectServer(controlName_, remoteServer_, port_, defaultTimeout_)
  gControlList.append({"name":controlName_, "description":description_, "sid":sid})
enddef

#################################################################################
#
# processCommand:
#
# process a command as typed on the command line prompt
#
#################################################################################
def processCommand(command_):
  global gControlList
  splitCommand = command_.lower().split()
  command = ' '.join(command_.split()[1:])
  if ((splitCommand[0] == "?") or (splitCommand[0] == "help")):
    print
    print "****************************************"
    print "*             COMMAND LIST             *"
    print "****************************************"
    print
    print "quit              -  exit interactive mode"
    print "help              -  show all available commands"
    for control in gControlList:
      print "%s  -  %s" % (control["name"], control["description"])
    endif
    print
  else:
    for control in gControlList:
      if (splitCommand[0] in control["name"]):
        if (((len(splitCommand) == 1) or (len(splitCommand) == 2) and ((splitCommand[1] == "?") or (splitCommand[1] == "help")))):
          print PshellControl.extractCommands(control["sid"])
          return
        elif (len(splitCommand) >= 2):
          print PshellControl.sendCommand3(control["sid"], command)
          return
        endif
      endif
    endfor
    print "PSHELL_ERROR: Command: '%s' not found" % command_
  endif
enddef

##############################
#
# start of main program
#
##############################

# default ports
pshellServerDemoPort = "6001"
traceFilterDemoPort = "6002"

# verify usage
if ((len(sys.argv) != 2) and (len(sys.argv) != 4)):
    print "Usage: pshellAggregatorDemo {<hostname> | <ipAddress>} [<pshellServerDemoPort> <traceFilterDemoPort>]"
    exit (0)
elif ((len(sys.argv) == 2) and (sys.argv[1] == "-h")):
    print "Usage: pshellAggregatorDemo {<hostname> | <ipAddress>} [<pshellServerDemoPort> <traceFilterDemoPort>]"
    exit (0)
elif (len(sys.argv) == 4):
    pshellServerDemoPort = sys.argv[2]
    traceFilterDemoPort = sys.argv[3]
endif
  
# add PshellControl entries to our aggregator list, the hostname/ipAddress,
# port and timeout values can be overridden via the pshell-control.conf file
addControl("pshellServerDemo", "control the remote pshellServerDemo process", sys.argv[1], pshellServerDemoPort, PshellControl.ONE_SEC*5)
addControl("traceFilterDemo", "control the remote traceFilterDemo process", sys.argv[1], traceFilterDemoPort, PshellControl.ONE_SEC*5)

# put up our window title banner
sys.stdout.write("\033]0;PSHELL: pshellAggregatorDemo[local], Mode: INTERACTIVE\007")
sys.stdout.flush()

command = NULL
while (command.lower() != "q"):
  command = raw_input("pshellAggregatorDemo[local]:PSHELL> ")
  if ((len(command) > 0) and (command.lower() != "q")):
    processCommand(command)
  endif
endwhile

# cleanup all system resources
PshellControl.disconnectAllServers()
