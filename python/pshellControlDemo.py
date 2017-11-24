#!/usr/bin/python

import sys
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

if ((len(sys.argv) < 3) or ((len(sys.argv)) > 6)):
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

while (command.lower() != "q"):
  command = raw_input("pshellControlCmd> ")
  if ((len(command) > 0) and (command.lower() != "q")):
    if (extract):
      results = PshellControl.sendCommand3(sid, command)
      print
      print "%d bytes extracted, results:" % len(results)
      print "%s" % results
    else:
      PshellControl.sendCommand1(sid, command)
    endif
  endif
endwhile

PshellControl.disconnectServer(sid)
