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
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <dirent.h>

#include <PshellCommon.h>
#include <PshellReadline.h>

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
#define PSHELL_CONFIG_DIR "/etc/pshell/config"
#endif

/*
 * the default batch dir is where the server will look for the specified filename
 * as given in the interacive "batch" command, if it does not find it in the directory
 * specified by the env variable PSHELL_BATCH_DIR, or the CWD, change this to any other
 * desired default location, the batch file can contain any commands that can be entered
 * to the server via the interacive command line, the batch file is executed during the
 * startup of the server, so it can serve as a program initialization file, it can also
 * be re-executed via the "batch" command
 */

#ifndef PSHELL_BATCH_DIR
#define PSHELL_BATCH_DIR "/etc/pshell/batch"
#endif

/*
 * the maximum number of command line arguments that can be tokenized
 */

#define MAX_TOKENS 64

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

bool _isBroadcastServer = false;
bool _serverResponseTimeoutOverride = false;
int _serverResponseTimeout = PSHELL_SERVER_RESPONSE_TIMEOUT;
int _socketFd;
struct sockaddr_in _sourceIpAddress;
struct sockaddr_in _destIpAddress;
struct sockaddr_un _sourceUnixAddress;
struct sockaddr_un _destUnixAddress;
int _destPort = 0;
bool _isUnixConnected = false;

#define MAX_UNIX_CLIENTS 1000

#define USEC_PER_SECOND 1000000

ServerType _serverType = UDP;

#define MAX_ACTIVE_SERVERS 1000
const char *_unixSocketPath = "/tmp/.pshell/";
const char *_lockFileExtension = ".lock";
static const char *_unixLockFileId = "unix.lock";
DIR *_dir;
struct ActiveServer
{
  const char *name;
  const char *type;
  const char *host;
  const char *port;
};
int _numActiveServers = 0;
ActiveServer _activeServers[MAX_ACTIVE_SERVERS] = {};

char _unixLocalSocketName[256];
char _interactiveCommand[PSHELL_RL_MAX_COMMAND_SIZE];
char _interactivePrompt[PSHELL_RL_MAX_COMMAND_SIZE];
char _serverName[PSHELL_RL_MAX_COMMAND_SIZE];
char _ipAddress[PSHELL_RL_MAX_COMMAND_SIZE];
char _title[PSHELL_RL_MAX_COMMAND_SIZE];
char _banner[PSHELL_RL_MAX_COMMAND_SIZE];
char _prompt[PSHELL_RL_MAX_COMMAND_SIZE];
char _serverDisplay[PSHELL_RL_MAX_COMMAND_SIZE];
const char *_host;
const char *_server;
unsigned _version;
Mode _mode;
unsigned _maxCommandLength = strlen("history");
struct Command
{
  const char *name;
  const char *description;
};
#define QUIT_INDEX 0
#define HELP_INDEX 1
#define HISTORY_INDEX 2
#define BATCH_INDEX 3
const Command _nativeInteractiveCommands [] =
{
  {"quit",    "exit interactive mode"},
  {"help",    "show all available commands"},
  {"history", "show history list of all entered commands"},
  {"batch",   "run commands from a batch file"}
};
unsigned _numNativeInteractiveCommands = sizeof(_nativeInteractiveCommands)/sizeof(Command);

/*
 * the initial message buffer is just used for the initial
 * PSHELL_QUERY_VERSION and PSHELL_QUERY_PAYLOAD_SIZE messages,
 * after that the message payload is dynaically allocated for
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
  const char *portNum;
  unsigned timeout;
};

PshellServers *_pshellServersList;
unsigned _numPshellServers = 0;
unsigned _pshellServersListSize = PSHELL_SERVERS_CHUNK;
unsigned _maxNamedServerLength = 11;
unsigned _maxActiveServerLength = 11;
unsigned _maxHostnameLenth = 4;

const char **_pshellCommandList;
unsigned _numPshellCommands = 0;
unsigned _maxPshellCommands = PSHELL_COMMAND_CHUNK;

/*
 * structures used to create a list of all the batch files found
 * in the specified directories
 */

struct File
{
  char directory[256];
  char filename[256];
};

#define MAX_BATCH_FILES 256
char _currentDir[PATH_MAX];

struct FileList
{
  int numFiles;
  int maxDirectoryLength;
  int maxFilenameLength;
  File files[MAX_BATCH_FILES];
};

FileList _batchFiles;

/* function prototypes */

bool isNumeric(const char *string_);
const char *findServerPort(const char *name_);
bool getServerName(void);
bool getTitle(void);
bool getBanner(void);
bool getPrompt(void);
bool getPayloadSize(void);
bool getVersion(void);
bool init(const char *destination_, const char *server_);
bool send(void);
bool receive(int serverResponseTimeout);
void tokenize(char *string_, const char *delimeter_, char *tokens_[], unsigned maxTokens_, unsigned *numTokens_);
bool processCommand(char msgType_, char *command_, int rate_, unsigned repeat_, bool clear_, bool silent_);
bool initInteractiveMode(void);
void processInteractiveMode(void);
void processBatchFile(char *filename_, int rate_, unsigned repeat_, bool clear_, bool showOnly_);
void getNamedServers(void);
void showNamedServers(void);
void showActiveServers(void);
const char *getActiveServer(unsigned index_);
int stringCompare(const void* string1_, const void* string2_);
void cleanupFileSystemResources(void);
void showCommands(void);
void stripWhitespace(char *string_);
void showUsage(void);
void buildCommandList(void);
bool isDuplicate(char *command_);
void clearScreen(void);
void showWelcome(void);
void exitProgram(int exitCode_);
void registerSignalHandlers(void);
void parseCommandLine(int *argc, char *argv[], char *host, char *port);
unsigned findCommand(char *command_);
bool isSubString(const char *string1_, const char *string2_, unsigned minChars_);
bool isEqual(const char *string1_, const char *string2_);
bool isFile(const char *directory_, const char *file_);
void findBatchFiles(const char *directory_);
void showBatchFiles(void);
bool isDec(const char *string_);

/*
 * we access this funciton via a 'backdoor' linking via the PshellReadline
 * library, since this function is not of the 'official' published API as
 * specified in the PshellReadline.h file
 */
extern void pshell_rl_showHistory(void);

/******************************************************************************/
/******************************************************************************/
void showWelcome(void)
{
  printf("\n");
  char sessionInfo[256];
  if (_isBroadcastServer)
  {
    sprintf(sessionInfo, "Multi-session BROADCAST server: %s", _serverDisplay);
  }
  else if (_serverType == UDP)
  {
    sprintf(sessionInfo, "Multi-session UDP server: %s", _serverDisplay);
  }
  else
  {
    sprintf(sessionInfo, "Multi-session UNIX server: %s", _serverDisplay);
  }
  unsigned maxLength = MAX(strlen(_banner), strlen(sessionInfo))+3;
  PSHELL_PRINT_WELCOME_BORDER(printf, maxLength);
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  %s\n", PSHELL_WELCOME_BORDER, _banner);
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  %s\n", PSHELL_WELCOME_BORDER, sessionInfo);
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  Idle session timeout: NONE\n", PSHELL_WELCOME_BORDER);
  if (!_isBroadcastServer)
  {
    printf("%s\n", PSHELL_WELCOME_BORDER);
    if (_serverResponseTimeout > 0)
    {
      printf("%s  Command response timeout: %d seconds\n", PSHELL_WELCOME_BORDER, _serverResponseTimeout);
    }
    else
    {
      printf("%s  Command response timeout: NONE\n", PSHELL_WELCOME_BORDER);
      printf("%s\n", PSHELL_WELCOME_BORDER);
      printf("%s  WARNING: Interactive client started with no command\n", PSHELL_WELCOME_BORDER);
      printf("%s           response timeout.  All commands will be\n", PSHELL_WELCOME_BORDER);
      printf("%s           sent as 'fire-and-forget', no results will\n", PSHELL_WELCOME_BORDER);
      printf("%s           be extracted or displayed\n", PSHELL_WELCOME_BORDER);
    }
    printf("%s\n", PSHELL_WELCOME_BORDER);
    printf("%s  The default response timeout can be changed on a\n", PSHELL_WELCOME_BORDER);
    printf("%s  per-command basis by preceeding the command with\n", PSHELL_WELCOME_BORDER);
    printf("%s  option -t<timeout> (use -t0 for no response)\n", PSHELL_WELCOME_BORDER);
    printf("%s\n", PSHELL_WELCOME_BORDER);
    printf("%s  e.g. -t10 <command>\n", PSHELL_WELCOME_BORDER);
    printf("%s\n", PSHELL_WELCOME_BORDER);
    printf("%s  The default timeout for all commands can be changed\n", PSHELL_WELCOME_BORDER);
    printf("%s  by using the -t<timeout> option with no command, to\n", PSHELL_WELCOME_BORDER);
    printf("%s  display the current default timeout, just use -t\n", PSHELL_WELCOME_BORDER);
  }
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  To show command elapsed execution time, use -t <command>\n", PSHELL_WELCOME_BORDER);
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  Type '?' or 'help' at prompt for command summary\n", PSHELL_WELCOME_BORDER);
  printf("%s  Type '?' or '-h' after command for command usage\n", PSHELL_WELCOME_BORDER);
  printf("%s\n", PSHELL_WELCOME_BORDER);
  printf("%s  Full <TAB> completion, command history, command\n", PSHELL_WELCOME_BORDER);
  printf("%s  line editing, and command abbreviation supported\n", PSHELL_WELCOME_BORDER);
  if (_isBroadcastServer)
  {
    printf("%s\n", PSHELL_WELCOME_BORDER);
    printf("%s  NOTE: Connected to a broadcast address, all commands\n", PSHELL_WELCOME_BORDER);
    printf("%s        are single-shot, 'fire-and-forget', with no\n", PSHELL_WELCOME_BORDER);
    printf("%s        response requested or expected, and no results\n", PSHELL_WELCOME_BORDER);
    printf("%s        displayed.  All commands are 'invisible' since\n", PSHELL_WELCOME_BORDER);
    printf("%s        no remote command query is requested.\n", PSHELL_WELCOME_BORDER);
  }
  printf("%s\n", PSHELL_WELCOME_BORDER);
  PSHELL_PRINT_WELCOME_BORDER(printf, maxLength);
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
bool isNumeric(const char *string_)
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
const char *findServerPort(const char *name_)
{
  unsigned server;
  unsigned index;
  unsigned numFound = 0;

  for (index = 0; index < _numPshellServers; index++)
  {
    if ((strlen(name_) == strlen(_pshellServersList[index].serverName)) &&
         strcmp(name_, _pshellServersList[index].serverName) == 0)
    {
      /* exact match, break out, assumes list does not have duplicate entries */
      server = index;
      numFound = 1;
      break;
    }
    else if (strncmp(name_, _pshellServersList[index].serverName, strlen(name_)) == 0)
    {
      server = index;
      numFound++;
    }
  }
  if (numFound == 1)
  {
    /* only set the server response timeout if we did not get a command line override */
    if (!_serverResponseTimeoutOverride)
    {
      _serverResponseTimeout = _pshellServersList[server].timeout;
    }
    return (_pshellServersList[server].portNum);
  }
  else if (numFound == 0)
  {
    printf("\n");
    printf("PSHELL_ERROR: Could not find server: '%s' in file: 'pshell-client.conf'\n", name_);
    showNamedServers();
    exitProgram(0);
  }
  else if (numFound > 1)
  {
    printf("\n");
    printf("PSHELL_ERROR: Ambiguous server name: '%s' found in pshell-client.conf file\n", name_);
    showNamedServers();
    exitProgram(0);
  }
  return (NULL);
}

/******************************************************************************/
/******************************************************************************/
bool getVersion(void)
{
  if (processCommand(PSHELL_QUERY_VERSION, NULL, -1, 0, false, true))
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
  if (processCommand(PSHELL_QUERY_NAME, NULL, -1, 0, false, true))
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
  if (processCommand(PSHELL_QUERY_TITLE, NULL, -1, 0, false, true))
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
  if (processCommand(PSHELL_QUERY_BANNER, NULL, -1, 0, false, true))
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
  if (processCommand(PSHELL_QUERY_PROMPT, NULL, -1, 0, false, true))
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
bool getPayloadSize(void)
{
  if (processCommand(PSHELL_QUERY_PAYLOAD_SIZE, NULL, -1, 0, false, true))
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
bool init(const char *destination_, const char *server_)
{
  char requestedHost[180];
  char destination[180];
  char unixLockFile[180];
  int unixLockFd;
  struct hostent *host;
  const char *port;
  int retCode = -1;
  char *ipAddrOctets[MAX_TOKENS];
  unsigned numTokens;
  int on = 1;

   /* see if it is a named server, numeric port, or UNIX domain server */
  if (strcmp(destination_, "unix") == 0)
  {
    _destPort = 0;
  }
  else if (isNumeric(server_))
  {
    _destPort = atoi(server_);
  }
  else if ((port = findServerPort(server_)) != NULL)
  {
    _destPort = atoi(port);
  }

  /* see if our destination is a UDP or UNIX domain socket */
  if (_destPort > 0)
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

    /*
     * see if we are trying to connect to a broadcast address,
     * if so, set our socket options to allow for broadcast
     */
    strcpy(destination, destination_);
    tokenize(destination, ".", ipAddrOctets, MAX_TOKENS, &numTokens);
    if ((numTokens == 4) && (strcmp(ipAddrOctets[3], "255") == 0))
    {
      /* setup socket for broadcast */
      setsockopt(_socketFd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
      _isBroadcastServer = true;
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
    _destIpAddress.sin_port = htons(_destPort);

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
    strcpy(_ipAddress, inet_ntoa(_destIpAddress.sin_addr));
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
      sprintf(_sourceUnixAddress.sun_path, "%s/%s-control%d", PSHELL_UNIX_SOCKET_PATH, server_, (rand()%MAX_UNIX_CLIENTS));
      sprintf(unixLockFile, "%s%s", _sourceUnixAddress.sun_path, _lockFileExtension);
      /* try to open lock file */
      if ((unixLockFd = open(unixLockFile, O_RDONLY | O_CREAT, 0600)) > -1)
      {
        /* file exists, try to see if another process has it locked */
        if (flock(unixLockFd, LOCK_EX | LOCK_NB) == 0)
        {
          /* we got the lock, nobody else has it, bind to our socket */
          retCode = bind(_socketFd, (struct sockaddr *) &_sourceUnixAddress, sizeof(_sourceUnixAddress));
        }
      }
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
    strcpy(_ipAddress, "unix");
  }

  if (_isBroadcastServer)
  {
    /* for a broadcast server, we cannot query any information  */
    _pshellSendMsg.header.respNeeded = false;
    _pshellSendMsg.header.dataNeeded = false;
    strcpy(_serverName, "broadcastServer");
    sprintf(_serverDisplay, "%s[%s:%d]", _serverName, _ipAddress, _destPort);
    _numNativeInteractiveCommands--;
    for (unsigned i = 0; i < _numNativeInteractiveCommands; i++)
    {
      pshell_rl_addTabCompletion(_nativeInteractiveCommands[i].name);
    }
    return (true);
  }
  else
  {
    if (getVersion() &&
        getPayloadSize() &&
        getServerName() &&
        getTitle() &&
        getBanner() &&
        getPrompt())
    {
      if (_serverType == UNIX)
      {
        sprintf(_serverDisplay, "%s[%s]", _serverName, _ipAddress);
      }
      else
      {
        sprintf(_serverDisplay, "%s[%s:%d]", _serverName, _ipAddress, _destPort);
      }
      return (true);
    }
    else
    {
      return (false);
    }
  }
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
bool receive(int serverResponseTimeout)
{
  bool bufferOverflow;
  int receivedSize = 0;
  fd_set readFd;
  struct timeval timeout;

  if (serverResponseTimeout > 0)
  {

    FD_ZERO (&readFd);
    FD_SET(_socketFd,&readFd);

    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = serverResponseTimeout;
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
  }
  else
  {
    _pshellRcvMsg->payload[0] = 0;
    _pshellRcvMsg->header.msgType = PSHELL_COMMAND_COMPLETE;
  }

  return (true);
}

/******************************************************************************/
/******************************************************************************/
bool isSubString(const char *string1_, const char *string2_, unsigned minChars_)
{
  unsigned length;
  if ((string1_ == NULL) && (string2_ == NULL))
  {
    return (true);
  }
  else if ((string1_ != NULL) && (string2_ != NULL))
  {
    if ((length = strlen(string1_)) > strlen(string2_))
    {
      return (false);
    }
    else
    {
      return (strncmp(string1_, string2_, MAX(length, minChars_)) == 0);
    }
  }
  else
  {
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool isEqual(const char *string1_, const char *string2_)
{
  if ((string1_ == NULL) && (string2_ == NULL))
  {
    return (true);
  }
  else if ((string1_ != NULL) && (string2_ != NULL))
  {
    return (strcmp(string1_, string2_) == 0);
  }
  else
  {
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
unsigned findCommand(char *command_)
{
  unsigned index;
  unsigned numFound = 0;
  for (index = 0; index < _numNativeInteractiveCommands; index++)
  {
    if (isSubString(command_, _nativeInteractiveCommands[index].name, strlen(command_)))
    {
      numFound++;
    }
  }
  /* see if we have a substring match */
  for (index = 0; index < _numPshellCommands; index++)
  {
    if (isSubString(command_, _pshellCommandList[index], strlen(command_)))
    {
      numFound++;
    }
  }

  /* if we have multiple matches in the substring, look for an exact match */
  if (numFound > 1)
  {
    numFound = 0;
    for (index = 0; index < _numPshellCommands; index++)
    {
      if (isEqual(command_, _pshellCommandList[index]))
      {
        numFound++;
      }
    }
    /*
     * if we did not find an exact match, that means the user entered an ambiguous command
     * abbreviation, set the numFound to something > 1 so we can get the correct error message
     */
     if (numFound == 0)
     {
       numFound = 2;
     }
  }

  return (numFound);
}

/******************************************************************************/
/******************************************************************************/
bool processCommand(char msgType_, char *command_, int rate_, unsigned repeat_, bool clear_, bool silent_)
{
  unsigned iteration = 0;
  unsigned numTokens;
  unsigned commandPos = 0;
  char *tokens[MAX_TOKENS];
  char command[PSHELL_RL_MAX_COMMAND_SIZE];
  int serverResponseTimeout = _serverResponseTimeout;

  _pshellSendMsg.header.respNeeded = true;
  if (msgType_ == PSHELL_USER_COMMAND)
  {
    strcpy(command, command_);
    tokenize(command, " ", tokens, MAX_TOKENS, &numTokens);
    if (isEqual("-t", tokens[0]))
    {
      commandPos = 1;
    }
    else if (isSubString("-t", tokens[0], 2))
    {
      if (numTokens == 1)
      {
         if (strlen(tokens[0]) > 2)
         {
           _serverResponseTimeout = atoi(&tokens[0][2]);
           printf("PSHELL_INFO: Setting server response timeout to: %d seconds\n", _serverResponseTimeout);
         }
         else
         {
           printf("PSHELL_INFO: Current server response timeout: %d seconds\n", _serverResponseTimeout);
         }
         return (true);
      }
      else if (strlen(tokens[0]) > 2)
      {
        serverResponseTimeout = atoi(&tokens[0][2]);
      }
      commandPos = 1;
      command_ = &command_[strlen(tokens[0])+1];
    }
    if (serverResponseTimeout == 0)
    {
      if ((numTokens == commandPos+2) && ((strcmp(tokens[commandPos+1], "?") == 0) || (strcmp(tokens[commandPos+1], "-h") == 0)))
      {
        /* force our timeout response for the command help request to non-0 */
        serverResponseTimeout = PSHELL_SERVER_RESPONSE_TIMEOUT;
      }
      else if (!findCommand(tokens[commandPos]) && _mode == INTERACTIVE)
      {
        printf("here-3\n");
        printf("PSHELL_ERROR: Command: '%s' not found\n", tokens[commandPos]);
        return (true);
      }
      else
      {
        printf("PSHELL_INFO: Command sent fire-and-forget, no response requested\n");
        _pshellSendMsg.header.respNeeded = false;
      }
    }
  }
  else if (serverResponseTimeout == 0)
  {
    /* need to use a non-0 server response timeout for all msgTypes other than PSHELL_USER_COMMAND */
    serverResponseTimeout = PSHELL_SERVER_RESPONSE_TIMEOUT;
  }

  _pshellSendMsg.header.msgType = msgType_;
  _pshellSendMsg.payload[0] = 0;

  if (command_ != NULL)
  {
    strncpy(_pshellSendMsg.payload, command_, PSHELL_RL_MAX_COMMAND_SIZE);
  }

  while (true)
  {
    if (repeat_ > 0)
    {
      iteration++;
      if (rate_ >= 0)
      {
        fprintf(stdout,
                "\033]0;%s: %s, Mode: COMMAND LINE[%s], Rate: %f SEC, Iteration: %d of %d\007",
                _title,
                _serverDisplay,
                command_,
                float(rate_)/float(USEC_PER_SECOND),
                iteration,
                repeat_);
      }
      else
      {
        fprintf(stdout,
                "\033]0;%s: %s, Mode: COMMAND LINE[%s], Iteration: %d of %d\007",
                _title,
                _serverDisplay,
                command_,
                iteration,
                repeat_);
      }
      fflush(stdout);
    }
    if (clear_)
    {
      clearScreen();
    }
    _pshellRcvMsg->header.msgType = msgType_;
    if (send())
    {
      if (!_isBroadcastServer)
      {
        while (_pshellRcvMsg->header.msgType != PSHELL_COMMAND_COMPLETE)
        {
          if (receive(serverResponseTimeout))
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
    }
    else
    {
      return (false);
    }

    if ((repeat_ > 0) && (iteration == repeat_))
    {
      break;
    }
    else if (rate_ >= 0)
    {
      usleep(rate_);
    }
    else if (repeat_ == 0)
    {
      break;
    }
  }

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
bool isDuplicate(char *command_)
{
  unsigned i;
  for (i = 0; i < _numNativeInteractiveCommands; i++)
  {
    if (strcmp(_nativeInteractiveCommands[i].name, command_) == 0)
    {
      printf("PSHELL_WARNING: Server command: '%s', is duplicate of a native interactive client command,\n", command_);
      printf("                server command will be available in command line mode only\n");
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
    _pshellCommandList[_numPshellCommands++] = _nativeInteractiveCommands[i].name;
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
    pshell_rl_addTabCompletion(_pshellCommandList[i]);
    if (strlen(_pshellCommandList[i]) > _maxCommandLength)
    {
      _maxCommandLength = strlen(_pshellCommandList[i]);
    }
  }

}

/******************************************************************************/
/******************************************************************************/
bool initInteractiveMode(void)
{

  /*
   * query our command list, need this to check for duplicates
   * with native interactive commands and build up a list for
   * the TAB completion if that feature is enabled
   */
  if  (!_isBroadcastServer)
  {
    if (!processCommand(PSHELL_QUERY_COMMANDS2, NULL, -1, 0, false, true))
    {
      return (false);
    }
    buildCommandList();
  }

  /* setup our prompt */
  sprintf(_interactivePrompt,
          "%s:%s",
          _serverDisplay,
          _prompt);

  /* setup our title bar */
  fprintf(stdout,
          "\033]0;%s: %s, Mode: INTERACTIVE\007",
          _title,
          _serverDisplay);
  fflush(stdout);

  return (true);

}

/******************************************************************************/
/******************************************************************************/
bool isDec(const char *string_)
{
  for (int i = 0; i < strlen(string_); i++)
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
bool isFile(const char *directory_, const char *file_)
{
  char path[256];
  sprintf(path, "%s/%s", directory_, file_);
  struct stat path_stat;
  stat(path, &path_stat);
  return S_ISREG(path_stat.st_mode);
}

/******************************************************************************/
/******************************************************************************/
void findBatchFiles(const char *directory_)
{
  FILE *fp;
  char file[256];
  char command[256];

  if (directory_ != NULL)
  {
    sprintf(command, "/bin/ls %s", directory_);

    /* Open the command for reading. */
    if ((fp = popen(command, "r")) != NULL)
    {
      /* Read the output a line at a time - output it. */
      while (fgets(file, sizeof(file), fp) != NULL)
      {
        if (file[strlen(file)-1] == '\n')
        {
          file[strlen(file)-1] = '\0';
        }
        /* check for the correct batch file extensions */
        if (((strstr(file, ".psh") != NULL) ||
            ((strstr(file, ".batch") != NULL))) &&
            (_batchFiles.numFiles < MAX_BATCH_FILES))
        {
          strcpy(_batchFiles.files[_batchFiles.numFiles].directory, directory_);
          strcpy(_batchFiles.files[_batchFiles.numFiles].filename, file);
          _batchFiles.maxDirectoryLength = MAX(_batchFiles.maxDirectoryLength, strlen(directory_));
          _batchFiles.maxFilenameLength = MAX(_batchFiles.maxFilenameLength, strlen(file));
          _batchFiles.numFiles++;
        }
      }
      /* close */
      pclose(fp);
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void showBatchFiles(void)
{
  printf("\n");
  printf("***********************************************\n");
  printf("*            AVAILABLE BATCH FILES            *\n");
  printf("***********************************************\n");
  printf("\n");
  printf("%s   %-*s   %-*s\n", "Index", _batchFiles.maxFilenameLength, "Filename", _batchFiles.maxDirectoryLength, "Directory");
  printf("%s   ", "=====");
  for (int i = 0; i < _batchFiles.maxFilenameLength; i++) printf("=");
  printf("   ");
  for (int i = 0; i < _batchFiles.maxDirectoryLength; i++) printf("=");
  printf("\n");
  for (int i = 0; i < _batchFiles.numFiles; i++)
  {
    printf("%-5d   %-*s   %-*s\n",
           i+1,
           _batchFiles.maxFilenameLength,
           _batchFiles.files[i].filename,
           _batchFiles.maxDirectoryLength,
           _batchFiles.files[i].directory);
  }
  printf("\n");
}

/******************************************************************************/
/******************************************************************************/
static bool getBatchFile(const char *filename_, char *batchFile_)
{
  char *batchPath = getenv("PSHELL_BATCH_DIR");

  getcwd(_currentDir, sizeof(_currentDir));

  _batchFiles.numFiles = 0;
  _batchFiles.maxDirectoryLength = 9;
  _batchFiles.maxFilenameLength = 8;

  findBatchFiles(_currentDir);
  findBatchFiles(batchPath);
  findBatchFiles(PSHELL_BATCH_DIR);

  if (isSubString(filename_, "-list", 2))
  {
    showBatchFiles();
    return (false);
  }
  else if (isDec(filename_))
  {
    if (atoi(filename_) > 0 && atoi(filename_) <= _batchFiles.numFiles)
    {
      sprintf(batchFile_, "%s/%s", _batchFiles.files[atoi(filename_)-1].directory, _batchFiles.files[atoi(filename_)-1].filename);
    }
    else
    {
      printf("ERROR: Invalid batch file index: %d, valid values 1-%d\n", atoi(filename_), _batchFiles.numFiles);
      return (false);
    }
  }
  else
  {
    int numMatches = 0;
    for (int i = 0; i < _batchFiles.numFiles; i++)
    {
      if (isSubString(filename_, _batchFiles.files[i].filename, strlen(filename_)))
      {
        sprintf(batchFile_, "%s/%s", _batchFiles.files[i].directory, _batchFiles.files[i].filename);
        numMatches++;
      }
    }
    if (numMatches == 0)
    {
      printf("PSHELL_ERROR: Could not find batch file: '%s', use -list option to see available files\n", filename_);
      return (false);
    }
    else if (numMatches > 1)
    {
      printf("PSHELL_ERROR: Ambiguous file: '%s', use -list option to see available files or <index> to select specific file\n", filename_);
      return (false);
    }
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
void processBatchFile(char *filename_, int rate_, unsigned repeat_, bool clear_, bool showOnly_)
{
  FILE *fp;
  char inputLine[180];
  char batchFile[180];
  unsigned iteration = 0;

  /* dropped through, found the batch file, try to open and process it */
  if (getBatchFile(filename_, batchFile) && (fp = fopen(batchFile, "r")) != NULL)
  {
    while (true)
    {
      if (repeat_ > 0)
      {
        iteration++;
        if (rate_ >= 0)
        {
          fprintf(stdout,
                  "\033]0;%s: %s, Mode: BATCH[%s], Rate: %d SEC, Iteration: %d of %d\007",
                  _title,
                  _serverDisplay,
                  filename_,
                  rate_,
                  iteration,
                  repeat_);
        }
        else
        {
          fprintf(stdout,
                  "\033]0;%s: %s, Mode: BATCH[%s], Iteration: %d of %d\007",
                  _title,
                  _serverDisplay,
                  filename_,
                  iteration,
                  repeat_);
        }
        fflush(stdout);
      }
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
          if (strlen(inputLine) > 0)
          {
            if (showOnly_)
            {
              printf("%s\n", inputLine);
            }
            else
            {
              processCommand(PSHELL_USER_COMMAND, inputLine, -1, 0, false, false);
            }
          }
        }
      }
      if ((repeat_ > 0) && (iteration == repeat_))
      {
        break;
      }
      else if (rate_ >= 0)
      {
        usleep(rate_);
      }
      else if (repeat_ == 0)
      {
        break;
      }
      rewind(fp);
    }
    fclose(fp);
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
    stripWhitespace(str);
    tokens_[(*numTokens_)++] = str;
    while (((str = strtok(NULL, delimeter_)) != NULL) && ((*numTokens_) < maxTokens_))
    {
      stripWhitespace(str);
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
      pshell_rl_getInput(_interactivePrompt, _interactiveCommand);
      strcpy(command, _interactiveCommand);
      tokenize(command, " ", tokens, MAX_TOKENS, &numTokens);
      if ((strstr(_nativeInteractiveCommands[HELP_INDEX].name, tokens[0]) == _nativeInteractiveCommands[HELP_INDEX].name) ||
          (strcmp(tokens[0], "?") == 0))
      {
        if (numTokens == 1)
        {
          if ((findCommand(tokens[0]) == 2) ||
              (strcmp(tokens[0], "?") == 0) ||
              (strstr(_nativeInteractiveCommands[HELP_INDEX].name, tokens[0]) == _nativeInteractiveCommands[HELP_INDEX].name))
          {
            showCommands();
          }
          else
          {
            printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n", tokens[0]);
          }
        }
        else
        {
          printf("Usage: help\n");
        }
      }
      else if (strstr(_nativeInteractiveCommands[QUIT_INDEX].name, tokens[0]) ==
               _nativeInteractiveCommands[QUIT_INDEX].name)
      {
        if (numTokens == 1)
        {
          if (findCommand(tokens[0]) > 2)
          {
            printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n", tokens[0]);
          }
          else
          {
            exitProgram(0);
          }
        }
        else
        {
          printf("Usage: quit\n");
        }
      }
      else if ((strstr(_nativeInteractiveCommands[BATCH_INDEX].name, tokens[0]) ==
                _nativeInteractiveCommands[BATCH_INDEX].name) && !_isBroadcastServer)
      {
        if (numTokens == 2)
        {
          if ((strcmp(tokens[1], "?") == 0) || ((strcmp(tokens[1], "-h") == 0)))
          {
            getcwd(_currentDir, sizeof(_currentDir));
            printf("\n");
            printf("Usage: batch {{<filename> | <index>} [-show]} | -list\n");
            printf("\n");
            printf("  where:\n");
            printf("    filename  - Filename of the batch file to execute\n");
            printf("    index     - Index of the batch file to execute (from the -list option)\n");
            printf("    -list     - List all the available batch files\n");
            printf("    -show     - Show the contents of the batch file without executing\n");
            printf("\n");
            printf("  NOTE: Batch files must have a .psh or .batch extension.  Batch\n");
            printf("        files will be searched in the following directory order:\n");
            printf("\n");
            printf("        current directory - %s\n", _currentDir);
            printf("        $PSHELL_BATCH_DIR - %s\n", getenv("PSHELL_BATCH_DIR"));
            printf("        default directory - %s\n", PSHELL_BATCH_DIR);
            printf("\n");
          }
          else
          {
            processBatchFile(tokens[1], -1, 0, false, false);
          }
        }
        else if ((numTokens == 3) && (isSubString(tokens[2], "-show", 2)))
        {
          processBatchFile(tokens[1], -1, 0, false, true);
        }
        else if (findCommand(tokens[0]) > 2)
        {
          printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n", tokens[0]);
        }
        else
        {
          printf("Usage: batch {{<filename> | <index>} [-show]} | -list\n");
        }
      }
      else if (strstr(_nativeInteractiveCommands[HISTORY_INDEX].name, tokens[0]) ==
               _nativeInteractiveCommands[HISTORY_INDEX].name)
      {
        if (numTokens == 1)
        {
          if (findCommand(tokens[0]) > 2)
          {
            printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n", tokens[0]);
          }
          else
          {
            pshell_rl_showHistory();
          }
        }
        else
        {
          printf("Usage: history\n");
        }
      }
      else
      {
        processCommand(PSHELL_USER_COMMAND, _interactiveCommand, -1, 0, false, false);
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
  FILE *fp;
  char *tokens[MAX_TOKENS];
  unsigned numTokens;
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
      if (line[0] != '#')
      {
        tokenize(line, ":", tokens, MAX_TOKENS, &numTokens);
        if ((numTokens >=2) && (numTokens <= 3))
        {
          /* got at least both args */
         if (_numPshellServers < _pshellServersListSize)
          {
            _pshellServersList[_numPshellServers].serverName = strdup(tokens[0]);
            _pshellServersList[_numPshellServers].portNum = strdup(tokens[1]);
            if (numTokens == 3)
            {
              _pshellServersList[_numPshellServers].timeout = atoi(tokens[2]);
            }
            else
            {
              _pshellServersList[_numPshellServers].timeout = _serverResponseTimeout;
            }
            if (strlen(_pshellServersList[_numPshellServers].serverName) > _maxNamedServerLength)
            {
              _maxNamedServerLength = strlen(_pshellServersList[_numPshellServers].serverName);
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
            _pshellServersList[_numPshellServers].serverName = strdup(tokens[0]);
            _pshellServersList[_numPshellServers].portNum = strdup(tokens[1]);
            if (numTokens == 3)
            {
              _pshellServersList[_numPshellServers].timeout = atoi(tokens[2]);
            }
            else
            {
              _pshellServersList[_numPshellServers].timeout = _serverResponseTimeout;
            }
            if (strlen(_pshellServersList[_numPshellServers].serverName) > _maxNamedServerLength)
            {
              _maxNamedServerLength = strlen(_pshellServersList[_numPshellServers].serverName);
            }
            _numPshellServers++;
          }
        }
      }
    }
    fclose(fp);
  }
}

/******************************************************************************/
/******************************************************************************/
int stringCompare(const void* string1_, const void* string2_)
{
  return strcmp(*(char**)string1_, *(char**)string2_);
}

/******************************************************************************/
/******************************************************************************/
void cleanupFileSystemResources(void)
{
  int unixLockFd;
  char unixLockFile[300];
  char unixSocketFile[300];
  char tempDirEntry[300];
  struct dirent *dirEntry;
  char *serverInfo[MAX_TOKENS];
  unsigned numTokens;
  _dir = opendir(_unixSocketPath);
  if (!_dir)
  {
    sprintf(tempDirEntry, "mkdir %s", _unixSocketPath);
    system(tempDirEntry);
    sprintf(tempDirEntry, "chmod 777 %s", _unixSocketPath);
    system(tempDirEntry);
    _dir = opendir(_unixSocketPath);
  }
  if (_dir)
  {
    while ((dirEntry = readdir(_dir)) != NULL)
    {
      if (strstr(dirEntry->d_name, _lockFileExtension))
      {
        strcpy(tempDirEntry, dirEntry->d_name);
        sprintf(unixLockFile, "%s/%s", PSHELL_UNIX_SOCKET_PATH, dirEntry->d_name);
        /* try to open lock file */
        if ((unixLockFd = open(unixLockFile, O_RDONLY | O_CREAT, 0600)) > -1)
        {
          *strchr(tempDirEntry, '-') = 0;
          *strrchr(dirEntry->d_name, '.') = 0;
          sprintf(unixSocketFile, "%s/%s", PSHELL_UNIX_SOCKET_PATH, tempDirEntry);
          /* file exists, try to see if another process has it locked */
          if (flock(unixLockFd, LOCK_EX | LOCK_NB) == 0)
          {
            /* we got the lock, nobody else has it, ok to clean it up */
            if (strstr(unixLockFile, _unixLockFileId) != NULL)
            {
              unlink(unixSocketFile);
            }
            unlink(unixLockFile);
          }
          else if ((_numActiveServers < MAX_ACTIVE_SERVERS) &&
                   (strstr(dirEntry->d_name, "-control") == NULL))
          {
            tokenize(dirEntry->d_name, "-", serverInfo, MAX_TOKENS, &numTokens);
            _activeServers[_numActiveServers].name = serverInfo[0];
            _activeServers[_numActiveServers].type = serverInfo[1];
            if (strlen(_activeServers[_numActiveServers].name) > _maxActiveServerLength)
            {
              _maxActiveServerLength = strlen(_activeServers[_numActiveServers].name);
            }
            if (numTokens == 2)
            {
              _activeServers[_numActiveServers].host = "N/A";
              _activeServers[_numActiveServers].port = "N/A";
            }
            else if (numTokens == 4)
            {
              _activeServers[_numActiveServers].host = serverInfo[2];
              _activeServers[_numActiveServers].port = serverInfo[3];
              if (strlen(_activeServers[_numActiveServers].host) > _maxHostnameLenth)
              {
                _maxHostnameLenth = strlen(_activeServers[_numActiveServers].host);
              }
            }
            _numActiveServers++;
          }
        }
      }
    }
    closedir(_dir);
  }
  //qsort(_activeServers, _numActiveServers, sizeof(const char*), stringCompare);
}

/******************************************************************************/
/******************************************************************************/
const char *getActiveServer(char *server_)
{
  int index;
  int server;
  int numFound = 0;
  if (isNumeric(server_))
  {
    index = atoi(server_);
    if ((index-1 >= 0) && (index-1 < _numActiveServers))
    {
      if (strcmp(_activeServers[index-1].type, "unix") == 0)
      {
        _host = "unix";
        return (_activeServers[index-1].name);
      }
      else if (strcmp(_activeServers[index-1].type, "udp") == 0)
      {
        if (strcmp(_activeServers[index-1].host, "anyhost") == 0)
        {
          _host = "localhost";
        }
        else if (strcmp(_activeServers[index-1].host, "anybcast") == 0)
        {
          _host = "255.255.255.255";
        }
        else
        {
          _host = _activeServers[index-1].host;
        }
        return (_activeServers[index-1].port);
      }
      else
      {
        printf("\n");
        printf("PSHELL_ERROR: Cannot use 'pshell' client for TCP server, use 'telnet' instead\n");
        showActiveServers();
      }
    }
    else
    {
      printf("\n");
      printf("PSHELL_ERROR: Index: %d out of range for UNIX server, valid range: 1-%d\n", index, _numActiveServers);
      showActiveServers();
      exitProgram(0);
    }
  }
  else
  {
    for (index = 0; index < _numActiveServers; index++)
    {
      if ((strlen(server_) == strlen(_activeServers[index].name)) &&
          (strcmp(_activeServers[index].type, "unix") == 0) &&
          (strcmp(server_, _activeServers[index].name) == 0))
      {
        /* exact match, break out, assumes list does not have duplicate entries */
        server = index;
        numFound = 1;
        break;
      }
      else if ((strncmp(server_, _activeServers[index].name, strlen(server_)) == 0) &&
               (strcmp(_activeServers[index].type, "unix") == 0))
      {
        server = index;
        numFound++;
      }
    }
    if (numFound == 1)
    {
      _host = "unix";
      return (_activeServers[server].name);
    }
  }
  return (NULL);
}

/******************************************************************************/
/******************************************************************************/
void showActiveServers(void)
{
  bool tcpFound = false;
  bool unixFound = false;
  bool udpFound = false;
  printf("\n");
  printf("***************************************************\n");
  printf("*   Active PSHELL Servers Running On Local Host   *\n");
  printf("***************************************************\n");
  printf("\n");
  if (_numActiveServers > 0)
  {
    printf("Index   %-*s   Type   %-*s   Port\n", _maxActiveServerLength, "Server Name", _maxHostnameLenth, "Host");
    printf("=====   ");
    for (unsigned i = 0; i < _maxActiveServerLength; i++)
    {
      printf("=");
    }
    printf("   ====   ");
    for (unsigned i = 0; i < _maxHostnameLenth; i++)
    {
      printf("=");
    }
    printf("   =====\n");
  }
  for (int index = 0; index < _numActiveServers; index++)
  {
    if (strcmp(_activeServers[index].type, "tcp") == 0)
    {
      tcpFound = true;
    }
    if (strcmp(_activeServers[index].type, "udp") == 0)
    {
      udpFound = true;
    }
    if (strcmp(_activeServers[index].type, "unix") == 0)
    {
      unixFound = true;
    }
    printf("%-5d   %-*s   %-4s   %-*s   %-4s\n",
           index+1,
           _maxActiveServerLength,
           _activeServers[index].name,
           _activeServers[index].type,
           _maxHostnameLenth,
           _activeServers[index].host,
           _activeServers[index].port);
  }
  if (_numActiveServers > 0)
  {
    printf("\n");
    if (tcpFound)
    {
      printf("Connect to TCP server with: telnet <host> <port>\n");
    }
    if (udpFound)
    {
      printf("Connect to UDP server with: pshell {<host> <port>} | <index>\n");
    }
    if (unixFound)
    {
      printf("Connect to UNIX server with: pshell <name> | <index>\n");
    }
    printf("\n");
  }
  exitProgram(0);
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
  printf("******************************************\n");
  printf("*     Available Named PSHELL Servers     *\n");
  printf("******************************************\n");
  printf("\n");

  strlen(banner) > _maxNamedServerLength ? fieldWidth = strlen(banner) : fieldWidth = _maxNamedServerLength;
  printf("%s", banner);

  for (i = strlen(banner); i < fieldWidth; i++)
  {
    printf(" ");
  }
  printf("  Port Number  Response Timeout\n");
  for (i = 0; i < fieldWidth; i++)
  {
    printf("=");
  }
  printf("  ===========  ================\n");
  for (server = 0; server < _numPshellServers; server++)
  {
    printf("%s", _pshellServersList[server].serverName);
    for (i = strlen(_pshellServersList[server].serverName); i < fieldWidth; i++)
    {
      printf(" ");
    }
    printf("  %-11s  %d seconds\n", _pshellServersList[server].portNum, _pshellServersList[server].timeout);
  }
  printf("\n");
  exitProgram(0);
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
      printf("%s", _nativeInteractiveCommands[i].name);
      for (pad = strlen(_nativeInteractiveCommands[i].name); pad < _maxCommandLength; pad++) printf(" ");
      printf("  -  %s\n", _nativeInteractiveCommands[i].description);
    }
  }
  if (!_isBroadcastServer)
  {
    processCommand(PSHELL_QUERY_COMMANDS1, NULL, -1, 0, false, false);
  }
  else
  {
    printf("\n");
    printf("NOTE: Connected to a broadcast address, all remote server\n");
    printf("      commands are 'invisible' to this client application\n");
    printf("      and are single-shot, 'fire-and-forget', with no response\n");
    printf("      requested or expected, and no results displayed\n");
    printf("\n");
  }
}

/******************************************************************************/
/******************************************************************************/
void showUsage(void)
{
  printf("\n");
  printf("Usage: pshell -s | -n | {{{<hostName | ipAddr>} {<portNum> | <udpServerName>}} | <unixServerName> | <serverIndex>} [-t<timeout>]\n");
  printf("                        [{{-c <command> | -f <filename>} [rate=<seconds>] [repeat=<count>] [clear]}]\n");
  printf("\n");
  printf("  where:\n");
  printf("    -s              - show all servers running on the local host\n");
  printf("    -n              - show named IP server/port mappings in pshell-client.conf file\n");
  printf("    -c              - run command from command line\n");
  printf("    -f              - run commands from a batch file\n");
  printf("    -t              - change the default server response timeout\n");
  printf("    hostName        - hostname of UDP server\n");
  printf("    ipAddr          - IP addr of UDP server\n");
  printf("    portNum         - port number of UDP server\n");
  printf("    udpServerName   - name of UDP server from pshell-client.conf file\n");
  printf("    unixServerName  - name of UNIX server (use '-s' option to list servers)\n");
  printf("    serverIndex     - index of local UNIX or UDP server (use '-s' option to list servers)\n");
  printf("    timeout         - response wait timeout in sec (default=5)\n");
  printf("    command         - optional command to execute (in double quotes, ex. -c \"myCommand arg1 arg2\")\n");
  printf("    fileName        - optional batch file to execute\n");
  printf("    rate            - optional rate to repeat command or batch file (in seconds)\n");
  printf("    repeat          - optional repeat count for command or batch file (default=forever)\n");
  printf("    clear           - optional clear screen between commands or batch file passes\n");
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
  printf("          command, type '<command> {? | -h}'.  Use TAB completion\n");
  printf("          to fill out partial commands and up-arrow to recall\n");
  printf("          for command history.\n");
  printf("\n");
  exitProgram(0);
}

/******************************************************************************/
/******************************************************************************/
void parseCommandLine(int *argc, char *argv[])
{
  int i;
  getNamedServers();
  cleanupFileSystemResources();
  for (i = 0; i < (*argc-1); i++)
  {
    argv[i] = argv[i+1];
  }
  (*argc)--;
  if (*argc == 0)
  {
    showUsage();
  }
  else if (*argc == 1)
  {
    if ((strcmp(argv[0], "-h") == 0) || (strcmp(argv[0], "?") == 0))
    {
      showUsage();
    }
    else if (strcmp(argv[0], "-n") == 0)
    {
      showNamedServers();
    }
    else if (strcmp(argv[0], "-s") == 0)
    {
      showActiveServers();
    }
    else if ((_server = getActiveServer(argv[0])) != NULL)
    {
      *argc = 0;
    }
  }
  else if (*argc < 8)
  {
    if ((_server = getActiveServer(argv[0])) == NULL)
    {
      if (isNumeric(argv[1]))
      {
        _host = argv[0];
        _server = argv[1];
        for (i = 0; i < (*argc-1); i++)
        {
          argv[i] = argv[i+2];
        }
        (*argc) -= 2;
      }
      else if ((_server = findServerPort(argv[1])) != NULL)
      {
        _host = argv[0];
        for (i = 0; i < (*argc-1); i++)
        {
          argv[i] = argv[i+2];
        }
        (*argc) -= 2;
      }
      else
      {
        _host = "unix";
        _server = argv[0];
        for (i = 0; i < (*argc-1); i++)
        {
          argv[i] = argv[i+1];
        }
        (*argc)--;
      }
    }
    else
    {
      for (i = 0; i < (*argc-1); i++)
      {
        argv[i] = argv[i+1];
      }
      (*argc)--;
    }
    if (strstr(argv[0], "-t") == argv[0])
    {
      if ((strlen(argv[0]) > 2) && (isNumeric(&argv[0][2])))
      {
        _serverResponseTimeout = atoi(&argv[0][2]);
        _serverResponseTimeoutOverride = true;
      }
      else
      {
        printf("PSHELL_ERROR: Must provide value for timeout, e.g. -t20\n");
      }
      for (i = 0; i < (*argc-1); i++)
      {
        argv[i] = argv[i+1];
      }
      (*argc)--;
    }
  }
  else
  {
    showUsage();
  }
}

/******************************************************************************/
/******************************************************************************/
void exitProgram(int exitCode_)
{
  char unixLockFile[180];
  if (_isUnixConnected)
  {
    sprintf(unixLockFile, "%s%s", _sourceUnixAddress.sun_path, _lockFileExtension);
    unlink(_sourceUnixAddress.sun_path);
    unlink(unixLockFile);
  }
  cleanupFileSystemResources();
  if (exitCode_ > 0)
  {
    printf("\n");
  }
  exit(exitCode_);
}

/******************************************************************************/
/******************************************************************************/
void registerSignalHandlers(void)
{
  signal(SIGHUP, exitProgram);    /* 1  Hangup (POSIX).  */
  signal(SIGINT, exitProgram);    /* 2  Interrupt (ANSI).  */
  signal(SIGQUIT, exitProgram);   /* 3  Quit (POSIX).  */
  signal(SIGILL, exitProgram);    /* 4  Illegal instruction (ANSI).  */
  signal(SIGABRT, exitProgram);   /* 6  Abort (ANSI).  */
  signal(SIGBUS, exitProgram);    /* 7  BUS error (4.2 BSD).  */
  signal(SIGFPE, exitProgram);    /* 8  Floating-point exception (ANSI).  */
  signal(SIGSEGV, exitProgram);   /* 11 Segmentation violation (ANSI).  */
  signal(SIGPIPE, exitProgram);   /* 13 Broken pipe (POSIX).  */
  signal(SIGALRM, exitProgram);   /* 14 Alarm clock (POSIX).  */
  signal(SIGTERM, exitProgram);   /* 15 Termination (ANSI).  */
#ifdef LINUX
  signal(SIGSTKFLT, exitProgram); /* 16 Stack fault.  */
#endif
  signal(SIGXCPU, exitProgram);   /* 24 CPU limit exceeded (4.2 BSD).  */
  signal(SIGXFSZ, exitProgram);   /* 25 File size limit exceeded (4.2 BSD).  */
  signal(SIGSYS, exitProgram);    /* 31 Bad system call.  */
}

/******************************************************************************/
/******************************************************************************/
int main(int argc, char *argv[])
{

  float rate = -1;
  unsigned repeat = 0;
  bool needFile = false;
  bool needCommand = false;
  bool clear = false;
  char *command = NULL;
  char *filename = NULL;
  char *rateOrRepeat[MAX_TOKENS];
  unsigned numTokens;

  /* register signal handlers so we can do a graceful termination and cleanup any system resources */
  registerSignalHandlers();

  _mode = COMMAND_LINE;

  strcpy(_title, "PSHELL");
  strcpy(_banner, "PSHELL: Process Specific Embedded Command Line Shell");
  strcpy(_prompt, "PSHELL> ");

  parseCommandLine(&argc, argv);

  if (argc == 0)
  {
    _mode = INTERACTIVE;
  }
  else if (argc <=5)
  {
    /* either command line or batch mode */
    if ((argc == 2) && ((strcmp(argv[1], "-h") == 0) ||
                        (strcmp(argv[1], "help") == 0) ||
                        (strcmp(argv[1], "-help") == 0) ||
                        (strcmp(argv[1], "--help") == 0) ||
                        (strcmp(argv[1], "?") == 0)))
    {
      /* they asked for the command list only, display it and exit */
      if (init(_host, _server))
      {
        showCommands();
      }
      exitProgram(0);
    }
    else
    {
      /* parse the command/batch mode options */
      for (int i = 0; i < argc; i++)
      {
        if (strstr(argv[i], "=") != NULL)
        {
          /* parse our options */
          tokenize(argv[i], "=", rateOrRepeat, MAX_TOKENS, &numTokens);
          if ((numTokens == 2) && (strcmp(rateOrRepeat[0], "rate") == 0))
          {
            rate = atof(rateOrRepeat[1])*USEC_PER_SECOND;
          }
          else if ((numTokens == 2) && (strcmp(rateOrRepeat[0], "repeat") == 0))
          {
            repeat = atoi(rateOrRepeat[1]);
          }
          else
          {
            showUsage();
          }
        }
        else if (strcmp(argv[i], "clear") == 0)
        {
          clear = true;
        }
        else if (strcmp(argv[i], "-c") == 0)
        {
          _mode = COMMAND_LINE;
          needCommand = true;
        }
        else if (needCommand == true)
        {
          command = argv[i];
          needCommand = false;
        }
        else if (strcmp(argv[i], "-f") == 0)
        {
          _mode = BATCH;
          needFile = true;
        }
        else if (needFile == true)
        {
          filename = argv[i];
          needFile = false;
        }
        else
        {
          showUsage();
        }
      }
    }
  }
  else
  {
    showUsage();
  }

  /* see if they speficied a -f and forgot to supply a filename */
  if (needFile || needCommand)
  {
    showUsage();
  }

  /* command line processed, now execute results */
  if (init(_host, _server))
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
      if ((rate >= 0) && (repeat == 0))
      {
        fprintf(stdout,
                "\033]0;%s: %s, Mode: COMMAND LINE[%s], Rate: %f SEC\007",
                _title,
                _serverDisplay,
                command,
                float(rate)/float(USEC_PER_SECOND));
        fflush(stdout);
      }
      processCommand(PSHELL_USER_COMMAND, command, rate, repeat, clear, false);
    }
    else  /* _mode == BATCH */
    {
      /*
      * setup our title bar, no real need to do this if we are not
      * repeating the command, since it will be too quick to see
      * anyway
      */
      if ((rate >= 0) && (repeat == 0))
      {
        fprintf(stdout,
                "\033]0;%s: %s, Mode: BATCH[%s], Rate: %f SEC\007",
                _title,
                _serverDisplay,
                filename,
                float(rate)/float(USEC_PER_SECOND));
        fflush(stdout);
      }
      processBatchFile(filename, rate, repeat, clear, false);
    }
  }
  exitProgram(0);
}
