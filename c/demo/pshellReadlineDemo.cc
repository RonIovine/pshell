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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <PshellReadline.h>

/*******************************************************************************
 *
 * This demo program shows the usage of the PshellReadline module.  This module
 * provides functionality similar to the readline library.  This will work with
 * any character based input stream, i.e.  terminal, serial, TCP etc.
 * 
 *******************************************************************************/

/******************************************************************************/
/******************************************************************************/
void showUsage(void)
{
  printf("\n");
  printf("Usage: pshellReadlineDemo {-tty | -socket} [-bash | -fast] [<idleTimeout>]\n");
  printf("\n");
  printf("  where:\n");
  printf("    -tty          - serial terminal using stdin and stdout (default)\n");
  printf("    -socket       - TCP socket terminal using telnet client\n");
  printf("    -bash         - Use bash/readline style tabbing\n");
  printf("    -fast         - Use \"fast\" style tabbing (default)\n");
  printf("    <idleTimeout> - the idle session timeout in minutes (default=none)\n");
  printf("\n");
  exit(0);
}

/******************************************************************************/
/******************************************************************************/
int main(int argc, char *argv[])
{
  char input[PSHELL_MAX_COMMAND_SIZE] = {0};
  bool idleSession = false;
  int on = 1;
  struct sockaddr_in ipAddress;
  int socketFd;
  int connectFd;
  SerialType serialType = PSHELL_TTY;
  int idleTimeout = IDLE_TIMEOUT_NONE;
  int port = 9005;

  if (argc > 4)
  {
    showUsage();
  }

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-bash") == 0)
    {
      pshell_setTabStyle(PSHELL_BASH_TAB);
    }
    else if (strcmp(argv[i], "-fast") == 0)
    {
      pshell_setTabStyle(PSHELL_FAST_TAB);
    }
    else if (strcmp(argv[i], "-tty") == 0)
    {
      serialType = PSHELL_TTY;
    }
    else if (strcmp(argv[i], "-socket") == 0)
    {
      serialType = PSHELL_SOCKET;
    }
    else if (strcmp(argv[i], "-h") == 0)
    {
      showUsage();
    }
    else
    {
      for (unsigned j = 0; j < strlen(argv[i]); j++)
      {
	if (!isdigit(argv[i][j]))
	{
	  showUsage();
	}
      }
      idleTimeout = ONE_MINUTE*atoi(argv[i]);;
    }
  }

  pshell_addTabCompletion("quit");
  pshell_addTabCompletion("help");
  pshell_addTabCompletion("hello");
  pshell_addTabCompletion("world");
  pshell_addTabCompletion("enhancedUsage");
  pshell_addTabCompletion("keepAlive");
  pshell_addTabCompletion("pshellAggregatorDemo");
  pshell_addTabCompletion("pshellControlDemo");
  pshell_addTabCompletion("pshellReadlineDemo");
  pshell_addTabCompletion("pshellServerDemo");
  pshell_addTabCompletion("myComm");
  pshell_addTabCompletion("myCommand123");
  pshell_addTabCompletion("myCommand456");
  pshell_addTabCompletion("myCommand789");

  if (serialType == PSHELL_SOCKET)
  {
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    ipAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    ipAddress.sin_family = AF_INET;
    ipAddress.sin_port = htons(port);
    bind(socketFd, (struct sockaddr *) &ipAddress, sizeof(ipAddress));
    listen(socketFd, 1);

    printf("waiting for a connection on port %d, use 'telnet localhost %d' to connect\n", port, port);
    connectFd = accept(socketFd, NULL, 0);
    printf("connection accepted\n");

    pshell_setFileDescriptors(connectFd, connectFd, PSHELL_SOCKET);

    shutdown(socketFd, SHUT_RDWR);
  }
	     
  pshell_setIdleTimeout(idleTimeout);

  while (!pshell_isSubString(input, "quit", 1) && !idleSession)
  {
    idleSession = pshell_getInput("prompt> ", input);
    if (!idleSession)
    {
      pshell_writeOutput("input: '%s'\n", input);
   }
  }
  
  return (0);
  
}
