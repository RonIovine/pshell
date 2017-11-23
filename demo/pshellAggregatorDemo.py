#!/usr/bin/python

import sys
import PshellControl

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

##############################
#
# start of main program
#
##############################

pshellServerDemoPort = "6001"
traceFilterDemoPort = "6002"

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
  
# connect to our remote servers, the hostname/ipAddress, port,
# and timeout values can be overridden via the pshell-control.conf 
# file

pshellServerDemoSid = PshellControl.connectServer("pshellServerDemo", sys.argv[1], pshellServerDemoPort, PshellControl.ONE_SEC*5)
traceFilterDemoSid = PshellControl.connectServer("traceFilterDemo", sys.argv[1], traceFilterDemoPort, PshellControl.ONE_SEC*5)

command = NULL
while (command.lower() != "q"):
  command = raw_input("pshellAggregatorDemo[local]:PSHELL> ")
  if ((len(command) > 0) and (command.lower() != "q")):
    splitCommand = command.lower().split()
    command = ' '.join(command.split()[1:])
    if (splitCommand[0] in "pshellServerDemo"):
      if ((len(splitCommand) == 2) and ((splitCommand[1] == "?") or (splitCommand[1] == "help"))):
        print PshellControl.extractCommands(pshellServerDemoSid)
      else:
        print PshellControl.sendCommand3(pshellServerDemoSid, command)
      endif
    elif (splitCommand[0] in "traceFilterDemo"):
      if ((len(splitCommand) == 2) and ((splitCommand[1] == "?") or (splitCommand[1] == "help"))):
        print PshellControl.extractCommands(traceFilterDemoSid)
      else:
        print PshellControl.sendCommand3(traceFilterDemoSid, command)
      endif
    endif
  endif
endwhile

PshellControl.disconnectAllServers()
