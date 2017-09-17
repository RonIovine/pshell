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

#ifndef PSHELL_CONTROL_H
#define PSHELL_CONTROL_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *
 * This API provides the ability for a client program to invoke pshell commands
 * that are registered in a remote program that is running a pshell UDP or UNIX
 * server.  The format of the command string that is sent to the pshell should
 * be in the same usage format that the given command is expecting.  This
 * provides a very lightweight way to provide a control mechanism into a program
 * that is running a pshell, this is analagous to a remote procedure call (rpc).
 *
 * A complete example of the usage of the API can be found in the included 
 * demo program file pshellControlDemo.cc
 *
 *******************************************************************************/

/*
 * pshell_setCotrolLogLevel:
 *
 * function and constants to let the client set the internal debug log level,
 * if the client of this API does not want to see any internal message printed,
 * set the debug log level to PSHELL_LOG_LEVEL_NONE (0)
 */
#define PSHELL_LOG_LEVEL_NONE      0   /* No debug log messages */
#define PSHELL_LOG_LEVEL_ERROR     1   /* PSHELL_ERROR */
#define PSHELL_LOG_LEVEL_WARNING   2   /* PSHELL_ERROR, PSHELL_WARNING */
#define PSHELL_LOG_LEVEL_INFO      3   /* PSHELL_ERROR, PSHELL_WARNING, PSHELL_INFO */
#define PSHELL_LOG_LEVEL_ALL       PSHELL_LOG_LEVEL_INFO
#define PSHELL_LOG_LEVEL_DEFAULT   PSHELL_LOG_LEVEL_WARNING

void pshell_setControlLogLevel(unsigned level_);

/*
 * pshell_registerControlLogFunction:
 *
 * typedef and function to allow a client program to register a logging
 * function for message output logging, if no output function is registered
 * 'printf' will be used to print out the log messages
 */
typedef void (*PshellLogFunction)(const char *outputString_);
void pshell_registerControlLogFunction(PshellLogFunction logFunction_);

/*
 * the following enum values are returned by the non-extraction
 * based pshell_sendCommand1 and pshell_sendCommand2 functions
 */
enum PshellControlResults
{
  /*
   * the following "COMMAND" enums are returned by the remote pshell server
   * and must match their corresponding values in PshellServer.cc
   */
  PSHELL_COMMAND_SUCCESS,        
  PSHELL_COMMAND_NOT_FOUND,      
  PSHELL_COMMAND_INVALID_ARG_COUNT,
  /* the following "SOCKET" enums are generated internally by the pshell_sendCommandN functions */
  PSHELL_SOCKET_SEND_FAILURE,
  PSHELL_SOCKET_SELECT_FAILURE,
  PSHELL_SOCKET_RECEIVE_FAILURE,
  PSHELL_SOCKET_TIMEOUT,
  PSHELL_SOCKET_NOT_CONNECTED,
};

/*
 * pshell_getResultsString:
 * 
 * map the above enums to their corresponding strings
 */
const char *pshell_getResultsString(int results_);

/*
 * pshell_connectServer:
 *
 * connect to a pshell server in another process, note that this does
 * not do any handshaking to the remote pshell or maintain a connection
 * state, it meerly sets the internal destination remote pshell server
 * information to use when sending commands via the pshell_sendCommand
 * function and sets up any resources necessary for the source socket,
 * the timeout value is the number of milliseconds to wait for a response
 * from the remote command in the pshell_sendCommandN functions, a timeout
 * value of 0 will not wait for a response, in which case the function
 * will return either PSHELL_SOCKET_NOT_CONNECTED, PSHELL_SOCKET_SEND_FAILURE,
 * or PSHELL_COMMAND_SUCCESS, the timeout value entered in this funcition 
 * will be used as the default timeout for all calls to pshell_sendCommandN 
 * that do not provide an override timeout value, for a UDP server, the 
 * remoteServer must be either a valid hostname or IP address and a valid 
 * destination port must be provided, for a UNIX server, only a valid server 
 * name must be provided along with the identifier PSHELL_UNIX_SERVER (i.e. 0) 
 * for the 'port' parameter
 *
 * this function returns a Server ID (sid) handle which must be saved and
 * used for all subsequent calls into this library, if the function fails
 * to allocate the necessary resources and connect to a server,
 * PSHELL_INVALID_SID is returned
 */
#define PSHELL_INVALID_SID -1
#define PSHELL_UNIX_SERVER  0
#define PSHELL_NO_WAIT      0
#define PSHELL_ONE_MSEC     1
#define PSHELL_ONE_SEC      PSHELL_ONE_MSEC*1000

int pshell_connectServer(const char *controlName_,
                         const char *remoteServer_,
                         unsigned port_ = PSHELL_UNIX_SERVER,
                         unsigned defaultTimeout_ = PSHELL_NO_WAIT);

/*
 * pshell_disconnectServer:
 *
 * cleanup any resources associated with the server connection,
 * including releasing any dynamically allocated memory, closing
 * any local socket handles etc.
 */
void pshell_disconnectServer(int sid_);

/*
 * pshell_disconnectAllServers:
 *
 * use this function to cleanup any resources for all connected servers,
 * this function should be called upon program termination, either in a
 * graceful termination or within an exception signal handler, it is especially
 * important that ths be called when a unix server is used since there are
 * associated file handles that need to be cleaned up
 */
void pshell_disconnectAllServers(void);

/*
 * pshell_setDefaultTimeout:
 *
 * set the default server response timeout that is used in the
 * 'send' commands that don't take a timeout override
 */
void pshell_setDefaultTimeout(int sid_, unsigned defaultTimeout_);

/*
 * pshell_extractCommands:
 *
 * this function will extract all the commands or a remote pshell
 * server and present them in a human readable form, this is useful
 * when writing a multi server control aggregator, see the demo program
 * pshellAggregatorDemo in the demo directory for examples
 */
void pshell_extractCommands(int sid_, char *results_, int size_);

/*
 * pshell_sendCommandN:
 *
 * these functions are used to send a pshell command to the remote process,
 * the format of the command is the same as the command's usage as registered
 * on the remote pshell server via the pshell_addCommand function, this function
 * provides a 'printf' style varargs interface to allow the caller to format
 * their command string directly in this function call rather than having to
 * create a separate string and calling 'sprintf' to format it
 *
 * there are 4 version of this command, since we are using standard 'C' linkage,
 * we cannot overload the function names, hence the separate names
 * 
 * the return of the following 2 functions is one of the PshellControlResults
 */
int pshell_sendCommand1(int sid_, const char *command_, ...);
int pshell_sendCommand2(int sid_, unsigned timeoutOverride_, const char *command_, ...);

/*
 * the following two commands will issue the remote command and extract any
 * results that are returned in the payload of the message, this will only
 * work with a non-0 timeout and a return result of PSHELL_COMMAND_SUCCESS,
 * the data in the payload is ascii formatted character data, it is the
 * responsibility of the calling application to parse & understand the results,
 * the return code of these functions is the number of bytes extracted
 */
int pshell_sendCommand3(int sid_, char *results_, int size_, const char *command_, ...);
int pshell_sendCommand4(int sid_, char *results_, int size_, unsigned timeoutOverride_, const char *command_, ...);

#ifdef __cplusplus
}
#endif

#endif
