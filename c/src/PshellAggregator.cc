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
 * This is a pshell client program that aggregates several remote pshell servers
 * into single local pshell server that serves as an interactive client to the
 * remote servers.  This can be useful in presenting a consolidated user shell
 * who's functionality spans several discrete pshell servers.  Since this uses
 * the PshellControl API, the external servers must all be either UDP or Unix
 * servers.  The consolidation point a local pshell server.
 *
 * This is a generic dynamic aggregator, i.e. it is server agnostic.  Servers can
 * be added to the aggregation via the 'add server' command either at startup via
 * the pshellAggregator.startup file or interactively via the interactive command
 * line.
 *
 * This program can also create multicast groups commands via the 'add multicast'
 * command (also at startup or interactively).  The multicast commands can then be
 * distributed to multiple aggregated servers.
 *
 * The aggregation and multicast functionality can be useful to manually drive a
 * set of processes that use the pshell control mechanism as a control plane IPC.
 *
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#include <PshellCommon.h>
#include <PshellServer.h>
#include <PshellControl.h>

#define MAX_SERVERS 256
#define MAX_MULTICAST 256
#define MAX_STRING_SIZE 180
#define MAX_COMMAND_SIZE 300

/* some special backdoor settings in PshellServer.cc to us to run as both server and client */
extern char *pshell_origCommandKeyword;
extern bool pshell_copyAddCommandStrings;
extern bool pshell_allowDuplicateFunction;

struct Server
{
  char localName[MAX_STRING_SIZE];
  char remoteServer[MAX_STRING_SIZE];
  int port;
  int sid;
};
struct Server servers[MAX_SERVERS];
unsigned numServers = 0;

struct Multicast
{
  char keyword[MAX_STRING_SIZE];
  struct Server *servers[MAX_SERVERS];
  unsigned numServers;
};
struct Multicast multicastGroups[MAX_MULTICAST];
unsigned numMulticast = 0;

const char *localNameLabel = "Local Server Name";
const char *remoteNameLabel = "Remote Server";
const char *keywordLabel = "Keyword";
unsigned maxLocalName = strlen(localNameLabel);
unsigned maxRemoteName = strlen(remoteNameLabel);
unsigned maxMulticastKeyword = strlen(keywordLabel);

/*
 * make sure the results array is big enough for any possible output,
 * we use the max datagram size
 */
char results[1024*64];

/******************************************************************************/
/******************************************************************************/
Multicast *getMulticast(char *keyword)
{
  for (unsigned multicast = 0; multicast < numMulticast; multicast++)
  {
    if (strcmp(multicastGroups[multicast].keyword, keyword) == 0)
    {
      return (&multicastGroups[multicast]);
    }
  }
  return (NULL);
}

/******************************************************************************/
/******************************************************************************/
Server *getServer(char *localName)
{
  for (unsigned server = 0; server < numServers; server++)
  {
    if (strncmp(localName, servers[server].localName, strlen(localName)) == 0)
    {
      return (&servers[server]);
    }
  }
  return (NULL);
}

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

/******************************************************************************/
/******************************************************************************/
void controlServer(int argc, char *argv[])
{
  Server *server = getServer(pshell_origCommandKeyword);
  if (server != NULL)
  {
    if ((argc == 0) || pshell_isHelp() || pshell_isEqual(argv[0], "help"))
    {
      pshell_extractCommands(server->sid, results, sizeof(results));
      pshell_printf("%s", results);
    }
    else
    {
      /* this contains the re-constituted command */
      char command[MAX_COMMAND_SIZE];
      if ((pshell_sendCommand3(server->sid,
                               results,
                               sizeof(results),
                               buildCommand(command, sizeof(command), argc, argv)) == PSHELL_COMMAND_SUCCESS) && (strlen(results) > 0))
      {
        pshell_printf("%s", results);
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
bool isDuplicate(char *localName, char *remoteServer, int port)
{
  for (unsigned server = 0; server < numServers; server++)
  {
    if ((strcmp(servers[server].localName, localName) == 0) ||
        ((strcmp(servers[server].remoteServer, remoteServer) == 0) &&
         (servers[server].port == port)))
    {
      return (true);
    }
  }
  return (false);
}

/******************************************************************************/
/******************************************************************************/
void add(int argc, char *argv[])
{
  if (pshell_isHelp())
  {
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("  where:\n");
    pshell_printf("    <localName>    - Local logical name of the server, must be unique\n");
    pshell_printf("    <remoteServer> - Hostname or IP address of UDP server or name of UNIX server\n");
    pshell_printf("    <port>         - UDP port number or 'unix' for UNIX server (can be omitted for UNIX)\n");
    pshell_printf("    <keyword>      - Multicast group keyword, must be valid registered remote command\n");
    pshell_printf("\n");
  }
  else if (pshell_isSubString(argv[0], "server", 1))
  {
    /* default port */
    unsigned port = 0;
    if (argc == 4)
    {
      port = atoi(argv[3]);
    }
    if (!isDuplicate(argv[1], argv[2], port))
    {
      if (numServers < MAX_SERVERS)
      {
        if (strlen(argv[1]) > maxLocalName)
        {
          maxLocalName = MAX(strlen(argv[1]), strlen(localNameLabel));
        }
        if (strlen(argv[2]) > maxRemoteName)
        {
          maxRemoteName = MAX(strlen(argv[2]), strlen(remoteNameLabel));
        }
        strcpy(servers[numServers].localName, argv[1]);
        strcpy(servers[numServers].remoteServer, argv[2]);
        servers[numServers].port = port;
        servers[numServers].sid = pshell_connectServer(argv[1],
                                                       argv[2],
                                                       port,
                                                       PSHELL_ONE_SEC*5);
        numServers++;
        char description[MAX_STRING_SIZE];
        sprintf(description, "control the remote %s process", argv[1]);
        pshell_addCommand(controlServer,
                          argv[1],
                          description,
                          "[<command> | ? | -h]",
                          0,
                          30,
                          false);
      }
      else
      {
        pshell_printf("ERROR: Max servers: %d exceeded, server local name: %s, remote server: %s, port: %s not added\n", MAX_SERVERS, argv[1], argv[2], argv[3]);
      }
    }
    else
    {
      pshell_printf("ERROR: Local name: %s, remote server: %s, port: %s already exists\n", argv[1], argv[2], argv[3]);
    }
  }
  else if (pshell_isSubString(argv[0], "multicast", 1))
  {
    Multicast *multicast = getMulticast(argv[1]);
    if (multicast == NULL)
    {
      if (numMulticast < MAX_MULTICAST)
      {
        /* new keyword */
        if (strlen(argv[1]) > maxMulticastKeyword)
        {
          maxMulticastKeyword = MAX(strlen(argv[1]), strlen(keywordLabel));
        }
        strcpy(multicastGroups[numMulticast].keyword, argv[1]);
        multicastGroups[numMulticast].numServers = 0;
        multicast = &multicastGroups[numMulticast];
        numMulticast++;
      }
      else
      {
        pshell_printf("ERROR: Max multicast keywords: %d exceeded, keyword: %s not added\n", MAX_MULTICAST, argv[1]);
        return;
      }
    }
    /* add servers to this keyword */
    for (int localName = 2; localName < argc; localName++)
    {
      Server *server = getServer(argv[localName]);
      if (server != NULL)
      {
        if (multicast->numServers < MAX_SERVERS)
        {
          pshell_addMulticast(argv[1], server->localName);
          multicast->servers[multicast->numServers] = server;
          multicast->numServers++;
        }
        else
        {
          pshell_printf("ERROR: Max servers: %d, exceeded for keyword: %s, server not added\n", MAX_SERVERS, argv[1]);
        }
      }
    }
  }
  else
  {
    pshell_showUsage();
  }
}

/******************************************************************************/
/******************************************************************************/
void strrpt(int count, const char *string)
{
  for (int i = 0; i < count; i++) pshell_printf(string);
}

/******************************************************************************/
/******************************************************************************/
void show(int argc, char *argv[])
{
  if (pshell_isSubString(argv[0], "servers", 1))
  {
    pshell_printf("\n");
    pshell_printf("*************************************************\n");
    pshell_printf("*           AGGREGATED REMOTE SERVERS           *\n");
    pshell_printf("*************************************************\n");
    pshell_printf("\n");
    pshell_printf("%-*s    %-*s    Port\n", maxLocalName,
                                            localNameLabel,
                                            maxRemoteName,
                                            remoteNameLabel);
    strrpt(maxLocalName, "=");
    pshell_printf("    ");
    strrpt(maxRemoteName, "=");
    pshell_printf("    ======\n");
    for (unsigned server = 0; server < numServers; server++)
    {
      pshell_printf("%-*s    %-*s",  maxLocalName,
                                     servers[server].localName,
                                     maxRemoteName,
                                     servers[server].remoteServer);
      if (servers[server].port == 0)
      {
        pshell_printf("    unix\n");
      }
      else
      {
        pshell_printf("    %d\n",  servers[server].port);
      }
    }
    pshell_printf("\n");
  }
  else if (pshell_isSubString(argv[0], "multicast", 1))
  {
    pshell_printf("\n");
    pshell_printf("*****************************************************\n");
    pshell_printf("*            REGISTERED MULTICAST GROUPS            *\n");
    pshell_printf("*****************************************************\n");
    pshell_printf("\n");
    pshell_printf("%-*s    %-*s    %-*s    Port\n", maxMulticastKeyword,
                                                    keywordLabel,
                                                    maxLocalName,
                                                    localNameLabel,
                                                    maxRemoteName,
                                                    remoteNameLabel);
    strrpt(maxMulticastKeyword, "=");
    pshell_printf("    ");
    strrpt(maxLocalName, "=");
    pshell_printf("    ");
    strrpt(maxRemoteName, "=");
    pshell_printf("    ======\n");
    for (unsigned multicast = 0; multicast < numMulticast; multicast++)
    {
      pshell_printf("%-*s    ", maxMulticastKeyword, multicastGroups[multicast].keyword);
      for (unsigned server = 0; server < multicastGroups[multicast].numServers; server++)
      {
        if (server > 0)
        {
          strrpt(maxMulticastKeyword, " ");
          pshell_printf("    ");
        }
        pshell_printf("%-*s    %-*s",  maxLocalName,
                                       multicastGroups[multicast].servers[server]->localName,
                                       maxRemoteName,
                                       multicastGroups[multicast].servers[server]->remoteServer);
        if (multicastGroups[multicast].servers[server]->port == 0)
        {
          pshell_printf("    unix\n");
        }
        else
        {
          pshell_printf("    %d\n",  multicastGroups[multicast].servers[server]->port);
        }
      }
    }
    pshell_printf("\n");
  }
  else
  {
    pshell_showUsage();
  }
}

/******************************************************************************/
/******************************************************************************/
void multicast(int argc, char *argv[])
{
  char command[MAX_COMMAND_SIZE];
  if ((argc == 0) || pshell_isHelp())
  {
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("  Send a registered multicast command to the associated\n");
    pshell_printf("  multicast remote server group\n");
    const char *argv[] = {"multicast"};
    show(1, (char **)argv);
  }
  else
  {
    /* reconstitute the original command */
    pshell_sendMulticast(buildCommand(command, sizeof(command), argc, argv));
  }
}

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

  /* verify usage */
  if (argc != 1)
  {
    printf("\n");
    printf("Usage: %s\n", argv[0]);
    printf("\n");
    printf("  Client program that will allow for the aggregation of multiple remote\n");
    printf("  UDP/UNIX pshell servers into one consolidated client shell.  This program\n");
    printf("  can also create multicast groups for sets of remote servers.  The remote\n");
    printf("  servers and multicast groups can be added interactively via the 'add'\n");
    printf("  command or at startup via the 'pshellAggregator.startup' file.\n");
    printf("\n");
    exit(0);
  }

  /* register signal handlers so we can do a graceful termination and cleanup any system resources */
  registerSignalHandlers();

  /* set some special backdoor settings in PshellServer.cc us to run as both server and client */
  pshell_copyAddCommandStrings = true;
  pshell_allowDuplicateFunction = true;

  /* register our callback commands */
  pshell_addCommand(add,
                    "add",
                    "add a new remote server or multicast group entry",
                    "{server <localName> <remoteServer> [<port>]} | {multicast <keyword> <localName1> [<localName2>...<localNameN>]}",
                    3,
                    30,
                    false);

  pshell_addCommand(show,
                    "show",
                    "show aggregated servers or multicast group info",
                    "servers | multicast",
                    1,
                    1,
                    true);

  pshell_addCommand(multicast,
                    "multicast",
                    "send multicast command to registered server group",
                    "<command>",
                    0,
                    30,
                    false);

  /* start our local pshell server */
  pshell_startServer("pshellAggregator", PSHELL_LOCAL_SERVER, PSHELL_BLOCKING);

  /* disconnect all our remote control servers */
  pshell_disconnectAllServers();

  /* cleanup our local server's resources */
  pshell_cleanupResources();

}
