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

#ifndef PSHELL_COMMON_H
#define PSHELL_COMMON_H

#include <netdb.h>

/* 
 * this file contains items that are used in common
 * between the pshell UDP/UNIX client and server
 */

#define PSHELL_UNIX_SOCKET_PATH  "/tmp"

/* 
 * this is just used for the initial receive message used by the 
 * UDP/UNIX client, the actual payload size is then negotiated between 
 * the UDP/UNIX client and server, this is also used as-is for the client 
 * side transmit message (i.e. in the "send"), so the payload size 
 * needs to be big enough for any reasonable sized command and
 * its arguments
 */
#define PSHELL_PAYLOAD_SIZE 1024*4   /* 4k payload size */

/* message structures used to communicate between the client and server */

struct PshellMsgHdr
{
  uint8_t msgType;
  uint8_t respNeeded;
  uint8_t dataNeeded;
  uint8_t pad;
  uint32_t seqNum;
};

struct PshellMsg
{
  PshellMsgHdr header;
  char payload[PSHELL_PAYLOAD_SIZE];
};

#define PSHELL_HEADER_SIZE sizeof(PshellMsgHdr)

/* 
 * the first message exchanged on the UDP/UNIX client/server handshake
 * is a request for the version info, the client and server need 
 * to be running compatible versions for the session to continue.  
 * The version number should only be changed if the actual handshaking 
 * protocol used between the client and server changes
 */
 
#define PSHELL_VERSION_1 1
#define PSHELL_VERSION PSHELL_VERSION_1

/*
 * These are all the message types used between the UDP/UNIX client and
 * server, all of these values are put in the msgType field of the PshellMsg
 */
#define PSHELL_QUERY_VERSION        1   /* pshell client initiated */
#define PSHELL_QUERY_PAYLOAD_SIZE   2   /* pshell client initiated */
#define PSHELL_QUERY_NAME           3   /* pshell client initiated */
#define PSHELL_QUERY_COMMANDS1      4   /* pshell client initiated, for 'help' display info */
#define PSHELL_QUERY_COMMANDS2      5   /* pshell client initiated, for TAB completion info */
#define PSHELL_UPDATE_PAYLOAD_SIZE  6   /* server initiated, upon buffer overflow */
#define PSHELL_USER_COMMAND         7   /* pshell client initiated */
#define PSHELL_COMMAND_COMPLETE     8   /* server initiated, in response to pshell client request */
#define PSHELL_QUERY_BANNER         9   /* pshell client initiated */
#define PSHELL_QUERY_TITLE          10  /* pshell client initiated */
#define PSHELL_QUERY_PROMPT         11  /* pshell client initiated */
#define PSHELL_CONTROL_COMMAND      12  /* control client initiated */

#define PSHELL_COMMAND_DELIMETER "/"  /* delimits commands for queryCommands2 */

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define XSTR(x) #x
#define STR(x) XSTR(x)

#define PSHELL_WELCOME_BORDER "#"
#define PSHELL_PRINT_WELCOME_BORDER(print, length) {for (int i = 0; i < MAX((int)(length+2), 56); i++) print(PSHELL_WELCOME_BORDER); print("\n");}

#endif
