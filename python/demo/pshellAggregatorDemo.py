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
import PshellServer
import PshellControl

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

# this function is the common generic function to control any server
# based on only the SID and entered command, it should be called from 
# every control specific callback for this aggregator

#################################################################################
#################################################################################
def controlServer(sid, argv):
  # reconstitute the original command
  command = ' '.join(argv)
  if (PshellServer.isHelp() or (argv[0] == "help")):
    PshellServer.printf(PshellControl.extractCommands(sid))
  else:
    (results, retCode) = PshellControl.sendCommand3(sid, command)
    if (retCode == PshellControl.COMMAND_SUCCESS):
      PshellServer.printf(results)
    endif
  endif
enddef

# the following two functions are the control specific functions that interface
# directly to a given remote server via the control API for a give SID, this is
# how multiple remote pshell servers can be aggregated into a single local pshell
# server, each separate SID for each remote server needs it's own dedicated
# callback that is registered to the local pshell server, the only thing these
# local pshell functions need to do is call the common 'controlServer' function
# with the passed in argc, argv, and their unique SID identifier

#################################################################################
#################################################################################
def pshellServerDemo(argv):
  global pshellServerDemoSid
  controlServer(pshellServerDemoSid,  argv)
enddef

#################################################################################
#################################################################################
def traceFilterDemo(argv):
  global traceFilterDemoSid
  controlServer(traceFilterDemoSid, argv)
enddef

# example meta command that will call multiple discrete pshell commands from
# multiple pshell servers

#################################################################################
#################################################################################
def meta(argv):
  global pshellServerDemoSid
  global traceFilterDemoSid
  (results, retCode) = PshellControl.sendCommand3(pshellServerDemoSid, "hello %s %s" % (argv[0], argv[1]))
  if (retCode == PshellControl.COMMAND_SUCCESS):
    PshellServer.printf(results)
  endif
  PshellControl.sendCommand1(traceFilterDemoSid, "set callback %s" % argv[2])
enddef

# example multicast command, this will send a given command to all the registered
# multicast receivers for that multicast group, multicast groups are based on
# the command's keyword
 
#################################################################################
#################################################################################
def multicast(argv):
  PshellControl.sendMulticast("test")
  PshellControl.sendMulticast("trace 1 2 3 4")
  PshellControl.sendMulticast("trace on")
  PshellControl.sendMulticast("hello")
enddef

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  # default ports
  pshellServerDemoPort = "6001"
  traceFilterDemoPort = "6002"

  # verify usage
  if ((len(sys.argv) != 2) and (len(sys.argv) != 4)):
      print("Usage: pshellAggregatorDemo {<hostname> | <ipAddress>} [<pshellServerDemoPort> <traceFilterDemoPort>]")
      exit (0)
  elif ((len(sys.argv) == 2) and (sys.argv[1] == "-h")):
      print("Usage: pshellAggregatorDemo {<hostname> | <ipAddress>} [<pshellServerDemoPort> <traceFilterDemoPort>]")
      exit (0)
  elif (len(sys.argv) == 4):
      pshellServerDemoPort = sys.argv[2]
      traceFilterDemoPort = sys.argv[3]
  endif
  
  # add PshellControl entries to our aggregator list, the hostname/ipAddress,
  # port and timeout values can be overridden via the pshell-control.conf file
  pshellServerDemoSid = PshellControl.connectServer("pshellServerDemo", sys.argv[1], pshellServerDemoPort, PshellControl.ONE_SEC*5)
  traceFilterDemoSid = PshellControl.connectServer("traceFilterDemo", sys.argv[1], traceFilterDemoPort, PshellControl.ONE_SEC*5)

  # add some multicast groups for our control sids, a multicast group is based
  # on the command's keyword
  PshellControl.addMulticast(pshellServerDemoSid, "trace");
  PshellControl.addMulticast(traceFilterDemoSid, "trace");  
  
  PshellControl.addMulticast(pshellServerDemoSid, "test");
  PshellControl.addMulticast(traceFilterDemoSid, "test");

  # register our callback commands
  PshellServer.addCommand(pshellServerDemo, "pshellServerDemo", "control the remote pshellServerDemo process", "<command> | ? | -h", 1, 30, False)
  PshellServer.addCommand(traceFilterDemo, "traceFilterDemo", "control the remote traceFilterDemo process", "<command> | ? | -h", 1, 30, False)

  # add any "meta" commands here, meta commands can aggregate multiple discrete
  # pshell commands, either within one server or across multiple servers, into
  # one command
  PshellServer.addCommand(meta, "meta", "meta command, wraps multiple seperate functions", "<arg1> <arg2> <arg3>", 3, 3)

  # add an example command that uses the one-to-many multicast feature of
  # the control API
  PshellServer.addCommand(multicast, "multicast", "example multicast command to several servers")

  # start our local pshell server
  PshellServer.startServer("pshellAggregatorDemo", PshellServer.LOCAL, PshellServer.BLOCKING)
  
  # disconnect all our remote control servers
  PshellControl.disconnectAllServers()
  
  # cleanup our local server's resources
  PshellServer.cleanupResources()

endif 
