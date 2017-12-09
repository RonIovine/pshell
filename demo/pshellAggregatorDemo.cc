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
 * This is an example demo program that shows the use of multiple pshell control
 * interfaces to aggregate all of the remote commands from all of the connected
 * servers into a single local pshell server.  This can be useful in presenting
 * a consolidated user shell who's functionality spans several discrete pshell
 * servers.  Since this uses the PshellControl API, the external servers must
 * all be either UDP or Unix servers.  The consolidation point in this example 
 * is a local pshell server.
 *
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#include <PshellServer.h>
#include <PshellControl.h>

/******************************************************************************/
/******************************************************************************/
void signalHandler(int signal_)
{
  pshell_disconnectAllServers();
  pshell_cleanupResources();
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

/*
 * this function will re-create the original command into one
 * string from the argc, argv items
 */
 
 /******************************************************************************/
/******************************************************************************/
char *buildCommand(char *command, unsigned size, int argc, char *argv[])
 {
    /* re-constitute the original command */
    command[0] = 0;
    for (int arg = 0; ((arg < argc) && ((size - strlen(command)) > strlen(argv[arg])+1)); arg++)
    {
      snprintf(&command[strlen(command)], (size-strlen(command)), "%s ", argv[arg]);
    }
    command[strlen(command)-1] = 0;
    return (command);
 }

/*
 * this function is the common generic function to control any server
 * based on only the SID and entered command, it should be called from 
 * every control specific callback for this aggregator
 */

/* make sure the results array is big enough for any possible output */
char results[4096];

/******************************************************************************/
/******************************************************************************/
void controlServer(int sid, int argc, char *argv[])
{
  if (pshell_isHelp() || pshell_isEqual(argv[0], "help"))
  {
    pshell_extractCommands(sid, results, sizeof(results));
    pshell_printf("%s", results);
  }
  else
  {
    /* this contains the re-constituted command */
    char command[300];
    if (pshell_sendCommand3(sid, results, sizeof(results), buildCommand(command, sizeof(command), argc, argv)) > 0)
    {
      pshell_printf("%s", results);
    }
  }
}

/* 
 * the SIDs of all the remote pshell servers we are aggregating, add more SID 
 * identifiers for each external server being controlled/aggregated
 */
int pshellServerDemoSid;
int traceFilterDemoSid;

/*
 * the following two functions are the control specific functions that interface
 * directly to a given remote server via the control API for a give SID, this is
 * how multiple remote pshell servers can be aggregated into a single local pshell
 * server, each separate SID for each remote server needs it's own dedicated
 * callback that is registered to the local pshell server, the only thing these
 * local pshell functions need to do is call the common 'controlServer' function
 * with the passed in argc, argv, and their unique SID identifier
 */

/******************************************************************************/
/******************************************************************************/
void pshellServerDemo(int argc, char *argv[])
{
  controlServer(pshellServerDemoSid, argc, argv);
}

/******************************************************************************/
/******************************************************************************/
void traceFilterDemo(int argc, char *argv[])
{
  controlServer(traceFilterDemoSid, argc, argv);
}

/*
 * example meta command that will call multiple discrete pshell commands from
 * multiple pshell servers
 */
 
/******************************************************************************/
/******************************************************************************/
void meta(int argc, char *argv[])
{
  if (pshell_sendCommand3(pshellServerDemoSid, results, sizeof(results), "hello %s %s", argv[0], argv[1]) > 0)
  {
    pshell_printf("%s", results);
  }
  pshell_sendCommand1(traceFilterDemoSid, "set callback %s", argv[2]);
}

/*
 * example multicast command, this will send a given command to all the registered
 * multicast receivers for that multicast group, multicast groups are based on
 * the command's keyword
 */
 
/******************************************************************************/
/******************************************************************************/
void multicast(int argc, char *argv[])
{
  pshell_sendMulticast("test");
  pshell_sendMulticast("trace 1 2 3 4");
  pshell_sendMulticast("hello");
}

/******************************************************************************/
/******************************************************************************/
int main (int argc, char *argv[])
{
  unsigned pshellServerDemoPort = 6001;
  unsigned traceFilterDemoPort = 6002;
  
  if ((argc != 2) && (argc != 4))
  {
    printf("Usage: pshellAggregatorDemo {<hostname> | <ipAddress>} [<pshellServerDemoPort> <traceFilterDemoPort>]\n");
    exit (0);
  }
  else if ((argc == 2) && (strcmp(argv[1], "-h") == 0))
  {
    printf("Usage: pshellAggregatorDemo {<hostname> | <ipAddress>} [<pshellServerDemoPort> <traceFilterDemoPort>]\n");
    exit (0);
  }
  else if (argc == 4)
  {
    pshellServerDemoPort = atoi(argv[2]);
    traceFilterDemoPort = atoi(argv[3]);
  }
  
  /* register signal handlers so we can do a graceful termination and cleanup any system resources */
  registerSignalHandlers();
  
  /* 
   * connect to our remote servers, the hostname/ipAddress, port,
   * and timeout values can be overridden via the pshell-control.conf 
   * file
   */
  if ((pshellServerDemoSid = pshell_connectServer("pshellServerDemo", argv[1], pshellServerDemoPort, PSHELL_ONE_SEC*5)) == PSHELL_INVALID_SID)
  {
    printf("ERROR: Could not connect to remote pshell server: pshellServerControl\n");
    exit(0);
  }

  if ((traceFilterDemoSid = pshell_connectServer("traceFilterDemo", argv[1], traceFilterDemoPort, PSHELL_ONE_SEC*5)) == PSHELL_INVALID_SID)
  {
    printf("ERROR: Could not connect to remote pshell server traceFilterControl\n");
    exit(0);
  }
  
  /* 
   * add some multicast groups for our control sids, a multicast group is based
   * on the command's keyword
   */
  pshell_addMulticast(pshellServerDemoSid, "trace");
  pshell_addMulticast(traceFilterDemoSid, "trace");  
  
  pshell_addMulticast(pshellServerDemoSid, "test");
  pshell_addMulticast(traceFilterDemoSid, "test");

  /* register our local pshell commands */
  
  /* these will aggregrate the commands from the two separate servers we are connected to */  
  pshell_addCommand(pshellServerDemo,                               /* function */
                    "pshellServerDemo",                             /* command */
                    "control the remote pshellServerDemo process",  /* description */
                    "<command> | ? | -h",                           /* usage */
                    1,                                              /* minArgs */
                    30,                                             /* maxArgs */
                    false);                                         /* showUsage on "?" */

  pshell_addCommand(traceFilterDemo,                               /* function */
                    "traceFilterDemo",                             /* command */
                    "control the remote traceFilterDemo process",  /* description */
                    "<command> | ? | -h",                          /* usage */
                    1,                                             /* minArgs */
                    30,                                            /* maxArgs */
                    false);                                        /* showUsage on "?" */
                    
  /* 
   * add any "meta" commands here, meta commands can aggregate multiple discrete
   * pshell commands, either within one server or across multiple servers, into
   * one command
   */
  pshell_addCommand(meta,                                               /* function */
                    "meta",                                             /* command */
                    "meta command, wraps multiple seperate functions",  /* description */
                    "<arg1> <arg2> <arg3>",                             /* usage */
                    3,                                                  /* minArgs */
                    3,                                                  /* maxArgs */
                    true);                                              /* showUsage on "?" */   

  /* 
   * add an example command that uses the one-to-many multicast feature of
   * the control API
   */
  pshell_addCommand(multicast,                                       /* function */
                    "multicast",                                     /* command */
                    "example multicast command to several servers",  /* description */
                    NULL,                                            /* usage */
                    0,                                               /* minArgs */
                    0,                                               /* maxArgs */
                    true);                                           /* showUsage on "?" */   

  /* start our local pshell server */
  pshell_startServer("pshellAggregatorDemo", PSHELL_LOCAL_SERVER, PSHELL_BLOCKING);
  
  /* disconnect all our remote control servers */
  pshell_disconnectAllServers();
  
  /* cleanup our local server's resources */
  pshell_cleanupResources();

}
