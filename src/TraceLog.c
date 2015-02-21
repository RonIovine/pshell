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
 * private "member" data
 *************************/

static TraceLogFunction _logFunction = NULL;
static char _logPrefix[MAX_STRING_SIZE] = {"TRACE"};
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;

/**************************************
 * public API "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void trace_registerLogFunction(TraceLogFunction logFunction_)
{
  _logFunction = logFunction_;
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
      _logPrefix[i] = toupper(name_[i]);
    }
    _logPrefix[length] = '_';
    _logPrefix[length+1] = 0;
  }
  else
  {
    _logPrefix[0] = 0;
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_outputLog(const char *type_, const char *file_, const char *function_, int line_, const char *format_, ...)
{
  pthread_mutex_lock(&_mutex);
  char outputString[MAX_STRING_SIZE];
  char timestamp[MAX_STRING_SIZE];
  struct timeval tv;
  struct tm tm;
  const char *file;
  // get timestamp
  gettimeofday(&tv, NULL);
  gmtime_r(&tv.tv_sec, &tm);
  // timestamp with date & time
  //strftime(timestamp, sizeof(timestamp), "%m/%d/%y %T", &tm);
  // timestamp with time only
  strftime(timestamp, sizeof(timestamp), "%T", &tm);
  // strip off any leading path of filename
  if ((file = strrchr(file_, '/')) != NULL)
  {
    file++;
  }
  else
  {
    file = file_;
  }
  sprintf(outputString, "%s%-7s | %s.%ld | %s(%s):%d | ", _logPrefix, type_, timestamp, tv.tv_usec, file, function_, line_);
  va_list args;
  va_start(args, format_);
  vsprintf(&outputString[strlen(outputString)], format_, args);
  va_end(args);
  strcat(outputString, "\n");
  if (_logFunction == NULL)
  {
    printf("%s", outputString);
  }
  else
  {
    (*_logFunction)(outputString);
  }
  pthread_mutex_unlock(&_mutex);
}
