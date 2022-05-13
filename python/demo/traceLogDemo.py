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
# This is an example demo program that shows how to use the TraceLog feature with
# an integrated PshellServer to allow for dynamic control of trace log settings
#
#################################################################################

# import all our necessary modules
import signal
import time
import sys

import PshellServer
import TraceLog as Trace

#################################################################################
#################################################################################
def sampleOutputFunction(outputString_):

   # this sample log output function is registered with the TraceLog
   # 'registerOutputFunction' call to show a client supplied
   # log function, the string passed in is a fully formatted log
   # string, it is up to the registering application to decide
   # what to do with that string, i.e. write to stdout, write to
   # a custom logfile, write to syslog etc

   # write to stdout
   print("CUSTOM OUTPUT - %s" % outputString_)

   # write to syslog
   #syslog(LOG_INFO, "%s", outputString_)

#################################################################################
#################################################################################
def sampleFormatFunction(logname_, level_, timestamp_, message_):

  # this sample format function is registered with the TraceLog
  # 'registerFormatFunction' call to show a client supplied
  #
  # formatting function, the formating must be done into the outputString_
  # variable, the user can check to see if the various formatting items
  # are enabled, or they can just use a fixed format

  # standard output format: name | level | timestamp | user-message
  # custom format is: timestamp level:[name] - user-message

  message = ""
  if Trace.isFormatEnabled():
    if Trace.isTimestampEnabled():
      message = "%s " % timestamp_
    message += "%-7s : " % level_
    if Trace.isLogNameEnabled():
      message += "[%s] - " % logname_
    message += message_
    return (message)
  else:
    return (message_)

#################################################################################
#################################################################################
def showUsage():
  print("")
  print("Usage: traceLogDemo <level> [custom]")
  print("")
  print("  where:")
  print("    <level>  - The desired log level value, 0-%d" % Trace.TL_ALL)
  print("    custom   - Use a custom log format")
  print("")
  exit(0)

#################################################################################
#################################################################################
def signalHandler(signal, frame):
  PshellServer.cleanupResources()
  print("")
  Trace.FORCE("Exiting program traceLogDemo...")
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
if __name__ == '__main__':

  registerSignalHandlers()

  logLevel = Trace.TL_ALL

  # validate our command line arguments and get desired log level
  if len(sys.argv) == 2 or len(sys.argv) == 3:
    if sys.argv[1] == "-h":
      showUsage()
    if len(sys.argv) == 3:
      if sys.argv[2] == "custom":
        # register our custom format function
        Trace.registerFormatFunction(sampleFormatFunction)
        # set a custom timestamp format, add the date, the time must be last because the usec portion is appended to the time
        Trace.setTimestampFormat("%Y-%m-%d %T")
        # register our custom output function
        Trace.registerOutputFunction(sampleOutputFunction)
      else:
        showUsage()
    logLevel = int(sys.argv[1])

  Trace.init("DEMO", "/var/log/traceLogDemo.log", logLevel)

  PshellServer.startServer("traceLogDemo", PshellServer.UDP, PshellServer.NON_BLOCKING, PshellServer.ANYHOST, 9292);

  while True:
    Trace.FORCE("Force Message")
    time.sleep(1)
    Trace.ERROR("Error Message")
    time.sleep(1)
    Trace.FAILURE("Failure Message")
    time.sleep(1)
    Trace.WARNING("Warning Message")
    time.sleep(1)
    Trace.INFO("Info Message")
    time.sleep(1)
    Trace.DEBUG("Debug Message")
    time.sleep(1)
    Trace.ENTER("Enter Message")
    time.sleep(1)
    Trace.EXIT("Exit Message")
    time.sleep(1)
    Trace.DUMP("Dump Message")
    time.sleep(1)
