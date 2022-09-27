/*******************************************************************************
 *
 * Copyright (c) 2009, Ron Iovine, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Ron Iovine nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Ron Iovine ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL Ron Iovine BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************/

#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include <PshellServer.h>
#include <TraceLog.h>

/* constants */

#define MAX_STRING_SIZE 512
#define MAX_USER_MESSAGE_SIZE 4096
#define MAX_OUTPUT_MESSAGE_SIZE MAX_USER_MESSAGE_SIZE+MAX_STRING_SIZE

#define TRACE_OUTPUT_FILE   0x0001
#define TRACE_OUTPUT_STDOUT 0x0002
#define TRACE_OUTPUT_CUSTOM 0x0004
#define TRACE_OUTPUT_ALL    (TRACE_OUTPUT_FILE | TRACE_OUTPUT_STDOUT | TRACE_OUTPUT_CUSTOM)

#define TRACE_USE_COLORS
#ifdef TRACE_USE_COLORS
#if 0   /* comment out the unused colors so we don't get compiler warnings */
static const char *GREEN     = "\33[1;32m";
static const char *BLUE      = "\33[1;34m";
static const char *YELLOW    = "\33[1;33m";
static const char *CYAN      = "\33[1;36m";
static const char *MAGENTA   = "\33[1;35m";
static const char *BOLD      = "\33[1m";
static const char *UNDERLINE = "\33[4m";
static const char *BLINK     = "\33[5m";
static const char *INVERSE   = "\33[7m";
#endif
static const char *RED    = "\33[1;31m";
static const char *GREEN  = "\33[1;32m";
static const char *NORMAL = "\33[0m";
static const char *ON     = "\33[1;32mON\33[0m";     /* GREEN */
static const char *OFF    = "\33[1;31mOFF\33[0m";    /* RED */
static const char *NONE   = "\33[1;36mNONE\33[0m";   /* CYAN */
#else
static const char *RED    = "";
static const char *GREEN  = "";
static const char *NORMAL = "";
static const char *ON     = "ON";
static const char *OFF    = "OFF";
static const char *NONE   = "NONE";
#endif

/*
 * coding convention is leading underscore for global data,
 * trailing underscore for function arguments, and no leading
 * or trailing underscores for local stack variables
 */

/*************************
 * public "member" data
 *************************/

unsigned trace_logLevel = TL_DEFAULT;
bool trace_logEnabled = true;

/*************************
 * private "member" data
 *************************/

static TraceOutputFunction _outputFunction = NULL;
static TraceFormatFunction _formatFunction = NULL;
static bool _customFormatEnabled = false;
static char _logName[MAX_STRING_SIZE] = {"Trace"};
static bool _logNameEnabled = true;
static unsigned _traceOutput = TRACE_OUTPUT_STDOUT;
static char _outputString[MAX_OUTPUT_MESSAGE_SIZE];
static char _userMessage[MAX_USER_MESSAGE_SIZE];
static char _timestamp[MAX_STRING_SIZE];
static const char *_timestampFormat = "%T";
static const char *_defaultTimestampFormat = "%T";
static const char *_customTimestampFormat = NULL;
static bool _timestampAddUsec = true;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static bool _printLocation = true;
static bool _printPath = false;
static bool _printTimestamp = true;
static unsigned _maxLevelLength = 0;
static char _logfileName[MAX_STRING_SIZE] = {0};
static FILE *_logfileFd = NULL;
static bool _formatEnabled = true;
static bool _fullDatetime = false;
static bool _useColors = true;
static unsigned _defaultLogLevel = TL_DEFAULT;

/***************************************
 * private "member" function prototypes
 ***************************************/

static const char *getTimestamp(void);
static void printLine(char *line_);
static void formatTrace(const char *name_,
                        const char *level_,
                        const char *file_,
                        const char *function_,
                        int line_,
                        const char *timestamp_,
                        const char *userMessage_,
                        char *outputString_);
static const char *getLevelName(unsigned level_);
/* pshell trace configuration function */
static void configureTrace(int argc_, char *argv_[]);
static void pshellLogFunction(const char *outputString_);
static void enableColors(bool enable_);
static bool isColorsEnabled(void);
static void setCustomFormat(bool custom_);

/**************************************
 * public API "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void trace_init(const char *logname_,
                const char *logfile_,
                unsigned loglevel_,
                TraceOutputFunction outputFunction_,
                TraceFormatFunction formatFunction_,
                const char *timestampFormat_,
                bool traceFilter_)
{
  trace_registerLevels();
  trace_setLogName(logname_);
  trace_setLogLevel(loglevel_);
  trace_setDefaultLogLevel(loglevel_);
  trace_setTimestampFormat(timestampFormat_);
  trace_setLogfile(logfile_);
  trace_registerOutputFunction(outputFunction_);
  trace_registerFormatFunction(formatFunction_);
  pshell_registerServerLogFunction(pshellLogFunction);
  if (!traceFilter_)
  {
    /* create PSHELL command to configure trace settings */
    pshell_addCommand(configureTrace,
                      "trace",
                      "configure/display various trace logger settings",
                      "on | off | show |\n"
                      "             output {file | stdout | custom | all | <filename>} |\n"
                      "             level {all | default | <value>} |\n"
                      "             default {all | <value>} |\n"
                      "             format {on | off | custom | default} |\n"
                      "             name {on | off | default | <value>} |\n"
                      "             location {on | off} |\n"
                      "             timestamp {on | off | datetime | time  | custom | default} |\n"
                      "             colors {on | off}",
                      1,
                      3,
                      false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool trace_setLogfile(const char *filename_)
{
  FILE *newLogfileFd = NULL;
  if (filename_ != NULL)
  {
    if ((newLogfileFd = fopen(filename_, "a+")) != NULL)
    {
      if (_logfileFd != NULL)
      {
        fclose(_logfileFd);
      }
      _logfileFd = newLogfileFd;
      strcpy(_logfileName, filename_);
      _traceOutput = TRACE_OUTPUT_FILE;
      return (true);
    }
    else if (_logfileFd != NULL)
    {
      // could not open new logfile, but we already have a previous one,
      // just stick with that one
      return (false);
    }
    else
    {
      // could not open new logfile and no old one open, disable the output to a file
      _traceOutput &= ~TRACE_OUTPUT_FILE;
      return (false);
    }
  }
  else
  {
    _logfileFd = NULL;
    _traceOutput &= ~TRACE_OUTPUT_FILE;
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
char *trace_getLogfile(void)
{
  return (_logfileName);
}

/******************************************************************************/
/******************************************************************************/
bool trace_isOutputStdout(void)
{
  return (_traceOutput & TRACE_OUTPUT_STDOUT);
}

/******************************************************************************/
/******************************************************************************/
bool trace_isOutputFile(void)
{
  return ((_traceOutput & TRACE_OUTPUT_FILE) && (_logfileFd != NULL));
}

/******************************************************************************/
/******************************************************************************/
bool trace_isOutputCustom(void)
{
  return ((_traceOutput & TRACE_OUTPUT_CUSTOM) && (_outputFunction != NULL));
}

/******************************************************************************/
/******************************************************************************/
bool trace_isOutputAll(void)
{
  return (trace_isOutputStdout() && trace_isOutputFile() && trace_isOutputCustom());
}

/******************************************************************************/
/******************************************************************************/
void trace_setOutput(char *location_, bool add_)
{
  if (pshell_isSubString(location_, "custom", 2))
  {
    if (_outputFunction != NULL)
    {
      if (add_)
      {
        // add to existing output setting
        _traceOutput |= TRACE_OUTPUT_CUSTOM;
      }
      else
      {
        // set to specified output setting
        _traceOutput = TRACE_OUTPUT_CUSTOM;
      }
    }
    else
    {
      pshell_printf("Cannot set custom log output, no custom log output function registered\n");
    }
  }
  else if (pshell_isSubString(location_, "file", 2))
  {
    if (_logfileFd != NULL)
    {
      if (add_)
      {
        // add to existing output setting
        _traceOutput |= TRACE_OUTPUT_FILE;
      }
      else
      {
        // set to specified output setting
        _traceOutput = TRACE_OUTPUT_FILE;
      }
    }
    else
    {
      pshell_printf("Need to set logfile before setting output to 'file', run 'trace output <filename>'\n");
    }
  }
  else if (pshell_isSubString(location_, "stdout", 2))
  {
    if (add_)
    {
      // add to existing output setting
      _traceOutput |= TRACE_OUTPUT_STDOUT;
    }
    else
    {
      // set to specified output setting
      _traceOutput = TRACE_OUTPUT_STDOUT;
    }
  }
  else if (pshell_isSubString(location_, "all", 2))
  {
    _traceOutput = TRACE_OUTPUT_ALL;
    if (_logfileFd == NULL)
    {
      _traceOutput &= ~TRACE_OUTPUT_FILE;
    }
    else if (_outputFunction == NULL)
    {
      _traceOutput &= ~TRACE_OUTPUT_CUSTOM;
    }
  }
  else
  {
    if (!trace_setLogfile(location_))
    {
      if (_logfileFd != NULL)
      {
        pshell_printf("Could not open logfile: %s, reverting to: %s\n", location_, _logfileName);
      }
      else
      {
        pshell_printf("Could not open logfile: %s, reverting to: stdout\n", location_);
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_setLogLevel(unsigned logLevel_)
{
  trace_logLevel = logLevel_;
}

/******************************************************************************/
/******************************************************************************/
unsigned trace_getLogLevel(void)
{
  return (trace_logLevel);
}

/******************************************************************************/
/******************************************************************************/
void trace_registerOutputFunction(TraceOutputFunction outputFunction_)
{
  if (outputFunction_ != NULL)
  {
    _outputFunction = outputFunction_;
    _traceOutput = TRACE_OUTPUT_CUSTOM;
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_registerFormatFunction(TraceFormatFunction formatFunction_)
{
  if (formatFunction_ != NULL)
  {
    _formatFunction = formatFunction_;
    _customFormatEnabled = true;
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_setLogName(const char *logname_)
{
  if ((logname_ != NULL) && (strlen(logname_) > 0))
  {
    _logName[0] = '\0';
    sprintf(_logName, "%s", logname_);
  }
}

/******************************************************************************/
/******************************************************************************/
const char *trace_getLogName(void)
{
  return (_logName);
}

/******************************************************************************/
/******************************************************************************/
void trace_enableLocation(bool enable_)
{
  _printLocation = enable_;
}

/******************************************************************************/
/******************************************************************************/
bool trace_isLocationEnabled(void)
{
  return (_printLocation);
}

/******************************************************************************/
/******************************************************************************/
void trace_enablePath(bool enable_)
{
  _printPath = enable_;
}

/******************************************************************************/
/******************************************************************************/
bool trace_isPathEnabled(void)
{
  return (_printPath);
}

/******************************************************************************/
/******************************************************************************/
void trace_enableTimestamp(bool enable_)
{
  _printTimestamp = enable_;
}

/******************************************************************************/
/******************************************************************************/
bool trace_isTimestampEnabled(void)
{
  return (_printTimestamp);
}

/******************************************************************************/
/******************************************************************************/
void trace_enableLogName(bool enable_)
{
  _logNameEnabled = enable_;
}

/******************************************************************************/
/******************************************************************************/
bool trace_isLogNameEnabled(void)
{
  return (_logNameEnabled);
}

/******************************************************************************/
/******************************************************************************/
void trace_enableFormat(bool enable_)
{
  _formatEnabled = enable_;
}

/******************************************************************************/
/******************************************************************************/
bool trace_isFormatEnabled(void)
{
  return (_formatEnabled);
}

/******************************************************************************/
/******************************************************************************/
void trace_enableLog(bool enable_)
{
  trace_logEnabled = enable_;
}

/******************************************************************************/
/******************************************************************************/
bool trace_isLogEnabled(void)
{
  return (trace_logEnabled);
}

/******************************************************************************/
/******************************************************************************/
void trace_enableFullDatetime(bool enable_)
{
  if (_customTimestampFormat == NULL)
  {
    if ((_fullDatetime = enable_) == true)
    {
      _timestampFormat = "%Y-%m-%d %T";
    }
    else
    {
      _timestampFormat = "%T";
    }
  }
}

/******************************************************************************/
/******************************************************************************/
bool trace_isFullDatetimeEnabed(void)
{
  return (_fullDatetime);
}

/******************************************************************************/
/******************************************************************************/
void trace_setDefaultLogLevel(unsigned level_)
{
  _defaultLogLevel = level_;
}

/******************************************************************************/
/******************************************************************************/
void trace_addUserLevel(const char *levelName_, unsigned levelValue_)
{
  /*
   * call this function so we can keep track of our max level name string
   * length so our trace display can be formatted and aligned correctly
   */
  if (strlen(levelName_) > _maxLevelLength)
  {
    _maxLevelLength = strlen(levelName_);
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_registerLevels(void)
{
  /*
   * register all our trace levels with the trace logger service
   * format of call is "name", level
   */
  trace_addUserLevel(TL_ERROR_STRING, TL_ERROR);
  trace_addUserLevel(TL_WARNING_STRING, TL_WARNING);
  trace_addUserLevel(TL_FAILURE_STRING, TL_FAILURE);
  trace_addUserLevel(TL_INFO_STRING, TL_INFO);
  trace_addUserLevel(TL_DEBUG_STRING, TL_DEBUG);
  trace_addUserLevel(TL_ENTER_STRING, TL_ENTER);
  trace_addUserLevel(TL_EXIT_STRING, TL_EXIT);
  trace_addUserLevel(TL_DUMP_STRING, TL_DUMP);
#ifdef DYNAMIC_TRACE_FILTER
  tf_addLevel(TL_ERROR_STRING, TL_ERROR, true, false);
  tf_addLevel(TL_WARNING_STRING, TL_WARNING, true, true);
  tf_addLevel(TL_FAILURE_STRING, TL_FAILURE, true, true);
  tf_addLevel(TL_INFO_STRING, TL_INFO, false, true);
  tf_addLevel(TL_DEBUG_STRING, TL_DEBUG, false, true);
  tf_addLevel(TL_ENTER_STRING, TL_ENTER, false, true);
  tf_addLevel(TL_EXIT_STRING, TL_EXIT, false, true);
  tf_addLevel(TL_DUMP_STRING, TL_DUMP, false, true);
#endif
}

/******************************************************************************/
/******************************************************************************/
void trace_setTimestampFormat(const char *format_, bool addUsec_)
{
  if (format_ != NULL)
  {
    _customTimestampFormat = format_;
    _timestampFormat = _customTimestampFormat;
    _timestampAddUsec = addUsec_;
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_setCustomTimestamp(bool custom_)
{
  if (custom_)
  {
    if (_customTimestampFormat != NULL)
    {
      _timestampFormat = _customTimestampFormat;
    }
  }
  else
  {
    _timestampFormat = _defaultTimestampFormat;
  }
}

/******************************************************************************/
/******************************************************************************/
bool trace_isCustomTimestamp(void)
{
  return (_timestampFormat == _customTimestampFormat);
}

/******************************************************************************/
/******************************************************************************/
void trace_outputLog(const char *level_,
                     const char *file_,
                     const char *function_,
                     int line_,
                     const char *format_, ...)
{
  pthread_mutex_lock(&_mutex);

  // format the user message
  _userMessage[0] = 0;
  va_list args;
  va_start(args, format_);
  vsnprintf(_userMessage, sizeof(_userMessage), format_, args);
  va_end(args);

  // format the trace
  formatTrace(_logName, level_, file_, function_, line_, getTimestamp(), _userMessage, _outputString);

  // output the trace
  printLine(_outputString);

  pthread_mutex_unlock(&_mutex);
}

/******************************************************************************/
/******************************************************************************/
void trace_outputDump(const void *address_,
                      unsigned length_,
                      const char *level_,
                      const char *file_,
                      const char *function_,
                      int line_,
                      const char *format_, ...)
{
  pthread_mutex_lock(&_mutex);

  char asciiLine[80];
  const unsigned bytesPerLine = 16;
  unsigned char *bytes = (unsigned char *)address_;
  unsigned short offset = 0;

  // format the user message
  _userMessage[0] = 0;
  va_list args;
  va_start(args, format_);
  vsnprintf(_userMessage, sizeof(_userMessage), format_, args);
  va_end(args);

  // format the trace
  formatTrace(_logName, level_, file_, function_, line_, getTimestamp(), _userMessage, _outputString);

  // output the trace
  printLine(_outputString);

  // output our hex dump and ascii equivalent
  asciiLine[0] = 0;
  _outputString[0] = 0;
  for (unsigned i = 0; i < length_; i++)
  {
    // see if we are on a full line boundry
    if ((i%bytesPerLine) == 0)
    {
      // see if we need to print out our line
      if (i > 0)
      {
        /* done with this line, add the asciii data & print it */
        sprintf(&_outputString[strlen(_outputString)], "  %s\n", asciiLine);
        printLine(_outputString);
        // line  printed, clear them it for next time
        asciiLine[0] = 0;
        _outputString[0] = 0;
      }
      // format our our offset
      sprintf(&_outputString[strlen(_outputString)], "  %04x  ", offset);
      offset += bytesPerLine;
    }
    // create our line of ascii data, for non-printable ascii characters just use a "."
    sprintf(&asciiLine[strlen(asciiLine)], "%c", ((isprint(bytes[i]) && (bytes[i] < 128)) ? bytes[i] : '.'));
    // format our line of hex byte
    sprintf(&_outputString[strlen(_outputString)], "%02x ", bytes[i]);
  }
  // done, format & print the final line & ascii data
  sprintf(&_outputString[strlen(_outputString)], "  %*s\n", (int)(((bytesPerLine-strlen(asciiLine))*3)+strlen(asciiLine)), asciiLine);
  printLine(_outputString);

  pthread_mutex_unlock(&_mutex);
}

/**************************************
 * private "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void pshellLogFunction(const char *outputString_)
{
  if (outputString_[0] != '\n')
  {
    TRACE_FORCE(outputString_);
  }
}

/******************************************************************************/
/******************************************************************************/
void enableColors(bool enable_)
{
  if ((_useColors = enable_) == true)
  {
    RED    = "\33[1;31m";
    GREEN  = "\33[1;32m";
    NORMAL = "\33[0m";
    ON     = "\33[1;32mON\33[0m";     /* GREEN */
    OFF    = "\33[1;31mOFF\33[0m";    /* RED */
    NONE   = "\33[1;36mNONE\33[0m";   /* CYAN */
  }
  else
  {
    RED    = "";
    GREEN  = "";
    NORMAL = "";
    ON     = "ON";
    OFF    = "OFF";
    NONE   = "NONE";
  }
}

/******************************************************************************/
/******************************************************************************/
bool isColorsEnabled(void)
{
  return (_useColors);
}

/******************************************************************************/
/******************************************************************************/
void setCustomFormat(bool custom_)
{
  if (custom_)
  {
    if (_formatFunction != NULL)
    {
      _customFormatEnabled = true;
    }
  }
  else
  {
    _customFormatEnabled = false;
  }
}

/******************************************************************************/
/******************************************************************************/
const char *getTimestamp(void)
{
  struct timeval tv;
  struct tm tm;

  // get timestamp
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tm);
  strftime(_timestamp, sizeof(_timestamp), _timestampFormat, &tm);
  // add the microseconds if requested
  if (_timestampAddUsec)
  {
    sprintf(_timestamp, "%s.%-6ld", _timestamp, (long)tv.tv_usec);
  }
  return (_timestamp);
}

/******************************************************************************/
/******************************************************************************/
void formatTrace(const char *name_,
                 const char *level_,
                 const char *file_,
                 const char *function_,
                 int line_,
                 const char *timestamp_,
                 const char *userMessage_,
                 char *outputString_)
{
  const char *file;

  _outputString[0] = 0;
  if (trace_isFormatEnabled())
  {
    // strip off any leading path of filename if the path is disabled
    if (!trace_isPathEnabled() && ((file = strrchr(file_, '/')) != NULL))
    {
      file++;
    }
    else
    {
      file = file_;
    }

    if ((_formatFunction != NULL) && (_customFormatEnabled))
    {
      // custom format function registered, call it
      (*_formatFunction)(name_, level_, file, function_, line_, timestamp_, userMessage_, outputString_);
    }
    else
    {
      // standard output format, see what items are enabled, each item is separated by a '|'

      // add the prefix if enabled, this is usually the program name
      if (trace_isLogNameEnabled())
      {
        sprintf(&_outputString[strlen(_outputString)], "%s | ", name_);
      }

      // add the level
      sprintf(&_outputString[strlen(_outputString)], "%-*s | ", _maxLevelLength, level_);

      // add any timestamp if enabled
      if (trace_isTimestampEnabled())
      {
        sprintf(&_outputString[strlen(_outputString)], "%s | ", timestamp_);
      }

      // add any location if enabled
      if (trace_isLocationEnabled())
      {
        sprintf(&_outputString[strlen(_outputString)], "%s(%s):%d | ", file, function_, line_);
      }

      // add the user message
      snprintf(&_outputString[strlen(_outputString)], sizeof(_outputString)-strlen(_outputString), "%s\n", userMessage_);
    }
  }
  else
  {
    // format not enabled, just add the user message
    snprintf(&_outputString[strlen(_outputString)], sizeof(_outputString)-strlen(_outputString), "%s\n", userMessage_);
  }

}

/******************************************************************************/
/******************************************************************************/
void printLine(char *line_)
{
  if (trace_isOutputCustom())
  {
    // custom output function registered, call it
    (*_outputFunction)(line_);
  }
  if (trace_isOutputFile())
  {
    fprintf(_logfileFd, "%s", line_);
    fflush(_logfileFd);
  }
  if (trace_isOutputStdout())
  {
    printf("%s", line_);
  }
}


/******************************************************************************/
/******************************************************************************/
const char *getLevelName(unsigned level_)
{
  switch (level_)
  {
    case TL_ERROR:
      return ("ERROR");
    case TL_FAILURE:
      return ("FAILURE");
    case TL_WARNING:
      return ("WARNING");
    case TL_INFO:
      return ("INFO");
    case TL_DEBUG:
      return ("DEBUG");
    case TL_ENTER:
      return ("ENTER");
    case TL_EXIT:
      return ("EXIT");
    case TL_DUMP:
      return ("DUMP");
    default:
      return ("UNKNOWN");
  }
}

/******************************************************************************/
/******************************************************************************/
void configureTrace(int argc_, char *argv_[])
{
  pthread_mutex_lock(&_mutex);
  if (pshell_isHelp())
  {
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("  where:\n");
    pshell_printf("    output    - set trace log output location\n");
    pshell_printf("    level     - set current trace log level\n");
    pshell_printf("    default   - set default trace log level\n");
    pshell_printf("    format    - enable/disable all trace header formatting\n");
    pshell_printf("    name      - enable/disable/set trace name prefix format item\n");
    pshell_printf("    location  - enable/disable trace file/function/line format item\n");
    pshell_printf("    timestamp - enable/disable/set trace timestamp and format item\n");
    pshell_printf("    colors    - enable/disable colors in 'trace show' command\n");
    pshell_printf("\n");
    pshell_printf("  NOTE: Setting 'trace off' will disable all trace outputs including\n");
    pshell_printf("        TRACE_ERROR except for any TRACE_FORCE statements, setting\n");
    pshell_printf("        back to 'trace on' will restore the previous level settings\n");
    pshell_printf("\n");
    pshell_printf("        The '*' marker next to a trace level in the 'trace show' command\n");
    pshell_printf("        indicates it's a current default level\n");
    pshell_printf("\n");
    pshell_printf("        If using colors for the 'trace show' display.  A green color for the\n");
    pshell_printf("        level indicates currently enabled, a red color indicates disabled\n");
    pshell_printf("\n");
  }
  else if (argc_ == 1)
  {
    if (pshell_isSubString(argv_[0], "on", 2))
    {
      trace_enableLog(true);
    }
    else if (pshell_isSubString(argv_[0], "off", 2))
    {
      trace_enableLog(false);
    }
    else if (pshell_isSubString(argv_[0], "show", 1))
    {
      pshell_printf("\n");
      pshell_printf("**********************************\n");
      pshell_printf("*   TRACE LOGGER CONFIGURATION   *\n");
      pshell_printf("**********************************\n");
      pshell_printf("\n");
      pshell_printf("Trace enabled.......: %s\n", (trace_isLogEnabled() ? ON : OFF));
      if (trace_isOutputAll())
      {
        pshell_printf("Trace output........: stdout\n");
        pshell_printf("                    : custom\n");
        pshell_printf("                    : %s\n", trace_getLogfile());
      }
      else if (trace_isOutputStdout() && trace_isOutputCustom())
      {
        pshell_printf("Trace output........: stdout\n");
        pshell_printf("                    : custom\n");
      }
      else if (trace_isOutputStdout() && trace_isOutputFile())
      {
        pshell_printf("Trace output........: stdout\n");
        pshell_printf("                    : %s\n", trace_getLogfile());
      }
      else if (trace_isOutputCustom() && trace_isOutputFile())
      {
        pshell_printf("Trace output........: custom\n");
        pshell_printf("                    : %s\n", trace_getLogfile());
      }
      else if (trace_isOutputCustom())
      {
        pshell_printf("Trace output........: custom\n");
      }
      else if (trace_isOutputStdout())
      {
        pshell_printf("Trace output........: stdout\n");
      }
      else if (trace_isOutputFile())
      {
        pshell_printf("Trace output........: %s\n", trace_getLogfile());
      }
      pshell_printf("Trace format........: %s (%s)\n", (trace_isFormatEnabled() ? ON : OFF), (_customFormatEnabled ? "custom" : "default"));
      pshell_printf("  Location..........: %s\n", (trace_isLocationEnabled() ? ON : OFF));
      pshell_printf("  Name..............: %s\n", (trace_isLogNameEnabled() ? ON : OFF));
      if (trace_isCustomTimestamp())
      {
        pshell_printf("  Timestamp.........: %s (custom)\n", (trace_isTimestampEnabled() ? ON : OFF));
      }
      else
      {
        pshell_printf("  Timestamp.........: %s (%s)\n", (trace_isTimestampEnabled() ? ON : OFF), (trace_isFullDatetimeEnabed() ? "date & time" : "time only"));
      }
      pshell_printf("Trace level(s)......: %s%s*%s\n", GREEN, getLevelName(TL_ERROR), NORMAL);
      const char *defaultMarker = "";
      for (unsigned level = 1; level <= TL_MAX; level++)
      {
        (level <= _defaultLogLevel) ? defaultMarker = "*" : defaultMarker = "";
        if (level <= trace_getLogLevel())
        {
          pshell_printf("                    : %s%s%s%s\n", GREEN, getLevelName(level), defaultMarker, NORMAL);
        }
        else
        {
          if (isColorsEnabled())
          {
            pshell_printf("                    : %s%s%s%s\n", RED, getLevelName(level), defaultMarker, NORMAL);
          }
          else
          {
            char noColorString[40];
            sprintf(noColorString, "%s%s", getLevelName(level), defaultMarker);
            pshell_printf("                    : %-*s (disabled)\n", _maxLevelLength+1, noColorString);
          }
        }
      }
      pshell_printf("\n");
    }
    else
    {
      pshell_showUsage();
    }
  }
  else if (pshell_isSubString(argv_[0], "output", 2))
  {
    if (argc_ == 2)
    {
      trace_setOutput(argv_[1]);
    }
    else
    {
      _traceOutput = 0;
      trace_setOutput(argv_[1], true);
      trace_setOutput(argv_[2], true);
    }
  }
  else if (argc_ == 2)
  {
    if (pshell_isSubString(argv_[0], "level", 2))
    {
      if (pshell_isSubString(argv_[1], "all", 2))
      {
        trace_setLogLevel(TL_ALL);
      }
      else if (pshell_isSubString(argv_[1], "default", 2))
      {
        trace_setLogLevel(_defaultLogLevel);
      }
      else if (pshell_isSubString(argv_[1], "error", 2))
      {
        trace_setLogLevel(TL_ERROR);
      }
      else if (pshell_isSubString(argv_[1], "failure", 2))
      {
        trace_setLogLevel(TL_FAILURE);
      }
      else if (pshell_isSubString(argv_[1], "warning", 2))
      {
        trace_setLogLevel(TL_WARNING);
      }
      else if (pshell_isSubString(argv_[1], "info", 2))
      {
        trace_setLogLevel(TL_INFO);
      }
      else if (pshell_isSubString(argv_[1], "debug", 2))
      {
        trace_setLogLevel(TL_DEBUG);
      }
      else if (pshell_isSubString(argv_[1], "enter", 2))
      {
        trace_setLogLevel(TL_ENTER);
      }
      else if (pshell_isSubString(argv_[1], "exit", 2))
      {
        trace_setLogLevel(TL_EXIT);
      }
      else if (pshell_isSubString(argv_[1], "dump", 2))
      {
        trace_setLogLevel(TL_DUMP);
      }
      else
      {
        pshell_printf("ERROR: Invalid log level: %s, run 'trace show' to see available levels\n", argv_[1]);
      }
    }
    else if (pshell_isSubString(argv_[0], "default", 2))
    {
      if (pshell_isSubString(argv_[1], "all", 2))
      {
        trace_setDefaultLogLevel(TL_ALL);
      }
      else if (pshell_isSubString(argv_[1], "error", 2))
      {
        trace_setDefaultLogLevel(TL_ERROR);
      }
      else if (pshell_isSubString(argv_[1], "failure", 2))
      {
        trace_setDefaultLogLevel(TL_FAILURE);
      }
      else if (pshell_isSubString(argv_[1], "warning", 2))
      {
        trace_setDefaultLogLevel(TL_WARNING);
      }
      else if (pshell_isSubString(argv_[1], "info", 2))
      {
        trace_setDefaultLogLevel(TL_INFO);
      }
      else if (pshell_isSubString(argv_[1], "debug", 2))
      {
        trace_setDefaultLogLevel(TL_DEBUG);
      }
      else if (pshell_isSubString(argv_[1], "enter", 2))
      {
        trace_setDefaultLogLevel(TL_ENTER);
      }
      else if (pshell_isSubString(argv_[1], "exit", 2))
      {
        trace_setDefaultLogLevel(TL_EXIT);
      }
      else if (pshell_isSubString(argv_[1], "dump", 2))
      {
        trace_setDefaultLogLevel(TL_DUMP);
      }
      else
      {
        pshell_printf("ERROR: Invalid log level: %s, run 'trace show' to see available levels\n", argv_[1]);
      }
    }
    else if (pshell_isSubString(argv_[0], "location", 2))
    {
      if (pshell_isSubString(argv_[1], "on", 2))
      {
        trace_enableLocation(true);
      }
      else if (pshell_isSubString(argv_[1], "off", 2))
      {
        trace_enableLocation(false);
      }
      else
      {
        pshell_showUsage();
      }
    }
    else if (pshell_isSubString(argv_[0], "format", 1))
    {
      if (pshell_isSubString(argv_[1], "on", 2))
      {
        trace_enableFormat(true);
      }
      else if (pshell_isSubString(argv_[1], "off", 2))
      {
        trace_enableFormat(false);
      }
      else if (pshell_isSubString(argv_[1], "custom", 2))
      {
        if (_formatFunction != NULL)
        {
          setCustomFormat(true);
        }
        else
        {
          pshell_printf("ERROR: Custom format function not registered\n");
        }
      }
      else if (pshell_isSubString(argv_[1], "default", 2))
      {
        setCustomFormat(false);
      }
      else
      {
        pshell_showUsage();
      }
    }
    else if (pshell_isSubString(argv_[0], "name", 1))
    {
      if (pshell_isSubString(argv_[1], "on", 2))
      {
        trace_enableLogName(true);
      }
      else if (pshell_isSubString(argv_[1], "off", 2))
      {
        trace_enableLogName(false);
      }
      else if (pshell_isSubString(argv_[1], "default", 2))
      {
        trace_setLogName("Trace");
      }
      else
      {
        trace_setLogName(argv_[1]);
      }
    }
    else if (pshell_isSubString(argv_[0], "colors", 1))
    {
      if (pshell_isSubString(argv_[1], "on", 2))
      {
        enableColors(true);
      }
      else if (pshell_isSubString(argv_[1], "off", 2))
      {
        enableColors(false);
      }
      else
      {
        pshell_showUsage();
      }
    }
    else if (pshell_isSubString(argv_[0], "timestamp", 1))
    {
      if (pshell_isSubString(argv_[1], "on", 2))
      {
        trace_enableTimestamp(true);
      }
      else if (pshell_isSubString(argv_[1], "off", 2))
      {
        trace_enableTimestamp(false);
      }
      else if (pshell_isSubString(argv_[1], "datetime", 1))
      {
        if (_customTimestampFormat == NULL)
        {
          trace_enableFullDatetime(true);
        }
        else
        {
          pshell_printf("ERROR: Cannot change timestamp type of custom format\n");
        }
      }
      else if (pshell_isSubString(argv_[1], "time", 1))
      {
        if (_customTimestampFormat == NULL)
        {
          trace_enableFullDatetime(false);
        }
        else
        {
          pshell_printf("ERROR: Cannot change timestamp type of custom format\n");
        }
      }
      else if (pshell_isSubString(argv_[1], "custom", 2))
      {
        if (_customTimestampFormat != NULL)
        {
          trace_setCustomTimestamp(true);
        }
        else
        {
          pshell_printf("ERROR: Custom timestamp format not registered\n");
        }
      }
      else if (pshell_isSubString(argv_[1], "default", 2))
      {
        trace_setCustomTimestamp(false);
      }
      else
      {
        pshell_showUsage();
      }
    }
    else
    {
      pshell_showUsage();
    }
  }
  else
  {
    pshell_showUsage();
  }
  pthread_mutex_unlock(&_mutex);
}
