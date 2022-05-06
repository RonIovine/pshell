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

"""
TraceLog Module

This module provides a simple API to output trace logs.  The output can either
be directed to stdout ot a logfile or both.  There are several levels setup in
a hierarchical manner.  The various trace settings can be controlled interactively
via pshell.  The process that uses the module but run a pshell server, the pshell
command to configure the trace settings is registered into the pshell server
framework automatically during the init() call.  That call must be before the call
to the pshell server startServer() call.

Functions:

  init()               -- Initialize the trace logging subsystem
  setLogFile()         -- Set the output logfile
  setLogName()         -- Set the current trace logger name
  setLogLevel()        -- Set the current log level
  setDefaultLogLevel() -- Set the default log level
  enableFormat()       -- Enable/disable log formatting or user-message only
  enableLog()          -- Enable/disable the overall output of the logs (except FORCE)
  enableTimestamp()    -- Enabledisable the timestamp in the trace output messages
  enableLogName()      -- Enable/disable the displaying of the logger name in the trace output messages
  enableFullDatetime() -- Enable/disable the full date/time timestamp format or disable (time only)

Trace log commands:

  FORCE()    -- Issue a FORCE trace log, cannot be masked or disabled
  ERROR()    -- Issue an ERROR trace log, can be disabled, but not masked
  FAILURE()  -- Issue a FAILURE trace log, can be both disabled and masked
  WARNING()  -- Issue a WARNING trace log, can be both disabled and masked
  INFO()     -- Issue an INFO trace log, can be both disabled and masked
  DEBUG()    -- Issue a DEBUG trace log, can be both disabled and masked
  ENTER()    -- Issue an ENTER trace log, can be both disabled and masked
  EXIT()     -- Issue an EXIT trace log, can be both disabled and masked
  DUMP()     -- Issue a hex DUMP trace log, can be both disabled and masked

Trace log levels:

  Use these identifiers for the setting the log level in the setLogLevel() and
  setDefaultLogLevel() function calls.

  TL_ERROR
  TL_FAILURE
  TL_WARNING
  TL_INFO
  TL_DEBUG
  TL_ENTER
  TL_EXIT
  TL_DUMP
  TL_ALL
  TL_DEFAULT
"""

# import all our necessary modules
import signal
import time
import sys
import datetime
import threading

import PshellServer

TL_ERROR     = 0
TL_FAILURE   = 1
TL_WARNING   = 2
TL_INFO      = 3
TL_DEBUG     = 4
TL_ENTER     = 5
TL_EXIT      = 6
TL_DUMP      = 7

TL_ALL     = TL_DUMP
TL_DEFAULT = TL_WARNING

_gLogLevels = ("ERROR", "FAILURE", "WARNING", "INFO", "DEBUG", "ENTER", "EXIT", "DUMP")

_TRACE_OUTPUT_FILE   = 0
_TRACE_OUTPUT_STDOUT = 1
_TRACE_OUTPUT_BOTH   = 2

_RED    = "\33[1;31m"
_GREEN  = "\33[1;32m"
_NORMAL = "\33[0m"
_ON     = "\33[1;32mON\33[0m"     # GREEN
_OFF    = "\33[1;31mOFF\33[0m"    # RED
_NONE   = "\33[1;36mNONE\33[0m"   # CYAN

_gMutex = threading.Lock()
_gUseColors = True
_gTraceOutput = _TRACE_OUTPUT_STDOUT
_gLogDefault = TL_DEFAULT
_gLogLevel = _gLogDefault
_gLogName = "Trace"
_gLogFileName = None
_gLogFileFd = None
_gBriefTimestampFormat = "%H:%M:%S.%f"
_gFullTimestampFormat = "%Y-%m-%d %H:%M:%S.%f"
_gTimestampFormat = _gBriefTimestampFormat
_gTimestampType = "time only"
_gLogEnabled = True
_gFormatEnabled = True
_gTimestampEnabled = True
_gLogNameEnabled = True

##################################
# 'public' functions
##################################

#################################################################################
#################################################################################
def init(logname = "Trace", logfile = None, loglevel = TL_DEFAULT):
  """
  Initialize the trace logger subsystem

    Args:
        logname (str)    Name of the logger, deafault='Trace', usually set to process name
        logfile (str)   : Logfile for trace output, default=stdout
        loglevel (int)  : Initial and default log level, default=TL_WARNING

    Returns:
        none
  """
  setLogName(logname)
  setLogLevel(loglevel)
  setDefaultLogLevel(loglevel)
  if not setLogFile(logfile):
    ERROR("Could not open logfile: {}, reverting to: stdout".format(logfile))
  PshellServer.setLogFunction(_pshellLogFunction);
  # create PSHELL command to configure trace settings
  PshellServer.addCommand(function    = _configureTrace,
                          command     = "trace",
                          description = "configure/display various trace logger settings",
                          usage       = "on | off | show |\n"
                                        "             output {file | stdout | both | <filename>} |\n"
                                        "             level {all | default | <value>} |\n"
                                        "             default {all | <value>} |\n"
                                        "             format {on | off} |\n"
                                        "             name {on | off | default | <value>} |\n"
                                        "             timestamp {on | off | datetime | time} |\n"
                                        "             colors {on | off}",
                          minArgs     = 1,
                          maxArgs     = 2,
                          showUsage   = False)

#################################################################################
#################################################################################
def setLogName(logname):
  """
  Set the name of the trace logger

    Args:
        logfile (str)   : Name of the trace logger, default='Trace'

    Returns:
        none
  """
  global _gLogName
  _gLogName = logname

#################################################################################
#################################################################################
def setLogFile(logfile):
  """
  Set the output logfile

    Args:
        logfile (str)   : Logfile for trace output, default=stdout

    Returns:
        none
  """
  global _gLogFileName
  global _gLogFileFd
  global _gTraceOutput
  if logfile != None:
    try:
      newLogFileFd = open(logfile, 'a+')
      if _gLogFileFd != None:
        _gLogFileFd.close()
      _gLogFileFd = newLogFileFd
      _gLogFileName = logfile
      _gTraceOutput = _TRACE_OUTPUT_FILE
      return True
    except:
      if _gLogFileFd != None:
        return False
      else:
        _gTraceOutput = _TRACE_OUTPUT_STDOUT
        return False
  else:
    _gLogFileFd = None
    _gTraceOutput = _TRACE_OUTPUT_STDOUT
    return False

#################################################################################
#################################################################################
def setLogLevel(loglevel):
  """
  Set the current log level

    Args:
        loglevel (int)  : Current log level

    Returns:
        none
  """
  global _gLogLevel
  _gLogLevel = loglevel

#################################################################################
#################################################################################
def setDefaultLogLevel(defaultLevel):
  """
  Set the default log level

    Args:
        defaultLevel (int)  : Default log level

    Returns:
        none
  """
  global _gLogDefault
  _gLogDefault = defaultLevel

#################################################################################
#################################################################################
def enableFormat(enable):
  """
  Enable/disable log formatting or user-message only

    Args:
        enable (bool)  : Enable/disable the display of the log formatting

    Returns:
        none
  """
  global _gFormatEnabled
  _gFormatEnabled = enable

#################################################################################
#################################################################################
def enableLog(enable):
  """
  Enable/disable the overall output of the logs (except FORCE)

    Args:
        enable (bool)  : Enable/disable the output of all logs (except FORCE)

    Returns:
        none
  """
  global _gLogEnabled
  _gLogEnabled = enable

#################################################################################
#################################################################################
def enableTimestamp(enable):
  """
  Enabledisable the timestamp in the trace output messages

    Args:
        enable (bool)  : Enable/disable display of timestamp in log messages

    Returns:
        none
  """
  global _gTimestampEnabled
  _gTimestampEnabled = enable_

#################################################################################
#################################################################################
def enableLogName(enable):
  """
  Enable/disable the displaying of the logger name in the trace output messages

    Args:
        enable (bool)  : Enable/disable logger namd in trace log messages
    Returns:
        none
  """
  global _gLogNameEnabled
  _gLogNameEnabled = enable

#################################################################################
#################################################################################
def enableFullDatetime(enable):
  """
  Enable/disable the full date/time timestamp format or disable (time only)

    Args:
        enable (bool)  : Enable/disable full datetime format

    Returns:
        none
  """
  global _gBriefTimestampFormat
  global _gFullTimestampFormat
  global _gTimestampFormat
  global _gTimestampType
  if enable:
    _gTimestampFormat = _gFullTimestampFormat
    _gTimestampType = "date & time"
  else:
    _gTimestampFormat = _gBriefTimestampFormat
    _gTimestampType = "time only"

#####################################
# trace log commands/functions
#####################################

#################################################################################
#################################################################################
def FORCE(message):
  """
  Output a trace FORCE message, this is unconditional and cannot be disabled
  or masked

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  _outputTrace("FORCE", message)

#################################################################################
#################################################################################
def ERROR(message):
  """
  Output a trace ERROR message

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  if _gLogEnabled and _gLogLevel >= TL_ERROR:
    _outputTrace(_gLogLevels[TL_ERROR], message)

#################################################################################
#################################################################################
def FAILURE(message):
  """
  Output a trace FAILURE message

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  if _gLogEnabled and _gLogLevel >= TL_FAILURE:
    _outputTrace(_gLogLevels[TL_FAILURE], message)

#################################################################################
#################################################################################
def WARNING(message):
  """
  Output a trace WARNING message

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  if _gLogEnabled and _gLogLevel >= TL_WARNING:
    _outputTrace(_gLogLevels[TL_WARNING], message)

#################################################################################
#################################################################################
def INFO(message):
  """
  Output a trace INFO message

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  if _gLogEnabled and _gLogLevel >= TL_INFO:
    _outputTrace(_gLogLevels[TL_INFO], message)

#################################################################################
#################################################################################
def DEBUG(message):
  """
  Output a trace DEBUG message

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  if _gLogEnabled and _gLogLevel >= TL_DEBUG:
    _outputTrace(_gLogLevels[TL_DEBUG], message)

#################################################################################
#################################################################################
def ENTER(message):
  """
  Output a trace ENTER message

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  if _gLogEnabled and _gLogLevel >= TL_ENTER:
    _outputTrace(_gLogLevels[TL_ENTER], message)

#################################################################################
#################################################################################
def EXIT(message):
  """
  Output a trace EXIT message

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  if _gLogEnabled and _gLogLevel >= TL_EXIT:
    _outputTrace(_gLogLevels[TL_EXIT], message)

#################################################################################
#################################################################################
def DUMP(message):
  """
  Output a trace hex DUMP message

    Args:
        message (str)  : User log message

    Returns:
        none
  """
  if _gLogEnabled and _gLogLevel >= TL_DUMP:
    _outputTrace(_gLogLevels[TL_DUMP], message)

##################################
# 'private' functions
##################################

#################################################################################
#################################################################################
def _enableColors(enable_):
  global _gUseColors
  global _RED
  global _GREEN
  global _NORMAL
  global _ON
  global _OFF
  global _NONE
  _gUseColors = enable_
  if _gUseColors:
    _RED    = "\33[1;31m"
    _GREEN  = "\33[1;32m"
    _NORMAL = "\33[0m"
    _ON     = "\33[1;32mON\33[0m"     # GREEN
    _OFF    = "\33[1;31mOFF\33[0m"    # RED
    _NONE   = "\33[1;36mNONE\33[0m"   # CYAN
  else:
    _RED    = ""
    _GREEN  = ""
    _NORMAL = ""
    _ON     = "ON"
    _OFF    = "OFF"
    _NONE   = "NONE"

########################################################################
# pshell callback function to dynamicaly configure trace settings
########################################################################

#################################################################################
#################################################################################
def _configureTrace(argv_):
  global _gTraceOutput
  global _gLogDefault
  global _gMutex
  _gMutex.acquire()
  if PshellServer.isHelp():
    PshellServer.printf("")
    PshellServer.showUsage()
    PshellServer.printf("")
    PshellServer.printf("  where:")
    PshellServer.printf("    output    - set trace log output location")
    PshellServer.printf("    level     - set current trace log level")
    PshellServer.printf("    default   - set default trace log level")
    PshellServer.printf("    format    - enable/disable all trace header formatting")
    PshellServer.printf("    name      - enable/disable/set trace name prefix format item")
    PshellServer.printf("    timestamp - enable/disable/set trace timestamp and format item")
    PshellServer.printf("    colors    - enable/disable colors in 'trace show' command")
    PshellServer.printf("")
    PshellServer.printf("  NOTE: Setting 'trace off' will disable all trace outputs including")
    PshellServer.printf("        TRACE_ERROR except for any TRACE_FORCE statements, setting")
    PshellServer.printf("        back to 'trace on' will restore the previous level settings")
    PshellServer.printf("")
    PshellServer.printf("        The '*' marker next to a trace level in the 'trace show' command")
    PshellServer.printf("        indicates it's a current default level")
    PshellServer.printf("")
    PshellServer.printf("        If using colors for the 'trace show' display.  A green color for the")
    PshellServer.printf("        level indicates currently enabled, a red color indicates disabled")
    PshellServer.printf("")
  elif len(argv_) == 1:
    if PshellServer.isSubString(argv_[0], "on", 2):
      enableLog(True)
    elif PshellServer.isSubString(argv_[0], "off", 2):
      enableLog(False)
    elif PshellServer.isSubString(argv_[0], "show", 1):
      PshellServer.printf("")
      PshellServer.printf("**********************************")
      PshellServer.printf("*   TRACE LOGGER CONFIGURATION   *")
      PshellServer.printf("**********************************")
      PshellServer.printf("")
      if _gLogEnabled:
        PshellServer.printf("Trace enabled.......: %s" % _ON)
      else:
        PshellServer.printf("Trace enabled.......: %s" % _OFF)
      if _gTraceOutput == _TRACE_OUTPUT_BOTH:
        PshellServer.printf("Trace output........: %s" % _gLogFileName)
        PshellServer.printf("                    : stdout")
      elif _gTraceOutput == _TRACE_OUTPUT_STDOUT:
        PshellServer.printf("Trace output........: stdout")
      else:
        PshellServer.printf("Trace output........: %s" % _gLogFileName)
      if _gFormatEnabled:
        PshellServer.printf("Trace format........: %s" % _ON)
      else:
        PshellServer.printf("Trace format........: %s" % _OFF)
      if _gLogNameEnabled:
        PshellServer.printf("  Name..............: %s" % _ON)
      else:
        PshellServer.printf("  Name..............: %s" % _OFF)
      if _gTimestampEnabled:
        PshellServer.printf("  Timestamp.........: %s (%s)" % (_ON, _gTimestampType))
      else:
        PshellServer.printf("  Timestamp.........: %s (%s)" % (_OFF, _gTimestampType))
      PshellServer.printf("Trace level(s)......: %s%s*%s" % (_GREEN, _gLogLevels[TL_ERROR], _NORMAL))
      defaultMarker = ""
      for level, name in enumerate(_gLogLevels[1:]):
        if level+1 <= _gLogDefault:
          defaultMarker = "*"
        else:
          defaultMarker = ""
        if level+1 <= _gLogLevel:
          PshellServer.printf("                    : %s%s%s%s" % (_GREEN, name, defaultMarker, _NORMAL))
        else:
          if _gUseColors:
            PshellServer.printf("                    : %s%s%s%s" % (_RED, name, defaultMarker, _NORMAL))
          else:
            noColorString = "%s%s" % (name, defaultMarker)
            PshellServer.printf("                    : %-8s (disabled)" % noColorString)
      PshellServer.printf("")
    else:
      PshellServer.showUsage()
  else:  # len(argv_) == 2
    if PshellServer.isSubString(argv_[0], "level", 2):
      if PshellServer.isSubString(argv_[1], "all", 2):
        setLogLevel(TL_ALL)
      elif PshellServer.isSubString(argv_[1], "default", 2):
        setLogLevel(_gLogDefault)
      elif PshellServer.isSubString(argv_[1], "error", 2):
        setLogLevel(TL_ERROR)
      elif PshellServer.isSubString(argv_[1], "failure", 2):
        setLogLevel(TL_FAILURE)
      elif PshellServer.isSubString(argv_[1], "warning", 2):
        setLogLevel(TL_WARNING)
      elif PshellServer.isSubString(argv_[1], "info", 2):
        setLogLevel(TL_INFO)
      elif PshellServer.isSubString(argv_[1], "debug", 2):
        setLogLevel(TL_DEBUG)
      elif PshellServer.isSubString(argv_[1], "enter", 2):
        setLogLevel(TL_ENTER)
      elif PshellServer.isSubString(argv_[1], "exit", 2):
        setLogLevel(TL_EXIT)
      elif PshellServer.isSubString(argv_[1], "dump", 2):
        setLogLevel(TL_DUMP)
      else:
        PshellServer.printf("ERROR: Invalid log level: %s, run 'trace show' to see available levels" % argv_[1])
    elif PshellServer.isSubString(argv_[0], "default", 2):
      if PshellServer.isSubString(argv_[1], "all", 2):
        setDefaultLogLevel(TL_ALL)
      elif PshellServer.isSubString(argv_[1], "error", 2):
        setDefaultLogLevel(TL_ERROR)
      elif PshellServer.isSubString(argv_[1], "failure", 2):
        setDefaultLogLevel(TL_FAILURE)
      elif PshellServer.isSubString(argv_[1], "warning", 2):
        setDefaultLogLevel(TL_WARNING)
      elif PshellServer.isSubString(argv_[1], "info", 2):
        setDefaultLogLevel(TL_INFO)
      elif PshellServer.isSubString(argv_[1], "debug", 2):
        setDefaultLogLevel(TL_DEBUG)
      elif PshellServer.isSubString(argv_[1], "enter", 2):
        setDefaultLogLevel(TL_ENTER)
      elif PshellServer.isSubString(argv_[1], "exit", 2):
        setDefaultLogLevel(TL_EXIT)
      elif PshellServer.isSubString(argv_[1], "dump", 2):
        setDefaultLogLevel(TL_DUMP)
      else:
        PshellServer.printf("ERROR: Invalid log level: %s, run 'trace show' to see available levels" % argv_[1])
    elif PshellServer.isSubString(argv_[0], "output", 2):
      if PshellServer.isSubString(argv_[1], "file", 2):
        if _gLogFileFd != None:
          _gTraceOutput = _TRACE_OUTPUT_FILE
        else:
          PshellServer.printf("ERROR: Need to set logfile before setting output to 'file', run 'trace output <filename>'")
      elif PshellServer.isSubString(argv_[1], "stdout", 2):
        _gTraceOutput = _TRACE_OUTPUT_STDOUT
      elif PshellServer.isSubString(argv_[1], "both", 2):
        if _gLogFileFd != None:
          _gTraceOutput = _TRACE_OUTPUT_BOTH
        else:
          PshellServer.printf("WARNING: Need to set logfile before setting output to 'both', run 'trace output <filename>', setting to 'stdout' only")
          _gTraceOutput = _TRACE_OUTPUT_STDOUT
      else:
        if not setLogFile(argv_[1]):
          if _gLogFileFd != None:
            PshellServer.printf("ERROR: Could not open logfile: %s, reverting to: %s" % (argv_[1], _logfileName))
          else:
            PshellServer.printf("ERROR: Could not open logfile: %s, reverting to: stdout" % argv_[1])
    elif PshellServer.isSubString(argv_[0], "format", 1):
      if PshellServer.isSubString(argv_[1], "on", 2):
        enableFormat(True)
      elif PshellServer.isSubString(argv_[1], "off", 2):
        enableFormat(False)
      else:
        PshellServer.showUsage()
    elif PshellServer.isSubString(argv_[0], "name", 1):
      if PshellServer.isSubString(argv_[1], "on", 2):
        enableLogName(True)
      elif PshellServer.isSubString(argv_[1], "off", 2):
        enableLogName(False)
      elif PshellServer.isSubString(argv_[1], "default", 2):
        setLogName("Trace")
      else:
        setLogName(argv_[1])
    elif PshellServer.isSubString(argv_[0], "colors", 1):
      if PshellServer.isSubString(argv_[1], "on", 2):
        _enableColors(True)
      elif PshellServer.isSubString(argv_[1], "off", 2):
        _enableColors(False)
      else:
        PshellServer.showUsage()
    elif PshellServer.isSubString(argv_[0], "timestamp", 1):
      if PshellServer.isSubString(argv_[1], "on", 2):
        enableTimestamp(True)
      elif PshellServer.isSubString(argv_[1], "off", 2):
        enableTimestamp(False)
      elif PshellServer.isSubString(argv_[1], "datetime", 1):
        enableFullDatetime(True)
      elif PshellServer.isSubString(argv_[1], "time", 1):
        enableFullDatetime(False)
      else:
        PshellServer.showUsage()
    else:
      PshellServer.showUsage()
  _gMutex.release()

#################################################################################
#################################################################################
def _formatTrace(level_, message_):
  global _gFormatEnabled
  global _gTimestampEnabled
  global _gLogNameEnabled
  message = ""
  if _gFormatEnabled:
    if _gLogNameEnabled:
      message = "%s | " % _gLogName
    message += "%-7s | " % level_
    if _gTimestampEnabled:
      message += "%s | " % datetime.datetime.now().strftime(_gTimestampFormat)
    message += message_
    return (message)
  else:
    return (message_)

#################################################################################
#################################################################################
def _outputTrace(level_, message_):
  global _gLogFileFd
  global _gTraceOutput
  global _gMutex
  _gMutex.acquire()
  message = _formatTrace(level_, message_)
  if _gTraceOutput == _TRACE_OUTPUT_STDOUT:
    print(message)
  elif _gTraceOutput == _TRACE_OUTPUT_FILE:
    message += "\n"
    _gLogFileFd.write(message)
    _gLogFileFd.flush()
  else:
    # output to both stdout & the specified file
    print(message)
    message += "\n"
    _gLogFileFd.write(message)
    _gLogFileFd.flush()
  _gMutex.release()

#################################################################################
#################################################################################
def _pshellLogFunction(outputString_):
  if outputString_[0] != '\n':
    FORCE(outputString_)
