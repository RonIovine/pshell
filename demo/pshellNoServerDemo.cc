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
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <PshellServer.h>

/*******************************************************************************
 *
 * This is an example demo program that uses all the basic features of the
 * PSHELL library in non-server mode, non-server mode is a non-interactive,
 * one shot command processing mode, i.e. it will take all the arguments 
 * typed after the program name and process them as a single-shot command
 * and exit.  This mode will also allow for the setup of each individual
 * command to be softlinked back to the main parent program via the '--setup'
 * command option to provide a Busybox like soflink shortcut access to each
 * individual command directly and not through this parent program.
 *
 *******************************************************************************/

/*
 * PSHELL user callback functions, the interface is identical to the "main"
 * in C, with the argc being the argument count (excluding the actual
 * command itself), and argv[] being the argument list (again, excluding
 * the command).
 *
 * Use the special 'pshell_printf' function call to display data back to the
 * remote client.  The interface to this function is exactly the same as
 * the standard 'printf' function.
 */
 
/******************************************************************************/
/******************************************************************************/
void hello(int argc, char *argv[])
{
  pshell_printf("hello command dispatched:\n");
  /* dump out our args */
  for (int i = 0; i < argc; i++)
  {
    pshell_printf("  argv[%d]: '%s'\n", i, argv[i]);
  }
}

/******************************************************************************/
/******************************************************************************/
void world(int argc, char *argv[])
{
  pshell_printf("world command dispatched:\n");
}

/*
 * this command shows matching the passed command arguments based on
 * substring matching rather than matching on the complete exact string,
 * the minimum number of characters that must be matched is the last
 * argument to the pshell_isSubString function, this must be the minimum
 * number of characters necessary to uniquely identify the argument from
 * the complete argument list
 */

/******************************************************************************/
/******************************************************************************/
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
    pshell_printf("\n");
    pshell_printf("    on\n");
    pshell_printf("    of*f\n");
    pshell_printf("    a*ll\n");
    pshell_printf("    sy*mbols\n");
    pshell_printf("    se*ttings\n");
    pshell_printf("    d*efault\n");
    pshell_printf("\n");
  }
}

/*
 * if a command is registered with the "showUsage" flag set to "false"
 * the PshellServer will invoke the command when the user types a "?" or 
 * "-h" rather than automatically giving the registered usage, the callback
 * command can then see if the user asked for help (i.e. typed a "?" or 
 * "-h") by calling pshell_isHelp, the user can then display the standard
 * registered usage with the pshell_showUsage call and then give some
 * optional enhanced usage with the pshell_printf call
 */

/******************************************************************************/
/******************************************************************************/ 
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

/*
 * this function demonstrates the various helper functions that assist
 * in the interpretation and conversion of command line arguments
 */

/******************************************************************************/
/******************************************************************************/
void formatChecking(int argc, char *argv[])
{

  pshell_printf("formatChecking command dispatched:\n");

  if (pshell_isDec(argv[0]))
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

/* function to show advanced command line parsing using the pshell_tokenize function */

#define MAX_YEAR   3000
#define MAX_MONTH  12
#define MAX_DAY    31
#define MAX_HOUR   23
#define MAX_MINUTE 59
#define MAX_SECOND 59

/******************************************************************************/
/******************************************************************************/
void advancedParsing(int argc, char *argv[])
{
  PshellTokens *timestamp = pshell_tokenize(argv[0], ":");

  if (timestamp->numTokens != 6)
  {
    pshell_printf("ERROR: Improper timestamp format!!\n");
    pshell_showUsage();
  }
  else if (!pshell_isDec(timestamp->tokens[0]) ||
           (pshell_getInt(timestamp->tokens[0]) > MAX_YEAR))
  {
    pshell_printf("ERROR: Invalid year: %s, must be numeric value <= %d\n",
                  timestamp->tokens[0],
                  MAX_YEAR);
  }
  else if (!pshell_isDec(timestamp->tokens[1]) ||
           (pshell_getInt(timestamp->tokens[1]) > MAX_MONTH))
  {
    pshell_printf("ERROR: Invalid month: %s, must be numeric value <= %d\n",
                  timestamp->tokens[1],
                  MAX_MONTH);
  }
  else if (!pshell_isDec(timestamp->tokens[2]) ||
           (pshell_getInt(timestamp->tokens[2]) > MAX_DAY))
  {
    pshell_printf("ERROR: Invalid day: %s, must be numeric value <= %d\n",
                  timestamp->tokens[2],
                  MAX_DAY);
  }
  else if (!pshell_isDec(timestamp->tokens[3]) ||
           (pshell_getInt(timestamp->tokens[3]) > MAX_HOUR))
  {
    pshell_printf("ERROR: Invalid hour: %s, must be numeric value <= %d\n",
                  timestamp->tokens[3],
                  MAX_HOUR);
  }
  else if (!pshell_isDec(timestamp->tokens[4]) ||
           (pshell_getInt(timestamp->tokens[4]) > MAX_MINUTE))
  {
    pshell_printf("ERROR: Invalid minute: %s, must be numeric value <= %d\n",
                  timestamp->tokens[4],
                  MAX_MINUTE);
  }
  else if (!pshell_isDec(timestamp->tokens[5]) ||
           (pshell_getInt(timestamp->tokens[5]) > MAX_SECOND))
  {
    pshell_printf("ERROR: Invalid second: %s, must be numeric value <= %d\n",
                  timestamp->tokens[5],
                  MAX_SECOND);
  }
  else
  {
    pshell_printf("Year   : %s\n", timestamp->tokens[0]);
    pshell_printf("Month  : %s\n", timestamp->tokens[1]);
    pshell_printf("Day    : %s\n", timestamp->tokens[2]);
    pshell_printf("Hour   : %s\n", timestamp->tokens[3]);
    pshell_printf("Minute : %s\n", timestamp->tokens[4]);
    pshell_printf("Second : %s\n", timestamp->tokens[5]);
  }
}

/*
 * function that shows the extraction of arg options using the pshell_getOption
 * function,the fromat of the options are either -<option><value> where <option>
 * is a single character option (e.g. -t10), or <option>=<value> where <option>
 * is any length character string (e.g. timeout=10), if the 'strlen(option_)'
 * is == 0, all option names & values will be extracted and returned in the
 * 'option_' and 'value_' parameters, if the 'strlen(option_)' is > 0, the
 * 'value_' will only be extracted if the option matches the requested option_
 * name,
 */
 
/******************************************************************************/
/******************************************************************************/
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
    pshell_printf("  where::\n");
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
int main(int argc, char *argv[])
{

  /*
   * Register all our pshell callback commands
   *
   * NOTE: Command names consist of one keyword only
   */

  pshell_addCommand(hello,                        /* function */
                    "hello",                      /* command */
                    "hello command description",  /* description */
                    "[<arg1> ... <arg20>]",       /* usage */
                    0,                            /* minArgs */
                    20,                           /* maxArgs */
                    true);                        /* showUsage on "?" */

  pshell_addCommand(world,                        /* function */
                    "world",                      /* command */
                    "world command description",  /* description */
                    NULL,                         /* usage */
                    0,                            /* minArgs */
                    0,                            /* maxArgs */
                    true);                        /* showUsage on "?" */

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
                    "<yyyy>:<mm>:<dd>:<hh>:<mm>:<ss>",              /* usage */
                    1,                                              /* minArgs */
                    1,                                              /* maxArgs */
                    true);                                          /* showUsage on "?" */
                  
  pshell_addCommand(getOptions,                                  /* function */
                    "getOptions",                                /* command */
                    "example of parsing command line options",   /* description */
                    "{all | <opt>} <opt1> [<opt2> <opt3>...]",   /* usage */
                    2,                                           /* minArgs */
                    20,                                          /* maxArgs */
                    false);                                      /* showUsage on "?" */
                  
  /*
   * example of running in non-server mode, i.e. non-interactive, command line mode,
   * this will run the command as typed at the command line as a single-shot command
   * and exit, this command must be run with the initial argc, argv as they are passed
   * into the 'main' from the host's command line
   */
  pshell_noServer(argc, argv);
  
  return (0);
}
