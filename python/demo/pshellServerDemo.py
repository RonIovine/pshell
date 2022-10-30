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
import datetime
import random
import PshellServer

# constants used for the advanved parsing date/time stamp range checking
MAX_YEAR   = 3000
MAX_MONTH  = 12
MAX_DAY    = 31
MAX_HOUR   = 23
MAX_MINUTE = 59
MAX_SECOND = 59

# dynamic value used for the dynamicOutput function
dynamicValue = "0"

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
#
# helloWorld:
#
# Simple helloWorld command that just prints out all the passed in arguments
#
#################################################################################
def helloWorld(argv):
  PshellServer.printf("helloWorld command dispatched:")
  for index, arg in enumerate(argv):
    PshellServer.printf("  argv[%d]: '%s'" % (index, arg))

#################################################################################
#
# keepAlive:
#
# This command shows an example client keep alive, the PSHELL UDP client has
# a default 5 second timeout, if a command will be known to take longer than
# 5 seconds, it must give some kind of output back to the client, this shows
# the two helper functions created the assist in this, the TCP client does not
# need a keep alive since the TCP protocol itself handles that, but it can be
# handy to show general command progress for commands that take a long time
#
#################################################################################
def keepAlive(argv):
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  NOTE: This function demonstrates intermediate flushes in a")
    PshellServer.printf("        callback command to keep the UDP/UNIX interactive client" )
    PshellServer.printf("        from timing out for commands that take longer than the")
    PshellServer.printf("        response timeout (default=5 sec).")
    PshellServer.printf()
    return
  elif (argv[0] == "dots"):
    PshellServer.printf("marching dots keep alive:")
    for i in range(0,10):
      PshellServer.march(".")
      time.sleep(1)
  elif (argv[0] == "bang"):
    PshellServer.printf("marching 'bang' keep alive:")
    for i in range(0,10):
      PshellServer.march("!")
      time.sleep(1)
  elif (argv[0] == "pound"):
    PshellServer.printf("marching pound keep alive:")
    for i in range(0,10):
      PshellServer.march("#")
      time.sleep(1)
  elif (argv[0] == "wheel"):
    PshellServer.printf("spinning wheel keep alive:")
    for i in range(0,10):
      # string is optional, leave empty or use None to omit
      PshellServer.wheel("optional string: ")
      time.sleep(1)
  elif (argv[0] == "clock"):
    PshellServer.printf("elapsed time keep alive in hh:mm:ss format:")
    for i in range(0,10):
      # string is optional, leave empty or use None to omit
      PshellServer.clock("optional string: ")
      time.sleep(1)
  else:
    PshellServer.showUsage()
    return
  PshellServer.printf()

#################################################################################
#
# wildcardMatch:
#
# This command shows matching the passed command arguments based on substring
# matching rather than matching on the complete exact string, the minimum
# number of characters that must be matched is the last argument to the
# PshellServer.isSubString function, this must be the minimum number of
# characters necessary to uniquely identify the argument from the complete
# argument list
#
# NOTE: This technique could have been used in the previous example for the
#       "wheel" and "dots" arguments to provide for wildcarding of those
#       arguments.  In the above example, as written, the entire string of
#       "dots" or "wheel" must be enter to be accepted.
#
#################################################################################
def wildcardMatch(argv):
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where valid <args> are:")
    PshellServer.printf("    on")
    PshellServer.printf("    of*f")
    PshellServer.printf("    a*ll")
    PshellServer.printf("    sy*mbols")
    PshellServer.printf("    se*ttings")
    PshellServer.printf("    d*efault")
    PshellServer.printf()
  elif (PshellServer.isSubString(argv[0], "on", 2)):
    PshellServer.printf("argv 'on' match")
  elif (PshellServer.isSubString(argv[0], "off", 2)):
    PshellServer.printf("argv 'off' match")
  elif (PshellServer.isSubString(argv[0], "all", 1)):
    PshellServer.printf("argv 'all' match")
  elif (PshellServer.isSubString(argv[0], "symbols", 2)):
    PshellServer.printf("argv 'symbols' match")
  elif (PshellServer.isSubString(argv[0], "settings", 2)):
    PshellServer.printf("argv 'settings' match")
  elif (PshellServer.isSubString(argv[0], "default", 1)):
    PshellServer.printf("argv 'default' match")
  else:
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where valid <args> are:")
    PshellServer.printf("    on")
    PshellServer.printf("    of*f")
    PshellServer.printf("    a*ll")
    PshellServer.printf("    sy*mbols")
    PshellServer.printf("    se*ttings")
    PshellServer.printf("    d*efault")
    PshellServer.printf()

#################################################################################
#
# enhancedUsage:
#
# This command shows a command that is registered with the "showUsage" flag
# set to "false", the PshellServer will invoke the command when the user types
# a "?" or "-h" rather than automatically giving the registered usage, the
# callback command can then see if the user asked for help (i.e. typed a "?"
# or "-h") by calling PshellServer.isHelp, the user can then display the
# standard registered usage with the PshellServer.showUsage call and then give
# some optional enhanced usage with the PshellServer.printf call
#
#################################################################################
def enhancedUsage(argv):
  # see if the user asked for help
  if (PshellServer.isHelp()):
    # show standard usage
    PshellServer.showUsage()
    # give some enhanced usage
    PshellServer.printf("Enhanced usage here...")
  else:
    # do normal function processing
    PshellServer.printf("enhancedUsage command dispatched:")
    for index, arg in enumerate(argv):
      PshellServer.printf("  argv[%d]: '%s'" % (index, arg))

#################################################################################
#
# formatChecking:
#
# This function demonstrates the various helper functions that assist in the
# interpretation and conversion of command line arguments
#
#################################################################################
def formatChecking(argv):
  PshellServer.printf("formatChecking command dispatched:")
  if PshellServer.isIpv4Addr(argv[0]):
    PshellServer.printf("IPv4 address: '%s' entered" % argv[0])
  elif PshellServer.isIpv4AddrWithNetmask(argv[0]):
    PshellServer.printf("IPv4 address/netmask: '%s' entered" % argv[0])
  elif PshellServer.isMacAddr(argv[0]):
    PshellServer.printf("MAC address: '%s' entered" % argv[0])
  elif PshellServer.isDec(argv[0]):
    PshellServer.printf("Decimal arg: %d entered" % PshellServer.getInt(argv[0]))
  elif PshellServer.isHex(argv[0]):
    PshellServer.printf("Hex arg: 0x%x entered" % PshellServer.getInt(argv[0]))
  elif PshellServer.isAlpha(argv[0]):
    if PshellServer.isEqual(argv[0], "myarg"):
      PshellServer.printf("Alphabetic arg: '%s' equal to 'myarg'" % argv[0])
    else:
      PshellServer.printf("Alphabetic arg: '%s' not equal to 'myarg'" % argv[0])
  elif PshellServer.isAlphaNumeric(argv[0]):
    if PshellServer.isEqual(argv[0], "myarg1"):
      PshellServer.printf("Alpha numeric arg: '%s' equal to 'myarg1'" % argv[0])
    else:
      PshellServer.printf("Alpha numeric arg: '%s' not equal to 'myarg1'" % argv[0])
  elif PshellServer.isFloat(argv[0]):
    PshellServer.printf("Float arg: %.2f entered" % PshellServer.getFloat(argv[0]))
  else:
    PshellServer.printf("Unknown arg format: '%s'" % argv[0])

#################################################################################
#
# advancedParsing:
#
# This function shows advanced command line parsing using the
# PshellServer.tokenize function
#
#################################################################################
def advancedParsing(argv):

  numDateTokens, date = PshellServer.tokenize(argv[0], "/")
  numTimeTokens, time = PshellServer.tokenize(argv[1], ":")

  if numDateTokens != 3 or numTimeTokens != 3:
    PshellServer.printf("ERROR: Improper timestamp format!!")
    PshellServer.showUsage()
  elif (not PshellServer.isDec(date[0]) or
        PshellServer.getInt(date[0]) > MAX_MONTH):
    PshellServer.printf("ERROR: Invalid month: %s, must be numeric value <= %d" %
                        (date[0], MAX_MONTH))
  elif (not PshellServer.isDec(date[1]) or
        PshellServer.getInt(date[1]) > MAX_DAY):
    PshellServer.printf("ERROR: Invalid day: %s, must be numeric value <= %d" %
                        (date[1], MAX_DAY))
  elif (not PshellServer.isDec(date[2]) or
        PshellServer.getInt(date[2]) > MAX_YEAR):
    PshellServer.printf("ERROR: Invalid year: %s, must be numeric value <= %d" %
                       (date[2], MAX_YEAR))
  elif (not PshellServer.isDec(time[0]) or
        PshellServer.getInt(time[0]) > MAX_HOUR):
    PshellServer.printf("ERROR: Invalid hour: %s, must be numeric value <= %d" %
                        (time[0], MAX_HOUR))
  elif (not PshellServer.isDec(time[1]) or
        PshellServer.getInt(time[1]) > MAX_MINUTE):
    PshellServer.printf("ERROR: Invalid minute: %s, must be numeric value <= %d" %
                        (time[1], MAX_MINUTE))
  elif (not PshellServer.isDec(time[2]) or
        PshellServer.getInt(time[2]) > MAX_SECOND):
    PshellServer.printf("ERROR: Invalid second: %s, must be numeric value <= %d" %
                        (time[2], MAX_SECOND))
  else:
    PshellServer.printf("Month  : %s" % date[0])
    PshellServer.printf("Day    : %s" % date[1])
    PshellServer.printf("Year   : %s" % date[2])
    PshellServer.printf("Hour   : %s" % time[0])
    PshellServer.printf("Minute : %s" % time[1])
    PshellServer.printf("Second : %s" % time[2])

#################################################################################
#
# dynamicOutput:
#
# This function show an output value that changes frequently, this is used to
# illustrate the command line mode with a repeated rate and an optional clear
# screen between iterations, using command line mode in this way along with a
# function with dynamically changing output information will produce a display
# similar to the familiar "top" display command output
#
#################################################################################
def dynamicOutput(argv):
  global dynamicValue
  if PshellServer.isEqual(argv[0], "show"):
    # get timestamp
    PshellServer.printf()
    PshellServer.printf("DYNAMICALLY CHANGING OUTPUT")
    PshellServer.printf("===========================")
    PshellServer.printf()
    PshellServer.printf("Timestamp ........: %s" % datetime.datetime.now().strftime("%T.%f"))
    PshellServer.printf("Random Value .....: %d" % random.randint(0, 2**32))
    PshellServer.printf("Dynamic Value ....: %s" % dynamicValue)
    PshellServer.printf()
  else:
    dynamicValue = argv[0]

#################################################################################
#
# getOptions:
#
# This function shows the extraction of arg options using the
# PshellServer.getOption function,the format of the options are either
# -<option><value> where <option> is a single character option (e.g. -t10),
# or <option>=<value> where <option> is any length character string (e.g.
# timeout=10)
#
#################################################################################
def getOptions(argv):
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where:")
    PshellServer.printf("    arg - agrument of the format -<key><value> or <key>=<value>")
    PshellServer.printf()
    PshellServer.printf("  For the first form, the <key> must be a single character, e.g. -t10")
    PshellServer.printf("  for the second form, <key> can be any length, e.g. timeout=10")
    PshellServer.printf()
  else:
    for index, arg in enumerate(argv):
      (parsed, key, value) = PshellServer.getOption(arg)
      PshellServer.printf("arg[%d]: '%s', parsed: %s, key: '%s', value: '%s'" % (index, arg, parsed, key, value))

#################################################################################
#################################################################################
def showUsage():
  print("")
  print("Usage: pshellServerDemo.py -udp [<port>] | -tcp [<port>] | -unix | -local")
  print("")
  print("  where:")
  print("    -udp   - Multi-session UDP server")
  print("    -tcp   - Single session TCP server")
  print("    -unix  - Multi-session UNIX domain server")
  print("    -local - Local command dispatching server")
  print("    <port> - Desired UDP or TCP port, default: %d" % PSHELL_DEMO_PORT)
  print("")
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
  signal.signal(signal.SIGSYS, signalHandler)      # 31 Bad system call

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  PSHELL_DEMO_PORT = 9001
  if (len(sys.argv) == 2 or len(sys.argv) == 3):
    if (sys.argv[1] == "-udp"):
      serverType = PshellServer.UDP
    elif (sys.argv[1] == "-unix"):
      serverType = PshellServer.UNIX
    elif (sys.argv[1] == "-tcp"):
      serverType = PshellServer.TCP
    elif (sys.argv[1] == "-local"):
      serverType = PshellServer.LOCAL
    else:
      showUsage()
    if len(sys.argv) == 3:
      PSHELL_DEMO_PORT = int(sys.argv[2])
  else:
    showUsage()

  # register signal handlers so we can do a graceful termination and cleanup any system resources
  registerSignalHandlers()

  # register our callback commands, commands consist of single keyword only
  PshellServer.addCommand(function    = helloWorld,
                          command     = "helloWorld",
                          description = "command that prints out arguments",
                          usage       = "[<arg1> ... <arg20>]",
                          minArgs     = 0,
                          maxArgs     = 20)

  PshellServer.addCommand(function    = keepAlive,
                          command     = "keepAlive",
                          description = "command to show UDP/UNIX client keep-alive",
                          usage       = "dots | bang | pound | wheel | clock",
                          minArgs     = 1,
                          showUsage   = False)

  PshellServer.addCommand(function    = wildcardMatch,
                          command     = "wildcardMatch",
                          description = "command that does a wildcard matching",
                          usage       = "<arg>",
                          minArgs     = 1,
                          showUsage   = False)

  PshellServer.addCommand(function    = enhancedUsage,
                          command     = "enhancedUsage",
                          description = "command with enhanced usage",
                          usage       = "<arg1>",
                          minArgs     = 1,
                          showUsage   = False)

  PshellServer.addCommand(function    = formatChecking,
                          command     = "formatChecking",
                          description = "command with arg format checking",
                          usage       = "<arg1>",
                          minArgs     = 1)

  PshellServer.addCommand(function    = advancedParsing,
                          command     = "advancedParsing",
                          description = "command with advanced command line parsing",
                          usage       = "<mm>/<dd>/<yyyy> <hh>:<mm>:<ss>",
                          minArgs     = 2)

  PshellServer.addCommand(function    = dynamicOutput,
                          command     = "dynamicOutput",
                          description = "command with dynamic output for command line mode",
                          usage       = "show | <value>",
                          minArgs     = 1)

  PshellServer.addCommand(function    = getOptions,
                          command     = "getOptions",
                          description = "example of parsing command line options",
                          usage       = "<arg1> [<arg2>...<argN>]",
                          minArgs     = 1,
                          maxArgs     = 20,
                          showUsage   = False)

  # run a registered command from within it's parent process, this can be done before
  # or after the server is started, as long as the command being called is regstered
  PshellServer.runCommand("helloWorld 1 2 3")

  # Now start our PSHELL server with the PshellServer.startServer function call.
  #
  # The 1st argument is our serverName (i.e. "pshellServerDemo").
  #
  # The 2nd argument specifies the type of PSHELL server, the four valid values are:
  #
  #   PshellServer.UDP   - Server runs as a multi-session UDP based server.  This requires
  #                        the special stand-alone command line UDP/UNIX client program
  #                        'pshell'.  This server has no timeout for idle client sessions.
  #                        It can be also be controlled programatically via an external
  #                        program running the PshellControl API and library.
  #   PshellServer.UNIX  - Server runs as a multi-session UNIX based server.  This requires
  #                        the special stand-alone command line UDP/UNIX client program
  #                        'pshell'.  This server has no timeout for idle client sessions.
  #                        It can be also be controlled programatically via an external
  #                        program running the PshellControl API and library.
  #   PshellServer.TCP   - Server runs as a single session TCP based server with a 10 minute
  #                        idle client session timeout.  The TCP server can be connected to
  #                        using a standard 'telnet' based client.
  #   PshellServer.LOCAL - Server solicits it's own command input via the system command line
  #                        shell, there is no access via a separate client program, when the
  #                        user input is terminated via the 'quit' command, the program is
  #                        exited.
  #
  # The 3rd argument is the server mode, the two valid values are:
  #
  #   PshellServer.NON_BLOCKING - A separate thread will be created to process user input, when
  #                               this function is called as non-blocking, the function will return
  #                               control to the calling context.
  #   PshellServer.BLOCKING     - No thread is created, all processing of user input is done within
  #                               this function call, it will never return control to the calling context.
  #
  # The 4th and 5th arguments must be provided for a UDP or TCP server, for a LOCAL or
  # UNIX server they can be omitted, and if provided they will be ignored.
  #
  # For the 4th argument, a valid IP address or hostname can be used.  There are also 3 special
  # "hostname" type identifiers defined as follows:
  #
  #   localhost - the loopback address (i.e. 127.0.0.1)
  #   myhost    - the hostname assigned to this host, this is usually defined in the
  #               /etc/hosts file and is assigned to the default interface
  #   anyhost   - all interfaces on a multi-homed host (including loopback)
  #
  # Finally, the 5th argument is the desired port number.
  #
  # All of these arguments (except the server name and mode, i.e. args 1 & 3) can be overridden
  # via the 'pshell-server.conf' file on a per-server basis.

  #
  # NOTE: This server is started up in BLOCKING mode, but if a pshell server is being added
  #       to an existing process that already has a control loop in it's main, it should be
  #       started in NON_BLOCKING mode before the main drops into it's control loop, see the
  #       demo program traceFilterDemo.cc for an example
  #
  PshellServer.startServer("pshellServerDemo", serverType, PshellServer.BLOCKING, PshellServer.ANYHOST, PSHELL_DEMO_PORT)

  #
  # should never get here because the server is started in BLOCKING mode,
  # but cleanup any pshell system resources as good practice
  #
  PshellServer.cleanupResources()
