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

#ifndef PSHELL_READLINE_H
#define PSHELL_READLINE_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *
 * This API implements a readline like functionality for user input.  This can
 * work with any character stream based input/output terminal device, i.e.
 * keyboard input over a serial tty, or over a TCP/telnet connection.  This module
 * will provide up-arrow command history recall, command line editing, and TAB
 * completion of registered keywords.
 *
 * Functions:
 *
 * pshell_rl_setFileDescriptors() -- set the input/output file descriptors
 * pshell_rl_setIdleTimeout()     -- set the idle session timeout
 * pshell_rl_addTabCompletion()   -- add a keyword to the TAB completion list
 * pshell_rl_setTabStyle()        -- sets the tab behavour style ("fast" or bash)
 * pshell_rl_getInput()           -- get a line of user input from our input file descriptor
 * pshell_rl_writeOutput()        -- write a string to our output file descriptor
 * pshell_rl_isSubString()        -- checks for string1 substring of string2 at position 0
 *
 * Use for the timeout value when setting the idleSessionTimeout, default=none
 *
 * IDLE_TIMEOUT_NONE
 * ONE_SECOND
 * ONE_MINUTE
 *
 * Valid serial types, TTY is for serial terminal control and defaults to
 * stdin and stdout, SOCKET uses a serial TCP socket placed in 'telnet'
 * mode for control via a telnet client, default=tty with stdin/stdout
 *
 * PSHELL_TTY
 * PSHELL_SOCKET
 *
 * Valid TAB style identifiers
 *
 * Standard bash/readline tabbing method, i.e. initiate via double tabbing
 *
 * PSHELL_BASH_TAB
 *
 * Fast tabbing, i.e. initiated via single tabbing, this is the default
 *
 * PSHELL_FAST_TAB
 *
 * A complete example of the usage of the API can be found in the included demo
 * program pshellReadlineDemo.cc
 *
 *******************************************************************************/

/*
 * Used to specify the TAB style, FAST TAB will only use a single TAB,
 * while BASH TAB will require double TABing
 */
enum PshellTabStyle
{
  PSHELL_FAST_TAB,
  PSHELL_BASH_TAB 
};

/*
 * valid serial types, TTY is for serial terminal control and defaults to
 * stdin and stdout, SOCKET uses a serial TCP socket placed in 'telnet'
 * mode for control via a telnet client, default=TTY with stdin/stdout
 */
enum PshellSerialType
{
  PSHELL_TTY,
  PSHELL_SOCKET
};

/* use this to sice the user supplied input string */
#define PSHELL_MAX_COMMAND_SIZE 256

/* use these to set the idle session timeout values, default=NONE */
#define IDLE_TIMEOUT_NONE 0
#define ONE_SECOND 1
#define ONE_MINUTE ONE_SECOND*60

/*
 * pshell_rl_setFileDescriptors:
 *
 * Set the input and output file descriptors, if this function is not called,
 * the default is stdin and stdout.  The file descriptors given to this function
 * must be opened and running in raw serial character mode.  The idleTimeout
 * specifies the time to wait for any user activity in the getInput function.
 * Use the identifiers PshellReadline.ONE_SECOND and PshellReadline.ONE_MINUTE
 * to set this timeout value, e.g. PshellReadline.ONE_MINUTE*5, for serialType
 * use the identifiers PshellReadline.TTY and PshellReadline.SOCKET.  For the
 * socket identifier, use the file descriptor that is returned from the TCP
 * socket server 'accept' call for both the inFd and outFd.
 */
void pshell_rl_setFileDescriptors(int inFd_,
                                  int outFd_,
                                  PshellSerialType serialType_,
                                  int idleTimeout_ = IDLE_TIMEOUT_NONE);

/*
 * pshell_rl_writeOutput:
 *
 * Write a string to our output file descriptor
 */
void pshell_rl_writeOutput(const char* format_, ...);

/*
 * pshell_rl_getInput:
 *
 * Issue the user prompt and return the entered command line value.  This
 * function will return the tuple (command, idleSession).  If the idle session
 * timeout is set to IDLE_TIMEOUT_NONE (default), the idleSession will always
 * be false and this function will not return until the user has typed a command
 * and pressed return.  Otherwise this function will set the idleSession value
 * to true and return if no user activity is detected for the idleSessionTimeout
 * period
 */
bool pshell_rl_getInput(const char *prompt_, char *input_);

/*
 * pshell_rl_isSubString:
 *
 * This function will return True if string1 is a substring of string2 at
 * position 0.  If the minMatchLength is 0, then it will compare up to the
 * length of string1.  If the minMatchLength > 0, it will require a minimum
 * of that many characters to match.  A string that is longer than the min
 * match length must still match for the remaining characters, e.g. with a
 * minMatchLength of 2, 'q' will not match 'quit', but 'qu', 'qui' or 'quit'
 * will match, 'quix' or 'uit' will not match.  This function is useful for
 * wildcard matching.
 */
bool pshell_rl_isSubString(const char *string1_,
                           const char *string2_,
                           unsigned minChars_ = 0);

/*
 * pshell_rl_addTabCompletion:
 * 
 * Add a keyword to the TAB completion list.  TAB completion will only be applied
 * to the first keyword of a given user typed command
 */
void pshell_rl_addTabCompletion(const char *keyword_);

/*
 * pshell_rl_setTabStyle:
 * 
 * Set the tabbing method to either be bash/readline style tabbing, i.e. double
 * tabbing to initiate and display completions, or "fast" tabbing, where all
 * completions and displays are initiated via a single tab only, the default is
 * "fast" tabbing, use the identifiers PSHELL_BASH_TAB and PSHELL_FAST_TAB for
 * the tabStyle
 */
void pshell_rl_setTabStyle(PshellTabStyle tabStyle_);

/*
 * pshell_rl_setIdleTimeout:
 * 
 * Set the idle session timeout as described above.  Use the identifiers
 * ONE_SEC and ONE_MINUTE as follows, e.g. ONE_MINUTE*5 for a 5 minute
 * idleSession timeout.
 */
void pshell_rl_setIdleTimeout(int timeout_);

#ifdef __cplusplus
}
#endif

#endif
