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
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *
 * This API provides the Process Specific Embedded Command Line Shell (PSHELL)
 * user API functionality.  It provides the ability for a client program to
 * register functions that can be invoked via a command line user interface.
 * The functions are similar to the prototype of the 'main' in 'C', i.e.
 * 'int myFunc(int argc, char *argv[]), there are several ways to invoke these
 * embedded functions based on how the pshell server is configured, which is
 * described in documentation further down in this file.
 *
 * A complete example of the usage of the API can be found in the included 
 * demo program file pshellServerDemo.cc
 *
 *******************************************************************************/

/****************
 * public enums
 ****************/

/*
 * defines the server type, a UDP/UNIX server supports multiple
 * client sessions, but needs the included 'pshell' client program
 * to operate, a TCP server only supports a single session but
 * can use a standard telnet client, a LOCAL server uses no external
 * client to provide the user input, rather, it initiates all user
 * prompting and control from within the calling program itself, both
 * the UDP and UNIX servers support external programmatic control via
 * the PshellControl.h API and corresponding library, however, a UNIX
 * domain server can only be controlled externally by a client (either
 * 'pshell' or the control API) running on same host as the target program
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
 * These four identifiers that can be used for the hostnameOrIpAddr argument 
 * of the startServer call.  PSHELL_ANYHOST will bind the server socket
 * to all interfaces of a multi-homed host, PSHELL_ANYBCAST will bind to
 * 255.255.255.255, PSHELL_LOCALHOST will bind the server socket to the 
 * local loopback address (i.e. 127.0.0.1), and PSHELL_MYHOST will bind 
 * to the default interface (i.e. eth0), note that subnet broadcast it 
 * also supported, e.g. x.y.z.255
 */
#define PSHELL_ANYHOST "anyhost"
#define PSHELL_ANYBCAST "anybcast"
#define PSHELL_LOCALHOST "localhost"
#define PSHELL_MYHOST "myhost"

/***************************************
 * public "member" function prototypes
 ***************************************/

/*
 * pshell_setServerLogLevel:
 * 
 * function and constants to let the host program set the
 * internal debug log level, if the user of this API does
 * not want to see any internal message printed out, set
 * the debug log level to PSHELL_SERVER_LOG_LEVEL_NONE (0)
 */
#define PSHELL_SERVER_LOG_LEVEL_NONE      0   /* No debug logs */
#define PSHELL_SERVER_LOG_LEVEL_ERROR     1   /* PSHELL_ERROR */
#define PSHELL_SERVER_LOG_LEVEL_WARNING   2   /* PSHELL_ERROR, PSHELL_WARNING */
#define PSHELL_SERVER_LOG_LEVEL_INFO      3   /* PSHELL_ERROR, PSHELL_WARNING, PSHELL_INFO */
#define PSHELL_SERVER_LOG_LEVEL_ALL       PSHELL_SERVER_LOG_LEVEL_INFO
#define PSHELL_SERVER_LOG_LEVEL_DEFAULT   PSHELL_SERVER_LOG_LEVEL_ALL

void pshell_setServerLogLevel(unsigned level_);

/*
 * pshell_registerServerLogFunction:
 * 
 * typedef and function to allow the host program to register a logging
 * function for message output logging, if no output function is registered
 * 'printf' will be used to print out the log messages
 */
typedef void (*PshellLogFunction)(const char *outputString_);
void pshell_registerServerLogFunction(PshellLogFunction logFunction_);

/*
 * PSHELL user callback function prototype definition, the interface 
 * is similar to the "main" in C, with the argc being the argument 
 * count (excluding the actual command itself), and argv[] being the 
 * argument list (also excluding the actual command), the interface
 * can be changed to include the actual command in argc and argv by
 * compiling the library with the command line compile option of
 * PSHELL_INCLUDE_COMMAND_IN_ARGS_LIST
 */
typedef void (*PshellFunction)(int argc_, char *argv_[]); 

/*
 * pshell_addCommand:
 * 
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
 */
void pshell_addCommand(PshellFunction function_,
                       const char *command_, 
                       const char *description_, 
                       const char *usage_ = NULL,           
                       unsigned char minArgs_ = 0,  
                       unsigned char maxArgs_ = 0,  
                       bool showUsage_ = true);

/*
 * pshell_runCommand:
 * 
 * this function can be called from within a program in order to execute any
 * locally registered callback functions, the passed in string command should
 * be the exact same format as when calling the same desired command interactively
 * via the pshell command line client, this function uses a 'printf' type vararg
 * interface
 */
void pshell_runCommand(const char *command_, ...);

/*
 * pshell_noServer: 
 * 
 * this function is used to run in non-server mode, i.e. non-interactive, command 
 * line mode, this will run the command as typed at the host's command line as a 
 * single-shot command and exit, this command must be run with the initial argc, 
 * argv as they are passed into the 'main' from the command line, see the example 
 * demo program pshellNoServerDemo.cc that is included with this package for the 
 * usage of this function
 */
void pshell_noServer(int argc_, char *argv_[]); 

/*
 * pshell_startServer:
 * 
 * this is the command used to start the pshell server, this function can
 * be invoked in either blocking or non-blocking mode, non-blocking mode
 * will create a separate thread to process the user input and return,
 * blocking mode will process the user input directly from this function,
 * i.e. it will never return control to the calling context
 * 
 * this command can only be invoked once per-process, an error message is 
 * displayed if a program tries to invoke this more than once
 * 
 * see the example demo program pshellServerDemo.cc that is included with
 * this package for the usage of this function
 *
 * NOTE: for a LOCAL or UNIX server, the last two parameters should be omitted
 */
void pshell_startServer(const char *serverName_,  
                        PshellServerType serverType_,
                        PshellServerMode serverMode_,
                        const char *hostnameOrIpAddr_ = NULL, 
                        unsigned port_ = 0);

/*
 * pshell_cleanupResources:
 * 
 * this is the command used to cleanup any system resources associated with 
 * a pshell server upon program termination, either via a normal, graceful 
 * termination or via a signal handler exception based termination, this is 
 * only really necessary for a 'unix' type pshell server in order to cleanup 
 * the temporary file handle in the /tmp directory that is associated with the
 * server, although as good practice, this function should be called upon program 
 * termination regardless of the server type
 */
void pshell_cleanupResources(void);

/*
 * the following commands should ONLY be called from 
 * within the scope of a PSHELL callback function
 */

/*
 * pshell_printf:
 * 
 * this is the command used to display user data back the the pshell client,
 * the interface is exactly the same as the normal printf, the UDP/UNIX
 * pshell client has a default 5 second response timeout (which can be changed
 * at the 'pshell' command line), if the callback function does not return to
 * the client within 5 seconds, either with a command  completion indication
 * or display output, the command will timeout, when using the TCP based PSHELL
 * server along with the 'telnet' client, there is no client based timeout
 * (other than the timeouts already built into the TCP protocol itself), there
 * is also no timeout for a LOCAL PSHELL server
 */
void pshell_printf(const char *format_, ...);

/*
 * pshell_flush:
 * 
 * this command is used to flush the transfer buffer from the pshell server
 * back to the client, which will then display the contents of the buffer
 * to the user, this only has an effect with a UDP, UNIX, or LOCAL server,
 * since a TCP server is a character based stream, there is no buffering of
 * transfer data
 */
void pshell_flush(void);

/*
 * these helper commands are used to keep the UDP/UNIX client alive with output 
 * if a command is known to take longer than the 5 second client timeout
 */
void pshell_wheel(const char *string_ = NULL);  /* spinning ascii wheel, user string string is optional */
void pshell_march(const char *string_);         /* march a string or character across the screen */

/*
 * pshell_isHelp:
 * 
 * this function will return "true" if the user types a '?' 
 * or '-h' after the command name, this can be checked from 
 * within a callback function to see if the user asked for 
 * help, the command MUST be registered with the "showUsage" 
 * arg set to "false" in its addCommand registration, otherwise 
 * the PSHELL server itself will give the registered usage on
 * a '?' or '-h' without dispatching the command
 */ 
bool pshell_isHelp(void);

/*
 * this function is used to show the usage the command was registered with
 */
void pshell_showUsage(void);

/*
 * pshell_tokenize:
 *
 * function to help tokenize command line arguments, this function
 * can ONLY be called from with an PSHELL callback function, this
 * function returns a pointer to this structure that contains the
 * tokenized items
 */
struct PshellTokens
{
  unsigned numTokens;
  char **tokens;
};
PshellTokens *pshell_tokenize(const char *string_, const char *delimeter_);

/*
 * pshell_getOption:
 *
 * this function will extract options from the callback function args list
 * of the formats -<option><value> where <option> is a single character
 * option (e.g. -t10), or <option>=<value> where <option> is any length
 * character string (e.g. timeout=10), if the 'strlen(option_)' == 0,
 * all option names & values will be extracted and returned in the 'option_'
 * and 'value_' parameters, if the 'strlen(option_)' is > 0, the 'value_'
 * will only be extracted if the option matches the requested option_ name,
 * see the pshellServerDemo program for examples on this function usage
 */
bool pshell_getOption(const char *string_, char *option_, char *value_);

/*
 * format checking functions:
 * 
 * the following functions are some simple helper functions
 * to assist in interpreting the string based command line 
 * arguments, note that unlike some of the previous functions,
 * the remaining functions do not need to be called strictly 
 * from within the context of a pshell callback function
 */
unsigned pshell_getLength(const char *string_);
bool pshell_isEqual(const char *string1_, const char *string2_);
bool pshell_isEqualNoCase(const char *string1_, const char *string2_);
bool pshell_isSubString(const char *string1_, const char *string2_, unsigned minChars_);
bool pshell_isSubStringNoCase(const char *string1_, const char *string2_, unsigned minChars_);
bool pshell_isAlpha(const char *string_);
bool pshell_isAlphaNumeric(const char *string_);
bool pshell_isDec(const char *string_);
/* if needHexPrefix == true, format is 0x<hexDigits>, if needHexPrefix == false, format is <hexDigits> */
bool pshell_isHex(const char *string_, bool needHexPrefix_ = true);      
bool pshell_isNumeric(const char *string_, bool needHexPrefix_ = true);  /* isDec || isHex */
bool pshell_isFloat(const char *string_);

/*
 * various data extraction functions:
 * 
 * the following functions are some simple helper functions
 * to assist in converting the string based command line 
 * arguments to other data types
 */

/* return address from string value */
void *pshell_getAddress(const char *string_);

/* return bool from string values of true/false, yes/no, on/off */
bool pshell_getBool(const char *string_);

/* desired radix of integer extractions */
enum PshellRadix
{
  PSHELL_RADIX_DEC, /* <decValue> only */
  PSHELL_RADIX_HEX, /* 0x<hexValue> or <hexValue> depending on setting of needHexPrefix */
  PSHELL_RADIX_ANY  /* will transparently work for <decValue> or 0x<hexValue> */
};

/* 
 * integer extraction functions:
 * 
 * return numeric values from string value, the needHexPrefix parameter is only used
 * for a radix of PSHELL_RADIX_HEX, if set to true, the value needs to be preceeded 
 * by the 0x identifier, if set to false, then the prefix is not necessary
 */
long pshell_getLong(const char *string_, PshellRadix radix_ = PSHELL_RADIX_ANY, bool needHexPrefix_ = true);
int pshell_getInt(const char *string_, PshellRadix radix_ = PSHELL_RADIX_ANY, bool needHexPrefix_ = true);
short pshell_getShort(const char *string_, PshellRadix radix_ = PSHELL_RADIX_ANY, bool needHexPrefix_ = true);
char pshell_getChar(const char *string_, PshellRadix radix_ = PSHELL_RADIX_ANY, bool needHexPrefix_ = true);
unsigned pshell_getUnsigned(const char *string_, PshellRadix radix_ = PSHELL_RADIX_ANY, bool needHexPrefix_ = true);
unsigned long pshell_getUnsignedLong(const char *string_, PshellRadix radix_ = PSHELL_RADIX_ANY, bool needHexPrefix_ = true);
unsigned short pshell_getUnsignedShort(const char *string_, PshellRadix radix_ = PSHELL_RADIX_ANY, bool needHexPrefix_ = true);
unsigned char pshell_getUnsignedChar(const char *string_, PshellRadix radix_ = PSHELL_RADIX_ANY, bool needHexPrefix_ = true);

/* 
 * floating point extraction functions 
 */
float pshell_getFloat(const char *string_);
double pshell_getDouble(const char *string_);

#ifdef __cplusplus
}
#endif

#endif
