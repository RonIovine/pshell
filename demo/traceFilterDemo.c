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

#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include <PshellServer.h>
#include <TraceLog.h>

/*******************************************************************************
 *
 * This is an example demo program that shows how to use the dynamically
 * controllable & configurable trace filtering mechanism.  This filtering
 * mechanism can be conficured interactivley on a love process via the
 * pshell interface.  This program utilizes a pshell server that can be
 * started as a UDP, UNIX, or TCP server.
 *
 *******************************************************************************/

/* the following items are examples for the dynamic trace filtering feature */

/* need to place this macro at the top of every source file in order to do
 * filename filtering if using the TF_FAST_FILENAME_LOOKUP compile flag,
 * if this flag is set, a simple pointer compare is done between filenames
 * when using a file filter, if not, a full strcmp of the filenames is done,
 * this macro should be typically be placed at the very top of the source file
 * immediatly after all the 'include' statements
 */
TF_SYMBOL_TABLE; 

/* a couple of functions to show function name filtering */

/******************************************************************************/
/******************************************************************************/
void foo(void)
{
  TRACE_ENTER("message 1");
  sleep(1);
  TRACE_EXIT("message 2");
}

/******************************************************************************/
/******************************************************************************/
void bar(void)
{
  TRACE_ENTER("message 1");
  sleep(1);
  TRACE_EXIT("message 2");
}

/******************************************************************************/
/******************************************************************************/
void *myThread(void*)
{

  /* 
   * add a trace thread name, this should be done in the initialization
   * of each thread (i.e. before the "infinite" loop), this will allow
   * the TraceFilter to be able to do thread based filtering 
   */
  tf_registerThread("myThread");

  for (;;)
  {
    TRACE_WARNING("message 1");
    sleep(1);
    TRACE_INFO("message 2");
    sleep(1);
    TRACE_DEBUG("message 3");
    sleep(1);
    TRACE_ERROR("message 4");
    sleep(1);
    TRACE_FAILURE("message 5");
    sleep(1);
  }

  return (NULL);

}

unsigned watchAddress = 0;

/* sample trace callback function */

/******************************************************************************/
/******************************************************************************/
bool callbackCondition = false;
bool callbackFunction(void)
{
  TRACE_INFO("Sample callback function called");
  return (callbackCondition);
}

/******************************************************************************/
/******************************************************************************/
void showUsage(void)
{
  printf("\n");
  printf("Usage: traceFilterDemo -udp | -unix | -tcp\n");
  printf("\n");
  printf("  where:\n");
  printf("    -udp   - Multi-session UDP server\n");
  printf("    -unix  - Multi-session UNIX domain server\n");
  printf("    -tcp   - Single session TCP server\n");
  printf("\n");
}

/******************************************************************************/
/******************************************************************************/
void setTriggers(int argc, char *argv[])
{
  if (pshell_isSubString(argv[0], "callback", 1))
  {
    if (pshell_isSubString(argv[1], "true", 1))
    {
      callbackCondition = true;
    }
    else if (pshell_isSubString(argv[1], "false", 1))
    {
      callbackCondition = false;
    }
    else
    {
      pshell_showUsage();
    }
  }
  else if (pshell_isSubString(argv[0], "watchpoint", 1))
  {
    if (pshell_isNumeric(argv[1]))
    {
      watchAddress = pshell_getUnsigned(argv[1]);
    }
    else
    {
      pshell_showUsage();
    }
  }
  else
  {
    pshell_showUsage();
  }
}

/******************************************************************************/
/******************************************************************************/
void sampleLogFunction(const char *outputString_)
{
  /*
   * this sample log function is registered with the TraceLog '
   * trace_registerLogFunction' call ot show a client supplied
   * log function, the string passed in is a fully formatted log
   * string, it is up to the registering application to decide
   * what to do with that string, i.e. write to stdout, write to
   * custom logfile, write to syslog etc
   */
   printf(outputString_);
}

/*
 * setup our port number, this is the default port number used
 * if our serverName is not found in the pshell.servers file
 */
#define TF_DEMO_PORT 6002
#define DUMP_BUFFER_SIZE 256

/******************************************************************************/
/******************************************************************************/
int main (int argc, char *argv[])
{

  PshellServerType serverType;
  char dumpBuffer[DUMP_BUFFER_SIZE];

  /* validate our command line arguments and get the server type */
  if (argc == 2)
  {
    if (strcmp(argv[1], "-udp") == 0)
    {
      serverType = PSHELL_UDP_SERVER;
    }
    else if (strcmp(argv[1], "-unix") == 0)
    {
      serverType = PSHELL_UNIX_SERVER;
    }
    else if (strcmp(argv[1], "-tcp") == 0)
    {
      serverType = PSHELL_TCP_SERVER;
    }
    else
    {
      showUsage();
      return (0);
    }
  }
  else
  {
    showUsage();
    return (0);
  }

  /* initialize a sample memory hex dump buffer */
  for (unsigned i = 0; i < DUMP_BUFFER_SIZE; i++)
  {
    dumpBuffer[i] = i;
  }

  /* must register our trace levels before callinig 'tf_init' */
  trace_registerLevels();

  /* 
   * add a trace thread name, this should be done in the initialization
   * of each thread (i.e. before the "infinite" loop), this will allow
   * the TraceFilter to be able to do thread based filtering 
   */
   
  tf_registerThread("main");

  /*
   * optionally set a log prefix, if not set, 'TRACE' will be used,
   * if set to 'NULL',no prefix will be used
   */
  trace_setLogPrefix("demo");

  /*
   * register a custom client provided log file, it is up to the
   * registering application to decide what to do with that string,
   * i.e. write to stdout, write to custom logfile, write to syslog
   * etc
   */
  trace_registerLogFunction(sampleLogFunction);

  /* initialize our dynamic trace filtering feature, this should be done
   * at the beginning of the program, right after the 'main' and before
   * any registration of pshell user commands, it will first look for the
   * file based on the TF_STARTUP_DIR env variable, and if not found
   * will open this file as an absolute path, call this function with 
   * 'NULL' if no startup file is desired, this call as well as all the
   * other 'tf_' calls are optional and can be omitted if dynamic trace 
   * filtering is not used
   */
   
  tf_init("traceFilterDemo.conf");

  /*
   * Register all our pshell callback commands here, after the 'tf_init'
   */

  pshell_addCommand(setTriggers,                                              /* function */
                    "set",                                                    /* command */
                    "set the callback and watchpoint trace trigger values",   /* description */
                    "{callback  {true | false}} | {watchpoint <value>}",      /* usage */
                    2,                                                        /* minArgs */
                    2,                                                        /* maxArgs */
                    true);                                                    /* showUsage on "?" */

  /* turn off all our traces */
  pshell_runCommand("trace off");

  /*
   * register a memory address watchpoint to continuously look for a change in the contents
   * at every trace statement, if the control is set to TF_CONTINUOUS, an indication will
   * be printed for every change detected, if the control is set to TF_ONCE, only the first
   * change will be shown, if the control is TF_ABORT, a program termination (with core file)
   * will be forced upon the first detected change
   */
   
  TF_WATCH("watchAddress", &watchAddress, sizeof(watchAddress), "0x%x", TF_CONTINUOUS);
   
  /*
   * register a callback function to be called at every trace statement, the function must
   * return either 'true' or 'false', an indication will be  printed upon the first transition
   * from 'false' to 'true', if the control is set to TF_CONTINUOUS, an indication will be
   * printed out for every transition (i.e. true->false, false->true), if the control is set
   * to TF_ABORT, a program termination (with core file) will be forced upon the first detected
   * transition
   */
   
  TF_CALLBACK("callbackFunction", callbackFunction, TF_CONTINUOUS);

  /* issue a trace so we can trigger the TRACE_WATCH functionality */
  TRACE_INFO("First trace");

  /* start our pshell server */
  pshell_startServer("traceFilterDemo", serverType, PSHELL_NON_BLOCKING, "localhost", TF_DEMO_PORT);
   
  /* create a sample thread to show thread based filtering */
  pthread_t threadId;
  pthread_create(&threadId, NULL, myThread, NULL);

  /* go into an infinite loop issuing some traces so we can demonstrate dynamic trace filtering */
  for (;;)
  {
    TRACE_WARNING("message 1");
    sleep(1);
    TRACE_INFO("message 2");
    sleep(1);
    foo();
    sleep(1);
    bar();
    sleep(1);
    TRACE_DEBUG("message 3");
    sleep(1);
    TRACE_DUMP(dumpBuffer, sizeof(dumpBuffer), "dumping buffer: dumpBuffer");
    sleep(1);
    TRACE_ERROR("message 4");
    sleep(1);
    TRACE_FAILURE("message 5");
    sleep(1);
   }
   return (0);
}
