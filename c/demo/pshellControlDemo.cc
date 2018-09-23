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

/*******************************************************************************
 *
 * This is an example demo program that shows how to use the pshell control
 * interface.  This API will allow any external client program to invoke
 * functions that are registered within a remote program running a pshell
 * server.  This will only control programs that are running either a UDP
 * or UNIX pshell server.
 *
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#include <PshellControl.h>

/******************************************************************************/
/******************************************************************************/
void showUsage(void)
{
  printf("\n");
  printf("Usage: pshellControlDemo {<hostname> | <ipAddress> | <unixServerName>} {<port> | unix}\n");
  printf("                         [-t<timeout>] [-l<logLevel>] [-extract]\n");
  printf("\n");
  printf("  where:\n");
  printf("    <hostname>       - hostname of UDP server\n");
  printf("    <ipAddress>      - IP address of UDP server\n");
  printf("    <unixServerName> - name of UNIX server\n");
  printf("    unix             - specifies a UNIX server\n");
  printf("    <port>           - port number of UDP server\n");
  printf("    <timeout>        - wait timeout for response in mSec (default=100)\n");
  printf("    <logLevel>       - log level of control library (0-3, default=3, i.e. all)\n");
  printf("    extract          - extract data contents of response (must have non-0 wait timeout)\n");
  printf("\n");
  exit(0);
}

/******************************************************************************/
/******************************************************************************/
void signalHandler(int signal_)
{
  pshell_disconnectAllServers();
  printf("\n");
  exit(0);
}

/******************************************************************************/
/******************************************************************************/
void registerSignalHandlers(void)
{
  signal(SIGHUP, signalHandler);    /* 1  Hangup (POSIX).  */
  signal(SIGINT, signalHandler);    /* 2  Interrupt (ANSI).  */
  signal(SIGQUIT, signalHandler);   /* 3  Quit (POSIX).  */
  signal(SIGILL, signalHandler);    /* 4  Illegal instruction (ANSI).  */
  signal(SIGABRT, signalHandler);   /* 6  Abort (ANSI).  */
  signal(SIGBUS, signalHandler);    /* 7  BUS error (4.2 BSD).  */
  signal(SIGFPE, signalHandler);    /* 8  Floating-point exception (ANSI).  */
  signal(SIGSEGV, signalHandler);   /* 11 Segmentation violation (ANSI).  */
  signal(SIGPIPE, signalHandler);   /* 13 Broken pipe (POSIX).  */
  signal(SIGALRM, signalHandler);   /* 14 Alarm clock (POSIX).  */
  signal(SIGTERM, signalHandler);   /* 15 Termination (ANSI).  */
#ifdef LINUX
  signal(SIGSTKFLT, signalHandler); /* 16 Stack fault.  */
#endif
  signal(SIGXCPU, signalHandler);   /* 24 CPU limit exceeded (4.2 BSD).  */
  signal(SIGXFSZ, signalHandler);   /* 25 File size limit exceeded (4.2 BSD).  */
  signal(SIGSYS, signalHandler);    /* 31 Bad system call.  */
}

/******************************************************************************/
/******************************************************************************/
int main (int argc, char *argv[])
{
  char inputLine[180];
  char results[4096];
  int timeout = PSHELL_ONE_MSEC*100;
  int port = 0;
  int logLevel = PSHELL_CONTROL_LOG_LEVEL_ALL;
  bool extract = false;
  int retCode;
  int sid;

  if ((argc < 3) || (argc > 6))
  {
    showUsage();
  }

  for (int i = 3; i < argc; i++)
  {
    if (strncmp(argv[i], "-t", 2) == 0)
    {
      timeout = atoi(&argv[i][2]);
    }
    else if (strncmp(argv[i], "-l", 2) == 0)
    {
      logLevel = atoi(&argv[i][2]);
    }
    else if (strcmp(argv[i], "-extract") == 0)
    {
      extract = true;
    }
    else
    {
      showUsage();
    }
  }

  if (strcmp(argv[2], "unix") != 0)
  {
    port = atoi(argv[2]);
  }

  /* register signal handlers so we can do a graceful termination and cleanup any system resources */
  registerSignalHandlers();

  if ((sid = pshell_connectServer(argv[0], argv[1], port, timeout)) != PSHELL_INVALID_SID)
  {
    // set our debug level
    pshell_setControlLogLevel(logLevel);
    inputLine[0] = 0;
    printf("Enter command or 'q' to quit\n");
    while (inputLine[0] != 'q')
    {
      inputLine[0] = 0;
      while (strlen(inputLine) == 0)
      {
        fprintf(stdout, "%s", "pshellControlCmd> ");
        fflush(stdout);
        fgets(inputLine, sizeof(inputLine), stdin);
        inputLine[strlen(inputLine)-1] = 0;
      }
      if (inputLine[0] != 'q')
      {
        if (extract)
        {
          retCode = pshell_sendCommand3(sid, results, sizeof(results), inputLine);
          if (retCode == PSHELL_COMMAND_SUCCESS)
          {
            printf("%d bytes extracted, results:\n", (int)strlen(results));
            printf("%s", results);
          }
          printf("retCode: %s\n",pshell_getResponseString(retCode));
        }
        else
        {
          retCode = pshell_sendCommand1(sid, inputLine);
          printf("retCode: %s\n",pshell_getResponseString(retCode));
        }
      }
    }
    pshell_disconnectServer(sid);
  }

}
