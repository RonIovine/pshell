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

# list of dictionaries that contains a control structure for each control client
_gPshellServers = []

_gMulticast = []

_gMaxLocalName = 0
_gMaxRemoteName = 0
_gMaxMulticastKeyword = 0
_gRemoteNameLabel = "Remote Name"
_gLocalNameLabel = "Local Name"
_gKeywordLabel = "Keyword"

#################################################################################
#################################################################################
def getMulticast(keyword):
  global _gMulticast
  for multicast in _gMulticast:
    if (keyword == multicast["keyword"]):
      return (multicast)
  return (None)

#################################################################################
#################################################################################
def getServer(localName):
  global _gPshellServers
  for server in _gPshellServers:
    if (PshellServer.isSubString(localName, server["localName"])):
      return (server)
  return (None)
  
#################################################################################
#################################################################################
def executeCommand(sid, argv):
  # reconstitute the original command
  command = ' '.join(argv)
  if ((len(argv) == 0) or 
      (argv[0] == "help") or 
      (argv[0] == "-h") or 
      (argv[0] == "-help") or 
      (argv[0] == "--h") or 
      (argv[0] == "--help") or 
      (argv[0] == "?")):
    PshellServer.printf(PshellControl.extractCommands(sid))
  else:
    (results, retCode) = PshellControl.sendCommand3(sid, command)
    if (retCode == PshellControl.COMMAND_SUCCESS):
      PshellServer.printf(results)

#################################################################################
#################################################################################
def controlServer(argv):
  server = getServer(argv[0])
  if (server != None):
    executeCommand(server["sid"], argv[1:])
    
#################################################################################
#################################################################################
def isDuplicate(localName_, remoteServer_, port_):
  global _gPshellServers
  for server in _gPshellServers:
    if ((localName_ == server["localName"]) or ((remoteServer_ == server["remoteServer"]) and (port_ == server["port"]))):
      return (True)
    else:
      return (False)

#################################################################################
#################################################################################
def add(argv):
  global _gPshellServers
  global _gMaxLocalName
  global _gMaxRemoteName
  global _gMulticast
  global _gMaxMulticastKeyword
  global _gRemoteNameLabel
  global _gLocalNameLabel
  global _gKeywordLabel
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where:\n")
    PshellServer.printf()
    PshellServer.printf("    <localName>    - Local logical name of the server, must be unique\n")
    PshellServer.printf("    <remoteServer> - Hostname or IP address of UDP server or name of UNIX server\n")
    PshellServer.printf("    <port>         - UDP port number or 'unix' for UNIX server (can be omitted for UNIX)\n")
    PshellServer.printf("    <keyword>      - Multicast group keyword, must be valid registered command\n")
    PshellServer.printf()
  elif (PshellServer.isSubString(argv[1], "server")):
    # default port
    port = PshellServer.UNIX
    if (len(argv) == 5):
      port = argv[4]
    if (not isDuplicate(argv[2], argv[3], port)):
      if (len(argv[2]) > _gMaxLocalName):
        _gMaxLocalName = max(len(argv[2]), len(_gLocalNameLabel))
      if (len(argv[3]) > _gMaxRemoteName):
        _gMaxRemoteName = max(len(argv[3]), len(_gRemoteNameLabel))
      _gPshellServers.append({"localName":argv[2],
                              "remoteServer":argv[3],
                              "port":port,
                              "sid":PshellControl.connectServer(argv[2], 
                                                                argv[3], 
                                                                port, 
                                                                PshellControl.ONE_SEC*5)})
      PshellServer.addCommand(controlServer, 
                              argv[2], 
                              "control the remote " + argv[2] + " process", 
                              "[<command> | ? | -h]", 
                              0, 
                              30, 
                              False)
    else:
      PshellServer.printf("ERROR: Local name: %s, remote server: %s, port: %s already exists\n" % (argv[2], argv[3], argv[4]))
  elif (PshellServer.isSubString(argv[1], "multicast")):
    multicast = getMulticast(argv[2])
    if (multicast == None):
      # new keyword
      if (len(argv[2]) > _gMaxMulticastKeyword):
        _gMaxMulticastKeyword = max(len(argv[2]), len(_gKeywordLabel))
      _gMulticast.append({"keyword":argv[2], "servers":[]})
      multicast = _gMulticast[-1]
    # add servers to this keyword
    for localName in argv[3:]:
      server = getServer(localName)
      if (server != None):
        PshellControl.addMulticast(server["sid"], argv[2])
        multicast["servers"].append(server)
  else:
    PshellServer.showUsage()

#################################################################################
#################################################################################
def show(argv):
  global _gPshellServers
  global _gMaxLocalName
  global _gMaxRemoteName
  global _gMulticast
  global _gMaxMulticastKeyword
  global _gRemoteNameLabel
  global _gLocalNameLabel
  global _gKeywordLabel
  if (PshellServer.isSubString(argv[1], "server")):
    PshellServer.printf()
    PshellServer.printf("***************************************\n")
    PshellServer.printf("*      AGGREGATED REMOTE SERVERS      *\n")
    PshellServer.printf("***************************************\n")
    PshellServer.printf()
    PshellServer.printf("%s    %s    Port\n" % (_gLocalNameLabel.ljust(_gMaxLocalName),
                                                _gRemoteNameLabel.ljust(_gMaxRemoteName)))
    PshellServer.printf("%s    %s    ======\n" % ("=".ljust(_gMaxLocalName, "="),
                                                  "=".ljust(_gMaxRemoteName, "=")))
    for server in _gPshellServers:
      PshellServer.printf("%s    %s    %s\n" % (server["localName"].ljust(_gMaxLocalName), server["remoteServer"].ljust(_gMaxRemoteName), server["port"]))
    PshellServer.printf()
  elif (PshellServer.isSubString(argv[1], "multicast")):
    PshellServer.printf()
    PshellServer.printf("************************************************\n")
    PshellServer.printf("*               MULTICAST GROUPS               *\n")
    PshellServer.printf("************************************************\n")
    PshellServer.printf()
    PshellServer.printf("%s    %s    %s    Port\n" % (_gKeywordLabel.ljust(_gMaxMulticastKeyword),
                                                      _gLocalNameLabel.ljust(_gMaxLocalName),
                                                      _gRemoteNameLabel.ljust(_gMaxRemoteName)))
    PshellServer.printf("%s    %s    %s    ======\n" % ("=".ljust(_gMaxMulticastKeyword, "="),
                                                        "=".ljust(_gMaxLocalName, "="),
                                                        "=".ljust(_gMaxRemoteName, "=")))
    for multicast in _gMulticast:
      PshellServer.printf("%s    " % multicast["keyword"].ljust(_gMaxMulticastKeyword))
      for index, server in enumerate(multicast["servers"]):
        if (index > 0):
          PshellServer.printf("%s    %s    %s    %s\n" % (" ".ljust(_gMaxMulticastKeyword, " "),
                                                          server["localName"].ljust(_gMaxLocalName), 
                                                          server["remoteServer"].ljust(_gMaxRemoteName), 
                                                          server["port"]))
        else:
          PshellServer.printf("%s    %s    %s\n" % (server["localName"].ljust(_gMaxLocalName), 
                                                    server["remoteServer"].ljust(_gMaxRemoteName), 
                                                    server["port"]))
    PshellServer.printf()
  else:
    PshellServer.showUsage()

#################################################################################
#################################################################################
def multicast(argv):
  # reconstitute the original command
  command = ' '.join(argv[1:])
  PshellControl.sendMulticast(command)
  
##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  # verify usage
  if (len(sys.argv) != 1):
    print("Usage: pshellAggregator\n")
    exit (0)

  # need to set the first arg position to 0 so we can pass
  # through the exact command to our remote server for dispatching
  PshellServer._setFirstArgPos(0)

  # supress the automatic invalid arg count message from the PshellControl.py
  # module so we can display the returned usage
  PshellControl._gSupressInvalidArgCountMessage = True

  # register our callback commands
  PshellServer.addCommand(add, 
                          "add", 
                          "add a new remote server or multicast entry", 
                          "{server <localName> <remoteServer> [<port>]} | {multicast <keyword> <localName1> [<localName2>...<localNameN>]}",
                          4, 
                          30, 
                          False)
                          
  PshellServer.addCommand(show, 
                          "show", 
                          "show server or multicast info", 
                          "servers | multicast", 
                          2, 
                          2, 
                          True)
                          
  PshellServer.addCommand(multicast, 
                          "multicast", 
                          "send multicast command to registered servers",
                          "<command>",
                          2,
                          30,
                          True)

  # start our local pshell server
  PshellServer.startServer("pshellAggregator", PshellServer.LOCAL, PshellServer.BLOCKING)
  
  # disconnect all our remote control servers
  PshellControl.disconnectAllServers()
  
  # cleanup our local server's resources
  PshellServer.cleanupResources()

 
