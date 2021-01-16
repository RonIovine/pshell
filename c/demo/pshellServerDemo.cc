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
 * This is an example demo program that uses all the basic features of the
 * PSHELL library, this program can be run as either a UDP, TCP,  UNIX, or
 * local server based on the command line options.  If it is run as a UDP
 * or UNIX based server, you must use the provided stand-alone UDP client
 * program 'pshell' to connect to it, if using the TCP based server you must
 * use a standard  'telnet' client, if it is run as a local server, user
 * command line input is solicited directly from this program, no external
 * client is needed.
 *
 *******************************************************************************/

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#include <PshellServer.h>

/*
 * setup our port number, this is the default port number used
 * if our serverName is not found in the pshell-server.conf file
 */
#define PSHELL_DEMO_PORT 6001

/* constants used for the advanced parsing date/time stamp range checking */
#define MAX_YEAR   3000
#define MAX_MONTH  12
#define MAX_DAY    31
#define MAX_HOUR   23
#define MAX_MINUTE 59
#define MAX_SECOND 59

/* dynamic value used for the dynamicOutput function */
char dynamicValue[80] = {"0"};

/******************************************************************************
 *
 * PSHELL user callback functions, the interface is identical to the "main" in
 * C, with the argc being the argument count (excluding the actual command
 * itself), and argv[] being the argument list (again, excluding the command).
 *
 * Use the special 'pshell_printf' function call to display data back to the
 * remote client.  The interface to this function is exactly the same as the
 * standard 'printf' function.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * helloWorld:
 *
 * Simple helloWorld command that just prints out all the passed in arguments
 *
 ******************************************************************************/
void helloWorld(int argc, char *argv[])
{
  pshell_printf("helloWorld command dispatched:\n");
  /* dump out our args */
  for (int i = 0; i < argc; i++)
  {
    pshell_printf("  argv[%d]: '%s'\n", i, argv[i]);
  }
}

/******************************************************************************
 *
 * keepAlive:
 *
 * This command shows an example client keep alive, the PSHELL UDP client has
 * a default 5 second timeout, if a command will be known to take longer than
 * 5 seconds, it must give some kind of output back to the client, this shows
 * the two helper functions created the assist in this, the TCP client does not
 * need a keep alive since the TCP protocol itself handles that, but it can be
 * handy to show general command progress for commands that take a long time
 *
 ******************************************************************************/
void keepAlive(int argc, char *argv[])
{
  if (pshell_isHelp())
  {
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("Note, this function demonstrates intermediate flushes in a\n");
    pshell_printf("callback command to keep the UDP/UNIX interactive client from\n" );
    pshell_printf("timing out for commands that take longer than the response\n");
    pshell_printf("timeout (default=5 sec).  This is only supported in the 'C'\n");
    pshell_printf("version of the pshell interactive client, the Python version\n");
    pshell_printf("of the interactive client does not support intermediate flushes.\n");
    pshell_printf("\n");
    return;
  }
  else if (pshell_isEqual(argv[0], "dots"))
  {
    pshell_printf("marching dots keep alive:\n");
    for (unsigned i = 0; i < 10; i++)
    {
      pshell_march(".");
      sleep(1);
    }
  }
  else if (pshell_isEqual(argv[0], "bang"))
  {
    pshell_printf("marching 'bang' keep alive:\n");
    for (unsigned i = 0; i < 10; i++)
    {
      pshell_march("!");
      sleep(1);
    }
  }
  else if (pshell_isEqual(argv[0], "pound"))
  {
    pshell_printf("marching pound keep alive:\n");
    for (unsigned i = 0; i < 10; i++)
    {
      pshell_march("#");
      sleep(1);
    }
  }
  else if (pshell_isEqual(argv[0], "wheel"))
  {
    pshell_printf("spinning wheel keep alive:\n");
    for (unsigned i = 0; i < 100; i++)
    {
      /* string is optional, use NULL to omit */
      pshell_wheel("optional string: ");
      sleep(1);
    }
  }
  else
  {
    pshell_showUsage();
    return;
  }
  pshell_printf("\n");
}

/******************************************************************************
 *
 * wildcardMatch:
 *
 * This command shows matching the passed command arguments based on substring
 * matching rather than matching on the complete exact string, the minimum
 * number of characters that must be matched is the last argument to the
 * pshell_isSubString function, this must be the minimum number of characters
 * necessary to uniquely identify the argument from the complete argument list
 *
 * NOTE: This technique could have been used in the previous example for the
 *       "wheel" and "dots" arguments to provide for wildcarding of those
 *       arguments.  In the above example, as written, the entire string of
 *       "dots" or "wheel" must be enter to be accepted.
 *
 ******************************************************************************/
void wildcardMatch(int argc, char *argv[])
{
  if (pshell_isHelp())
  {
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("  where valid <args> are:\n");
    pshell_printf("    on\n");
    pshell_printf("    of*f\n");
    pshell_printf("    a*ll\n");
    pshell_printf("    sy*mbols\n");
    pshell_printf("    se*ttings\n");
    pshell_printf("    d*efault\n");
    pshell_printf("\n");
  }
  else if (pshell_isSubString(argv[0], "on", 2))
  {
    pshell_printf("argv 'on' match\n");
  }
  else if (pshell_isSubString(argv[0], "off", 2))
  {
    pshell_printf("argv 'off' match\n");
  }
  else if (pshell_isSubString(argv[0], "all", 1))
  {
    pshell_printf("argv 'all' match\n");
  }
  else if (pshell_isSubString(argv[0], "symbols", 2))
  {
    pshell_printf("argv 'symbols' match\n");
  }
  else if (pshell_isSubString(argv[0], "settings", 2))
  {
    pshell_printf("argv 'settings' match\n");
  }
  else if (pshell_isSubString(argv[0], "default", 1))
  {
    pshell_printf("argv 'default' match\n");
  }
  else
  {
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("  where valid <args> are:\n");
    pshell_printf("    on\n");
    pshell_printf("    of*f\n");
    pshell_printf("    a*ll\n");
    pshell_printf("    sy*mbols\n");
    pshell_printf("    se*ttings\n");
    pshell_printf("    d*efault\n");
    pshell_printf("\n");
  }
}

/******************************************************************************
 *
 * enhancedUsage:
 *
 * This command shows a command that is registered with the "showUsage" flag
 * set to "false", the PshellServer will invoke the command when the user types
 * a "?" or "-h" rather than automatically giving the registered usage, the
 * callback command can then see if the user asked for help (i.e. typed a "?"
 * or "-h") by calling pshell_isHelp, the user can then display the standard
 * registered usage with the pshell_showUsage call and then give some optional
 * enhanced usage with the pshell_printf call
 *
 ******************************************************************************/
void enhancedUsage(int argc, char *argv[])
{

  /* see if the user asked for help */
  if (pshell_isHelp())
  {
    /* show standard usage */
    pshell_showUsage();
    /* give some enhanced usage */
    pshell_printf("Enhanced usage here...\n");
  }
  else
  {
    /* do normal function processing */
    pshell_printf("enhancedUsage command dispatched:\n");
    for (int i = 0; i < argc; i++)
    {
      pshell_printf("  argv[%d]: '%s'\n", i, argv[i]);
    }
  }
}

/******************************************************************************
 *
 * formatChecking:
 *
 * This function demonstrates the various helper functions that assist in the
 * interpretation and conversion of command line arguments
 *
 ******************************************************************************/
void formatChecking(int argc, char *argv[])
{

  pshell_printf("formatChecking command dispatched:\n");

  if (pshell_isIpv4Addr(argv[0]))
  {
    pshell_printf("IPv4 address: '%s' entered\n", argv[0]);
  }
  else if (pshell_isIpv4AddrWithNetmask(argv[0]))
  {
    pshell_printf("IPv4 address/netmask: '%s' entered\n", argv[0]);
  }
  else if (pshell_isDec(argv[0]))
  {
    pshell_printf("Decimal arg: %d entered\n", pshell_getUnsigned(argv[0]));
  }
  else if (pshell_isHex(argv[0]))
  {
    pshell_printf("Hex arg: 0x%x entered\n", pshell_getUnsigned(argv[0]));
  }
  else if (pshell_isAlpha(argv[0]))
  {
    pshell_isEqual(argv[0], "myarg") ?
      pshell_printf("Alphabetic arg: '%s' equal to 'myarg'\n", argv[0]) :
      pshell_printf("Alphabetic arg: '%s' not equal to 'myarg'\n", argv[0]);
  }
  else if (pshell_isAlphaNumeric(argv[0]))
  {
    pshell_isEqual(argv[0], "myarg1") ?
      pshell_printf("Alpha numeric arg: '%s' equal to 'myarg1'\n", argv[0]) :
      pshell_printf("Alpha numeric arg: '%s' not equal to 'myarg1'\n", argv[0]);
  }
  else if (pshell_isFloat(argv[0]))
  {
    pshell_printf("Float arg: %.2f entered\n", pshell_getFloat(argv[0]));
  }
  else
  {
    pshell_printf("Unknown arg format: '%s'\n", argv[0]);
  }
}

/******************************************************************************
 *
 * advancedParsing:
 *
 * This function shows advanced command line parsing using the pshell_tokenize
 * function
 *
 ******************************************************************************/
void advancedParsing(int argc, char *argv[])
{
  PshellTokens *date = pshell_tokenize(argv[0], "/");
  PshellTokens *time = pshell_tokenize(argv[1], ":");

  if ((date->numTokens != 3) || (time->numTokens != 3))
  {
    pshell_printf("ERROR: Improper timestamp format!!\n");
    pshell_showUsage();
  }
  else if (!pshell_isDec(date->tokens[0]) ||
           (pshell_getInt(date->tokens[0]) > MAX_MONTH))
  {
    pshell_printf("ERROR: Invalid month: %s, must be numeric value <= %d\n",
                  date->tokens[0],
                  MAX_MONTH);
  }
  else if (!pshell_isDec(date->tokens[1]) ||
           (pshell_getInt(date->tokens[1]) > MAX_DAY))
  {
    pshell_printf("ERROR: Invalid day: %s, must be numeric value <= %d\n",
                  date->tokens[1],
                  MAX_DAY);
  }
  else if (!pshell_isDec(date->tokens[2]) ||
           (pshell_getInt(date->tokens[2]) > MAX_YEAR))
  {
    pshell_printf("ERROR: Invalid year: %s, must be numeric value <= %d\n",
                  date->tokens[2],
                  MAX_YEAR);
  }
  else if (!pshell_isDec(time->tokens[0]) ||
           (pshell_getInt(time->tokens[0]) > MAX_HOUR))
  {
    pshell_printf("ERROR: Invalid hour: %s, must be numeric value <= %d\n",
                  time->tokens[0],
                  MAX_HOUR);
  }
  else if (!pshell_isDec(time->tokens[1]) ||
           (pshell_getInt(time->tokens[1]) > MAX_MINUTE))
  {
    pshell_printf("ERROR: Invalid minute: %s, must be numeric value <= %d\n",
                  time->tokens[1],
                  MAX_MINUTE);
  }
  else if (!pshell_isDec(time->tokens[2]) ||
           (pshell_getInt(time->tokens[2]) > MAX_SECOND))
  {
    pshell_printf("ERROR: Invalid second: %s, must be numeric value <= %d\n",
                  time->tokens[2],
                  MAX_SECOND);
  }
  else
  {
    pshell_printf("Month  : %s\n", date->tokens[0]);
    pshell_printf("Day    : %s\n", date->tokens[1]);
    pshell_printf("Year   : %s\n", date->tokens[2]);
    pshell_printf("Hour   : %s\n", time->tokens[0]);
    pshell_printf("Minute : %s\n", time->tokens[1]);
    pshell_printf("Second : %s\n", time->tokens[2]);
  }
}

/******************************************************************************
 *
 * dynamicOutput:
 *
 * This function shows an output value that changes frequently, this is used to
 * illustrate the command line mode with a repeated rate and an optional clear
 * screen between iterations, using command line mode in this way along with a
 * function with dynamically changing output information will produce a display
 * similar to the familiar "top" display command output
 *
 ******************************************************************************/
void dynamicOutput(int argc, char *argv[])
{
  char timestamp[80];
  struct timeval tv;
  struct tm tm;

  if (pshell_isEqual(argv[0], "show"))
  {
    // get timestamp
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    strftime(timestamp, sizeof(timestamp), "%T", &tm);
    sprintf(timestamp, "%s.%-6ld", timestamp, (long)tv.tv_usec);
    pshell_printf("\n");
    pshell_printf("DYNAMICALLY CHANGING OUTPUT\n");
    pshell_printf("===========================\n");
    pshell_printf("\n");
    pshell_printf("Timestamp ........: %s\n", timestamp);
    pshell_printf("Random Value .....: %d\n", rand());
    pshell_printf("Dynamic Value ....: %s\n", dynamicValue);
    pshell_printf("\n");
  }
  else
  {
    strcpy(dynamicValue, argv[0]);
  }
}

/******************************************************************************
 *
 * getOptions:
 *
 * This function shows the extraction of arg options using the pshell_getOption
 * function,the format of the options are either -<option><value> where <option>
 * is a single character option (e.g. -t10), or <option>=<value> where <option>
 * is any length character string (e.g. timeout=10), if the 'strlen(option)'
 *  == 0, all option names & values will be extracted and returned in the
 * 'option' and 'value' parameters, if the 'strlen(option)' is > 0, the
 * 'value' will only be extracted if the option matches the requested option
 * name
 *
 ******************************************************************************/
void getOptions(int argc, char *argv[])
{
  char returnedOption[40];
  char returnedValue[40];
  char *requestedOption;
  if (pshell_isHelp())
  {
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("  where:\n");
    pshell_printf("    all    - extract all options\n");
    pshell_printf("    <opt>  - option identifier to extract (e.g. '-t', 'timeout' etc)\n");
    pshell_printf("    <optN> - option identifier along with value (e.g. '-t10', 'timeout=10', etc)\n");
    pshell_printf("\n");
  }
  else
  {
    /* extract our args */
    for (int i = 1; i < argc; i++)
    {
      /* see if we want to extract all options & values or a specific one */
      if (pshell_isEqual(argv[0], "all"))
      {
        /*
         * we want to extract all options with the option name and value
         * being returned to us, the strlen(option) must be 0 to do this
         */
        returnedOption[0] = 0;
        if (pshell_getOption(argv[i], returnedOption, returnedValue))
        {
          pshell_printf("  arg[%d]: '%s', option[%d]: '%s', value[%d]: '%s'\n",
                        i, argv[i], i, returnedOption, i, returnedValue);
        }
      }
      else
      {
        /* we are looking to extract the value for a specific option */
        requestedOption = argv[0];
        if (pshell_getOption(argv[i], requestedOption, returnedValue))
        {
          pshell_printf("  arg[%d]: '%s', option[%d]: '%s', value[%d]: '%s'\n",
                        i, argv[i], i, requestedOption, i, returnedValue);
        }
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void showUsage(void)
{
  printf("\n");
  printf("Usage: pshellServerDemo -udp [<port>] | -tcp [<port>] | -unix | -local\n");
  printf("\n");
  printf("  where:\n");
  printf("    -udp   - Multi-session UDP server\n");
  printf("    -tcp   - Single session TCP server\n");
  printf("    -unix  - Multi-session UNIX domain server\n");
  printf("    -local - Local command dispatching server\n");
  printf("    <port> - Desired UDP or TCP port, default: %d\n", PSHELL_DEMO_PORT);
  printf("\n");
  exit(0);
}

/******************************************************************************/
/******************************************************************************/
void signalHandler(int signal_)
{
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
#ifdef LINUX
  signal(SIGSTKFLT, signalHandler); /* 16 Stack fault.  */
#endif
  signal(SIGXCPU, signalHandler);   /* 24 CPU limit exceeded (4.2 BSD).  */
  signal(SIGXFSZ, signalHandler);   /* 25 File size limit exceeded (4.2 BSD).  */
  signal(SIGSYS, signalHandler);    /* 31 Bad system call.  */
}

/******************************************************************************/
/******************************************************************************/
int main(int argc, char *argv[])
{

  PshellServerType serverType = PSHELL_LOCAL_SERVER;
  unsigned port = PSHELL_DEMO_PORT;

  /* validate our command line arguments and get the server type */
  if (( argc == 2) || (argc == 3))
  {
    if (strcmp(argv[1], "-udp") == 0)
    {
      serverType = PSHELL_UDP_SERVER;
    }
    else if (strcmp(argv[1], "-tcp") == 0)
    {
      serverType = PSHELL_TCP_SERVER;
    }
    else if (strcmp(argv[1], "-local") == 0)
    {
      serverType = PSHELL_LOCAL_SERVER;
    }
    else if (strcmp(argv[1], "-unix") == 0)
    {
      serverType = PSHELL_UNIX_SERVER;
    }
    else
    {
      showUsage();
    }
    if (argc == 3)
    {
      port = atoi(argv[2]);
    }
  }
  else
  {
    showUsage();
  }

  /* register signal handlers so we can do a graceful termination and cleanup any system resources */
  registerSignalHandlers();

  /*
   * Register all our pshell callback commands
   *
   * NOTE: Command names consist of one keyword only
   */

  pshell_addCommand(helloWorld,                           /* function */
                    "helloWorld",                         /* command */
                    "command that prints out arguments",  /* description */
                    "[<arg1> ... <arg20>]",               /* usage */
                    0,                                    /* minArgs */
                    20,                                   /* maxArgs */
                    true);                                /* showUsage on "?" */

  pshell_addCommand(keepAlive,                                              /* function */
                    "keepAlive",                                            /* command */
                    "command to show client keep-alive ('C' client only)",  /* description */
                    "dots | bang | pound | wheel",                          /* usage */
                    1,                                                      /* minArgs */
                    1,                                                      /* maxArgs */
                    false);                                                 /* showUsage on "?" */

  pshell_addCommand(wildcardMatch,                            /* function */
                    "wildcardMatch",                          /* command */
                    "command that does a wildcard matching",  /* description */
                    "<arg>",                                  /* usage */
                    1,                                        /* minArgs */
                    1,                                        /* maxArgs */
                    false);                                   /* showUsage on "?" */

  pshell_addCommand(enhancedUsage,                   /* function */
                    "enhancedUsage",                 /* command */
                    "command with enhanced usage",   /* description */
                    "<arg1>",                        /* usage */
                    1,                               /* minArgs */
                    1,                               /* maxArgs */
                    false);                          /* showUsage on "?" */

  pshell_addCommand(formatChecking,                       /* function */
                    "formatChecking",                     /* command */
                    "command with arg format checking",   /* description */
                    "<arg1>",                             /* usage */
                    1,                                    /* minArgs */
                    1,                                    /* maxArgs */
                    true);                                /* showUsage on "?" */

  pshell_addCommand(advancedParsing,                                /* function */
                    "advancedParsing",                              /* command */
                    "command with advanced command line parsing",   /* description */
                    "<mm>/<dd>/<yyyy> <hh>:<mm>:<ss>",              /* usage */
                    2,                                              /* minArgs */
                    2,                                              /* maxArgs */
                    true);                                          /* showUsage on "?" */

  pshell_addCommand(dynamicOutput,                                         /* function */
                    "dynamicOutput",                                       /* command */
                    "command with dynamic output for command line mode",   /* description */
                    "show | <value>",                                      /* usage */
                    1,                                                     /* minArgs */
                    1,                                                     /* maxArgs */
                    true);                                                 /* showUsage on "?" */

  pshell_addCommand(getOptions,                                  /* function */
                    "getOptions",                                /* command */
                    "example of parsing command line options",   /* description */
                    "{all | <opt>} <opt1> [<opt2> <opt3>...]",   /* usage */
                    2,                                           /* minArgs */
                    20,                                          /* maxArgs */
                    false);                                      /* showUsage on "?" */

  /*
   * example of issuing an pshell command from within a program, this can be done before
   * or after the server is started, as long as the command being called is regstered
   */

  pshell_runCommand("helloWorld 1 2 3");

  /*
   * Now start our PSHELL server with the pshell_startServer function call.
   *
   * The 1st argument is our serverName (i.e. "pshellServerDemo").
   *
   * The 2nd argument specifies the type of PSHELL server, the four valid values are:
   *
   *   PSHELL_UDP_SERVER   - Server runs as a multi-session UDP based server.  This requires
   *                         the special stand-alone command line UDP/UNIX client program
   *                         'pshell'.  This server has no timeout for idle client sessions.
   *                         It can be also be controlled programatically via an external
   *                         program running the PshellControl.h API and library.
   *   PSHELL_UNIX_SERVER  - Server runs as a multi-session UNIX based server.  This requires
   *                         the special stand-alone command line UDP/UNIX client program
   *                         'pshell'.  This server has no timeout for idle client sessions.
   *                         It can be also be controlled programatically via an external
   *                         program running the PshellControl.h API and library.
   *   PSHELL_TCP_SERVER   - Server runs as a single session TCP based server with a 10 minute
   *                         idle client session timeout.  The TCP server can be connected to
   *                         using a standard 'telnet' based client.
   *   PSHELL_LOCAL_SERVER - Server solicits it's own command input via the system command line
   *                         shell, there is no access via a separate client program, when the
   *                         user input is terminated via the 'quit' command, the program is
   *                         exited.
   *
   * The 3rd argument is the server mode, the two valid values are:
   *
   *   PSHELL_NON_BLOCKING - A separate thread will be created to process user input, when
   *                         this function is called as non-blocking, the function will return
   *                         control to the calling context.
   *   PSHELL_BLOCKING     - No thread is created, all processing of user input is done within
   *                         this function call, it will never return control to the calling context.
   *
   * The 4th and 5th arguments must be provided for a UDP or TCP server, for a LOCAL or
   * UNIX server they can be omitted, and if provided they will be ignored.
   *
   * For the 4th argument, a valid IP address or hostname can be used.  There are also 3 special
   * "hostname" type identifiers defined as follows:
   *
   *   localhost - the loopback address (i.e. 127.0.0.1)
   *   myhost    - the hostname assigned to this host, this is usually defined in the
   *               /etc/hosts file and is assigned to the default interface
   *   anyhost   - all interfaces on a multi-homed host (including loopback)
   *
   * Finally, the 5th argument is the desired port number.
   *
   * All of these arguments (except the server name and mode, i.e. args 1 & 3) can be overridden
   * via the 'pshell-server.conf' file on a per-server basis.
   *
   */

  /*
   * NOTE: This server is started up in BLOCKING mode, but if a pshell server is being added
   *       to an existing process that already has a control loop in it's main, it should be
   *       started in NON_BLOCKING mode before the main drops into it's control loop, see the
   *       demo program traceFilterDemo.cc for an example
   */
  pshell_startServer("pshellServerDemo", serverType, PSHELL_BLOCKING, PSHELL_ANYHOST, port);

  /*
   * should never get here because the server is started in BLOCKING mode,
   * but cleanup any pshell system resources as good practice
   */
  pshell_cleanupResources();

  return (0);

}
