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

#include <TraceLog.h>

/* constants */

#define MAX_STRING_SIZE 512

/*
 * coding convention is leading underscore for global data,
 * trailing underscore for function arguments, and no leading
 * or trailing underscores for local stack variables
 */

/*************************
 * public "member" data
 *************************/

/*
 * note that this data item is only used for a stand-alone TraceLog module, i.e.
 * it is not part of the dynamic trace filtering mechanism, i.e. this module is
 * built with the DYNAMIC_TRACE_FILTER flag NOT set, the levels can  be set from
 * within the program by making calls to the trace_setLogLevel function
 */
#ifndef DYNAMIC_TRACE_FILTER
unsigned _traceLogLevel = TL_DEFAULT_LEVEL;
#endif

/*************************
 * private "member" data
 *************************/

static TraceOutputFunction _outputFunction = NULL;
static TraceFormatFunction _formatFunction = NULL;
static char _logName[MAX_STRING_SIZE] = {"Trace"};
static bool _logNameEnabled = true;
static char _outputString[MAX_STRING_SIZE];
static char _userMessage[MAX_STRING_SIZE];
static char _timestamp[MAX_STRING_SIZE];
static const char *_timestampFormat = NULL;
static bool _timestampAddUsec = true;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static bool _printLocation = true;
static bool _printPath = false;
static bool _printTimestamp = true;
static unsigned _maxLevelLength = 0;

/***************************************
 * private "member" function prototypes
 ***************************************/

static const char *getTimestamp(void);
static void printLine(char *line_);
static void formatTrace(const char *level_,
                        const char *file_,
                        const char *function_,
                        int line_,
                        const char *timestamp_,
                        const char *userMessage_,
                        char *outputString_);

/**************************************
 * public API "member" function bodies
 **************************************/

#ifndef DYNAMIC_TRACE_FILTER

/******************************************************************************/
/******************************************************************************/
void trace_setLogLevel(unsigned _logLevel)
{
  _traceLogLevel = _logLevel;
}

/******************************************************************************/
/******************************************************************************/
unsigned trace_getLogLevel(void)
{
  return (_traceLogLevel);
}

#endif

/******************************************************************************/
/******************************************************************************/
void trace_registerOutputFunction(TraceOutputFunction outputFunction_)
{
  if (outputFunction_ != NULL)
  {
    _outputFunction = outputFunction_;
  }
  else
  {
    TRACE_ERROR("NULL outputFunction, not registered");
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_registerFormatFunction(TraceFormatFunction formatFunction_)
{
  if (formatFunction_ != NULL)
  {
    _formatFunction = formatFunction_;
  }
  else
  {
    TRACE_ERROR("NULL formatFunction, not registered");
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_setLogName(const char *name_)
{
  if ((name_ != NULL) && (strlen(name_) > 0))
  {
    sprintf(_logName, "%s", name_);
  }
  else
  {
    _logName[0] = '\0';
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
void trace_enablePath(bool enable_)
{
  _printPath = enable_;
}

/******************************************************************************/
/******************************************************************************/
void trace_enableTimestamp(bool enable_)
{
  _printTimestamp = enable_;
}

/******************************************************************************/
/******************************************************************************/
bool trace_isLocationEnabled(void)
{
  return (_printLocation);
}

/******************************************************************************/
/******************************************************************************/
bool trace_isTimestampEnabled(void)
{
  return (_printTimestamp);
}

/******************************************************************************/
/******************************************************************************/
bool trace_isPathEnabled(void)
{
  return (_printPath);
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
void trace_addUserLevel(const char *levelName_, unsigned levelValue_, bool isDefault_, bool isMaskable_)
{
  /*
   * call this function instead of 'tf_addLevel' directly so we can keep
   * track of our max level name string length so our trace display can
   * be formatted and aligned correctly
   */
  if (strlen(levelName_) > _maxLevelLength)
  {
    _maxLevelLength = strlen(levelName_);
  }
#ifdef DYNAMIC_TRACE_FILTER
  tf_addLevel(levelName_, levelValue_, isDefault_, isMaskable_);
#endif
}

/******************************************************************************/
/******************************************************************************/
void trace_registerLevels(void)
{
  /*
   * register all our trace levels with the trace filter service
   * format of call is "name", level, isDefault, isMaskable
   */
  trace_addUserLevel(TL_ERROR_STRING, TL_ERROR, true, false);
  trace_addUserLevel(TL_WARNING_STRING, TL_WARNING, true, true);
  trace_addUserLevel(TL_FAILURE_STRING, TL_FAILURE, true, true);
  trace_addUserLevel(TL_INFO_STRING, TL_INFO, false, true);
  trace_addUserLevel(TL_DEBUG_STRING, TL_DEBUG, false, true);
  trace_addUserLevel(TL_ENTER_STRING, TL_ENTER, false, true);
  trace_addUserLevel(TL_EXIT_STRING, TL_EXIT, false, true);
  trace_addUserLevel(TL_DUMP_STRING, TL_DUMP, false, true);
}

/******************************************************************************/
/******************************************************************************/
void trace_setTimestampFormat(const char *format_, bool addUsec_)
{
  _timestampFormat = format_;
  _timestampAddUsec = addUsec_;
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
  vsprintf(&_userMessage[strlen(_userMessage)], format_, args);
  va_end(args);

  // format the trace
  formatTrace(level_, file_, function_, line_, getTimestamp(), _userMessage, _outputString);

  // output the trace
  printLine(_outputString);

  pthread_mutex_unlock(&_mutex);
}

/******************************************************************************/
/******************************************************************************/
void trace_outputDump(void *address_,
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
  vsprintf(&_userMessage[strlen(_userMessage)], format_, args);
  va_end(args);

  // format the trace
  formatTrace(level_, file_, function_, line_, getTimestamp(), _userMessage, _outputString);

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
const char *getTimestamp(void)
{
  struct timeval tv;
  struct tm tm;

  // get timestamp
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tm);
  if (_timestampFormat == NULL)
  {
    strftime(_timestamp, sizeof(_timestamp), "%T", &tm);
  }
  else
  {
    strftime(_timestamp, sizeof(_timestamp), _timestampFormat, &tm);
  }
  // add the microseconds is requested
  if (_timestampAddUsec)
  {
    sprintf(_timestamp, "%s.%-6ld", _timestamp, (long)tv.tv_usec);
  }
  return (_timestamp);
}

/******************************************************************************/
/******************************************************************************/
void formatTrace(const char *level_,
                 const char *file_,
                 const char *function_,
                 int line_,
                 const char *timestamp_,
                 const char *userMessage_,
                 char *outputString_)
{
  const char *file;

  // strip off any leading path of filename if the path is disabled
  if (!trace_isPathEnabled() && ((file = strrchr(file_, '/')) != NULL))
  {
    file++;
  }
  else
  {
    file = file_;
  }

  _outputString[0] = 0;
  if (_formatFunction != NULL)
  {
    // custom format function registered, call it
    (*_formatFunction)(level_, file, function_, line_, timestamp_, userMessage_, outputString_);
  }
  else
  {
    // standard output format, see what items are enabled, each item is separated by a '|'

    // add the prefix if enabled, this is usually the program name
    if (trace_isLogNameEnabled())
    {
      sprintf(&_outputString[strlen(_outputString)], "%s | ", trace_getLogName());
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
    sprintf(&_outputString[strlen(_outputString)], "%s\n", userMessage_);
  }

}

/******************************************************************************/
/******************************************************************************/
void printLine(char *line_)
{
  if (_outputFunction != NULL)
  {
    // custom output function registered, call it
    (*_outputFunction)(line_);
  }
  else
  {
    printf("%s", line_);
  }
}
