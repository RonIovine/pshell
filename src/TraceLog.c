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

#define MAX_STRING_SIZE 256

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

static TraceLogFunction _logFunction = NULL;
static char _logPrefix[MAX_STRING_SIZE] = {"TRACE | "};
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static bool _printLocation = true;
static bool _printPath = false;
static const char *_printPrefix = _logPrefix;
static bool _printTimestamp = true;
static unsigned _maxTypeLength = 0;

/***************************************
 * private "member" function prototypes
 ***************************************/
 
static void printLine(char *line_);
static void formatHeader(const char *type_,
                         const char *file_,
                         const char *function_,
                         int line_,
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
void trace_registerLogFunction(TraceLogFunction logFunction_)
{
  if (logFunction_ != NULL)
  {
    _logFunction = logFunction_;
  }
  else
  {
    TRACE_ERROR("NULL logFunction, not registered");
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_setLogPrefix(const char *name_)
{
  if ((name_ != NULL) && (strlen(name_) > 0))
  {
    int length = strlen(name_);
    for (int i = 0; i < length; i++)
    {
      //_logPrefix[i] = toupper(name_[i]);
      _logPrefix[i] = name_[i];
    }
    _logPrefix[length] = ' ';
    _logPrefix[length+1] = '|';
    _logPrefix[length+2] = ' ';
    _logPrefix[length+3] = 0;
  }
  else
  {
    _logPrefix[0] = 0;
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_showLocation(bool show_)
{
  _printLocation = show_;
}

/******************************************************************************/
/******************************************************************************/
void trace_showPath(bool show_)
{
  _printPath = show_;
}

/******************************************************************************/
/******************************************************************************/
void trace_showTimestamp(bool show_)
{
  _printTimestamp = show_;
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
void trace_showPrefix(bool show_)
{
  if (show_)
  {
    _printPrefix = _logPrefix;
  }
  else
  {
    _printPrefix = "";
  }
}

/******************************************************************************/
/******************************************************************************/
bool trace_isPrefixEnabled(void)
{
  return (strcmp(_printPrefix, "") != 0);
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
  if (strlen(levelName_) > _maxTypeLength)
  {
    _maxTypeLength = strlen(levelName_);
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
void trace_outputLog(const char *type_,
                     const char *file_,
                     const char *function_,
                     int line_,
                     const char *format_, ...)
{
  pthread_mutex_lock(&_mutex);
  char outputString[MAX_STRING_SIZE];
  formatHeader(type_, file_, function_, line_, outputString);
  va_list args;
  va_start(args, format_);
  vsprintf(&outputString[strlen(outputString)], format_, args);
  va_end(args);
  strcat(outputString, "\n");
  printLine(outputString);
  pthread_mutex_unlock(&_mutex);
}

/******************************************************************************/
/******************************************************************************/
void trace_outputDump(void *address_,
                      unsigned length_,
                      const char *type_,
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
  char outputString[MAX_STRING_SIZE];
  formatHeader(type_, file_, function_, line_, outputString);
  va_list args;
  va_start(args, format_);
  vsprintf(&outputString[strlen(outputString)], format_, args);
  va_end(args);
  strcat(outputString, "\n");
  printLine(outputString);
  asciiLine[0] = 0;
  outputString[0] = 0;
  for (unsigned i = 0; i < length_; i++)
  {
    // see if we are on a full line boundry
    if ((i%bytesPerLine) == 0)
    {
      // see if we need to print out our line
      if (i > 0)
      {
        /* done with this line, add the asciii data & print it */
        sprintf(&outputString[strlen(outputString)], "  %s\n", asciiLine);
        printLine(outputString);
        // line  printed, clear them it for next time
        asciiLine[0] = 0;
        outputString[0] = 0;
      }
      // format our our offset
      sprintf(&outputString[strlen(outputString)], "  %04x  ", offset);
      offset += bytesPerLine;
    }
    // create our line of ascii data, for non-printable ascii characters just use a "."
    sprintf(&asciiLine[strlen(asciiLine)], "%c", ((isprint(bytes[i]) && (bytes[i] < 128)) ? bytes[i] : '.'));
    // format our line of hex byte
    sprintf(&outputString[strlen(outputString)], "%02x ", bytes[i]);
  }
  // done, format & print the final line & ascii data
  sprintf(&outputString[strlen(outputString)], "  %*s\n", (int)(((bytesPerLine-strlen(asciiLine))*3)+strlen(asciiLine)), asciiLine);
  printLine(outputString);
  pthread_mutex_unlock(&_mutex);
}

/**************************************
 * private "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void formatHeader(const char *type_, const char *file_, const char *function_, int line_, char *outputString_)
{
  char timestamp[MAX_STRING_SIZE];
  struct timeval tv;
  struct tm tm;
  const char *file;
  // get timestamp
  gettimeofday(&tv, NULL);
  gmtime_r(&tv.tv_sec, &tm);
  strftime(timestamp, sizeof(timestamp), "%T", &tm);
  // strip off any leading path of filename
  if (!_printPath && ((file = strrchr(file_, '/')) != NULL))
  {
    file++;
  }
  else
  {
    file = file_;
  }
  
  if (_printLocation && _printTimestamp)
  {
    sprintf(outputString_, "%s%-*s | %s.%-6ld | %s(%s):%d | ", _printPrefix, _maxTypeLength, type_, timestamp, tv.tv_usec, file, function_, line_);
  }
  else if (_printLocation)
  {
    sprintf(outputString_, "%s%-*s | %s(%s):%d | ", _printPrefix, _maxTypeLength, type_, file, function_, line_);
  }
  else if (_printTimestamp)
  {
    sprintf(outputString_, "%s%-*s | %s.%-6ld | ", _printPrefix, _maxTypeLength, type_, timestamp, tv.tv_usec);
  }
  else
  {
    sprintf(outputString_, "%s%-*s | ", _printPrefix, _maxTypeLength, type_);
  }
  
}

/******************************************************************************/
/******************************************************************************/
void printLine(char *line_)
{
  if (_logFunction == NULL)
  {
    printf("%s", line_);
  }
  else
  {
    (*_logFunction)(line_);
  }
}
