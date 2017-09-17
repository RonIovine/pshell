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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#include <PshellServer.h>
#include <PshellControl.h>

/*******************************************************************************
 *
 * This is an example demo program that shows the use of multiple pshell control
 * interfaces to aggregate all of the remote commands from all of the connected
 * servers into a single local pshell server
 *
 *******************************************************************************/

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
  signal(SIGSTKFLT, signalHandler); /* 16 Stack fault.  */
  signal(SIGXCPU, signalHandler);   /* 24 CPU limit exceeded (4.2 BSD).  */
  signal(SIGXFSZ, signalHandler);   /* 25 File size limit exceeded (4.2 BSD).  */
  signal(SIGPWR, signalHandler);    /* 30 Power failure restart (System V).  */
  signal(SIGSYS, signalHandler);    /* 31 Bad system call.  */
}

/* make sure the results array is big enough for any possible output */
char results[4096];
char command[300];

/* the SIDs of all the remote pshell servers we are aggregating */
int pshellServerDemoSid;
int traceFilterDemoSid;

/*
 * this function is a common function to show all the remote commands
 * for a given control sid, it should be called from every control 
 * specific callback for this aggregator when the user asks for help
 */

/******************************************************************************/
/******************************************************************************/
void showRemoteCommands(int sid)
{
    results[0] = 0;
    pshell_extractCommands(sid, results, sizeof(results));
    pshell_printf("%s", results);
}

/*
 * this function is a common function to dispatch a given remote command
 * for a given control sid, it should be called from every control 
 * specific callback for any non-help command
 */

/******************************************************************************/
/******************************************************************************/
void dispatchRemoteCommand(int sid, int argc, char *argv[])
{
    command[0] = 0;
    results[0] = 0;
    for (int arg = 0; arg < argc; arg++)
    {
      sprintf(&command[strlen(command)], "%s ", argv[arg]);
    }
    command[strlen(command)-1] = 0;
    pshell_sendCommand3(sid, results, sizeof(results), command);
    pshell_printf("%s\n", results);
}

/*
 * the following two functions are the control specific functions that interface
 * directly to a given remote server via the control API for a give SID, this is
 * how multiple remote pshell servers can be aggregated into a single pshell
 * server
 */

/******************************************************************************/
/******************************************************************************/
void pshellServerControl(int argc, char *argv[])
{
  if (pshell_isHelp() || pshell_isEqual(argv[0], "help"))
  {
    showRemoteCommands(pshellServerDemoSid);
  }
  else
  {
    dispatchRemoteCommand(pshellServerDemoSid, argc, argv);
  }
}

/******************************************************************************/
/******************************************************************************/
void traceFilterControl(int argc, char *argv[])
{
  if (pshell_isHelp() || pshell_isEqual(argv[0], "help"))
  {
    showRemoteCommands(traceFilterDemoSid);
  }
  else
  {
    dispatchRemoteCommand(traceFilterDemoSid, argc, argv);
  }
}

#define PSHELL_DEMO_PORT 6001
#define TF_DEMO_PORT 6002

/******************************************************************************/
/******************************************************************************/
int main (int argc, char *argv[])
{
  
  if (argc != 2)
  {
    printf("Usage: pshellAggregatorDemo <remoteServer>\n");
    exit (0);
  }
  
  /* 
   * connect to our remote servers, the hostname/ipAddress, port,
   * and timeout values can be overridden via the pshell-control.conf 
   * file
   */
  if ((pshellServerDemoSid = pshell_connectServer("pshellServerDemo", argv[1], PSHELL_DEMO_PORT, PSHELL_ONE_SEC*5)) == PSHELL_INVALID_SID)
  {
    printf("ERROR: Could not connect to remote pshell server: pshellServerControl\n");
    exit(0);
  }

  if ((traceFilterDemoSid = pshell_connectServer("traceFilterDemo", argv[1], TF_DEMO_PORT, PSHELL_ONE_SEC*5)) == PSHELL_INVALID_SID)
  {
    printf("ERROR: Could not connect to remote pshell server traceFilterControl\n");
    exit(0);
  }
  
  /* register our local pshell commands */
  
  /* these will aggregrate the commands from the two separate servers we are connected to */
  
  pshell_addCommand(pshellServerControl,                            /* function */
                    "pshellServerDemo",                             /* command */
                    "control the remote pshellServerDemo process",  /* description */
                    "<command> | ? | -h",                           /* usage */
                    1,                                              /* minArgs */
                    30,                                             /* maxArgs */
                    false);                                         /* showUsage on "?" */

  pshell_addCommand(traceFilterControl,                            /* function */
                    "traceFilterDemo",                             /* command */
                    "control the remote traceFilterDemo process",  /* description */
                    "<command> | ? | -h",                          /* usage */
                    1,                                             /* minArgs */
                    30,                                            /* maxArgs */
                    false);                                        /* showUsage on "?" */

  /* start our local pshell server */
  pshell_startServer("pshellAggregatorDemo", PSHELL_LOCAL_SERVER, PSHELL_BLOCKING);
  
  /* disconnect all out remote control servers */
  pshell_disconnectAllServers();

}
