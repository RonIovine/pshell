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

#ifndef PSHELL_SERVER_H
#define PSHELL_SERVER_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *
 * This API provides the Process Specific Embedded Command Line Shell (PSHELL)
 * user API functionality.  It provides the ability for a client program to
 * register functions that can be invoked via a command line user interface.
 * The functions are of the same prototype of the 'main' in 'C', i.e.
 * 'int myFunc(int argc, char *argv[]), there are several ways and client
 * methods to call these functions, they are described in documentation further
 * down in this file.
 *
 *******************************************************************************/

/****************
 * public enums
 ****************/

/*
 * defines the server type, a UDP/UNIX server supports multiple
 * client sessions, but needs the 'pshell' client program to
 * operate, a TCP server only supports a single session but
 * can use a standard telnet client, a LOCAL server uses
 * no external client to provide the user input, rather,
 * it initiates all user prompting and control from within
 * the calling program itself, both the UDP and UNIX servers
 * support external programmatic control via the PshellControl.h
 * API, however, a UNIX domain server can only be controlled
 * externally by a client (either 'pshell' or control API)
 * running on same host as the target program
 */
enum PshellServerType
{
  PSHELL_UDP_SERVER,
  PSHELL_TCP_SERVER,
  PSHELL_UNIX_SERVER,
  PSHELL_LOCAL_SERVER 
};

/*
 * defines the server mode, this refers to the operation of the pshell_startServer
 * function, non-blocking mode will spawn a separate thread to process incoming
 * commands, causing the pshell_startServer function to return, blocking mode will
 * cause the pshell_startServer to process all incoming commands within the calling
 * context, causing the pshell_startServer function to be blocking (i.e. it will
 * never return control to the caller)
 */
enum PshellServerMode
{
  PSHELL_BLOCKING,
  PSHELL_NON_BLOCKING 
};

/*
 * the following enum values are returned by the pshell_runCommand
 * function as well as the actual registered callback itself, these
 * must match their corresponding values in PshellControl.h, all of
 * these values except for PSHELL_COMMAND_NOT_FOUND can be returned
 * by the registered pshell callback function, if the callback function
 * is designed to be called via the remote PshellControl client API
 * mechanism, it is recommended that the full compliment of return
 * codes be used as appropriate in order to give the remote control
 * client as much visibility as possible as to the results of the call,
 * however if the callback if only to be used from an interactive CLI
 * client program (either 'telnet' or 'pshell'), then the command can
 * just return PSHELL_COMMAND_SUCCESS
 */
enum PshellCommandResults
{
  PSHELL_COMMAND_SUCCESS,        
  PSHELL_COMMAND_FAILURE,        
  PSHELL_COMMAND_TIMEOUT,        
  PSHELL_COMMAND_NOT_FOUND,      
  PSHELL_COMMAND_INVALID_ARG_USAGE,  
  PSHELL_COMMAND_INVALID_ARG_COUNT,  
  PSHELL_COMMAND_INVALID_ARG_VALUE,  
  PSHELL_COMMAND_INVALID_ARG_FORMAT,   
  PSHELL_COMMAND_USAGE_REQUESTED,  
};


/******************************
 * public typedefs structures
 ******************************/

/* structure used to tokenize command line arguments */

struct PshellTokens
{
  unsigned numTokens;
  char **tokens;
};

/***************************************
 * public "member" function prototypes
 ***************************************/

/*
 * function and constants to let the client set the internal
 * debug log level
 */
#define PSHELL_LOG_LEVEL_0         0      /* No debug logs */
#define PSHELL_LOG_LEVEL_1         1      /* PSHELL_ERROR only */
#define PSHELL_LOG_LEVEL_2         2      /* Level 1 plus PSHELL_WARNING */
#define PSHELL_LOG_LEVEL_3         3      /* Level 2 plus PSHELL_INFO */
#define PSHELL_LOG_LEVEL_NONE      PSHELL_LOG_LEVEL_0
#define PSHELL_LOG_LEVEL_ALL       PSHELL_LOG_LEVEL_3
#define PSHELL_LOG_LEVEL_DEFAULT   PSHELL_LOG_LEVEL_3

void pshell_setLogLevel(unsigned level_);

/*
 * typedef and function to allow a client program to register a logging
 * function for message output logging, if no output function is registered
 * 'printf' will be used to print out the log messages, if the client of
 * this API does not want to see any internal message printed out, set the
 * debug log level to PSHELL_LOG_LEVEL_NONE (0)
 */
typedef void (*PshellLogFunction)(const char *outputString_);
void pshell_registerLogFunction(PshellLogFunction logFunction_);

/*
 * PSHELL user callback function prototype definition, the interface 
 * is identical to the "main" in C, with the argc being the argument 
 * count (excluding the actual command itself), and argv[] being the 
 * argument list (also excluding the actual command), the interface
 * can be changed to include the actual command in argc and argv by
 * compiling the library with the command line compile option of
 * PSHELL_INCLUDE_COMMAND_IN_ARGS_LIST
 * 
 */
typedef int (*PshellFunction) (int argc, char *argv[]); 

/*
 * this is the function used to register pshell commands, if the command
 * takes no arguments, the minArgs and maxArgs should be 0 and the usage
 * should be NULL, a usage is required if the command takes any arguments,
 * if the command takes a fixed number of arguments, then set minArgs and
 * maxArgs equal to that fixed count, if the parameter count validation
 * fails, the command will not be dispatched and the usage will be shown,
 * if the showUsage argument is set to "true", then the PshellServer will 
 * display the registered usage upon the user typing a "?" or "-h" after 
 * the command, the command will not be dispatched, if this argument is set 
 * to "false", the command will be dispatched, the command can then call 
 * the pshell_isHelp function below to determine if the user is requesting
 * help, finally, the function's command cannot contain any whitespace (i.e.
 * multi-keyword commands are not supported)
 *
 * NOTE: the below 'pshell_addCommand' function is NOT threadsafe, this function 
 * should only be invoked under a single task context, or a sequentually based 
 * multi-threaded context (i.e. sequential multi-thread startup sequence)
 */
void pshell_addCommand(PshellFunction function_,
                       const char *command_, 
                       const char *description_, 
                       const char *usage_ = NULL,           
                       unsigned char minArgs_ = 0,  
                       unsigned char maxArgs_ = 0,  
                       bool showUsage_ = true);

/*
 * this function can be called from within a program in order to execute any
 * registered callback function, the passed in string command should be the
 * exact same format as when calling the same desired command interactively
 * via the pshell command line client, this function uses a 'printf' type
 * vararg interface,the valid return values are defined in the enum
 * PshellCommandResults
 */
int pshell_runCommand(const char *command_, ...);

/* map the PshellCommandResults enums to their corresponding strings */
const char *pshell_getResultsString(int results_);

/*
 * this is the command used to invoke the pshell server, this function can
 * be invoked in either blocking or non-blocking mode, non-blocking mode
 * will create a separate thread to process the user input and return,
 * blocking mode will process the user input directly from this function,
 * it will not return
 * 
 * this command can only be invoked once per-process, an error message is 
 * displayed if a program tries to invoke this more than once (i.e. under 
 * two different contexts)
 *
 * NOTE: for a LOCAL or UNIX server, the last two parameters should be omitted
 */
void pshell_startServer(const char *serverName_,  
                        PshellServerType serverType_,
                        PshellServerMode serverMode_,
                        const char *hostnameOrIpAddr_ = NULL, 
                        unsigned port_ = 0);

/*
 * the following commands should ONLY be called from 
 * within the scope of an PSHELL callback function
 */

/*
 * this is the command used to display user data back the the pshell client,
 * the interface is exactly the same as the normal printf, the UDP/UNIX
 * pshell client has a 5 second response timeout, if the callback function
 * does not return to the client within 5 seconds, either with a command 
 * completion indication or display output, the command will timeout, when
 * using the TCP based PSHELL server along with the 'telnet' client, there
 * is no client based timeout (other than the timeouts already built into
 * the TCP protocol itself), there is also no timeout for a LOCAL PSHELL
 * server
 */
void pshell_printf(const char *format_, ...);

/*
 * this command is used to flush the transfer buffer from the pshell server
 * back to the client, which will then display the contents of the buffer
 * to the user, this only has an effect with a UDP or LOCAL server
 */
void pshell_flush(void);

/*
 * these helper commands are used to keep the UDP client alive with output 
 * if a command is known to take longer than the 5 second client timeout
 */
void pshell_wheel(const char *string_ = NULL);  /* user string string is optional */
void pshell_march(const char *string_);         /* march a string or character */

/*
 * this function will return "true" if the user types a '?' 
 * or '-h' after the command name, this can be checked from 
 * within a callback function to see if the user asked for 
 * help, the command MUST be registered with the "showUsage" 
 * arg set to "false" in its addCommand registration, otherwise 
 * the PshServer itself will give the usage on a '?' or '-h'
 * without dispatching the command
 */ 
bool pshell_isHelp(void);

/*
 * this function is used to show the usage the command was registered with,
 * it will return PSHELL_COMMAND_USAGE_REQUESTED
 */
int pshell_showUsage(void);

/*
 * the following functions are some simple helper functions
 * to assist in interpreting the string based command line 
 * arguments
 */

/*
 * the following 'pshell_tokenize' function can ONLY be called
 * from with an PSHELL callback function, the other following
 * functions do not have this restriction
 */
PshellTokens *pshell_tokenize(const char *string_, const char *delimeter_);
unsigned pshell_getLength(const char *string_);
bool pshell_isEqual(const char *string1_, const char *string2_);
bool pshell_isEqualNoCase(const char *string1_, const char *string2_);
bool pshell_isSubString(const char *string1_, const char *string2_, unsigned minChars_);
bool pshell_isAlpha(const char *string_);
bool pshell_isNumeric(const char *string_);  /* isDec || isHex */
bool pshell_isAlphaNumeric(const char *string_);
bool pshell_isDec(const char *string_);
bool pshell_isHex(const char *string_);      /* format 0x<hexDigits> */
bool pshell_isFloat(const char *string_);

/*
 * the following functions are some simple helper functions
 * to assist in converting the string based command line 
 * arguments to other data types
 */
void *pshell_getVoid(const char *string_);
char *pshell_getString(const char *string_);
bool pshell_getBool(const char *string_);
long pshell_getLong(const char *string_);
int pshell_getInt(const char *string_);
short pshell_getShort(const char *string_);
char pshell_getChar(const char *string_);
unsigned pshell_getUnsigned(const char *string_);
unsigned long pshell_getUnsignedLong(const char *string_);
unsigned short pshell_getUnsignedShort(const char *string_);
unsigned char pshell_getUnsignedChar(const char *string_);
float pshell_getFloat(const char *string_);
double pshell_getDouble(const char *string_);

#ifdef __cplusplus
}
#endif

#endif
