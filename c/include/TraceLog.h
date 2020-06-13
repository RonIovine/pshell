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

#ifndef TRACE_LOG_H
#define TRACE_LOG_H

#include <stdio.h>
#ifdef DYNAMIC_TRACE_FILTER
#include <TraceFilter.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *
 * This file and API are an example trace logging service to show how to
 * integrate the trace filtering mechanism into an already existing trace
 * logging service that uses the __FILE__, __LINE__, __FUNCTION__, level
 * paradigm
 *
 *******************************************************************************/

/*
 * these trace levels must be registered into the trace filter service via the
 * 'tf_addLevel' function, that function must be called before calling 'tf_init',
 * in this example, all the levels are added in the function 'trace_registerLevels'
 */
#define TL_ERROR      0
#define TL_WARNING    1
#define TL_FAILURE    2
#define TL_INFO       3
#define TL_DEBUG      4
#define TL_ENTER      5
#define TL_EXIT       6
#define TL_DUMP       7

/* start all user defined levels after the MAX */
#define TL_MAX_LEVELS TL_DUMP
#define TL_ALL_LEVELS TL_MAX_LEVELS
#define TL_DEFAULT_LEVEL TL_FAILURE

/* define the string based names of the trace levels */
#define TL_ERROR_STRING "ERROR"
#define TL_WARNING_STRING "WARNING"
#define TL_FAILURE_STRING "FAILURE"
#define TL_INFO_STRING "INFO"
#define TL_DEBUG_STRING "DEBUG"
#define TL_ENTER_STRING "ENTER"
#define TL_EXIT_STRING "EXIT"
#define TL_DUMP_STRING "DUMP"
#define TL_FORCE_STRING "FORCE"

/*
 * the following are some example TRACE macros to illustrate integrating the
 * function 'tf_isFilterPassed' into an existing trace logging system to provide
 * dynamic trace control via an integrated pshell server, the existing trace
 * logging utility must utilize the __FILE__, __FUNCTION__, __LINE__ (and
 * optionally a 'level') trace logging paradigm
 */

/* trace output macros, these are called directly by client code */

#ifdef TRACE_LOG_DISABLED

/* no trace logs, completly compile out all trace logs for performance */
#define TRACE_FORCE(format, args...)

#else

/* this trace cannot be disabled, hence no call to the 'tf_isFilterPassed' function is necessary */
#define TRACE_FORCE(format, args...) trace_outputLog(TL_FORCE_STRING, __FILE__, __FUNCTION__, __LINE__, format, ## args)

#endif

/* these traces must pass through the 'tf_isFilterPassed' function in order to be displayed */
#define TRACE_ERROR(format, args...) __TRACE(TL_ERROR, TL_ERROR_STRING, format, ## args)
#define TRACE_WARNING(format, args...) __TRACE(TL_WARNING, TL_WARNING_STRING, format, ## args)
#define TRACE_FAILURE(format, args...) __TRACE(TL_FAILURE, TL_FAILURE_STRING, format, ## args)
#define TRACE_INFO(format, args...) __TRACE(TL_INFO, TL_INFO_STRING, format, ## args)
#define TRACE_DEBUG(format, args...) __TRACE(TL_DEBUG, TL_DEBUG_STRING, format, ## args)
#define TRACE_ENTER(format, args...) __TRACE(TL_ENTER, TL_ENTER_STRING, format, ## args)
#define TRACE_EXIT(format, args...) __TRACE(TL_EXIT, TL_EXIT_STRING, format, ## args)

/* hex dump trace, must also pass the 'tf_isFilterPassed' criteria */
#define TRACE_DUMP(address, length, format, args...) __DUMP(address, length, TL_DUMP, TL_DUMP_STRING, format, ## args)

/*
 * trace_registerOutputFunction:
 *
 * typedef and function to allow a client program to register a cusatom logging
 * function for message output logging, if no output function is registered
 * 'printf' will be used to print out the log messages
 */
typedef void (*TraceOutputFunction)(const char *outputString_);
void trace_registerOutputFunction(TraceOutputFunction outputFunction_);

/*
 * trace_registerFormatFunction:
 *
 * typedef and function to allow a client program to register a custom format
 * function for formatting the trace output line, if no format function is
 * registered the default formatting will be used, e.g.
 *
 * name | level | timestamp | file(function):line | user-message
 */
typedef void (*TraceFormatFunction)(const char *level_,
                                    const char *file_,
                                    const char *function_,
                                    int line_,
                                    const char *timestamp_,
                                    const char *userMessage_,
                                    char *outputString_);
void trace_registerFormatFunction(TraceFormatFunction formatFunction_);

/*
 * trace_setTimestampFormat:
 *
 * set the timestamp format for the strftime function, if the 'addUsec_' argument is
 * set to true, be sure to use a format where the sec is last since the usec will be
 * appended to that, if no format is specified, the default formatting will be used,
 * e.g. hh:mm:ss.usec in 24 hour local time
 */
void trace_setTimestampFormat(const char *format_, bool addUsec_ = true);

/*
 * trace_setLogName:
 *
 * set a log name that is used as a prefix for the log type,
 * if this function is not called, the prefix will default to
 * "TRACE", e.g. TRACE | ERROR..., TRACE | WARNING... etc, if
 * this function is called with a NULL prefix, the trace name
 * type will have no prefix, e.g. just ERROR..., WARNING... etc.
 * this should typically be set to the program name
 */
void trace_setLogName(const char *name_);

/*
 * trace_getLogName:
 *
 * returns the registred log name
 */
const char *trace_getLogName(void);

/*
 * trace_registerLevels:
 *
 * register all our above trace levels with the trace filter, note
 * that all levels MUST be registered before calling 'tf_init'
 */
void trace_registerLevels(void);

/*
 * trace_addUserLevel:
 *
 * add a user defined trace level, the level will be registered with the TraceFilter
 * API via the tf_addLevel call, we wrap that call with this function so we can get
 * our maximum trace type level string length so our display is formatted and aligned
 * correctly
 */
void trace_addUserLevel(const char *levelName_, unsigned levelValue_, bool isDefault_ = false, bool isMaskable_ = true);

/*
 * if we are using a stand-alone traceLog system and not integrating it into the
 * traceFilter mechanism we need to provide an internal trace log level and a way
 * to set it
 */
#ifndef DYNAMIC_TRACE_FILTER

/*
 * trace_setLogLevel:
 *
 * this function is used to set the internal trace log level when this module
 * is built with the DYNAMIC_TRACE_FILTER flag NOT set (i.e. stand-alone mode)
 */
void trace_setLogLevel(unsigned _logLevel);

/*
 * trace_getLogLevel:
 *
 * this function is used to return the internal trace log level when this module
 * is built with the DYNAMIC_TRACE_FILTER flag NOT set (i.e. stand-alone mode)
 */
unsigned trace_getLogLevel(void);

#endif

/*
 * trace_enableLocation:
 *
 * enable/disable the displaying of the file, function, and line information in the trace logs
 */
void trace_enableLocation(bool enable_);

/*
 * trace_isLocationEnabled:
 *
 * returns if the trace location format is enabled
 */
bool trace_isLocationEnabled(void);

/*
 * trace_enablePath:
 *
 * enable/disable the displaying of any preceeding path information from the __FILE__ string
 */
void trace_enablePath(bool enable_);

/*
 * trace_isPathEnabled:
 *
 * returns whether we are stripping any preceeding path from the __FILE__ string
 */
bool trace_isPathEnabled(void);

/*
 * trace_enableTimestamp:
 *
 * enable/disable the displaying of the timestamp in the trace logs
 */
void trace_enableTimestamp(bool enable_);

/*
 * trace_isTimestampEnabled:
 *
 * returns if the trace timestamp format is enabled
 */
bool trace_isTimestampEnabled(void);

/*
 * trace_enableLogName
 *
 * enable/disable the displaying of the name in the trace logs
 */
void trace_enableLogName(bool enable_);

/*
 * trace_isLogNameEnabled:
 *
 * returns if the trace log name is enabled
 */
bool trace_isLogNameEnabled(void);

/****************************************************************************
 *
 * NOTE: There are no public APIs beyhond this point, all public functional
 *       APIs and macros appear above this section, do not call anything
 *       below this section directly!!
 *
 ****************************************************************************/

/*
 * common TRACE and DUMP output functions and macros that all the different
 * levels map to, these functions and macros should NOT be called directly
 * by client code
 */
extern void trace_outputLog(const char *level_, const char *file_, const char *function_, int line_, const char *format_, ...);
extern void trace_outputDump(void *address_, unsigned length_, const char *level_, const char *file_, const char *function_, int line_, const char *format_, ...);

#ifdef TRACE_LOG_DISABLED

/* no trace logs, completly compile out all trace logs for performance */
#define __TRACE(level, name, format, args...)
#define __DUMP(address, length, level, name, format, args...)

#else  /* trace logs enabled */

#ifdef DYNAMIC_TRACE_FILTER

/*
 * dynamic trace filtering enabled, use the tf_isFilterPassed function in the dynamic
 * trace filter module to determine if a given trace should be printed out
 */
#define __TRACE(level, name, format, args...) if (tf_isFilterPassed(__FILE__, __LINE__, __FUNCTION__, level)) {trace_outputLog(name, __FILE__, __FUNCTION__, __LINE__, format, ## args);}
#define __DUMP(address, length, level, name, format, args...) if (tf_isFilterPassed(__FILE__, __LINE__, __FUNCTION__, level)) {trace_outputDump(address, length, name, __FILE__, __FUNCTION__, __LINE__, format, ## args);}

#else  /* stand-alone trace logs (i.e. no dynamic filtering) */

/*
 * dynamic trace filtering not enabled, use use a simple compare of the desired hierarchical
 * trace level against the configures trace level to determine if a given trace should be
 * printed out
 */
extern unsigned _traceLogLevel;
#define __TRACE(level, name, format, args...) if (_traceLogLevel >= level) {trace_outputLog(name, __FILE__, __FUNCTION__, __LINE__, format, ## args);}
#define __DUMP(address, length, level, name, format, args...) if (_traceLogLevel >= level) {trace_outputDump(address, length, name, __FILE__, __FUNCTION__, __LINE__, format, ## args);}

#endif  /* DYNAMIC_TRACE_FILTER */

#endif  /* TRACE_LOG_DISABLED */

#ifdef __cplusplus
}
#endif

#endif
