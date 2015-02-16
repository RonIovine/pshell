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
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <PshellCommon.h>

/* constants */

/*
 * these are the memory size chunk allocations when reading in the servers
 * in the pshell-client.conf file and obtaining the command list from the
 * server on a query command
 */

#define PSHELL_SERVERS_CHUNK 50
#define PSHELL_COMMAND_CHUNK 50

/*
 * this is the default response timeout the client will use when invoking
 * commands on the server, if the client does not hear from the server
 * within this time, either with output or a command completion indication,
 * the command will timeout
 */

#ifndef PSHELL_SERVER_RESPONSE_TIMEOUT
#define PSHELL_SERVER_RESPONSE_TIMEOUT 5     /* seconds */
#endif

/*
 * this is the default directory which contains the pshell-client.conf
 * file that has the port to server mappings so a server can be referenced
 * by name rather than port number, it will try to use this directory if
 * the environment variable 'PSHELL_CONFIG_DIR' is not defined
 */

#ifndef PSHELL_CONFIG_DIR
#define PSHELL_CONFIG_DIR "/etc"
#endif

/*
 * the maximum number of command line arguments that can be tokenized
 */
 
#define MAX_TOKENS 20

/* enums */

enum Mode
{
  INTERACTIVE,
  COMMAND_LINE,
  BATCH
};

/* this client can control both UDP socket and UNIX domain (DGRAM) socket servers */
enum ServerType
{
  UDP,
  UNIX
};

/*
 * coding convention is leading underscore for global data,
 * trailing underscore for function arguments, and no leading
 * or trailing underscores for local stack variables
 */

int _serverResponseTimeout = PSHELL_SERVER_RESPONSE_TIMEOUT;
int _socketFd;
struct sockaddr_in _sourceIpAddress;
struct sockaddr_in _destIpAddress;
struct sockaddr_un _sourceUnixAddress;
struct sockaddr_un _destUnixAddress;
bool _isUnixConnected = false;

#define MAX_UNIX_CLIENTS 1000

ServerType _serverType = UDP;

char _unixLocalSocketName[256];
char _interactiveCommand[PSHELL_PAYLOAD_SIZE];
char _interactivePrompt[PSHELL_PAYLOAD_SIZE];
char _serverName[PSHELL_PAYLOAD_SIZE];
char _ipAddress[PSHELL_PAYLOAD_SIZE];
char _title[PSHELL_PAYLOAD_SIZE];
char _banner[PSHELL_PAYLOAD_SIZE];
char _prompt[PSHELL_PAYLOAD_SIZE];
unsigned _version;
Mode _mode;
unsigned _maxCommandLength = strlen("batch");
const char *_nativeInteractiveCommands [] =
{
  "quit",
  "help",
  "batch"
};
#define QUIT_INDEX 0
#define HELP_INDEX 1
#define BATCH_INDEX 2
const char *_nativeInteractiveCommandDescriptions [] =
{
  "exit interactive mode",
  "show all available commands",
  "run commands from a batch file"
};
unsigned _numNativeInteractiveCommands = sizeof(_nativeInteractiveCommands)/sizeof(char*);

/*
 * the initial message buffer is just used for the initial
 * PSHELL_QUERY_VERSION and PSHELL_QUERY_PAYLOAD_SIZE messages,
 * after that the message payload is dynaically allocate for
 * the negotiated transfer size, this is to ensure the client
 * does not use a receive buffer that is smaller than the one
 * the server is using, the server will also send an
 * PSHELL_UPDATE_PAYLOAD_SIZE message if it overflows its buffers
 * to keep the client in sync with its buffer size
 */

PshellMsg _pshellSendMsg;
/*
 * the rcv and send message buffers only stay the same until
 * the response for the PSHELL_QUERY_PAYLOAD_SIZE is received,
 * after that these two messages go their separate ways, a new
 * rcv message is allocated from the heap based on the servers
 * payload size response
 */
PshellMsg *_pshellRcvMsg = &_pshellSendMsg;
unsigned _pshellPayloadSize = PSHELL_PAYLOAD_SIZE;

struct PshellServers
{
  const char *serverName;
  unsigned portNum;
};

PshellServers *_pshellServersList;
unsigned _numPshellServers = 0;
unsigned _pshellServersListSize = PSHELL_SERVERS_CHUNK;
unsigned _maxServerNameLength = 0;

const char **_pshellCommandList;
unsigned _numPshellCommands = 0;
unsigned _maxPshellCommands = PSHELL_COMMAND_CHUNK;

#ifdef READLINE
bool _commandFound;
int _matchLength;
unsigned _commandPos;
bool _completionEnabled;
#endif

/* function prototypes */

bool isNumeric(char *string_);
unsigned findServerPort(char *name_);
bool getServerName(void);
bool getIpAddress(void);
bool getTitle(void);
bool getBanner(void);
bool getPrompt(void);
bool getPayloadSize(void);
bool getVersion(void);
bool init(char *destination_, char *server_);
bool send(void);
bool receive(void);
void tokenize(char *string_, const char *delimeter_, char *tokens_[], unsigned maxTokens_, unsigned *numTokens_);
bool processCommand(char msgType_, char *command_, unsigned rate_, bool clear_, bool silent_);
void getInput(void);
bool initInteractiveMode(void);
void processInteractiveMode(void);
void processBatchFile(char *filename_, unsigned rate_, bool clear_);
void getNamedServers(void);
void showNamedServers(void);
void showCommands(void);
void stripWhitespace(char *string_);
void showUsage(void);
void buildCommandList(void);
bool isDuplicate(char *command_);
void clearScreen(void);
void showWelcome(void);
void exitProgram(int exitCode_);

#ifdef READLINE
char **commandCompletion (const char *command_, int start_, int end_);
char *commandGenerator (const char *command_, int state_);
#endif

/******************************************************************************/
/******************************************************************************/
void showWelcome(void)
{
  printf("\n");
  PSHELL_PRINT_WELCOME_BORDER(printf, strlen(_banner));
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  %s\n", PSHELL_WELCOME_BORDER, _banner);
  printf("%s\n", PSHELL_WELCOME_BORDER);
  if (_serverType == UDP)
  {
    printf("%s  Multi-session UDP server: %s[%s]\n", PSHELL_WELCOME_BORDER, _serverName, _ipAddress);
  }
  else
  {
    printf("%s  Multi-session UNIX server: %s[%s]\n", PSHELL_WELCOME_BORDER, _serverName, _ipAddress);
  }
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  Idle session timeout: NONE\n", PSHELL_WELCOME_BORDER);
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  Type '?' or 'help' at prompt for command summary\n", PSHELL_WELCOME_BORDER);
  printf("%s  Type '?' or '-h' after command for command usage\n", PSHELL_WELCOME_BORDER);
  printf("%s\n", PSHELL_WELCOME_BORDER);
#ifdef READLINE
  printf("%s  Full <TAB> completion, up-arrow recall, command\n", PSHELL_WELCOME_BORDER);
  printf("%s  line editing and command abbreviation supported\n", PSHELL_WELCOME_BORDER);
#else
  printf("%s  Command abbreviation supported\n", PSHELL_WELCOME_BORDER);
#endif
  printf("%s\n", PSHELL_WELCOME_BORDER);
  PSHELL_PRINT_WELCOME_BORDER(printf, strlen(_banner));
  printf("\n");
}

/******************************************************************************/
/******************************************************************************/
void clearScreen(void)
{
  fprintf(stdout, "\033[H\033[J");
  fflush(stdout);
}

/******************************************************************************/
/******************************************************************************/
bool isNumeric(char *string_)
{
  unsigned i;
  for (i = 0; i < strlen(string_); i++)
  {
    if (!isdigit(string_[i]))
    {
      return (false);
    }
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
unsigned findServerPort(char *name_)
{
  unsigned server;

  for (server = 0; server < _numPshellServers; server++)
  {
    if (strcmp(name_, _pshellServersList[server].serverName) == 0)
    {
      return (_pshellServersList[server].portNum);
    }
  }
  /* 0 should never be a valid server port, so we can use it to indicate a failure */
  return (0);
}

/******************************************************************************/
/******************************************************************************/
bool getVersion(void)
{
  if (processCommand(PSHELL_QUERY_VERSION, NULL, 0, false, true))
  {
    _version = (unsigned)atoi(_pshellRcvMsg->payload);
    if ((_version < PSHELL_VERSION_1) || (_version > PSHELL_VERSION))
    {
      printf("PSHELL_ERROR: Invalid server version: %d, valid versions are %d-%d\n",
             _version,
             PSHELL_VERSION_1,
             PSHELL_VERSION);
             return (false);
    }
    else
    {
      return (true);
    }
  }
  else
  {
    printf("PSHELL_ERROR: Could not obtain version info from server\n");
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool getServerName(void)
{
  /* query for our process name so we can setup our prompt and title bar */
  if (processCommand(PSHELL_QUERY_NAME, NULL, 0, false, true))
  {
    strcpy(_serverName, _pshellRcvMsg->payload);
    return (true);
  }
  else
  {
    printf("PSHELL_ERROR: Could not obtain server name info from server\n");
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool getTitle(void)
{
  if (processCommand(PSHELL_QUERY_TITLE, NULL, 0, false, true))
  {
    strcpy(_title, _pshellRcvMsg->payload);
    return (true);
  }
  else
  {
    printf("PSHELL_ERROR: Could not obtain terminal 'title' info from server\n");
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool getBanner(void)
{
  /* query for our welcome banner message */
  if (processCommand(PSHELL_QUERY_BANNER, NULL, 0, false, true))
  {
    strcpy(_banner, _pshellRcvMsg->payload);
    return (true);
  }
  else
  {
    printf("PSHELL_ERROR: Could not obtain welcome 'banner' info from server\n");
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool getPrompt(void)
{
  /* query for our prompt */
  if (processCommand(PSHELL_QUERY_PROMPT, NULL, 0, false, true))
  {
    strcpy(_prompt, _pshellRcvMsg->payload);
    return (true);
  }
  else
  {
    printf("PSHELL_ERROR: Could not obtain 'prompt' info from server\n");
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool getIpAddress(void)
{
  /* query for our ip address so we can setup our prompt and title bar */
  if (_version == PSHELL_VERSION_1)
  {
    strcpy(_ipAddress, inet_ntoa(_destIpAddress.sin_addr));
    return (true);
  }
  else if (processCommand(PSHELL_QUERY_IP_ADDRESS, NULL, 0, false, true))
  {
    strcpy(_ipAddress, _pshellRcvMsg->payload);
    return (true);
  }
  else
  {
    printf("PSHELL_ERROR: Could not obtain IP address info from server\n");
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool getPayloadSize(void)
{
  if (processCommand(PSHELL_QUERY_PAYLOAD_SIZE, NULL, 0, false, true))
  {
    _pshellPayloadSize = atoi(_pshellRcvMsg->payload);
    _pshellRcvMsg = (PshellMsg*)malloc(_pshellPayloadSize+PSHELL_HEADER_SIZE);
    return (true);
  }
  else
  {
    printf("PSHELL_ERROR: Could not negotiate payload size between client and server\n");
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool init(char *destination_, char *server_)
{
  char requestedHost[180];
  struct hostent *host;
  int destPort;
  int retCode = -1;

   /* see if it is a named server, numeric port, or UNIX domain server */
  if (strcmp(destination_, "unix") == 0)
  {
    destPort = 0;
  }
  else if (isNumeric(server_))
  {
    destPort = atoi(server_);
  }
  else
  {
    destPort = findServerPort(server_);
  }

  /* see if our destination is a UDP or UNIX domain socket */
  if (destPort > 0)
  {

    /* UDP socket destination */
    _serverType = UDP;
    
    /*
     * we don't care what our source address is because the pshellServer
     * will just do a reply to us based on the source address it receives
     * the request from
     */
    _sourceIpAddress.sin_family = AF_INET;
    _sourceIpAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    _sourceIpAddress.sin_port = htons(0);

    /* open our source socket */
    if ((_socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      printf("PSHELL_ERROR: Cannot create UDP socket\n");
      return (false);
    }

    /* bind to our source socket */
    if (bind(_socketFd,
             (struct sockaddr *) &_sourceIpAddress,
             sizeof(_sourceIpAddress)) < 0)
    {
      printf("PSHELL_ERROR: Cannot bind UDP socket: address: %s, port: %d\n",
             inet_ntoa(_sourceIpAddress.sin_addr),
             ntohs(_sourceIpAddress.sin_port));
             return (false);
    }

    /* setup our destination */
    _destIpAddress.sin_family = AF_INET;
    _destIpAddress.sin_port = htons(destPort);

    /* see if they supplied an IP address */
    if (inet_aton(destination_, &_destIpAddress.sin_addr) == 0)
    {
      strcpy(requestedHost, destination_);
      if (strcmp(requestedHost, "localhost") == 0)
      {
        _destIpAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
      }
      else
      {
        /* see if they are requesting our local host address */
        if (strcmp(requestedHost, "myhost") == 0)
        {
          gethostname(requestedHost, sizeof(requestedHost));
        }

        /* they did not supply an IP address, try to resolve the hostname */
        if ((host = gethostbyname(requestedHost)) != NULL)
        {
          memcpy(&_destIpAddress.sin_addr.s_addr,
                 *host->h_addr_list,
                 sizeof(_destIpAddress.sin_addr.s_addr));
        }
        else
        {
          printf("PSHELL_ERROR: Cannot resolve destination hostname: '%s'\n", destination_);
          return (false);
        }
      }
    }
  }
  else
  {
    /* UNIX domain socket destination */
    _serverType = UNIX;

    if ((_socketFd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
      printf("PSHELL_ERROR: Cannot create UNIX socket\n");
      return (false);
    }

    _sourceUnixAddress.sun_family = AF_UNIX;

    /* bind to our source socket */
    for (int i = 0; ((i < MAX_UNIX_CLIENTS) && (retCode < 0)); i++)
    {
      sprintf(_sourceUnixAddress.sun_path, "%s/pshellCliClient%d", PSHELL_UNIX_SOCKET_PATH, (rand()%MAX_UNIX_CLIENTS));
      retCode = bind(_socketFd, (struct sockaddr *) &_sourceUnixAddress, sizeof(_sourceUnixAddress));
    }
    
    if (retCode < 0)
    {
      printf("PSHELL_ERROR: Cannot bind to UNIX socket: %s\n", _sourceUnixAddress.sun_path);
      return (false);
    }

    /* setup our destination server information */
    _isUnixConnected = true;
    _destUnixAddress.sun_family = AF_UNIX;
    sprintf(_destUnixAddress.sun_path, "%s/%s", PSHELL_UNIX_SOCKET_PATH, server_);
    
  }

  return (getVersion() && 
          getPayloadSize() && 
          getServerName() && 
          getIpAddress() && 
          getTitle() && 
          getBanner() && 
          getPrompt());

}

/******************************************************************************/
/******************************************************************************/
bool send(void)
{
  if (_serverType == UDP)
  {
    if (sendto(_socketFd,
               (char*)&_pshellSendMsg,
               strlen(_pshellSendMsg.payload)+PSHELL_HEADER_SIZE,
               0,
               (struct sockaddr *) &_destIpAddress,
               sizeof(_destIpAddress)) < 0)
    {
      printf("PSHELL_ERROR: Not all data sent\n");
      return (false);
    }
  }
  else  /* UNIX server */
  {
    if (sendto(_socketFd,
               (char*)&_pshellSendMsg,
               strlen(_pshellSendMsg.payload)+PSHELL_HEADER_SIZE,
               0,
               (struct sockaddr *) &_destUnixAddress,
               sizeof(_destUnixAddress)) < 0)
    {
      printf("PSHELL_ERROR: Not all data sent\n");
      return (false);
    }
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
bool receive(void)
{
  bool bufferOverflow;
  int receivedSize = 0;
  fd_set readFd;
  struct timeval timeout;

  FD_ZERO (&readFd);
  FD_SET(_socketFd,&readFd);

  memset(&timeout, 0, sizeof(timeout));
  timeout.tv_sec = _serverResponseTimeout;
  timeout.tv_usec = 0;

  do
  {
    bufferOverflow = false;
    if ((select(_socketFd+1, &readFd, NULL, NULL, &timeout)) < 0)
    {
      printf("PSHELL_ERROR: Error on socket select\n");
      return (false);
    }

    if (FD_ISSET(_socketFd, &readFd))
    {
      if ((receivedSize = recvfrom(_socketFd,
                                   (char *)_pshellRcvMsg,
                                   _pshellPayloadSize+PSHELL_HEADER_SIZE,
                                   0,
                                   (struct sockaddr *)0,
                                   (socklen_t *)0)) < 0)
      {
        printf("PSHELL_ERROR: Data receive error from remote pshellServer\n");
        return (false);
      }
    }
    else
    {
      printf("PSHELL_ERROR: Response timeout from remote pshellServer\n");
      return (false);
    }

    /* make sure we null terminate the buffer so it displays good */
    _pshellRcvMsg->payload[receivedSize-PSHELL_HEADER_SIZE] = 0;

    /* check for a buffer overflow indication */
    if (_pshellRcvMsg->header.msgType == PSHELL_UPDATE_PAYLOAD_SIZE)
    {
      /* buffer overflow, adjust our buffer size */
      _pshellPayloadSize = atoi(_pshellRcvMsg->payload);
      _pshellRcvMsg = (PshellMsg*)realloc(_pshellRcvMsg, _pshellPayloadSize+PSHELL_HEADER_SIZE);
      bufferOverflow = true;
    }

  } while (bufferOverflow);

  return (true);
}

/******************************************************************************/
/******************************************************************************/
bool processCommand(char msgType_, char *command_, unsigned rate_, bool clear_, bool silent_)
{
  _pshellSendMsg.header.msgType = msgType_;
  _pshellSendMsg.header.respNeeded = true;
  _pshellSendMsg.payload[0] = 0;
  if (command_ != NULL)
  {
    strncpy(_pshellSendMsg.payload, command_, PSHELL_PAYLOAD_SIZE);
  }

  do
  {
    if (clear_)
    {
      clearScreen();
    }
    _pshellRcvMsg->header.msgType = msgType_;
    if (send())
    {
      while (_pshellRcvMsg->header.msgType != PSHELL_COMMAND_COMPLETE)
      {
        if (receive())
        {
          if (!silent_)
          {
            fprintf(stdout, "%s", _pshellRcvMsg->payload);
            fflush(stdout);
          }
        }
        else
        {
          return (false);
        }
      }
    }
    else
    {
      return (false);
    }
    sleep(rate_);
  } while (rate_);

  return (true);

}

/******************************************************************************/
/******************************************************************************/
void stripWhitespace(char *string_)
{
  unsigned i;
  char *str = string_;

  for (i = 0; i < strlen(string_); i++)
  {
    if (!isspace(string_[i]))
    {
      str = &string_[i];
      break;
    }
  }
  if (string_ != str)
  {
    strcpy(string_, str);
  }
  /* now NULL terminate the string */
  if (string_[strlen(string_)-1] == '\n')
  {
    string_[strlen(string_)-1] = 0;
  }
}

/******************************************************************************/
/******************************************************************************/
void getInput(void)
{
#ifdef READLINE
  char *commandLine;
  _interactiveCommand[0] = 0;
  while (strlen(_interactiveCommand) == 0)
  {
    commandLine = NULL;
    _commandFound = false;
    commandLine = readline(_interactivePrompt);
    if (commandLine && *commandLine)
    {
      add_history(commandLine);
      strcpy(_interactiveCommand, commandLine);
      stripWhitespace(_interactiveCommand);
    }
    free(commandLine);
  }
#else
  _interactiveCommand[0] = 0;
  while (strlen(_interactiveCommand) == 0)
  {
    fprintf(stdout, "%s", _interactivePrompt);
    fflush(stdout);
    fgets(_interactiveCommand, PSHELL_PAYLOAD_SIZE, stdin);
    stripWhitespace(_interactiveCommand);
  }
#endif
}

/******************************************************************************/
/******************************************************************************/
bool isDuplicate(char *command_)
{
  unsigned i;
  for (i = 0; i < _numNativeInteractiveCommands; i++)
  {
    if (strcmp(_nativeInteractiveCommands[i], command_) == 0)
    {
      printf("PSHELL_WARNING: Server command: '%s', is duplicate of a native interactive client command\n",
             command_);
      printf("              server command will be available in command line mode only\n");
      return (true);
    }
  }
  return (false);
}

/******************************************************************************/
/******************************************************************************/
void buildCommandList(void)
{
  unsigned i;
  char *str;

  _pshellCommandList = (const char**)malloc((_maxPshellCommands+_numNativeInteractiveCommands)*sizeof(char*));
  for (i = 0; i < _numNativeInteractiveCommands; i++)
  {
    _pshellCommandList[_numPshellCommands++] = _nativeInteractiveCommands[i];
  }
  if ((str = strtok(_pshellRcvMsg->payload, PSHELL_COMMAND_DELIMETER)) != NULL)
  {
    /* got our first command */
    if (!isDuplicate(str))
    {
      _pshellCommandList[_numPshellCommands++] = strdup(str);
    }
    /* now go get any others */
    while ((str = strtok(NULL, PSHELL_COMMAND_DELIMETER)) != NULL)
    {
      if (_numPshellCommands < _maxPshellCommands)
      {
        if (!isDuplicate(str))
        {
          _pshellCommandList[_numPshellCommands++] = strdup(str);
        }
      }
      else
      {
        /*
        * we have exceeded our max commands, go get another chunk of
        * of memory and copy over the current commands and release
        * the original chunk of memory
        */
        if (!isDuplicate(str))
        {
          _maxPshellCommands += PSHELL_COMMAND_CHUNK;
          _pshellCommandList = (const char**)realloc(_pshellCommandList,
                                                   (_maxPshellCommands+_numNativeInteractiveCommands)*sizeof(char*));
                                                   _pshellCommandList[_numPshellCommands++] = strdup(str);
        }
      }
    }
  }

  _maxCommandLength = 0;
  for (i = 0; i < _numPshellCommands; i++)
  {
    if (strlen(_pshellCommandList[i]) > _maxCommandLength)
    {
      _maxCommandLength = strlen(_pshellCommandList[i]);
    }
  }

}

#ifdef READLINE

/******************************************************************************/
/******************************************************************************/
char **commandCompletion(const char *command_, int start_, int end_)
{
  _commandFound = (start_ > 0);
  return (rl_completion_matches(command_, commandGenerator));
}

/******************************************************************************/
/******************************************************************************/
char *commandGenerator(const char *command_, int state_)
{
  const char *command;
  char *returnCommand;

  /*
  * if this is a new command to complete, set the starting search
  * command, this funciton is called repeatedly, the state must be
  * preserved between calls, that is why _matchLength and _currentPos
  * are static global variables, we need to pick up where we left off
  */
  if (state_ == 0)
  {
    _matchLength = strlen(command_);
    _commandPos = 0;
  }

  while ((_commandPos < _numPshellCommands) && (!_commandFound))
  {

    /* get our command string */
    command = _pshellCommandList[_commandPos++];

    if (::strncmp(command, command_, _matchLength) == 0)
    {
      returnCommand = (char*)malloc(strlen(command));
      strcpy(returnCommand, command);
      return (returnCommand);
    }

  }

  /*
  * if no names matched, then return NULL,
  * supress normal filename completion when
  * we are done finding matches
  */
  rl_attempted_completion_over = 1;
  return (NULL);

}
#endif /* READLINE */

/******************************************************************************/
/******************************************************************************/
bool initInteractiveMode(void)
{

  /*
   * query our command list, need this to check for duplicates
   * with native interactive commands and build up a list for
   * the TAB completion if that feature is enabled
   */
  if (!processCommand(PSHELL_QUERY_COMMANDS2, NULL, 0, false, true))
  {
    return (false);
  }
  buildCommandList();

#ifdef READLINE
  /* register our TAB completion function */
  rl_attempted_completion_function = commandCompletion;
#endif

  /* setup our prompt */
  sprintf(_interactivePrompt,
          "%s[%s]:%s",
          _serverName,
          _ipAddress,
          _prompt);

  /* setup our title bar */
  fprintf(stdout,
          "\033]0;%s: %s[%s], Mode: INTERACTIVE\007",
          _title,
          _serverName,
          _ipAddress);
  fflush(stdout);

  return (true);

}

/******************************************************************************/
/******************************************************************************/
void processBatchFile(char *filename_, unsigned rate_, bool clear_)
{
  FILE *fp;
  char inputLine[180];
  char batchFile[180];
  char *batchPath;

  if ((batchPath = getenv("PSHELL_BATCH_DIR")) != NULL)
  {
    sprintf(batchFile, "%s/%s", batchPath, filename_);
  }
  if ((fp = fopen(batchFile, "r")) != NULL)
  {
    do
    {
      if (clear_)
      {
        clearScreen();
      }
      while (fgets(inputLine, 180, fp) != NULL)
      {
        if (inputLine[0] != '#')
        {
          if (inputLine[strlen(inputLine)-1] == '\n')
          {
            inputLine[strlen(inputLine)-1] = 0;  /* NULL terminate */
          }
          processCommand(PSHELL_USER_COMMAND, inputLine, 0, false, false);
        }
      }
      sleep(rate_);
      rewind(fp);
    } while (rate_);
    fclose(fp);
  }
  else
  {
    printf("PSHELL_ERROR: Could not open batch file: '%s'\n", batchFile);
  }
}

/******************************************************************************/
/******************************************************************************/
void tokenize(char *string_,
              const char *delimeter_,
              char *tokens_[],
              unsigned maxTokens_,
              unsigned *numTokens_)
{
  char *str;
  *numTokens_ = 0;
  if ((str = strtok(string_, delimeter_)) != NULL)
  {
    tokens_[(*numTokens_)++] = str;
    while ((str = strtok(NULL, delimeter_)) != NULL)
    {
      tokens_[(*numTokens_)++] = str;
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void processInteractiveMode(void)
{
  char command[80];
  char *tokens[MAX_TOKENS];
  unsigned numTokens;
  if (initInteractiveMode())
  {
    for (;;)
    {
      getInput();
      strcpy(command, _interactiveCommand);
      tokenize(command, " ", tokens, MAX_TOKENS, &numTokens);
      if ((strstr(_nativeInteractiveCommands[HELP_INDEX], tokens[0]) ==
          _nativeInteractiveCommands[HELP_INDEX]) ||
          (strcmp(_interactiveCommand, "?") == 0))
      {
        if (numTokens == 1)
        {
          showCommands();
        }
        else
        {
          printf("Usage: help\n");
        }
      }
      else if (strstr(_nativeInteractiveCommands[QUIT_INDEX], tokens[0]) ==
               _nativeInteractiveCommands[QUIT_INDEX])
      {
        if (numTokens == 1)
        {
          exitProgram(0);
        }
        else
        {
          printf("Usage: quit\n");
        }
      }
      else if (strstr(_nativeInteractiveCommands[BATCH_INDEX], tokens[0]) ==
               _nativeInteractiveCommands[BATCH_INDEX])
      {
        if (numTokens == 2)
        {
          if ((strcmp(tokens[1], "?") == 0) || (strcmp(tokens[1], "-h") == 0))
          {
            printf("Usage: batch <filename>\n");
          }
          else
          {
            processBatchFile(tokens[1], 0, false);
          }
        }
        else
        {
          printf("Usage: batch <filename>\n");
        }
      }
      else
      {
        processCommand(PSHELL_USER_COMMAND, _interactiveCommand, 0, false, false);
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void getNamedServers(void)
{
  char serversFile[180];
  char defaultServersFile[180];
  char line[180];
  char *serversPath;
  char *str1;
  char *str2;
  FILE *fp;

  sprintf(defaultServersFile, "%s/pshell-client.conf", PSHELL_CONFIG_DIR);
  if ((serversPath = getenv("PSHELL_CONFIG_DIR")) != NULL)
  {
    sprintf(serversFile, "%s/pshell-client.conf", serversPath);
  }

  if ((fp = fopen(serversFile, "r")) == NULL)
  {
    fp = fopen(defaultServersFile, "r");
  }

  if (fp != NULL)
  {
    _pshellServersList = (PshellServers*)malloc(_pshellServersListSize*sizeof(PshellServers));
    while (fgets(line, 180, fp) != NULL)
    {
      if ((line[0] != '#') &&
          ((str1 = strtok(line, ":")) != NULL) &&
          ((str2 = strtok(NULL, ":")) != NULL))
      {
        /* got both args */
        if (_numPshellServers < _pshellServersListSize)
        {
          _pshellServersList[_numPshellServers].serverName = strdup(str1);
          _pshellServersList[_numPshellServers].portNum = atoi(str2);
          if (strlen(_pshellServersList[_numPshellServers].serverName) > _maxServerNameLength)
          {
            _maxServerNameLength = strlen(_pshellServersList[_numPshellServers].serverName);
          }
          _numPshellServers++;
        }
        else
        {
          /* grab a new chunk for our servers list */
          _pshellServersListSize += PSHELL_SERVERS_CHUNK;
          _pshellServersList = (PshellServers*)realloc(_pshellServersList,
                                                   _pshellServersListSize*sizeof(PshellServers));
          /* now add the new server */
          _pshellServersList[_numPshellServers].serverName = strdup(str1);
          _pshellServersList[_numPshellServers].portNum = atoi(str2);
          if (strlen(_pshellServersList[_numPshellServers].serverName) > _maxServerNameLength)
          {
            _maxServerNameLength = strlen(_pshellServersList[_numPshellServers].serverName);
          }
          _numPshellServers++;
        }
      }
    }
    fclose(fp);
  }
}

/******************************************************************************/
/******************************************************************************/
void showNamedServers(void)
{

  unsigned server;
  unsigned i;
  const char *banner = "Server Name";
  unsigned fieldWidth = 0;

  printf("\n");
  printf("************************************\n");
  printf("*      Available PSHELL Servers      *\n");
  printf("************************************\n");
  printf("\n");

  strlen(banner) > _maxServerNameLength ? fieldWidth = strlen(banner) : fieldWidth = _maxServerNameLength;
  printf("%s", banner);

  for (i = strlen(banner); i < fieldWidth; i++)
  {
    printf(" ");
  }
  printf("  Port Number\n");
  for (i = 0; i < fieldWidth; i++)
  {
    printf("-");
  }
  printf("  -----------\n");
  for (server = 0; server < _numPshellServers; server++)
  {
    printf("%s", _pshellServersList[server].serverName);
    for (i = strlen(_pshellServersList[server].serverName); i < fieldWidth; i++)
    {
      printf(" ");
    }
    printf("  %d\n", _pshellServersList[server].portNum);
  }
  printf("\n");

}

/******************************************************************************/
/******************************************************************************/
void showCommands(void)
{
  unsigned i;
  unsigned pad;

  printf("\n");
  printf("****************************************\n");
  printf("*             COMMAND LIST             *\n");
  printf("****************************************\n");
  printf("\n");

  if (_mode == INTERACTIVE)
  {
    for (i = 0; i < _numNativeInteractiveCommands; i++)
    {
      printf("%s", _nativeInteractiveCommands[i]);
      for (pad = strlen(_nativeInteractiveCommands[i]); pad < _maxCommandLength; pad++) printf(" ");
      printf("  -  %s\n", _nativeInteractiveCommandDescriptions[i]);
    }
  }
  processCommand(PSHELL_QUERY_COMMANDS1, NULL, 0, false, false);
}

/******************************************************************************/
/******************************************************************************/
void showUsage(void)
{
  printf("\n");
  printf("Usage: pshell -h | ? | -s | [-t<timeout>] {<hostName> | <ipAddress> | unix} {<serverName> | <portNum>}\n");
  printf("              [-h | ? | {<command> [<rate> [clear]]} | {-f <fileName> [<rate> [clear]}]\n");
  printf("\n");
  printf("  where:\n");
  printf("\n");
  printf("    -h,?       - show pshell usage or command set of specified PSHELL server\n");
  printf("    -s         - show named servers in $PSHELL_CONFIG_DIR/pshell-client.conf file\n");
  printf("    -f         - run commands from a batch file\n");
  printf("    -t         - change the default server response timeout\n");
  printf("    hostName   - hostname of desired PSHELL server (UDP only)\n");
  printf("    ipAddress  - IP address of desired PSHELL server (UDP only)\n");
  printf("    unix       - Specifies server is a UNIX domain server\n");
  printf("    serverName - name of desired PSHELL server (UDP or UNIX)\n");
  printf("    portNum    - port number of desired PSHELL server (UDP only)\n");
  printf("    timeout    - value in seconds to use for server response timeout\n");
  printf("    command    - optional command to execute\n");
  printf("    fileName   - optional batch file to execute\n");
  printf("    rate       - optional rate to repeat command or batch file (in seconds)\n");
  printf("    clear      - clear screen between commands or batch file passes\n");
  printf("\n");
  printf("    NOTE: If no <command> is given, pshell will be started\n");
  printf("          up in interactive mode, commands issued in command\n");
  printf("          line mode that require arguments must be enclosed \n");
  printf("          in double quotes, commands issued in interactive\n");
  printf("          mode that require arguments do not require double\n");
  printf("          quotes.\n");
  printf("\n");
  printf("          To get help on a command in command line mode, type\n");
  printf("          \"<command> ?\" or \"<command> -h\".  To get help in\n");
  printf("          interactive mode type 'help' or '?' at the prompt to\n");
  printf("          see all available commands, to get help on a single\n");
  printf("          command, type '<command> {? | -h}'.");
#ifdef READLINE
  printf("  Use TAB completion\n");
  printf("          to fill out partial commands and up-arrow to recall\n");
  printf("          the last entered command.\n");
#else
  printf("\n");
#endif
  printf("\n");
}

/******************************************************************************/
/******************************************************************************/
bool isDec(char *string_)
{
  unsigned i;
  if (string_ != NULL)
  {
    for (i = 0; i < strlen(string_); i++)
    {
      if (!isdigit(string_[i]))
      {
        return (false);
      }
    }
    return (true);
  }
  else
  {
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
void checkForTimeoutOverride(int *argc, char *argv[])
{
  int i;
  if (*argc > 3)
  {
    if (strstr(argv[1], "-t") == argv[1])
    {
      if ((strlen(argv[1]) > 2) && (isDec(&argv[1][2])))
      {
        _serverResponseTimeout = atoi(&argv[1][2]);
      }
      else
      {
        printf("PSHELL_ERROR: Must provide value for timeout, e.g. -t20\n");
      }
      for (i = 1; i < (*argc-1); i++)
      {
        argv[i] = argv[i+1];
      }
      (*argc)--;
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void exitProgram(int exitCode_)
{
  if (_isUnixConnected)
  {
    unlink(_sourceUnixAddress.sun_path);
  }
  exit(exitCode_);
}

/******************************************************************************/
/******************************************************************************/
int main(int argc, char *argv[])
{

  unsigned rate = 0;
  bool clear = false;
  char commandName[PSHELL_PAYLOAD_SIZE];
  char *str;

  _mode = COMMAND_LINE;

  strcpy(_title, "PSHELL");
  strcpy(_banner, "PSHELL: Process Specific Embedded Command Line Shell");
  strcpy(_prompt, "PSHELL> ");

  checkForTimeoutOverride(&argc, argv);

  /* check for the proper usage */
  if ((argc >= 2) && (argc <= 7))
  {
    getNamedServers();
    if (argc == 2)
    {
      if ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "?") == 0))
      {
        showUsage();
        exitProgram(1);
      }
      else if (strcmp(argv[1], "-s") == 0)
      {
        showNamedServers();
        exitProgram(0);
      }
      else
      {
        showUsage();
        exit(1);
      }
    }
    else if (argc == 3)
    {
      _mode = INTERACTIVE;
    }
    else if (argc == 4)
    {
      if ((strcmp(argv[3], "-h") == 0) ||
          (strcmp(argv[3], "help") == 0) ||
          (strcmp(argv[3], "?") == 0))
        {
          if (init(argv[1], argv[2]))
          {
            showCommands();
          }
          exitProgram(0);
        }
        _mode = COMMAND_LINE;
    }
    else if (argc == 5)
    {
      if (strcmp(argv[3], "-f") == 0)
      {
        _mode = BATCH;
      }
      else
      {
        _mode = COMMAND_LINE;
        rate = atoi(argv[4]);
      }
    }
    else  if (argc == 6)
    {
      if (strcmp(argv[3], "-f") == 0)
      {
        _mode = BATCH;
        rate = atoi(argv[5]);
      }
      else if (strcmp(argv[5], "clear") == 0)
      {
        _mode = COMMAND_LINE;
        rate = atoi(argv[4]);
        clear = true;
      }
      else
      {
        showUsage();
        exitProgram(0);
      }
    }
    else  /* argc == 7 */
    {
      if ((strcmp(argv[3], "-f") == 0) && (strcmp(argv[6], "clear") == 0))
      {
        _mode = BATCH;
        rate = atoi(argv[5]);
        clear = true;
      }
      else
      {
        showUsage();
        exitProgram(0);
      }
    }
    if (init(argv[1], argv[2]))
    {
      if (_mode == INTERACTIVE)
      {
        showWelcome();
        processInteractiveMode();
      }
      else if (_mode == COMMAND_LINE)
      {
        /*
        * setup our title bar, no real need to do this if we are not
        * repeating the command, since it will be too quick to see
        * anyway
        */
        if (rate)
        {
          /* just get the command name and not any parameters for the title */
          strcpy(commandName, argv[3]);
          str = strtok(commandName, " ");
          fprintf(stdout,
                  "\033]0;%s: %s[%s], Mode: COMMAND LINE[%s], Rate: %d SEC\007",
                  _title,
                  _serverName,
                  _ipAddress,
                  str,
                  rate);
                  fflush(stdout);
        }
        processCommand(PSHELL_USER_COMMAND, argv[3], rate, clear, false);
      }
      else  /* _mode == BATCH */
      {
        /*
        * setup our title bar, no real need to do this if we are not
        * repeating the command, since it will be too quick to see
        * anyway
        */
        if (rate)
        {
          fprintf(stdout,
                  "\033]0;%s: %s[%s], Mode: BATCH[%s], Rate: %d SEC\007",
                  _title,
                  _serverName,
                  _ipAddress,
                  argv[4],
                  rate);
                  fflush(stdout);
        }
        processBatchFile(argv[4], rate, clear);
      }
    }
  }
  else
  {
    showUsage();
    exitProgram(1);
  }
  exitProgram(0);
}
