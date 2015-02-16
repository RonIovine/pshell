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

#ifndef TRACE_FILTER_H
#define TRACE_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *
 * This API provides the ability for a client program to add dynamic and
 * configurable trace control to a program that has a trace logging mechanism
 * that uses a __FILE__, __FUNCTION__, __LINE__, level, paradigm.  See the
 * included files TraceLog.h and TraceLog.cc example trace logging system along
 * with the demo program traceFilterDemo.c for examples on how to intrgrate and
 * use this feature in an existing appliation.
 *
 * This module is dependent on the application running a pshell server, but
 * the converse it not true (i.e. this module can be omitted in the build
 * of the pshell library if this functionality is not desired).
 *
 *******************************************************************************/

/*
 * If the following ifdef is uncommented, then for every file you want to do
 * file specific trace filtering you must put the 'TF_SYMBOL_TABLE' macro at
 * the top of the .cc file
 */
//#define TF_FAST_FILENAME_LOOKUP

#ifdef TF_FAST_FILENAME_LOOKUP

#ifndef TF_MAX_TRACE_SYMBOLS
#define TF_MAX_TRACE_SYMBOLS 5000
#endif

struct TraceSymbols
{
  TraceSymbols(const char *file_){if (_numSymbols < TF_MAX_TRACE_SYMBOLS) _symbolTable[_numSymbols++] = file_;};
  static int _numSymbols;
  static const char *_symbolTable[TF_MAX_TRACE_SYMBOLS];
};
#define TF_SYMBOL_TABLE static TraceSymbols __tf_traceSymbols(__FILE__)

#else
#define TF_SYMBOL_TABLE
#endif

typedef bool (*tf_TraceCallback) (void);

enum tf_TraceControl
{
  TF_ONCE,
  TF_CONTINUOUS,
  TF_ABORT
};

void tf_init(const char *configFile_);

void tf_registerThread(const char *threadName_);

bool tf_isFilterPassed(const char *file_,
                       int line_,
                       const char *function_,
                       unsigned level_);
                       
void tf_watch(const char *file_,
              int line_,
              const char *function_,
              const char *symbol_,
              void *address_,
              int width_,
              const char *format_,
              tf_TraceControl control_);
              
void tf_callback(const char *file_,
                 int line_,
                 const char *function_,
                 const char *callbackName_,
                 tf_TraceCallback callbackFunction_,
                 tf_TraceControl control_);

/*
 * macro to registger a memory location to watch at every trace statement,
 * use this instead of a direct call to tf_watch
 */ 
#define TF_WATCH(symbol, address, width, format, control) \
          tf_watch(__FILE__, __LINE__, __FUNCTION__, symbol, address, width, format, control)

/*
 * macro to registger a function to be called at every trace statement,
 * use this instead of a direct call to tf_callback
 */
#define TF_CALLBACK(name, function, control) \
          tf_callback(__FILE__, __LINE__, __FUNCTION__, name, function, control)

#ifdef __cplusplus
}
#endif

#endif
