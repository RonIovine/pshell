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

#include <errno.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cstdarg>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>

#include <PshellCommon.h>
#include <PshellReadline.h>
#include <PshellServer.h>

/* constants */

/*
 * change the default window title bar, welcome banner, and prompt and
 * TCP server timeout to suit, your product/application, note that these
 * can also be changed on a per-server basis at startup time via the
 * pshell-server.conf file or for all servers via the .config makefile
 * settings (requires a re-build from source)
 */

/* the full window title format is 'title: shellName[ipAddress]' */
#ifdef PSHELL_DEFAULT_TITLE
static const char *_defaultTitle  = STR(PSHELL_DEFAULT_TITLE);
#else
static const char *_defaultTitle  = "PSHELL";
#endif

/* this is the first line of the welcome banner */
#ifdef PSHELL_DEFAULT_BANNER
static const char *_defaultBanner = STR(PSHELL_DEFAULT_BANNER);
#else
static const char *_defaultBanner = "PSHELL: Process Specific Embedded Command Line Shell";
#endif

/* the full prompt format is 'shellName[ipAddress]:prompt' */
#ifdef PSHELL_DEFAULT_PROMPT
static const char *_defaultPrompt = STR(PSHELL_DEFAULT_PROMPT);
#else
static const char *_defaultPrompt = "PSHELL> ";
#endif

/* the default idle session timeout for TCP sessions in minutes */
#ifdef PSHELL_DEFAULT_TIMEOUT
static int _defaultIdleTimeout = PSHELL_DEFAULT_TIMEOUT;
#else
static int _defaultIdleTimeout = 10;
#endif

/*
 * the default config dir is where the server will look for the pshell-server.conf
 * file if it does not find it in the directory specified by the env variable
 * PSHELL_CONFIG_DIR, or the CWD, change this to any other desired location,
 * the config file can contain settings that override the defined default
 * configurations for the title, banner, prompt, address, port, type and mode
 * values, this is also the directory the TraceFilter 'init' function looks in
 * for its startup config file in the event that the environment variable of
 * the same name is not found
 */
#ifdef CONFIG_PSHELL_CONFIG_DIR
#define PSHELL_CONFIG_DIR STR(CONFIG_PSHELL_CONFIG_DIR)
#else
#define PSHELL_CONFIG_DIR "/etc/pshell/config"
#endif

/*
 * the default batch dir is where the server will look for the specified filename
 * as given in the interacive "batch" command, if it does not find it in the directory
 * specified by the env variable PSHELL_BATCH_DIR, or the CWD, change this to any other
 * desired default location, the batch file can contain any commands that can be entered
 * to the server via the interacive command line
 */
#ifdef CONFIG_PSHELL_BATCH_DIR
#define PSHELL_BATCH_DIR STR(CONFIG_PSHELL_BATCH_DIR)
#else
#define PSHELL_BATCH_DIR "/etc/pshell/batch"
#endif

/*
 * the default startup dir is where the server will look for the <serverName>.startup
 * file if it does not find it in the directory specified by the env variable
 * PSHELL_STARTUP_DIR, or the CWD, change this to any other desired default location,
 * the startup file can contain any commands that can be entered to the server via
 * the interacive command line, the startup file is executed during the startup
 * of the server, so it can serve as a program initialization file
 */
#ifdef CONFIG_PSHELL_STARTUP_DIR
#define PSHELL_STARTUP_DIR STR(CONFIG_PSHELL_STARTUP_DIR)
#else
#define PSHELL_STARTUP_DIR "/etc/pshell/startup"
#endif

/*
 * This is the size of the transfer buffer used to communicate between the
 * pshell server and client.  This is the initial buffer size as well as the
 * chunk size the buffer will grow by in the event of a buffer overflow when
 * the server compile flag PSHELL_GROW_PAYLOAD_IN_CHUNKS is set.
 *
 * If that compile flag is NOT set, the behavour of the transfer buffer upon
 * an overflow is dependent on the setting of the PSHELL_VSNPRINTF compile flag as
 * described below:
 *
 * PSHELL_VSNPRINTF is set:
 *
 * The buffer will grow in the EXACT amount needed in the event of an overflow
 * (note, this is only for glibc 2.1 and higher, for glibc 2.0, we grow by the
 * chunk size because in the 2.0 'C' library the "vsnprintf" function does not
 * support returning the overflow size).
 *
 * PSHELL_VSNPRINTF is NOT set:
 *
 * The buffer will be flushed to the client upon overflow.  Note that care must
 * be taken when setting the PSHELL_PAYLOAD_GUARDBAND value when operating in this
 * mode.
 */

#ifndef PSHELL_PAYLOAD_CHUNK
#define PSHELL_PAYLOAD_CHUNK 1024*64   /* 64k UDP/Unix max datagram size in bytes */
#endif

/*
 * if we are not using "vsnprintf" to format and populate the transfer buffer,
 * we must specify a guardband to detect a potential buffer overflow condition,
 * this value must be smaller than the above payload chunk size, but must be
 * larger than the maximum expected data size for and given SINGLE "pshell_printf"
 * call, otherwise we risk an undetected buffer overflow condition and subsequent
 * memory corruption
 */

#ifndef PSHELL_VSNPRINTF
#ifndef PSHELL_PAYLOAD_GUARDBAND
#define PSHELL_PAYLOAD_GUARDBAND 4096  /* bytes */
#endif
#endif

/*
 * this is the amount we will grow our Tokens list by in the event
 * we need more tokens, a new Tokens list is potentially created by
 * each pshell_tokenize call as necessary, Tokens lists are reused
 * for each callback function, this is how command line arguments are
 * parsed
 */

#ifndef PSHELL_TOKEN_LIST_CHUNK
#define PSHELL_TOKEN_LIST_CHUNK 10
#endif

/*
 * this is the amount we will grow our list of tokens by in the
 * event we need more tokens for a given pshell_tokenize call, token
 * entries are reused for each callback function
 */

#ifndef PSHELL_TOKEN_CHUNK
#define PSHELL_TOKEN_CHUNK 10
#endif

/*
 * this is the chunk size (and the initial table size)
 * we use to allocate memory for the PshellCmd table
 */

#ifndef PSHELL_COMMAND_CHUNK
#define PSHELL_COMMAND_CHUNK 50
#endif

/*
 * the following enum values are returned in the msgType field of the
 * PshellMsg by processCommand function when the command is an external
 * control type command, these must match their corresponding values in
 * PshellControl.h
 */
enum PshellControlResponse
{
  PSHELL_COMMAND_SUCCESS,
  PSHELL_COMMAND_NOT_FOUND,
  PSHELL_COMMAND_INVALID_ARG_COUNT
};

/* structure entry for an individual pshell command */

struct PshellCmd
{
  const char *command;
  const char *usage;
  const char *description;
  PshellFunction function;
  unsigned char minArgs;
  unsigned char maxArgs;
  unsigned char showUsage;
  unsigned length;  /* strlen of the command, used for substring matching */
};

/*
 * structure used to tokenize command line arguments, this is
 * setup as a pseudo "class" in which only the "public" data
 * member (PshellTokens) is returned to the caller, while the "private"
 * data members (maxTokens, string) are just kept for internal
 * token management
 */

struct Tokens
{
  PshellTokens _public;  /* returned to the caller of "pshell_tokenize" */
  unsigned maxTokens;    /* used for internal token management */
  char *string;          /* used for internal token management */
};

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

struct FileList
{
  unsigned numFiles;
  unsigned maxDirectoryLength;
  unsigned maxFilenameLength;
  File files[MAX_BATCH_FILES];
};

/*************************
 * private "member" data
 *************************/

/*
 * coding convention is leading underscore for "member" data,
 * trailing underscore for function arguments, and no leading
 * or trailing underscores for local stack variables
 */

/* UDP/UNIX server data */

static struct sockaddr_in _fromIpAddress;
static struct sockaddr_un _fromUnixAddress;
static const char *_wheel = "|/-\\";
static unsigned _wheelPos = 0;
static bool _isControlCommand = false;
static int _connectFd;
static bool _quit = false;
static char _interactivePrompt[PSHELL_RL_MAX_COMMAND_SIZE];

/* common data (for UDP, TCP, UNIX, and LOCAL servers */

#ifdef PSHELL_INCLUDE_COMMAND_IN_ARGS_LIST
#define PSHELL_BASE_ARG_OFFSET 0
#else
#define PSHELL_BASE_ARG_OFFSET 1
#endif

char *pshell_origCommandKeyword;
bool pshell_allowDuplicateFunction = false;
#ifdef PSHELL_COPY_ADD_COMMAND_STRINGS
bool pshell_copyAddCommandStrings = true;
#else
bool pshell_copyAddCommandStrings = false;
#endif

static bool _isRunning = false;
static bool _isCommandDispatched = false;
static bool _isCommandInteractive = true;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;

static PshellLogFunction _logFunction = NULL;
static unsigned _logLevel = PSHELL_SERVER_LOG_LEVEL_DEFAULT;

static int _socketFd;
static struct sockaddr_in _localIpAddress;
static struct sockaddr_un _localUnixAddress;
static char _lockFile[300];
static const char *_fileSystemPath = "/tmp/.pshell/";
static const char *_lockFileExtension = ".lock";
static const char *_unixLockFileId = "unix.lock";

static PshellServerType _serverType = PSHELL_LOCAL_SERVER;
static PshellServerMode _serverMode = PSHELL_BLOCKING;
static char _serverName[180];
static char _hostnameOrIpAddr[180];
static char _ipAddress[180];
static char _title[180];
static char _banner[180];
static char _prompt[180];
static unsigned _port;

static PshellMsg *_pshellMsg;
static unsigned _pshellPayloadSize = PSHELL_PAYLOAD_CHUNK;

static PshellCmd *_commandTable = NULL;
static PshellCmd *_foundCommand;
static unsigned _commandTableSize = 0;
static unsigned _numCommands = 0;
static unsigned _maxCommandLength = strlen("history");

static PshellTokens _dummyTokens;
static Tokens *_tokenList = NULL;
static unsigned _maxTokenLists = 0;
static unsigned _numTokenLists = 0;
static int _argc;
static char **_argv;
static PshellCmd _helpCmd;
static PshellCmd _quitCmd;
static PshellCmd _historyCmd;
static PshellCmd _batchCmd;
static PshellCmd _setupCmd;

static FileList _batchFiles;
static char _currentDir[PATH_MAX];

/* to time elapsed time for pshell_clock keep-alive function */
//static time_t _startTime;
//static time_t _currTime;
static bool _showElapsedTime;
static struct timeval _startTime;
static struct timeval _currTime;
static struct timeval _elapsedTime;

/****************************************
 * private "member" function prototypes
 ****************************************/

/* LOCAL server functions */

static void runLocalServer(void);
static void clearScreen(void);

/* UDP and UNIX server functions */

static void runUDPServer(void);
static void receiveUDP(void);
static void replyUDP(PshellMsg *pshellMsg_);
static void runUNIXServer(void);
static void receiveUNIX(void);
static void replyUNIX(PshellMsg *pshellMsg_);
static void processQueryVersion(void);
static void processQueryPayloadSize(void);
static void processQueryName(void);
static void processQueryTitle(void);
static void processQueryBanner(void);
static void processQueryPrompt(void);
static void processQueryCommands1(void);
static void processQueryCommands2(void);
static void reply(PshellMsg *pshellMsg_);

/* TCP server functions */

static bool acceptConnection(void);
static void runTCPServer(void);
static void receiveTCP(void);

/* common functions (TCP and LOCAL servers) */

static void showWelcome(void);
static void processBatchFile(char *filename_, unsigned rate_, unsigned repeat_, bool clear_);
static void findBatchFiles(const char *directory_);
static void showBatchFiles(void);

/* common functions (UDP and TCP servers) */

static int getIpAddress(const char *interface_, char *ipAddress_);
static bool createSocket(void);
static bool bindSocket(void);
static void cleanupFileSystemResources(void);

/* common functions (UDP, TCP, UNIX, and LOCAL servers) */

static void addNativeCommands(void);
static void loadConfigFile(void);
static bool loadCommandFile(const char *filename_, bool interactive_, bool showOnly_);
static void loadStartupFile(void);
static void loadBatchFile(const char *filename_, bool showOnly_);
static bool getBatchFile(const char *filename_, char *batchFile_);
static bool checkForWhitespace(const char *string_);
static char *createArgs(char *command_);
static unsigned findCommand(char *command_);
static void processCommand(char *command_);
static bool allocateTokens(void);
static void cleanupTokens(void);
static void *serverThread(void*);
static void runServer(void);
static void dispatchCommand(char *command_);
static void getElapsedTime(void);

/* command that is run in no-server mode to setup busybox like
 * softlinks to all of the commands
 */
static void setup(int argc, char *argv[]);

/* native callback commands (TCP and LOCAL server only) */

static void help(int argc, char *argv[]);
static void quit(int argc, char *argv[]);
static void history(int argc, char *argv[]);
static void batch(int argc, char *argv[]);

#define PSHELL_NO_SERVER (PshellServerType)255
#define PSHELL_MAX_BIND_ATTEMPTS 1000

/* output display macros */
static void _printf(const char *format_, ...);
#define PSHELL_ERROR(format_, ...) if (_logLevel >= PSHELL_SERVER_LOG_LEVEL_ERROR) {_printf("PSHELL_ERROR: " format_, ##__VA_ARGS__);_printf("\n");}
#define PSHELL_WARNING(format_, ...) if (_logLevel >= PSHELL_SERVER_LOG_LEVEL_WARNING) {_printf("PSHELL_WARNING: " format_, ##__VA_ARGS__);_printf("\n");}
#define PSHELL_INFO(format_, ...) if (_logLevel >= PSHELL_SERVER_LOG_LEVEL_INFO) {_printf("PSHELL_INFO: " format_, ##__VA_ARGS__);_printf("\n");}

/*
 * we access this funciton via a 'backdoor' linking via the PshellReadline
 * library, since this function is not of the 'official' published API as
 * specified in the PshellReadline.h file
 */
extern void pshell_rl_showHistory(void);

/**************************************
 * public API "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void pshell_setServerLogLevel(unsigned level_)
{
  _logLevel = level_;
}

/******************************************************************************/
/******************************************************************************/
void pshell_registerServerLogFunction(PshellLogFunction logFunction_)
{
  if (logFunction_ != NULL)
  {
    _logFunction = logFunction_;
  }
  else
  {
    PSHELL_WARNING("NULL logFunction, not registered");
  }
}

/******************************************************************************/
/******************************************************************************/
void pshell_addCommand(PshellFunction function_,
                       const char *command_,
                       const char *description_,
                       const char *usage_,
                       unsigned char minArgs_,
                       unsigned char maxArgs_,
                       bool showUsage_)
{

  unsigned entry;
  void *ptr;

  pthread_mutex_lock(&_mutex);

  /* perform some command validation */

  /* see if we have a NULL command name */
  if ((command_ == NULL) || (strlen(command_) == 0))
  {
    PSHELL_ERROR("NULL command name, command not added");
    pthread_mutex_unlock(&_mutex);
    return;
  }

  /* see if we have a NULL description */
  if ((description_ == NULL) || (strlen(description_) == 0))
  {
    PSHELL_ERROR("NULL description, command: '%s' not added", command_);
    pthread_mutex_unlock(&_mutex);
    return;
  }

  /* see if we have a NULL function */
  if (function_ == NULL)
  {
    PSHELL_ERROR("NULL function, command: '%s' not added", command_);
    pthread_mutex_unlock(&_mutex);
    return;
  }

  /* see if they provided a usage for a function with no arguments */
  if ((minArgs_ == 0) && (maxArgs_ == 0) && (usage_ != NULL))
  {
    PSHELL_ERROR("Usage provided for function that takes no arguments, command: '%s' not added",
                 command_);
    pthread_mutex_unlock(&_mutex);
    return;
  }

  /* see if they provided no usage for a function with arguments */
  if (((maxArgs_ > 0) || (minArgs_ > 0)) && ((usage_ == NULL) || (strlen(usage_) == 0)))
  {
    PSHELL_ERROR("NULL usage for command that takes arguments, command: '%s' not added",
                 command_);
    pthread_mutex_unlock(&_mutex);
    return;
  }

  /*
   * see if their minArgs is greater than their maxArgs, we ignore if maxArgs is 0
   * because that is the default value and we will set maxArgs to minArgs if that
   * case later on in this function
   */
  if ((minArgs_ > maxArgs_) && (maxArgs_ > 0))
  {
    PSHELL_ERROR("minArgs: %d is greater than maxArgs: %d, command: '%s' not added",
                 minArgs_,
                 maxArgs_,
                 command_);
    pthread_mutex_unlock(&_mutex);
    return;
  }

  /* see if we have a whitespace in the command name */
  if (checkForWhitespace(command_))
  {
    PSHELL_ERROR("Whitespace found, command: '%s' not added", command_);
    pthread_mutex_unlock(&_mutex);
    return;
  }

  /*
   * see if the command name is a duplicate of one of the
   * native UDP interactive commands, only give a warning because
   * we can still use the command in command line mode
   */
  if (((_serverType == PSHELL_UDP_SERVER) || (_serverType == PSHELL_UNIX_SERVER)) &&
      (pshell_isEqual(command_, "help") ||
       pshell_isEqual(command_, "quit")))
  {
    PSHELL_WARNING("Command: '%s' is duplicate of a native interactive UDP/UNIX client command", command_);
    PSHELL_WARNING("Command: '%s'  will be available in command line mode only", command_);
  }

  /* look for duplicate command names and function pointers */
  for (entry = 0; entry < _numCommands; entry++)
  {

    /* check for duplicate command names */
    if (pshell_isEqual(_commandTable[entry].command, command_))
    {
      PSHELL_ERROR("Duplicate command found, command: '%s' not added", command_);
      pthread_mutex_unlock(&_mutex);
      return;
    }

    /* check for duplicate function pointers */
    if ((_commandTable[entry].function == function_) && (!pshell_allowDuplicateFunction))
    {
      PSHELL_ERROR("Duplicate function found, command: '%s' not added", command_);
      pthread_mutex_unlock(&_mutex);
      return;
    }

  }

  /*
   * if we've fallen through to here, we must have
   * validated ok, perpare to add the new command
   */

  /*
   * see if we need to grab a new chunk of memory
   * for our command table
   */
  if (_numCommands >= _commandTableSize)
  {
    /* grab a chunk for our command table */
    if ((ptr = realloc(_commandTable, (_commandTableSize+PSHELL_COMMAND_CHUNK)*sizeof(PshellCmd))) == NULL)
    {
      PSHELL_ERROR("Could not allocate memory to expand command table, size: %d, command: '%s' not added",
                   (int)((_commandTableSize+PSHELL_COMMAND_CHUNK)*sizeof(PshellCmd)),
                   command_);
      pthread_mutex_unlock(&_mutex);
      return;
    }
    /* success, update our pointer and allocated size */
    _commandTableSize += PSHELL_COMMAND_CHUNK;
    _commandTable = (PshellCmd*)ptr;
  }

  if (pshell_copyAddCommandStrings)
  {

    /*
     * make a duplicate of the command, usage and description strings,
     * this compile time flag should be set if there is a chance of
     * any of these strings used in the addCommand call going out of
     * scope
     */

    /* now populate the table with the new entry */
    if ((_commandTable[_numCommands].command = strdup(command_)) == NULL)
    {
      PSHELL_ERROR("Could not allocate memory for command name, size: %d, command: '%s' not added",
                   (pshell_getLength(command_)),
                   command_);
      pthread_mutex_unlock(&_mutex);
      return;
    }

    /* strdup will segment fault on a NULL pointer, so catch that */
    if (usage_ != NULL)
    {
      if ((_commandTable[_numCommands].usage = strdup(usage_)) == NULL)
      {
        PSHELL_ERROR("Could not allocate memory for command usage, size: %d, command: '%s' not added",
                     (pshell_getLength(usage_)),
                     command_);
        free((void *)_commandTable[_numCommands].command);
        pthread_mutex_unlock(&_mutex);
        return;
      }
    }
    else
    {
      _commandTable[_numCommands].usage = usage_;
    }

    if ((_commandTable[_numCommands].description = strdup(description_)) == NULL)
    {
      PSHELL_ERROR("Could not allocate memory for command description, size: %d, command: '%s' not added",
                   (pshell_getLength(description_)),
                   command_);
      free((void *)_commandTable[_numCommands].command);
      free((void *)_commandTable[_numCommands].usage);
      pthread_mutex_unlock(&_mutex);
      return;
    }
  }
  else
  {

    /* just set the pointers but don't clone the strings, this will use less memory */
    _commandTable[_numCommands].command = command_;
    _commandTable[_numCommands].description = description_;
    _commandTable[_numCommands].usage = usage_;

  }

  _commandTable[_numCommands].function = function_;
  _commandTable[_numCommands].minArgs = minArgs_;
  /* see if they gave the default for maxArgs, if so, set maxArgs to minArgs */
  if (maxArgs_ > 0)
  {
    _commandTable[_numCommands].maxArgs = maxArgs_;
  }
  else
  {
    _commandTable[_numCommands].maxArgs = minArgs_;
  }
  _commandTable[_numCommands].showUsage = showUsage_;
  _commandTable[_numCommands].length = strlen(command_);
  _numCommands++;

  /* see if they are adding commands after the server is started, if so, add it here,
   * otherwise, they will all be added to the TAB completion in the addNativeCommands function
   */
  if (_isRunning)
  {
    pshell_rl_addTabCompletion(command_);
  }

  /* figure out our max command length so the help display can be aligned */
  if (pshell_getLength(command_) > _maxCommandLength)
  {
    _maxCommandLength = pshell_getLength(command_);
  }

  pthread_mutex_unlock(&_mutex);

}

/******************************************************************************/
/******************************************************************************/
void pshell_runCommand(const char *command_, ...)
{
  char *commandName;
  char fullCommand[256];
  char command[256];
  /*
   * only dispatch command if we are not already in the middle of
   * dispatching an interactive command
   */
  if (!_isCommandDispatched)
  {
    /* set this to false so we can short circuit any calls to pshell_printf since
     * there is no client side program to receive the output */
    _isCommandInteractive = false;
    /* set this to true so we can tokenize our command line with pshell_tokenize */
    _isCommandDispatched = true;
    /* create the command from our varargs */
    va_list args;
    va_start(args, command_);
    vsprintf(command, command_, args);
    va_end(args);
    strcpy(fullCommand, command);
    /* user command, create the arg list and look for the command */
    if (((commandName = createArgs(fullCommand)) != NULL) &&
         (findCommand(commandName) == 1) &&
         (_argc >= _foundCommand->minArgs) &&
         (_argc <= _foundCommand->maxArgs))
    {
      /*
       * dispatch user command, don't need to check for a NULL
       * function pointer because the validation in the addCommand
       * function will catch that and not add the command
       */
      _foundCommand->function(_argc, _argv);
    }
    /*
     * set this to false to be sure nobody calls pshell_tokenize outside the context
     * of an pshell callback function because the cleanupTokens must be called to
     * avoid any memory leaks
     */
    _isCommandDispatched = false;
    cleanupTokens();

    /* set back to interactive mode */
    _isCommandInteractive = true;
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isHelp(void)
{
  return ((_argc == 1) &&
          (pshell_isEqual(_argv[0], "?") ||
           pshell_isEqual(_argv[0], "-h") ||
           pshell_isEqual(_argv[0], "--h") ||
           pshell_isEqual(_argv[0], "-help") ||
           pshell_isEqual(_argv[0], "--help")));
}

/******************************************************************************/
/******************************************************************************/
void pshell_showUsage(void)
{
  (_foundCommand->usage != NULL) ?
    pshell_printf("Usage: %s %s\n", _foundCommand->command, _foundCommand->usage) :
    pshell_printf("Usage: %s\n", _foundCommand->command);
}

/******************************************************************************/
/******************************************************************************/
void pshell_wheel(const char *string_)
{
  (string_ != NULL) ?
    pshell_printf("\r%s%c", string_, _wheel[(_wheelPos++)%4]) :
    pshell_printf("\r%c", _wheel[(_wheelPos++)%4]);
  pshell_flush();
}

/******************************************************************************/
/******************************************************************************/
void pshell_clock(const char *string_)
{
  getElapsedTime();
  /* format the elapsed time in hh:mm:ss format */
  (string_ != NULL) ?
    pshell_printf("\r%s%02d:%02d:%02d",
                  string_,
                  _elapsedTime.tv_sec/3600,
                  (_elapsedTime.tv_sec%3600)/60,
                  _elapsedTime.tv_sec%60) :
    pshell_printf("\r%02d:%02d:%02d",
                  _elapsedTime.tv_sec/3600,
                  (_elapsedTime.tv_sec%3600)/60,
                  _elapsedTime.tv_sec%60);
  pshell_flush();
}

/******************************************************************************/
/******************************************************************************/
void pshell_march(const char *string_)
{
  pshell_printf(string_);
  pshell_flush();
}

/******************************************************************************/
/******************************************************************************/
void pshell_flush(void)
{
  /*
   * if we called a command non-interactively, just bail because there is no
   * client side program (either pshell or telnet) to receive the output
   */
  if (_isCommandInteractive)
  {
    if ((_serverType == PSHELL_UDP_SERVER) || (_serverType == PSHELL_UNIX_SERVER))
    {
      /* don't do any intermediate flushes if this command was from a control client */
      if (!_isControlCommand)
      {
        reply(_pshellMsg);
        /* clear out buffer for next time */
        _pshellMsg->payload[0] = 0;
      }
    }
    else  /* TCP or local server */
    {
      pshell_rl_writeOutput(_pshellMsg->payload);
      /* clear out buffer for next time */
      _pshellMsg->payload[0] = '\0';

    }
  }
}

/******************************************************************************/
/******************************************************************************/
void pshell_printf(const char *format_, ...)
{

  /*
   * if we called a command non-interactively, just bail because there is no
   * pshell client side program to receive the output
   */
  if (!_isCommandInteractive || !_isRunning)
  {
    return;
  }

  va_list args;
  int offset = pshell_getLength(_pshellMsg->payload);
  char *address = &_pshellMsg->payload[offset];

#ifndef PSHELL_VSNPRINTF

#ifdef PSHELL_GROW_PAYLOAD_IN_CHUNKS
  void *ptr;
  int newPayloadSize;
#endif

  /* check for buffer overflow */
  if (offset > (int)(_pshellPayloadSize - PSHELL_PAYLOAD_GUARDBAND))
  {

#ifndef PSHELL_GROW_PAYLOAD_IN_CHUNKS  /* not growing buffer on overflow, flush instead */
    if (!_isControlCommand)
    {
      pshell_flush();
      address = _pshellMsg->payload;
    }
    else
    {
      PSHELL_ERROR("Overflow detected in static buffer for control command: '%s', truncating buffer to: %d bytes",
                   _foundCommand->command,
                   _pshellPayloadSize);
      return;
    }

#else   /* growing buffer on overflow, grab another chunk */

    newPayloadSize = _pshellPayloadSize + PSHELL_PAYLOAD_CHUNK;

    if ((ptr = realloc(_pshellMsg, newPayloadSize+PSHELL_HEADER_SIZE)) == NULL)
    {
      PSHELL_ERROR("Could not allocate memory to expand pshellMsg payload, size: %d, payload truncated",
                   (newPayloadSize+PSHELL_HEADER_SIZE));
      return;
    }
    else
    {

      /*
       * successfully got a new chunk of memory,
       * update our pointer and set our payload size
       */
      _pshellPayloadSize = newPayloadSize;
      _pshellMsg = (PshellMsg*)ptr;

      /*
       * we need to do this because our address could have changed in
       * the realloc, we need to preserve our original offset for the
       * vsnprintf call
       */
      address = &_pshellMsg->payload[offset];
    }

#endif  /* ifdef PSHELL_FLUSH_ON_OVERFLOW */

  }

  va_start(args, format_);
  vsprintf(address, format_, args);
  va_end(args);

#else  /* ifndef PSHELL_VSNPRINTF */

  void *ptr;
  int newPayloadSize;
  bool printComplete = false;
  int requestedSize;
  int availableSize;

  while (!printComplete)
  {

    availableSize = _pshellPayloadSize - pshell_getLength(_pshellMsg->payload);

    va_start(args, format_);
    requestedSize = vsnprintf(address, availableSize, format_, args);
    va_end(args);

    if ((requestedSize > -1) && (requestedSize < availableSize))
    {
      /* data written ok, we can return */
      printComplete = true;
    }
    else
    {

      /* buffer overflow, grab some more memory */

#ifdef PSHELL_GROW_PAYLOAD_IN_CHUNKS

      /* grab another chunk */
      newPayloadSize = _pshellPayloadSize + PSHELL_PAYLOAD_CHUNK;

#else     /* grow by exact amount */

      if (requestedSize > -1)
      {
        /*
         * grab exactly what is needed,
         * glibc 2.1 returns the overflow size
         */
        newPayloadSize = _pshellPayloadSize + requestedSize + 1;
      }
      else
      {
        /*
         * grab another chunk, glibc 2.0 will return -1
         * on an overflow instead of the overflow size
         */
        newPayloadSize = _pshellPayloadSize + PSHELL_PAYLOAD_CHUNK;
      }
#endif

      if ((ptr = realloc(_pshellMsg, newPayloadSize+PSHELL_HEADER_SIZE)) == NULL)
      {
        PSHELL_ERROR("Could not allocate memory to expand pshellMsg payload, size: %d, payload truncated",
                     (int)(newPayloadSize+PSHELL_HEADER_SIZE));
        printComplete = true;
      }
      else
      {

        /*
         * successfully got a new chunk of memory,
         * update our pointer and set our payload size
         */
        _pshellPayloadSize = newPayloadSize;
        _pshellMsg = (PshellMsg*)ptr;

        /*
         * we need to do this because our address could have changed in
         * the realloc, we need to preserver our original offset for the
         * vsnprintf call
         */
        address = &_pshellMsg->payload[offset];

      }
    }
  }
#endif  /* ifndef PSHELL_VSNPRINTF */

  /* if we are running a TCP server, flush the output to the socket */
  if ((_serverType == PSHELL_TCP_SERVER) ||
      (_serverType == PSHELL_LOCAL_SERVER) ||
      (_serverType == PSHELL_NO_SERVER))
  {
    pshell_flush();
  }

}

/******************************************************************************/
/******************************************************************************/
void pshell_startServer(const char *serverName_,
                        PshellServerType serverType_,
                        PshellServerMode serverMode_,
                        const char *hostnameOrIpAddr_,
                        unsigned port_)
{
  pthread_t pshellServerThreadTID;

  cleanupFileSystemResources();

  if ((serverType_ != PSHELL_UDP_SERVER) &&
      (serverType_ != PSHELL_UNIX_SERVER) &&
      (serverType_ != PSHELL_TCP_SERVER) &&
      (serverType_ != PSHELL_LOCAL_SERVER))
  {
    PSHELL_ERROR("Invalid shell type: %d, valid types are: %d (UDP), %d (TCP), %d (UNIX), or %d (LOCAL)",
                 serverType_,
                 PSHELL_UDP_SERVER,
                 PSHELL_TCP_SERVER,
                 PSHELL_UNIX_SERVER,
                 PSHELL_LOCAL_SERVER);
  }
  else if (((serverType_ == PSHELL_UDP_SERVER) || (serverType_ == PSHELL_TCP_SERVER)) && ((port_ == 0) || (hostnameOrIpAddr_ == NULL)))
  {
    PSHELL_ERROR("%s server must supply valid IP/hostname and port", ((serverType_ == PSHELL_UDP_SERVER) ? "UDP" : "TCP"));
  }
  else if (!_isRunning)
  {

    /* allocate our transfer buffer */
    if ((_pshellMsg = (PshellMsg*)malloc(_pshellPayloadSize+PSHELL_HEADER_SIZE)) != NULL)
    {

      if (serverType_ == PSHELL_LOCAL_SERVER)
      {
        _defaultIdleTimeout = 0;
      }

      /* initialize payload of transfer buffer */
      _pshellMsg->payload[0] = '\0';

      /* make sure we are only invoked once */
      _isRunning = true;
      strcpy(_serverName, serverName_);
      if (hostnameOrIpAddr_ != NULL)
      {
        strcpy(_hostnameOrIpAddr, hostnameOrIpAddr_);
      }

      _ipAddress[0] = '\0';
      strcpy(_title, _defaultTitle);
      strcpy(_banner, _defaultBanner);
      strcpy(_prompt, _defaultPrompt);
      _port = port_;
      _serverType = serverType_;
      _serverMode = serverMode_;
      _dummyTokens.numTokens = 0;

      /* load any config file */
      loadConfigFile();

      /* add our native commands */
      addNativeCommands();

      /* load any startup file */
      loadStartupFile();

      if (_prompt[strlen(_prompt)-1] != ' ')
      {
        strcat(_prompt, " ");
      }

      if (_serverMode == PSHELL_BLOCKING)
      {
        /* blocking mode, run the server directly from this function */
        runServer();
      }
      else
      {
        /* non-blocking mode, kick off our server thread */
        pthread_create(&pshellServerThreadTID, NULL, serverThread, NULL);
      }

    }
    else
    {
      PSHELL_ERROR("Could not allocate memory for initial pshellMsg, size: %d",
                   (int)(_pshellPayloadSize+PSHELL_HEADER_SIZE));
    }

  }
  else
  {
    PSHELL_ERROR("PSHELL server: %s is already running", serverName_);
  }

}

/******************************************************************************/
/******************************************************************************/
void pshell_noServer(int argc, char *argv[])
{
  unsigned numMatches;
  char *commandName;
  _serverType = PSHELL_NO_SERVER;
  _isCommandInteractive = true;
  _isControlCommand = false;
  _isCommandDispatched = true;
  _isRunning = true;
  strcpy(_serverName, argv[0]);
  strcpy(_ipAddress, "local");
  strcpy(_title, "PSHELL");
  _pshellMsg = (PshellMsg*)malloc(_pshellPayloadSize+PSHELL_HEADER_SIZE);
  addNativeCommands();
  /* initialize payload of transfer buffer */
  _pshellMsg->payload[0] = '\0';
  for (int i = 0; i < argc; i++)
  {
    commandName = argv[i];
    _argc = argc-(PSHELL_BASE_ARG_OFFSET+i);
    _argv = &argv[PSHELL_BASE_ARG_OFFSET+i];
    if ((numMatches = findCommand(commandName)) == 1)
    {
      /* see if they asked for the command usage */
      if (pshell_isHelp() && _foundCommand->showUsage)
      {
        pshell_showUsage();
        break;
      }
      else if (((_argc >= _foundCommand->minArgs) &&
                (_argc <= _foundCommand->maxArgs)) ||
                (pshell_isHelp() && !_foundCommand->showUsage))
      {
        /*
         * dispatch user command, don't need to check for a NULL
         * function pointer because the validation in the addCommand
         * function will catch that and not add the command
         */
        _foundCommand->function(_argc, _argv);
        break;
      }
      else
      {
        /* arg count validation failure, show the usage */
        pshell_showUsage();
        break;
      }
    }
  }
  if (numMatches == 0)
  {
    if (argc == 1)
    {
      help(argc, argv);
    }
    else
    {
      printf("PSHELL_ERROR: Command: '%s' not found\n", commandName);
    }
  }
  else if (numMatches > 1)
  {
    printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n", commandName);
  }
}

/******************************************************************************/
/******************************************************************************/
void pshell_cleanupResources(void)
{
  if (_isRunning && (_serverType == PSHELL_UNIX_SERVER))
  {
    unlink(_localUnixAddress.sun_path);
  }
  unlink(_lockFile);
  cleanupFileSystemResources();
}

/**************************************************
 * command line agrument interpretation functions
 **************************************************/

/******************************************************************************/
/******************************************************************************/
PshellTokens *pshell_tokenize(const char *string_, const char *delimeter_)
{

  void *ptr;
  char *str;

  if (_isCommandDispatched)
  {
    /* see if we need to get a new token list */
    if (_numTokenLists >= _maxTokenLists)
    {
      /* we need a new token list */
      if ((ptr = realloc(_tokenList, (_maxTokenLists+PSHELL_TOKEN_LIST_CHUNK)*sizeof(Tokens))) == NULL)
      {
        PSHELL_ERROR("Could not allocate memory to expand token list, size: %d",
                     (int)((_maxTokenLists+1)*sizeof(PshellTokens)));
        return (&_dummyTokens);
      }
      _tokenList = (Tokens*)ptr;
      _maxTokenLists += PSHELL_TOKEN_LIST_CHUNK;
    }

    _tokenList[_numTokenLists]._public.tokens = NULL;
    _tokenList[_numTokenLists]._public.numTokens = 0;
    _tokenList[_numTokenLists].maxTokens = 0;
    _tokenList[_numTokenLists].string = NULL;

    /* see if we need to grab a new token */
    if (!allocateTokens())
    {
      return (&_dummyTokens);
    }

    /*
     * get our first token, copy our original string
     * so we don't hack it up with our tokenization
     */
    _tokenList[_numTokenLists].string =  strdup(string_);
    if ((str = strtok(_tokenList[_numTokenLists].string, delimeter_)) != NULL)
    {
      _tokenList[_numTokenLists]._public.tokens[_tokenList[_numTokenLists]._public.numTokens++] = str;
      /* now get the rest of our tokens */
      while ((str = strtok(NULL, delimeter_)) != NULL)
      {
        /* see if we need to grab a new token */
        if (!allocateTokens())
        {
          return (&_dummyTokens);
        }
        _tokenList[_numTokenLists]._public.tokens[_tokenList[_numTokenLists]._public.numTokens++] = str;
      }
    }
    _numTokenLists++;
    return (&_tokenList[_numTokenLists-1]._public);
  }
  else
  {
    PSHELL_ERROR("Can only call 'pshell_tokenize' from within an PSHELL callback function");
    return (&_dummyTokens);
  }
}

/******************************************************************************/
/******************************************************************************/
unsigned pshell_getLength(const char *string_)
{
  if (string_ == NULL)
  {
    return (0);
  }
  else
  {
    return (strlen(string_));
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isEqual(const char *string1_, const char *string2_)
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
bool pshell_isEqualNoCase(const char *string1_, const char *string2_)
{
  if ((string1_ == NULL) && (string2_ == NULL))
  {
    return (true);
  }
  else if ((string1_ != NULL) && (string2_ != NULL))
  {
    return (strcasecmp(string1_, string2_) == 0);
  }
  else
  {
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isSubString(const char *string1_, const char *string2_, unsigned minChars_)
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
bool pshell_isSubStringNoCase(const char *string1_, const char *string2_, unsigned minChars_)
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
      return (strncasecmp(string1_, string2_, MAX(length, minChars_)) == 0);
    }
  }
  else
  {
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isAlpha(const char *string_)
{
  unsigned i;
  for (i = 0; i < pshell_getLength(string_); i++)
  {
    if (!isalpha(string_[i]))
    {
      return (false);
    }
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isNumeric(const char *string_, bool needHexPrefix_)
{
  return (pshell_isDec(string_) || pshell_isHex(string_, needHexPrefix_));
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isAlphaNumeric(const char *string_)
{
  unsigned i;
  for (i = 0; i < pshell_getLength(string_); i++)
  {
    if (!isalnum(string_[i]))
    {
      return (false);
    }
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isDec(const char *string_)
{
  unsigned i;
  unsigned offset = 0;
  if (pshell_getLength(string_) > 0)
  {
    if (string_[0] == '-')
    {
      offset = 1;
      if (pshell_getLength(string_) < 2)
      {
        return (false);
      }
    }
    for (i = offset; i < pshell_getLength(string_); i++)
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
bool pshell_isHex(const char *string_, bool needHexPrefix_)
{
  unsigned i;
  unsigned start = 0;
  if (needHexPrefix_)
  {
    /* if they are requesting the 0x prefix, make sure it is there */
    if ((pshell_getLength(string_) > 2) &&
        (string_[0] == '0') &&
        (tolower(string_[1]) == 'x'))
    {
      start = 2;
    }
    else
    {
      return (false);
    }
  }
  for (i = start; i < pshell_getLength(string_); i++)
  {
    if (!isxdigit(string_[i]))
    {
      return (false);
    }
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isFloat(const char *string_)
{
  unsigned i;
  unsigned numDecimalPonts = 0;
  unsigned offset = 0;
  if (pshell_getLength(string_) > 1)
  {
    if (string_[0] == '-')
    {
      offset = 1;
      if (pshell_getLength(string_) < 3)
      {
        return (false);
      }
    }
    for (i = offset; i < pshell_getLength(string_); i++)
    {
      if (!isdigit(string_[i]) && (string_[i] != '.'))
      {
        return (false);
      }
      if (string_[i] == '.')
      {
        numDecimalPonts++;
      }
    }
    return (numDecimalPonts == 1);
  }
  else
  {
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isIpv4Addr(const char *string_)\
{
  PshellTokens *addr;
  addr = pshell_tokenize(string_, ".");
  return (addr->numTokens == 4 &&
          pshell_isDec(addr->tokens[0]) &&
          pshell_getInt(addr->tokens[0]) >= 0 &&
          pshell_getInt(addr->tokens[0]) <= 255 &&
          pshell_isDec(addr->tokens[1]) &&
          pshell_getInt(addr->tokens[1]) >= 0 &&
          pshell_getInt(addr->tokens[1]) <= 255 &&
          pshell_isDec(addr->tokens[2]) &&
          pshell_getInt(addr->tokens[2]) >= 0 &&
          pshell_getInt(addr->tokens[2]) <= 255 &&
          pshell_isDec(addr->tokens[3]) &&
          pshell_getInt(addr->tokens[3]) >= 0 &&
          pshell_getInt(addr->tokens[3]) <= 255);
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isIpv4AddrWithNetmask(const char *string_)
{
  PshellTokens *addr;
  addr = pshell_tokenize(string_, "/");
  return (addr->numTokens == 2 &&
          pshell_isIpv4Addr(addr->tokens[0]) &&
          pshell_isDec(addr->tokens[1]) &&
          pshell_getInt(addr->tokens[1]) >= 0 &&
          pshell_getInt(addr->tokens[1]) <= 32);
}

/******************************************************************************/
/******************************************************************************/
bool pshell_isMacAddr(const char *string_)\
{
  PshellTokens *addr;
  addr = pshell_tokenize(string_, ":");
  return (addr->numTokens == 6 &&
          pshell_isHex(addr->tokens[0], false) && strlen(addr->tokens[0]) == 2 &&
          pshell_isHex(addr->tokens[1], false) && strlen(addr->tokens[1]) == 2 &&
          pshell_isHex(addr->tokens[2], false) && strlen(addr->tokens[2]) == 2 &&
          pshell_isHex(addr->tokens[3], false) && strlen(addr->tokens[3]) == 2 &&
          pshell_isHex(addr->tokens[4], false) && strlen(addr->tokens[4]) == 2 &&
          pshell_isHex(addr->tokens[5], false) && strlen(addr->tokens[5]) == 2);
}

/**********************************************
 * command line agrument conversion functions
 **********************************************/

/******************************************************************************/
/******************************************************************************/
long pshell_getLong(const char *string_, PshellRadix radix_, bool needHexPrefix_)
{
  if (radix_ == PSHELL_RADIX_ANY)
  {
    if (pshell_isDec(string_))
    {
      return (strtol(string_, (char **)NULL, 10));
    }
    else if (pshell_isHex(string_))
    {
      return (strtol(string_, (char **)NULL, 16));
    }
    else
    {
      PSHELL_ERROR("Could not extract numeric value from string: '%s', consider checking format with pshell_isNumeric()", string_);
      return (0);
    }
  }
  else if ((radix_ == PSHELL_RADIX_DEC) && pshell_isDec(string_))
  {
    return (strtol(string_, (char **)NULL, 10));
  }
  else if ((radix_ == PSHELL_RADIX_HEX) && pshell_isHex(string_, needHexPrefix_))
  {
    return (strtol(string_, (char **)NULL, 16));
  }
  else
  {
    PSHELL_ERROR("Could not extract numeric value from string: '%s', consider checking format with pshell_isNumeric()", string_);
    return (0);
  }
}

/******************************************************************************/
/******************************************************************************/
unsigned long pshell_getUnsignedLong(const char *string_, PshellRadix radix_, bool needHexPrefix_)
{
  if (radix_ == PSHELL_RADIX_ANY)
  {
    if (pshell_isDec(string_))
    {
      return (strtol(string_, (char **)NULL, 10));
    }
    else if (pshell_isHex(string_))
    {
      return (strtol(string_, (char **)NULL, 16));
    }
    else
    {
      PSHELL_ERROR("Could not extract numeric value from string: '%s', consider checking format with pshell_isNumeric()", string_);
      return (0);
    }
  }
  else if ((radix_ == PSHELL_RADIX_DEC) && pshell_isDec(string_))
  {
    return (strtoul(string_, (char **)NULL, 10));
  }
  else if ((radix_ == PSHELL_RADIX_HEX) && pshell_isHex(string_, needHexPrefix_))
  {
    return (strtoul(string_, (char **)NULL, 16));
  }
  else
  {
    PSHELL_ERROR("Could not extract numeric value from string: '%s', consider checking format with pshell_isNumeric()", string_);
    return (0);
  }
}

/******************************************************************************/
/******************************************************************************/
double pshell_getDouble(const char *string_)
{
  if (pshell_isFloat(string_))
  {
    return (strtod(string_, NULL));
  }
  else
  {
    PSHELL_ERROR("Could not extract floating point value from string: '%s', consider checking format with pshell_isFloat()", string_);
    return (0.0);
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_getOption(const char *string_, char *option_, char *value_)
{
  char *valueStart;
  PshellTokens *option = pshell_tokenize(string_, "=");
  /* if the 'strlen(option_) == 0', we always extract the option into
   * 'value_' and copy the extracted option name into 'option_', if
   * the 'strlen(option_) != 0', we only extract the 'value_' if the
   * option matches the one specified in the 'option_' string, the
   * format of the option can have two different forms, -<option><value>
   * type option where <option> is a single character identifier or
   * <option>=<value> where <option> can be any length character string,
   * if 'option_ == NULL', we do not extract any option and return 'false'
   */
  if (option_ == NULL)
  {
    /* cannot extract the option into a NULL string */
    return (false);
  }
  else if (strlen(option_) == 0)
  {
    /* return the found option and corresponding value */

    /* see if they are looking for a -<option><value> type option */
    if (string_[0] == '-')
    {
      strncpy(option_, &string_[0], 2);
      option_[2] = 0;
      strcpy(value_, &string_[2]);
      return (true);
    }
    else
    {
      /* they are looking for a <option>=<value> type option */
      if (option->numTokens >= 2)
      {
        strcpy(option_, option->tokens[0]);
        valueStart = strstr((char *)string_, "=");
        strcpy(value_, &valueStart[1]);
        return (true);
      }
      else
      {
        return (false);
      }
    }
  }
  else
  {

    /* non-NULL option, look for the requested option */

    /* see if they are looking for a -<option><value> type option */
    if (option_[0] == '-')
    {
      if ((string_[0] == '-') && (option_[1] == string_[1]))
      {
        /* got a match, set the value & return true */
        strcpy(value_, &string_[2]);
        return (true);
      }
      else
      {
        return (false);
      }
    }
    else
    {
      /* assume they are looking for a <option>=<value> type option */
      if ((option->numTokens >= 2) && pshell_isEqual(option->tokens[0], option_))
      {
        valueStart = strstr((char *)string_, "=");
        strcpy(value_, &valueStart[1]);
        return (true);
      }
      else
      {
        return (false);
      }
    }
  }
  return (false);
}

/******************************************************************************/
/******************************************************************************/
void *pshell_getAddress(const char *string_)
{
  return ((void*)pshell_getUnsignedLong(string_));
}

/******************************************************************************/
/******************************************************************************/
bool pshell_getBool(const char *string_)
{
  return (pshell_isEqual(string_, "true") ||
          pshell_isEqual(string_, "yes") ||
          pshell_isEqual(string_, "on"));
}

/******************************************************************************/
/******************************************************************************/
float pshell_getFloat(const char *string_)
{
  return ((float)pshell_getDouble(string_));
}

/******************************************************************************/
/******************************************************************************/
int pshell_getInt(const char *string_, PshellRadix radix_, bool needHexPrefix_)
{
  return ((int)pshell_getLong(string_, radix_, needHexPrefix_));
}

/******************************************************************************/
/******************************************************************************/
short pshell_getShort(const char *string_, PshellRadix radix_, bool needHexPrefix_)
{
  return ((short)pshell_getLong(string_, radix_, needHexPrefix_));
}

/******************************************************************************/
/******************************************************************************/
char pshell_getChar(const char *string_, PshellRadix radix_, bool needHexPrefix_)
{
  return ((char)pshell_getLong(string_, radix_, needHexPrefix_));
}

/******************************************************************************/
/******************************************************************************/
unsigned pshell_getUnsigned(const char *string_, PshellRadix radix_, bool needHexPrefix_)
{
  return ((unsigned)pshell_getUnsignedLong(string_, radix_, needHexPrefix_));
}

/******************************************************************************/
/******************************************************************************/
unsigned short pshell_getUnsignedShort(const char *string_, PshellRadix radix_, bool needHexPrefix_)
{
  return ((unsigned short)pshell_getUnsignedLong(string_, radix_, needHexPrefix_));
}

/******************************************************************************/
/******************************************************************************/
unsigned char pshell_getUnsignedChar(const char *string_, PshellRadix radix_, bool needHexPrefix_)
{
  return ((unsigned char)pshell_getUnsignedLong(string_, radix_, needHexPrefix_));
}

/************************************
 * private "member" function bodies
 ************************************/

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
  pshell_printf("\n");
  pshell_printf("***********************************************\n");
  pshell_printf("*            AVAILABLE BATCH FILES            *\n");
  pshell_printf("***********************************************\n");
  pshell_printf("\n");
  pshell_printf("%s   %-*s   %-*s\n", "Index", _batchFiles.maxFilenameLength, "Filename", _batchFiles.maxDirectoryLength, "Directory");
  pshell_printf("%s   ", "=====");
  for (unsigned i = 0; i < _batchFiles.maxFilenameLength; i++) pshell_printf("=");
  pshell_printf("   ");
  for (unsigned i = 0; i < _batchFiles.maxDirectoryLength; i++) pshell_printf("=");
  pshell_printf("\n");
  for (unsigned i = 0; i < _batchFiles.numFiles; i++)
  {
    pshell_printf("%-5d   %-*s   %-*s\n",
                  i+1,
                  _batchFiles.maxFilenameLength,
                  _batchFiles.files[i].filename,
                  _batchFiles.maxDirectoryLength,
                  _batchFiles.files[i].directory);
  }
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
static void clearScreen(void)
{
  fprintf(stdout, "\033[H\033[J");
  fflush(stdout);
}

/******************************************************************************/
/******************************************************************************/
static void processBatchFile(char *filename_, unsigned rate_, unsigned repeat_, bool clear_)
{
  FILE *fp;
  char inputLine[180];
  char batchFile[180];
  unsigned count = 0;

  if (getBatchFile(filename_, batchFile) && (fp = fopen(batchFile, "r")) != NULL)
  {
    while ((repeat_ == 0) || (count < repeat_))
    {
      if (repeat_ != 0)
      {
        pshell_printf("\033]0;%s: %s[%s], Mode: BATCH[%s], Rate: %d SEC, Iteration: %d of %d\007", _title, _serverName, _ipAddress, filename_, rate_, count+1, repeat_);
      }
      else
      {
        pshell_printf("\033]0;%s: %s[%s], Mode: BATCH[%s], Rate: %d SEC, Iteration: %d\007", _title, _serverName, _ipAddress, filename_, rate_, count+1);
      }
      if (clear_)
      {
        clearScreen();
      }
      while (fgets(inputLine, sizeof(inputLine), fp) != NULL)
      {
        if (inputLine[0] != '#')
        {
          _pshellMsg->header.msgType = PSHELL_USER_COMMAND;
          if (inputLine[strlen(inputLine)-1] == '\n')
          {
            inputLine[strlen(inputLine)-1] = 0;  /* NULL terminate */
          }
          if (strlen(inputLine) > 0)
          {
            processCommand(inputLine);
          }
        }
      }
      count++;
      if ((repeat_ == 0) || (count < repeat_))
      {
        sleep(rate_);
        rewind(fp);
      }
    }
    fclose(fp);
    /* restore our terminal title bar */
    pshell_printf("\033]0;%s: %s[%s], Mode: INTERACTIVE\007", _title, _serverName, _ipAddress);
  }
}

/******************************************************************************/
/******************************************************************************/
static void runUDPServer(void)
{
  /* startup our UDP server */
  if (createSocket())
  {
    PSHELL_INFO("UDP Server: %s Started On Host: %s, Port: %d",
                _serverName,
                _hostnameOrIpAddr,
                _port);
    for (;;)
    {
      receiveUDP();
    }
  }
  else
  {
    PSHELL_ERROR("Cannot create socket for UDP Server: %s On Host: %s, Port: %d",
                 _serverName,
                 _hostnameOrIpAddr,
                 _port);
  }
}

/******************************************************************************/
/******************************************************************************/
static bool acceptConnection(void)
{
  _connectFd = accept(_socketFd, NULL, 0);
  return (_connectFd > 0);
}

/******************************************************************************/
/******************************************************************************/
static void runTCPServer(void)
{
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  bool socketCreated = true;
  bool connectionAccepted = true;
  bool initialStartup = true;
  /* startup our TCP server and accept new connections */
  while (socketCreated && connectionAccepted)
  {
    socketCreated = createSocket();
    if (socketCreated)
    {
      if (initialStartup)
      {
        PSHELL_INFO("TCP Server: %s Started On Host: %s, Port: %d",
                    _serverName,
                    _hostnameOrIpAddr,
                    _port);
        initialStartup = false;
      }
      connectionAccepted = acceptConnection();
      if (connectionAccepted)
      {
        /* shutdown original socket to not allow any new connections until we are done with this one */
        getsockname(_connectFd, (sockaddr *)&addr, &addrlen);
        strcpy(_ipAddress, inet_ntoa(addr.sin_addr));
        sprintf(_interactivePrompt, "%s[%s:%d]:%s", _serverName, _ipAddress, _port, _prompt);
        pshell_rl_setFileDescriptors(_connectFd,
                                     _connectFd,
                                     PSHELL_RL_SOCKET,
                                     PSHELL_RL_ONE_MINUTE*_defaultIdleTimeout);
        shutdown(_socketFd, SHUT_RDWR);
        receiveTCP();
        shutdown(_connectFd, SHUT_RDWR);
        close(_connectFd);
        close(_socketFd);
      }
    }
  }
  if (!socketCreated || !connectionAccepted)
  {
    PSHELL_ERROR("Cannot create socket for TCP Server: %s On Host: %s, Port: %d",
                 _serverName,
                 _hostnameOrIpAddr,
                 _port);
  }
}

/******************************************************************************/
/******************************************************************************/
static void runUNIXServer(void)
{
  /* startup our UNIX server */
  if (createSocket())
  {
    PSHELL_INFO("UNIX Server: %s Started", _serverName);
    for (;;)
    {
      receiveUNIX();
    }
  }
  else
  {
    PSHELL_ERROR("Cannot create socket for UNIX Server: %s", _serverName);
  }
}

/******************************************************************************/
/******************************************************************************/
static void runLocalServer(void)
{
  char inputLine[180] = {0};
  bool idleSession;
  strcpy(_ipAddress, "local");
  sprintf(_interactivePrompt, "%s[%s]:%s", _serverName, _ipAddress, _prompt);
  showWelcome();
  pshell_rl_setIdleTimeout(PSHELL_RL_ONE_MINUTE*_defaultIdleTimeout);
  while (!_quit && ((idleSession = pshell_rl_getInput(_interactivePrompt, inputLine)) == false))
  {
    _pshellMsg->header.msgType = PSHELL_USER_COMMAND;
    /* if we are currently processing a non-interactive command (via the pshell_runCommand
     * function call), wait a little bit for it to complete before processing an interactive command */
    while (!_isCommandInteractive) sleep(1);
    /* good to go, process an interactive command */
    processCommand(inputLine);
  }
}

/******************************************************************************/
/******************************************************************************/
static int getIpAddress(const char *interface_, char *ipAddress_)
{
  struct ifaddrs *ifaddr;
  struct ifaddrs *ifa;

  if (getifaddrs(&ifaddr) == 0)
  {
    /* walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
      if ((ifa->ifa_addr->sa_family == AF_INET) && pshell_isEqual(ifa->ifa_name, interface_))
      {
        strcpy(ipAddress_, inet_ntoa(((struct sockaddr_in *)&ifa->ifa_addr->sa_family)->sin_addr));
        freeifaddrs(ifaddr);
        return (((struct sockaddr_in *)&ifa->ifa_addr->sa_family)->sin_addr.s_addr);
      }
    }
    freeifaddrs(ifaddr);
  }
  return (0);
}

/******************************************************************************/
/******************************************************************************/
static void showWelcome(void)
{
  char sessionInfo[256];
  unsigned maxLength;
  /* set our terminal title bar */
  if (_serverType == PSHELL_TCP_SERVER)
  {
    sprintf(sessionInfo, "Single session %s server: %s[%s:%d]", ((_serverType == PSHELL_TCP_SERVER) ? "TCP" : "LOCAL"), _serverName, _ipAddress, _port);
    pshell_printf("\033]0;%s: %s[%s:%d], Mode: INTERACTIVE\007", _title, _serverName, _ipAddress, _port);
  }
  else
  {
    sprintf(sessionInfo, "Single session %s server: %s[%s]", ((_serverType == PSHELL_TCP_SERVER) ? "TCP" : "LOCAL"), _serverName, _ipAddress);
    pshell_printf("\033]0;%s: %s[%s], Mode: INTERACTIVE\007", _title, _serverName, _ipAddress);
  }
  maxLength = MAX(strlen(_banner), strlen(sessionInfo))+3;
  /* show our welcome screen */
  pshell_printf("\n");
  PSHELL_PRINT_WELCOME_BORDER(pshell_printf, maxLength);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  %s\n", PSHELL_WELCOME_BORDER, _banner);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  %s\n", PSHELL_WELCOME_BORDER, sessionInfo);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  if (_defaultIdleTimeout == PSHELL_RL_IDLE_TIMEOUT_NONE)
  {
    pshell_printf("%s  Idle session timeout: NONE\n", PSHELL_WELCOME_BORDER);
  }
  else
  {
    pshell_printf("%s  Idle session timeout: %d minutes\n", PSHELL_WELCOME_BORDER, _defaultIdleTimeout);
  }
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  To show command elapsed execution time, use -t <command>\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  Type '?' or 'help' at prompt for command summary\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  Type '?' or '-h' after command for command usage\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  Full <TAB> completion, command history, command\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  line editing, and command abbreviation supported\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  PSHELL_PRINT_WELCOME_BORDER(pshell_printf, maxLength);
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
static void quit(int argc, char *argv[])
{
  _quit = true;
}

/******************************************************************************/
/******************************************************************************/
static void setup(int argc, char *argv[])
{
  char command[80];
  char line[80];
  FILE *fp;
  pshell_printf("\n");
  if (pshell_isHelp())
  {
    pshell_printf("Usage: %s --setup\n", _serverName);
    pshell_printf("\n");
    pshell_printf("This command will setup Busybox like softlink shortcuts to\n");
    pshell_printf("all the registered commands.  This command must be run from\n");
    pshell_printf("the same directory as the invoking program and may require\n");
    pshell_printf("root privlidges to setup the softlinks, depending on the\n");
    pshell_printf("directory settings.\n");
  }
  else
  {
    struct stat buffer;
    if (stat(_serverName, &buffer) == 0)
    {
      pshell_printf("Busybox softlink setup:\n");
      pshell_printf("\n");
      for (unsigned i = 2; i < _numCommands; i++)
      {
        pshell_printf("Setting up softlink: %s -> %s\n", _commandTable[i].command, _serverName);
        sprintf(command, "rm -f %s", _commandTable[i].command);
        system(command);
        sprintf(command, "ln -s %s %s", _serverName, _commandTable[i].command);
        system(command);
      }
    }
    else
    {
      pshell_printf("ERROR: Setup command must be run from same directory as '%s' resides,\n", _serverName);
      sprintf(command, "which %s", _serverName);
      if ((fp = popen(command, "r")) != NULL)
      {
        if (fgets(line, sizeof(line), fp) != NULL)
        {
          pshell_printf("       i.e. %s", line);
        }
      }
    }
  }
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
static void help(int argc, char *argv[])
{
  unsigned i;
  unsigned pad;
  pshell_printf("\n");
  pshell_printf("****************************************\n");
  pshell_printf("*             COMMAND LIST             *\n");
  pshell_printf("****************************************\n");
  pshell_printf("\n");
  for (i = 0; i < _numCommands; i++)
  {
    pshell_printf("%s",  _commandTable[i].command);
    for (pad = strlen(_commandTable[i].command); pad < _maxCommandLength; pad++) pshell_printf(" ");
    pshell_printf("  -  %s\n", _commandTable[i].description);
  }
  if (_serverType == PSHELL_NO_SERVER)
  {
    pshell_printf("\n");
    pshell_printf("To run command type '%s <command>'\n", _serverName);
    pshell_printf("\n");
    pshell_printf("To get command usage type '%s <command> {? | -h}'\n", _serverName);
    pshell_printf("\n");
    pshell_printf("The special command '%s --setup' can be run\n", _serverName);
    pshell_printf("to automatically setup Busybox like softlink shortcuts for\n");
    pshell_printf("each of the commands.  This will allow direct access to each\n");
    pshell_printf("command from the command line shell without having to use the\n");
    pshell_printf("actual parent program name.  This command must be run from the\n");
    pshell_printf("same directory the parent program resides and may require root\n");
    pshell_printf("privlidges depending on the directory settings.\n");
  }
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
static void history(int argc, char *argv[])
{
  pshell_rl_showHistory();
}

/******************************************************************************/
/******************************************************************************/
static void batch(int argc, char *argv[])
{
  unsigned rate = 0;
  unsigned repeat = 0;
  bool clear = false;
  PshellTokens *argAndValue;
  if (pshell_isHelp())
  {
    getcwd(_currentDir, sizeof(_currentDir));
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("  where:\n");
    pshell_printf("    filename  - name of batch file to run\n");
    if (_serverType == PSHELL_NO_SERVER)
    {
      pshell_printf("    rate      - rate in seconds to repeat batch file (default=0)\n");
      pshell_printf("    repeat    - number of times to repeat command or 'forever' (default=1)\n");
      pshell_printf("    clear     - clear the screen between batch file runs\n");
    }
    else  // TCP or LOCAL server
    {
      pshell_printf("    index     - Index of the batch file to execute (from the -list option)\n");
      pshell_printf("    -list     - List all the available batch files\n");
      pshell_printf("    -show     - Show the contents of batch file without executing\n");
    }
    pshell_printf("\n");
    pshell_printf("  NOTE: Batch files must have a .psh or .batch extension.  Batch\n");
    pshell_printf("        files will be searched in the following directory order:\n");
    pshell_printf("\n");
    pshell_printf("        current directory - %s\n", _currentDir);
    pshell_printf("        $PSHELL_BATCH_DIR - %s\n", getenv("PSHELL_BATCH_DIR"));
    pshell_printf("        default directory - %s\n", PSHELL_BATCH_DIR);
    pshell_printf("\n");
  }
  else if (_serverType == PSHELL_NO_SERVER)
  {
    for (int i = 1; i < argc; i++)
    {
      argAndValue = pshell_tokenize(argv[i], "=");
      if (argAndValue->numTokens == 2)
      {
        if (pshell_isEqual(argAndValue->tokens[0], "rate") && pshell_isNumeric(argAndValue->tokens[1]))
        {
          rate = pshell_getUnsigned(argAndValue->tokens[1]);
        }
        else if (pshell_isEqual(argAndValue->tokens[0], "repeat"))
        {
          if (pshell_isNumeric(argAndValue->tokens[1]))
          {
            repeat = pshell_getUnsigned(argAndValue->tokens[1]);
          }
          else if (pshell_isEqual(argAndValue->tokens[1], "forever"))
          {
            repeat = 0;
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
      else if ((argAndValue->numTokens == 1) && pshell_isEqual(argAndValue->tokens[0], "clear"))
      {
        clear = true;
      }
      else
      {
        pshell_showUsage();
      }
    }
    processBatchFile(argv[0], rate, repeat, clear);
  }
  else  // TCP or LOCAL server
  {
    if (argc == 1)
    {
      loadBatchFile(argv[0], false);
    }
    else if (pshell_isSubString(argv[1], "-show", 2))
    {
      loadBatchFile(argv[0], true);
    }
    else
    {
      pshell_showUsage();
      return;
    }
  }
}

/******************************************************************************/
/******************************************************************************/
static void receiveTCP(void)
{
  char command[PSHELL_RL_MAX_COMMAND_SIZE] = {0};
  bool idleSession;

  /* print out our welcome banner */
  showWelcome();

  _quit = false;
  while (!_quit && ((idleSession = pshell_rl_getInput(_interactivePrompt, command)) == false))
  {
    _pshellMsg->header.msgType = PSHELL_USER_COMMAND;
    /* if we are currently processing a non-interactive command (via the pshell_runCommand
     * function call), wait a little bit for it to complete before processing an interactive command */
    while (!_isCommandInteractive) sleep(1);
    /* good to go, process an interactive command */
    processCommand(command);
  }
}

/******************************************************************************/
/******************************************************************************/
static void runServer(void)
{
  if (_serverType == PSHELL_TCP_SERVER)
  {
    runTCPServer();
  }
  else if (_serverType == PSHELL_UDP_SERVER)
  {
    runUDPServer();
  }
  else if (_serverType == PSHELL_UNIX_SERVER)
  {
    runUNIXServer();
  }
  else   /* local server */
  {
    runLocalServer();
  }
}

/******************************************************************************/
/******************************************************************************/
static void *serverThread(void*)
{
  runServer();
  return (NULL);
}

/******************************************************************************/
/******************************************************************************/
static bool loadCommandFile(const char *filename_, bool inteactive_, bool showOnly_)
{
  char line[180];
  FILE *fp;
  bool retCode = false;
  if ((fp = fopen(filename_, "r")) != NULL)
  {
    while (fgets(line, sizeof(line), fp) != NULL)
    {
      /* ignore comment or blank lines */
      if ((line[0] != '#') && (strlen(line) > 0))
      {
        _pshellMsg->header.msgType = PSHELL_USER_COMMAND;
        if (line[strlen(line)-1] == '\n')
        {
          line[strlen(line)-1] = 0;  /* NULL terminate */
        }
        if (strlen(line) > 0)
        {
          if (showOnly_)
          {
            pshell_printf("%s\n", line);
          }
          else if (inteactive_)
          {
            processCommand(line);
          }
          else
          {
            pshell_runCommand(line);
          }
        }
      }
    }
    fclose(fp);
    retCode = true;
  }
  return (retCode);
}

/******************************************************************************/
/******************************************************************************/
static void loadStartupFile(void)
{
  char startupFile[180];
  char *startupPath;

  startupFile[0] = '\0';
  if ((startupPath = getenv("PSHELL_STARTUP_DIR")) != NULL)
  {
    sprintf(startupFile, "%s/%s.startup", startupPath, _serverName);
    /* look for file in path pointed to by env variable */
    if (loadCommandFile(startupFile, false, false) == false)
    {
      /* not found in env variable, look for file in default directory */
      sprintf(startupFile, "%s/%s.startup", PSHELL_STARTUP_DIR, _serverName);
      loadCommandFile(startupFile, false, false);
    }
  }
  else
  {
    /* env variable not found, look for file in default directory */
    sprintf(startupFile, "%s/%s.startup", PSHELL_STARTUP_DIR, _serverName);
    loadCommandFile(startupFile, false, false);
  }

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

  if (pshell_isSubString(filename_, "-list", 2))
  {
    showBatchFiles();
    return (false);
  }
  else if (pshell_isDec(filename_))
  {
    if (atoi(filename_) > 0 && atoi(filename_) <= (int)_batchFiles.numFiles)
    {
      sprintf(batchFile_, "%s/%s", _batchFiles.files[atoi(filename_)-1].directory, _batchFiles.files[atoi(filename_)-1].filename);
    }
    else
    {
      pshell_printf("ERROR: Invalid batch file index: %d, valid values 1-%d\n", filename_, _batchFiles.numFiles);
      return (false);
    }
  }
  else
  {
    int numMatches = 0;
    for (unsigned i = 0; i < _batchFiles.numFiles; i++)
    {
      if (pshell_isSubString(filename_, _batchFiles.files[i].filename, strlen(filename_)))
      {
        sprintf(batchFile_, "%s/%s", _batchFiles.files[i].directory, _batchFiles.files[i].filename);
        numMatches++;
      }
    }
    if (numMatches == 0)
    {
      pshell_printf("PSHELL_ERROR: Could not find batch file: '%s'\n", filename_);
      return (false);
    }
    else if (numMatches > 1)
    {
      pshell_printf("PSHELL_ERROR: Ambiguous file: '%s', use -list option to see available files or <index> to select specific file\n", filename_);
      return (false);
    }
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
static void loadBatchFile(const char *filename_, bool showOnly_)
{
  char batchFile[180];
  if (getBatchFile(filename_, batchFile) && !loadCommandFile(batchFile, true, showOnly_))
  {
    pshell_printf("PSHELL_ERROR: Could not open batch file: '%s', use -list option to see available files\n", filename_);
  }
}

/******************************************************************************/
/******************************************************************************/
static void addNativeCommands(void)
{
  int numNativeCommands = 0;
  int i;

  /*
   * add our two built in commands for the TCP/LOCAL server,
   * for the UDP/UNIX server, these are implemented in the
   * stand-alone 'pshell' client program
   */
  if ((_serverType == PSHELL_TCP_SERVER) ||
      (_serverType == PSHELL_LOCAL_SERVER) ||
      (_serverType == PSHELL_NO_SERVER))
  {
    if (_serverType != PSHELL_NO_SERVER)
    {
      pshell_addCommand(quit,
                        "quit",
                        "exit interactive mode",
                        NULL,
                        0,
                        0,
                        true);
      _quitCmd.function = _commandTable[_numCommands-1].function;
      _quitCmd.command = _commandTable[_numCommands-1].command;
      _quitCmd.usage = _commandTable[_numCommands-1].usage;
      _quitCmd.description = _commandTable[_numCommands-1].description;
      _quitCmd.minArgs = _commandTable[_numCommands-1].minArgs;
      _quitCmd.maxArgs = _commandTable[_numCommands-1].maxArgs;
      _quitCmd.showUsage = _commandTable[_numCommands-1].showUsage;
      _quitCmd.length = _commandTable[_numCommands-1].length;
      numNativeCommands += 1;
    }

    pshell_addCommand(help,
                      "help",
                      "show all available commands",
                      NULL,
                      0,
                      0,
                      true);
    _helpCmd.function = _commandTable[_numCommands-1].function;
    _helpCmd.command = _commandTable[_numCommands-1].command;
    _helpCmd.usage = _commandTable[_numCommands-1].usage;
    _helpCmd.description = _commandTable[_numCommands-1].description;
    _helpCmd.minArgs = _commandTable[_numCommands-1].minArgs;
    _helpCmd.maxArgs = _commandTable[_numCommands-1].maxArgs;
    _helpCmd.showUsage = _commandTable[_numCommands-1].showUsage;
    _helpCmd.length = _commandTable[_numCommands-1].length;
    numNativeCommands += 1;

    if (_serverType == PSHELL_NO_SERVER)
    {
      /* add our built in command for all server types */
      pshell_addCommand(batch,
                        "batch",
                        "run commands from a batch file",
                        "<filename> [repeat=<count> [rate=<seconds>]] [clear]",
                        1,
                        4,
                        false);

      /* save off the command info */
      _batchCmd.function = _commandTable[_numCommands-1].function;
      _batchCmd.command = _commandTable[_numCommands-1].command;
      _batchCmd.usage = _commandTable[_numCommands-1].usage;
      _batchCmd.description = _commandTable[_numCommands-1].description;
      _batchCmd.minArgs = _commandTable[_numCommands-1].minArgs;
      _batchCmd.maxArgs = _commandTable[_numCommands-1].maxArgs;
      _batchCmd.showUsage = _commandTable[_numCommands-1].showUsage;
      _batchCmd.length = _commandTable[_numCommands-1].length;
      numNativeCommands += 1;
    }
    else
    {
      pshell_addCommand(history,
                        "history",
                        "show history list of all entered commands",
                        NULL,
                        0,
                        0,
                        true);
      /* save off the command info */
      _historyCmd.function = _commandTable[_numCommands-1].function;
      _historyCmd.command = _commandTable[_numCommands-1].command;
      _historyCmd.usage = _commandTable[_numCommands-1].usage;
      _historyCmd.description = _commandTable[_numCommands-1].description;
      _historyCmd.minArgs = _commandTable[_numCommands-1].minArgs;
      _historyCmd.maxArgs = _commandTable[_numCommands-1].maxArgs;
      _historyCmd.showUsage = _commandTable[_numCommands-1].showUsage;
      _historyCmd.length = _commandTable[_numCommands-1].length;
      numNativeCommands += 1;

      pshell_addCommand(batch,
                        "batch",
                        "run commands from a batch file",
                        "{{<filename> | <index>} [-show]} | -list",
                        1,
                        2,
                        false);
      /* save off the command info */
      _batchCmd.function = _commandTable[_numCommands-1].function;
      _batchCmd.command = _commandTable[_numCommands-1].command;
      _batchCmd.usage = _commandTable[_numCommands-1].usage;
      _batchCmd.description = _commandTable[_numCommands-1].description;
      _batchCmd.minArgs = _commandTable[_numCommands-1].minArgs;
      _batchCmd.maxArgs = _commandTable[_numCommands-1].maxArgs;
      _batchCmd.showUsage = _commandTable[_numCommands-1].showUsage;
      _batchCmd.length = _commandTable[_numCommands-1].length;
      numNativeCommands += 1;
    }

  }

  /* move these commands to be first in the command list */

  /* move all the other commands down in the command list */
  for (i = (int)(_numCommands-(numNativeCommands+1)); i >= 0; i--)
  {
    _commandTable[i+numNativeCommands].function = _commandTable[i].function;
    _commandTable[i+numNativeCommands].command = _commandTable[i].command;
    _commandTable[i+numNativeCommands].usage = _commandTable[i].usage;
    _commandTable[i+numNativeCommands].description = _commandTable[i].description;
    _commandTable[i+numNativeCommands].minArgs = _commandTable[i].minArgs;
    _commandTable[i+numNativeCommands].maxArgs = _commandTable[i].maxArgs;
    _commandTable[i+numNativeCommands].showUsage = _commandTable[i].showUsage;
    _commandTable[i+numNativeCommands].length = _commandTable[i].length;
  }

  /* restore the saved native command info to be first in the command list */
  if (numNativeCommands == 4)
  {
    _commandTable[0].function = _quitCmd.function;
    _commandTable[0].command = _quitCmd.command;
    _commandTable[0].usage = _quitCmd.usage;
    _commandTable[0].description = _quitCmd.description;
    _commandTable[0].minArgs = _quitCmd.minArgs;
    _commandTable[0].maxArgs = _quitCmd.maxArgs;
    _commandTable[0].showUsage = _quitCmd.showUsage;
    _commandTable[0].length = _quitCmd.length;

    _commandTable[1].function = _helpCmd.function;
    _commandTable[1].command = _helpCmd.command;
    _commandTable[1].usage = _helpCmd.usage;
    _commandTable[1].description = _helpCmd.description;
    _commandTable[1].minArgs = _helpCmd.minArgs;
    _commandTable[1].maxArgs = _helpCmd.maxArgs;
    _commandTable[1].showUsage = _helpCmd.showUsage;
    _commandTable[1].length = _helpCmd.length;

    _commandTable[2].function = _historyCmd.function;
    _commandTable[2].command = _historyCmd.command;
    _commandTable[2].usage = _historyCmd.usage;
    _commandTable[2].description = _historyCmd.description;
    _commandTable[2].minArgs = _historyCmd.minArgs;
    _commandTable[2].maxArgs = _historyCmd.maxArgs;
    _commandTable[2].showUsage = _historyCmd.showUsage;
    _commandTable[2].length = _historyCmd.length;

    _commandTable[3].function = _batchCmd.function;
    _commandTable[3].command = _batchCmd.command;
    _commandTable[3].usage = _batchCmd.usage;
    _commandTable[3].description = _batchCmd.description;
    _commandTable[3].minArgs = _batchCmd.minArgs;
    _commandTable[3].maxArgs = _batchCmd.maxArgs;
    _commandTable[3].showUsage = _batchCmd.showUsage;
    _commandTable[3].length = _batchCmd.length;
  }
  else if (numNativeCommands == 3)
  {
    _commandTable[0].function = _helpCmd.function;
    _commandTable[0].command = _helpCmd.command;
    _commandTable[0].usage = _helpCmd.usage;
    _commandTable[0].description = _helpCmd.description;
    _commandTable[0].minArgs = _helpCmd.minArgs;
    _commandTable[0].maxArgs = _helpCmd.maxArgs;
    _commandTable[0].showUsage = _helpCmd.showUsage;
    _commandTable[0].length = _helpCmd.length;

    _commandTable[1].function = _historyCmd.function;
    _commandTable[1].command = _historyCmd.command;
    _commandTable[1].usage = _historyCmd.usage;
    _commandTable[1].description = _historyCmd.description;
    _commandTable[1].minArgs = _historyCmd.minArgs;
    _commandTable[1].maxArgs = _historyCmd.maxArgs;
    _commandTable[1].showUsage = _historyCmd.showUsage;
    _commandTable[1].length = _historyCmd.length;

    _commandTable[2].function = _batchCmd.function;
    _commandTable[2].command = _batchCmd.command;
    _commandTable[2].usage = _batchCmd.usage;
    _commandTable[2].description = _batchCmd.description;
    _commandTable[2].minArgs = _batchCmd.minArgs;
    _commandTable[2].maxArgs = _batchCmd.maxArgs;
    _commandTable[2].showUsage = _batchCmd.showUsage;
    _commandTable[2].length = _batchCmd.length;
  }
  else if (numNativeCommands == 2)
  {
    _commandTable[0].function = _helpCmd.function;
    _commandTable[0].command = _helpCmd.command;
    _commandTable[0].usage = _helpCmd.usage;
    _commandTable[0].description = _helpCmd.description;
    _commandTable[0].minArgs = _helpCmd.minArgs;
    _commandTable[0].maxArgs = _helpCmd.maxArgs;
    _commandTable[0].showUsage = _helpCmd.showUsage;
    _commandTable[0].length = _helpCmd.length;

    _commandTable[1].function = _batchCmd.function;
    _commandTable[1].command = _batchCmd.command;
    _commandTable[1].usage = _batchCmd.usage;
    _commandTable[1].description = _batchCmd.description;
    _commandTable[1].minArgs = _batchCmd.minArgs;
    _commandTable[1].maxArgs = _batchCmd.maxArgs;
    _commandTable[1].showUsage = _batchCmd.showUsage;
    _commandTable[1].length = _batchCmd.length;
  }

  _setupCmd.function = setup;
  _setupCmd.command = "--setup";
  _setupCmd.usage = NULL;
  _setupCmd.description = "setup busybox like softlink shortcuts to all registered commands";
  _setupCmd.minArgs = 0;
  _setupCmd.maxArgs = 0;
  _setupCmd.showUsage = false;
  _setupCmd.length = strlen("--setup");

  /* add to our TAB completion list */
  for (unsigned i = 0; i < _numCommands; i++)
  {
    pshell_rl_addTabCompletion(_commandTable[i].command);
  }

}

/******************************************************************************/
/******************************************************************************/
static void loadConfigFile(void)
{
  PshellTokens *config;
  PshellTokens *item;
  char configFile[180];
  char cwd[180];
  char *configPath;
  char line[180];
  FILE *fp;

  configFile[0] = '\0';
  if ((configPath = getenv("PSHELL_CONFIG_DIR")) != NULL)
  {
    sprintf(configFile, "%s/pshell-server.conf", configPath);
  }

  if ((fp = fopen(configFile, "r")) == NULL)
  {
    /* either the env variable is not found or the file is not found
     * look in our default directory
     */
    sprintf(configFile, "%s/pshell-server.conf", PSHELL_CONFIG_DIR);
    if ((fp = fopen(configFile, "r")) == NULL)
    {
      /* not found in our default directory, look in our CWD */
      getcwd(cwd, sizeof(cwd));
      sprintf(configFile, "%s/pshell-server.conf", cwd);
      fp = fopen(configFile, "r");
    }
  }

  if (fp != NULL)
  {
    /* set to true so we can call pshell_tokenize, need to make sure we call cleanupTokens after each call */
    _isCommandDispatched = true;
    while (fgets(line, sizeof(line), fp) != NULL)
    {
      /* look for our server name on every non-commented line */
      if (line[0] != '#')
      {
        line[strlen(line)-1] = '\0';
        config = pshell_tokenize(line, "=");
        if (config->numTokens == 2)
        {
          item = pshell_tokenize(config->tokens[0], ".");
          if ((item->numTokens == 2) && pshell_isEqual(item->tokens[0], _serverName))
          {
            if (pshell_isEqual(item->tokens[1], "title"))
            {
              strcpy(_title, config->tokens[1]);
            }
            else if (pshell_isEqual(item->tokens[1], "banner"))
            {
              strcpy(_banner, config->tokens[1]);
            }
            else if (pshell_isEqual(item->tokens[1], "prompt"))
            {
              strcpy(_prompt, config->tokens[1]);
            }
            else if (pshell_isEqual(item->tokens[1], "host"))
            {
              strcpy(_hostnameOrIpAddr, config->tokens[1]);
            }
            else if (pshell_isEqual(item->tokens[1], "port") &&
                     pshell_isNumeric(config->tokens[1]))
            {
              _port = pshell_getUnsigned(config->tokens[1]);
            }
            else if (pshell_isEqual(item->tokens[1], "timeout"))
            {
              if (pshell_isNumeric(config->tokens[1]))
              {
                _defaultIdleTimeout = pshell_getInt(config->tokens[1]);
              }
              else if (pshell_isEqualNoCase(config->tokens[1], "none"))
              {
                _defaultIdleTimeout = PSHELL_RL_IDLE_TIMEOUT_NONE;
              }
              pshell_rl_setIdleTimeout(_defaultIdleTimeout*PSHELL_RL_ONE_MINUTE);
            }
            else if (pshell_isEqual(item->tokens[1], "type"))
            {
              if (pshell_isEqualNoCase(config->tokens[1], "UDP"))
              {
                _serverType = PSHELL_UDP_SERVER;
              }
              else if (pshell_isEqualNoCase(config->tokens[1], "UNIX"))
              {
                _serverType = PSHELL_UNIX_SERVER;
              }
              else if (pshell_isEqualNoCase(config->tokens[1], "TCP"))
              {
                _serverType = PSHELL_TCP_SERVER;
              }
              else if (pshell_isEqualNoCase(config->tokens[1], "LOCAL"))
              {
                _serverType = PSHELL_LOCAL_SERVER;
              }
            }
          }
        }
      }
    }
    /* set back to false so we prevent a call to the pshell_tokenize function outside of a callback function */
    _isCommandDispatched = false;
    cleanupTokens();
    fclose(fp);
  }
}

/******************************************************************************/
/******************************************************************************/
static void cleanupFileSystemResources(void)
{
  DIR *dir;
  int unixLockFd;
  char unixLockFile[300];
  char unixSocketFile[300];
  char tempDirEntry[300];
  struct dirent *dirEntry;
  dir = opendir(_fileSystemPath);
  if (!dir)
  {
    sprintf(tempDirEntry, "mkdir %s", _fileSystemPath);
    system(tempDirEntry);
    sprintf(tempDirEntry, "chmod 777 %s", _fileSystemPath);
    system(tempDirEntry);
    dir = opendir(_fileSystemPath);
  }
  if (dir)
  {
    while ((dirEntry = readdir(dir)) != NULL)
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
        }
      }
    }
    closedir(dir);
  }
}

/******************************************************************************/
/******************************************************************************/
static bool bindSocket(void)
{
  unsigned port = _port;
  int unixLockFd;
  if (_serverType == PSHELL_UNIX_SERVER)
  {
    _localUnixAddress.sun_family = AF_UNIX;
    sprintf(_localUnixAddress.sun_path, "%s/%s", PSHELL_UNIX_SOCKET_PATH, _serverName);
    sprintf(_lockFile, "%s-unix%s", _localUnixAddress.sun_path, _lockFileExtension);
    for (unsigned attempt = 1; attempt <= PSHELL_MAX_BIND_ATTEMPTS+1; attempt++)
    {
      /* try to open lock file */
      if ((unixLockFd = open(_lockFile, O_RDONLY | O_CREAT, 0600)) > -1)
      {
        /* file exists, try to see if another process has it locked */
        if (flock(unixLockFd, LOCK_EX | LOCK_NB) == 0)
        {
          /* we got the lock, nobody else has it, bind to our socket */
          if (bind(_socketFd,
                   (struct sockaddr *) &_localUnixAddress,
                   sizeof(_localUnixAddress)) < 0)
          {
            PSHELL_ERROR("Cannot bind to UNIX socket: %s", _serverName);
            return (false);
          }
          else
          {
            /* success */
            strcpy(_ipAddress, "unix");
            if (attempt > 1)
            {
              sprintf(_serverName, "%s%d", _serverName, attempt-1);
            }
            return (true);
          }
        }
        else if (attempt == 1)
        {
          PSHELL_WARNING("Could not bind to UNIX address: %s, looking for first available address", _serverName);
        }
        sprintf(_localUnixAddress.sun_path, "%s/%s%d", PSHELL_UNIX_SOCKET_PATH, _serverName, attempt);
        sprintf(_lockFile, "%s-unix%s", _localUnixAddress.sun_path, _lockFileExtension);
      }
    }
    PSHELL_ERROR("Could not find available address after %d attempts", PSHELL_MAX_BIND_ATTEMPTS)
    PSHELL_ERROR("Cannot bind to UNIX socket: %s", _serverName);
  }
  else
  {
    /* IP socket */
    for (unsigned attempt = 1; attempt <= PSHELL_MAX_BIND_ATTEMPTS+1; attempt++)
    {
      _localIpAddress.sin_port = htons(port);
      if (bind(_socketFd,
               (struct sockaddr *) &_localIpAddress,
               sizeof(_localIpAddress)) < 0)
      {
        if (attempt == 1)
        {
          PSHELL_WARNING("Could not bind to requested port: %d, looking for first available port", _port);
        }
        port = _port + attempt;
      }
      else
      {
        _port = port;
        if (_serverType == PSHELL_UDP_SERVER)
        {
          sprintf(_lockFile, "%s%s-udp-%s-%d%s", _fileSystemPath, _serverName, _hostnameOrIpAddr, _port, _lockFileExtension);
        }
        else  /* TCP server */
        {
          sprintf(_lockFile, "%s%s-tcp-%s-%d%s", _fileSystemPath, _serverName, _hostnameOrIpAddr, _port, _lockFileExtension);
        }
        if ((unixLockFd = open(_lockFile, O_RDONLY | O_CREAT, 0600)) > -1)
        {
          flock(unixLockFd, LOCK_EX | LOCK_NB);
        }
        return (true);
      }
    }
    PSHELL_ERROR("Could not find available port after %d attempts", PSHELL_MAX_BIND_ATTEMPTS);
    PSHELL_ERROR("Cannot bind to socket: address: %s, port: %d",
                 inet_ntoa(_localIpAddress.sin_addr),
                 _port);
    return (false);
  }
  return (false);
}

/******************************************************************************/
/******************************************************************************/
static bool createSocket(void)
{
  char requestedHost[180];
  struct hostent *host;
  int on = 1;

  /* open our source socket */
  if (_serverType == PSHELL_UDP_SERVER)
  {
    if ((_socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      PSHELL_ERROR("Cannot create UDP socket");
      return (false);
    }
  }
  else if (_serverType == PSHELL_UNIX_SERVER)
  {
    if ((_socketFd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
      PSHELL_ERROR("Cannot create UNIX socket");
      return (false);
    }
  }
  else   /* TCP server */
  {
    if ((_socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      PSHELL_ERROR("Cannot create TCP socket");
      return (false);
    }
  }

  if (_serverType == PSHELL_UNIX_SERVER)
  {
    if (!bindSocket())
    {
      return (false);
    }
  }
  else
  {
    _localIpAddress.sin_family = AF_INET;
    _localIpAddress.sin_port = htons(_port);

    /* see if they supplied an IP address */
    if (inet_aton(_hostnameOrIpAddr, &_localIpAddress.sin_addr) == 0)
    {
      strcpy(requestedHost, _hostnameOrIpAddr);
      if (pshell_isEqual(requestedHost, PSHELL_ANYHOST))
      {
        _localIpAddress.sin_addr.s_addr = htonl(INADDR_ANY);
      }
      else if (pshell_isEqual(requestedHost, PSHELL_LOCALHOST))
      {
        _localIpAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
        strcpy(_ipAddress, "127.0.0.1");
      }
      else
      {
        /* see if they are requesting our local host address */
        if (pshell_isEqual(requestedHost, PSHELL_MYHOST))
        {
          gethostname(requestedHost, sizeof(requestedHost));
        }

        if ((host = gethostbyname(requestedHost)) != NULL)
        {
          memcpy(&_localIpAddress.sin_addr.s_addr,
                 *host->h_addr_list,
                 sizeof(_localIpAddress.sin_addr.s_addr));
        }
        else if ((_localIpAddress.sin_addr.s_addr = getIpAddress("eth0", _ipAddress)) == 0)
        {
          PSHELL_ERROR("Could not resolve local hostname: '%s'", requestedHost);
          return (false);
        }
      }
    }

    if (_serverType == PSHELL_TCP_SERVER)
    {
      setsockopt(_socketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    }
    else if (_serverType == PSHELL_UDP_SERVER)
    {
      /* see if they have supplied a broadcast address */
      /* set to true so we can call pshell_tokenize, need to make sure we call cleanupTokens after each call */
      _isCommandDispatched = true;
      PshellTokens *ipAddrOctets = pshell_tokenize(requestedHost, ".");
      if (strcmp(requestedHost, PSHELL_ANYBCAST) == 0)
      {
        _localIpAddress.sin_addr.s_addr = inet_addr("255.255.255.255");
        strcpy(_ipAddress, "255.255.255.255");
        /* setup socket for broadcast */
        setsockopt(_socketFd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
      }
      else if ((ipAddrOctets->numTokens == 4) && (strcmp(ipAddrOctets->tokens[3], "255") == 0))
      {
        /* setup socket for broadcast */
        setsockopt(_socketFd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
      }
      /* set back to false so we prevent a call to the pshell_tokenize function outside of a callback function */
      _isCommandDispatched = false;
      cleanupTokens();
    }

    if (!bindSocket())
    {
      return (false);
    }

    if ((_serverType == PSHELL_TCP_SERVER) && (listen(_socketFd, 1) < 0))
    {
      PSHELL_ERROR("Cannot listen on socket");
      return (false);
    }

  }

  if (_serverType == PSHELL_UNIX_SERVER)
  {
    /* for a UNIX server the _ipAddress value is set to 'unix' for display purposes only */
    sprintf(_interactivePrompt, "%s[%s]:%s", _serverName, _ipAddress, _prompt);
  }
  else
  {
    sprintf(_interactivePrompt, "%s[%s:%d]:%s", _serverName, _ipAddress, _port, _prompt);
  }

  return (true);

}

/******************************************************************************/
/******************************************************************************/
static bool checkForWhitespace(const char *string_)
{
  unsigned i;
  for (i = 0; i < pshell_getLength(string_); i++)
  {
    if (isspace(string_[i]))
    {
      return (true);
    }
  }
  return (false);
}

/******************************************************************************/
/******************************************************************************/
static void processQueryVersion(void)
{
  pshell_printf("%d", PSHELL_VERSION);
}

/******************************************************************************/
/******************************************************************************/
static void processQueryPayloadSize(void)
{
  pshell_printf("%d", _pshellPayloadSize);
}

/******************************************************************************/
/******************************************************************************/
static void processQueryName(void)
{
  pshell_printf("%s", _serverName);
}

/******************************************************************************/
/******************************************************************************/
static void processQueryTitle(void)
{
  pshell_printf("%s", _title);
}

/******************************************************************************/
/******************************************************************************/
static void processQueryBanner(void)
{
  pshell_printf("%s", _banner);
}

/******************************************************************************/
/******************************************************************************/
static void processQueryPrompt(void)
{
  pshell_printf("%s", _prompt);
}

/******************************************************************************/
/******************************************************************************/

/*
 * called when pshell client runs in interactive mode, the client needs
 * the command list for its TAB completion function, it is much easier to
 * parse a list of just the command names with a simple delimeter rather
 * than try to parse the output that is returned from the processQueryCommands1
 * function in order to pick out just the command names
 */
static void processQueryCommands2(void)
{
  unsigned entry;
  for (entry = 0; entry < _numCommands; entry++)
  {
    pshell_printf("%s%s", _commandTable[entry].command, PSHELL_COMMAND_DELIMETER);
  }
}

/******************************************************************************/
/******************************************************************************/

/*
 * called when user types 'help' or '?' in
 * interactive mode and '-h' in command line mode
 */
static void processQueryCommands1(void)
{
  unsigned entry;
  unsigned pad;

  for (entry = 0; entry < _numCommands; entry++)
  {
    pshell_printf("%s", _commandTable[entry].command);
    for (pad = pshell_getLength(_commandTable[entry].command);
         pad < _maxCommandLength;
         pad++)
    {
      pshell_printf(" ");
    }
    pshell_printf("  -  %s\n", _commandTable[entry].description);
  }

  pshell_printf("\n");

}

/******************************************************************************/
/******************************************************************************/
static char *createArgs(char *command_)
{
  char *command = NULL;
  PshellTokens *args = pshell_tokenize(command_, " ");

  _showElapsedTime = pshell_isEqual(args->tokens[0], "-t");
  _argc = 0;
  if (args->numTokens > 0)
  {
    if (_showElapsedTime && (args->numTokens > 1))
    {
      args->numTokens--;
      command = args->tokens[1];
      pshell_origCommandKeyword = command;
      _argc = args->numTokens-PSHELL_BASE_ARG_OFFSET;
      _argv = &args->tokens[PSHELL_BASE_ARG_OFFSET+1];
    }
    else
    {
      command = args->tokens[0];
      pshell_origCommandKeyword = command;
      _argc = args->numTokens-PSHELL_BASE_ARG_OFFSET;
      _argv = &args->tokens[PSHELL_BASE_ARG_OFFSET];
    }
  }
  return (command);
}

/******************************************************************************/
/******************************************************************************/
static bool allocateTokens(void)
{
  void *ptr;

  if (_tokenList[_numTokenLists]._public.numTokens >= _tokenList[_numTokenLists].maxTokens)
  {
    if ((ptr = realloc(_tokenList[_numTokenLists]._public.tokens,
        (_tokenList[_numTokenLists].maxTokens+PSHELL_TOKEN_CHUNK)*sizeof(char*))) == NULL)
    {
      PSHELL_ERROR("Could not allocate memory to expand tokens, size: %d",
                   (int)((_tokenList[_numTokenLists].maxTokens+PSHELL_TOKEN_CHUNK)*sizeof(char*)));
      return (false);
    }
    _tokenList[_numTokenLists]._public.tokens = (char**)ptr;
    _tokenList[_numTokenLists].maxTokens += PSHELL_TOKEN_CHUNK;
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
static void cleanupTokens(void)
{
  unsigned tokenList;

  for (tokenList = 0; tokenList < _numTokenLists; tokenList++)
  {
    _tokenList[tokenList]._public.numTokens = 0;
    free(_tokenList[tokenList].string);
  }
  _numTokenLists = 0;
}

/******************************************************************************/
/******************************************************************************/
static unsigned findCommand(char *command_)
{
  unsigned entry;
  unsigned numMatches = 0;
  unsigned length = strlen(command_);
  _foundCommand = NULL;
  if (pshell_isEqual(command_, "?") ||
      pshell_isEqual(command_, "-h") ||
      pshell_isEqual(command_, "-help") ||
      pshell_isEqual(command_, "--help"))
  {
    if ((_serverType != PSHELL_UDP_SERVER) && (_serverType != PSHELL_UNIX_SERVER))
    {
      /* the position of the "help" command  */
      _foundCommand = &_helpCmd;
      numMatches = 1;
    }
  }
  else if ((_serverType == PSHELL_NO_SERVER) && pshell_isEqual(command_, "--setup"))
  {
    /* command to setup our busybox like softlinks based on our command names  */
    _foundCommand = &_setupCmd;
    numMatches = 1;
  }
  else
  {
    for (entry = 0; entry < _numCommands; entry++)
    {
      /* see if we have a substring match, we do this for command abbreviation */
      if (pshell_isSubString(command_, _commandTable[entry].command, strlen(command_)))
      {
        /* set our _foundCommand pointer */
        _foundCommand = &_commandTable[entry];
        numMatches++;
        /* if we have an exact match, take it and break out */
        if (_foundCommand->length == length)
        {
          numMatches = 1;
          break;
        }
      }
    }
  }
  return (numMatches);
}

/******************************************************************************/
/******************************************************************************/
static void getElapsedTime(void)
{
  uint64_t elapsedTimeUsec;
  gettimeofday(&_currTime, NULL);
  elapsedTimeUsec = (_currTime.tv_sec-_startTime.tv_sec)*1000000 +
                    (_currTime.tv_usec-_startTime.tv_usec);
  _elapsedTime.tv_sec = elapsedTimeUsec/1000000;
  _elapsedTime.tv_usec = elapsedTimeUsec%1000000;
}

/******************************************************************************/
/******************************************************************************/
static void dispatchCommand(char *command_)
{
  gettimeofday(&_startTime, NULL);
  if (_showElapsedTime)
  {
    pshell_printf("PSHELL_INFO: Measuring elapsed time for command: '%s'...\n", &command_[3]);
  }
  _foundCommand->function(_argc, _argv);
  if (_showElapsedTime)
  {
    getElapsedTime();
    pshell_printf("PSHELL_INFO: Command: '%s', elapsed time: %02d:%02d:%02d.%06ld\n",
                  &command_[3],
                  _elapsedTime.tv_sec/3600,
                  (_elapsedTime.tv_sec%3600)/60,
                  _elapsedTime.tv_sec%60,
                  (long)_elapsedTime.tv_usec);
  }
}

/******************************************************************************/
/******************************************************************************/
static void processCommand(char *command_)
{
  unsigned numMatches = 1;
  char savedCommand[180];
  char fullCommand[180];
  char retCode = PSHELL_COMMAND_COMPLETE;
  char *commandName = fullCommand;  /* make a non NULL ptr so we don't get "cannot
                                       create args" error for the non-user commands */
  unsigned payloadSize = _pshellPayloadSize;  /* save off current size in case it changes
                                                 and we need to grab more memory, UDP server only */
  PshellMsg updatePayloadSize;

  /* save off our original command */
  strcpy(savedCommand, command_);
  strcpy(fullCommand, command_);

  /*
   * clear out our message payload so all of our pshell_printf
   * commands start at the beginning of the buffer
   */
  _pshellMsg->payload[0] = 0;

  _isControlCommand = false;

  /* figure out our message type and process accordingly */
  if (_pshellMsg->header.msgType == PSHELL_USER_COMMAND)
  {
    /* set this to true so we can tokenize our command line with pshell_tokenize */
    _isCommandDispatched = true;
    /* user command, create the arg list and look for the command */
    if (((commandName = createArgs(fullCommand)) != NULL) &&
        ((numMatches = findCommand(commandName)) == 1))
    {
      /* see if they asked for the command usage */
      if (pshell_isHelp() && _foundCommand->showUsage)
      {
        pshell_showUsage();
      }
      else if (((_argc >= _foundCommand->minArgs) &&
                (_argc <= _foundCommand->maxArgs)) ||
                (pshell_isHelp() && !_foundCommand->showUsage))
      {
        /*
         * dispatch user command, don't need to check for a NULL
         * function pointer because the validation in the addCommand
         * function will catch that and not add the command
         */
        dispatchCommand(savedCommand);
      }
      else
      {
        /* arg count validation failure, show the usage */
        pshell_showUsage();
      }
    }
  }
  else if (_pshellMsg->header.msgType == PSHELL_CONTROL_COMMAND)
  {
    retCode = PSHELL_COMMAND_SUCCESS;
    /*
     * if the caller does not need any data back, we set this value to
     * false which will short circuit any calls to pshell_printf
     */
    _isCommandInteractive = _pshellMsg->header.dataNeeded;
    /* set this to true so we can tokenize our command line with pshell_tokenize */
    _isCommandDispatched = true;
    /* set the to true so we prevent any intermediate flushes back to the control client */
    _isControlCommand = true;
    /* user command, create the arg list and look for the command */
    if (((commandName = createArgs(fullCommand)) != NULL) &&
        ((numMatches = findCommand(commandName)) == 1))
    {
      /* see if they asked for the command usage */
      if (pshell_isHelp() && _foundCommand->showUsage)
      {
        pshell_showUsage();
      }
      else if (((_argc >= _foundCommand->minArgs) &&
                (_argc <= _foundCommand->maxArgs)) ||
                (pshell_isHelp() && !_foundCommand->showUsage))
      {
        /*
         * dispatch user command, don't need to check for a NULL
         * function pointer because the validation in the addCommand
         * function will catch that and not add the command
         */
        dispatchCommand(savedCommand);
      }
      else
      {
        /* arg count validation failure, show the usage */
        pshell_showUsage();
        retCode = PSHELL_COMMAND_INVALID_ARG_COUNT;
      }
    }
    else
    {
      /* command not found, set our return code */
      retCode = PSHELL_COMMAND_NOT_FOUND;
    }
  }
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_VERSION)
  {
    processQueryVersion();
  }
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_PAYLOAD_SIZE)
  {
    processQueryPayloadSize();
  }
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_NAME)
  {
    processQueryName();
  }
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_TITLE)
  {
    processQueryTitle();
  }
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_BANNER)
  {
    processQueryBanner();
  }
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_PROMPT)
  {
    processQueryPrompt();
  }
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_COMMANDS1)
  {
    processQueryCommands1();
  }
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_COMMANDS2)
  {
    processQueryCommands2();
  }
  else
  {
    pshell_printf("PSHELL_ERROR: Unknown msgType: %d\n", _pshellMsg->header.msgType);
  }

  if (commandName == NULL)
  {
    pshell_printf("PSHELL_ERROR: Could not create args list for command: '%s'\n", savedCommand);
  }
  else if (numMatches == 0)
  {
    pshell_printf("PSHELL_ERROR: Command: '%s' not found\n", commandName);
  }
  else if (numMatches > 1)
  {
    pshell_printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n", commandName);
  }

  /* set back to interactive mode */
  _isCommandInteractive = true;

  /* see if we need to update the client's payload size */
  if (_pshellPayloadSize > payloadSize)
  {
    updatePayloadSize.header.msgType = PSHELL_UPDATE_PAYLOAD_SIZE;
    sprintf(updatePayloadSize.payload, "%d", _pshellPayloadSize);
    reply(&updatePayloadSize);
  }

  /* now reply with our actual response */
  _pshellMsg->header.msgType = retCode;
  reply(_pshellMsg);

  /*
   * set this to false to be sure nobody calls pshell_tokenize outside the context
   * of an pshell callback function because the cleanupTokens must be called to
   * avoid any memory leaks
   */
  _isCommandDispatched = false;
  cleanupTokens();

}

/******************************************************************************/
/******************************************************************************/
static void receiveUDP(void)
{
  int    fromAddrLen = sizeof(_fromIpAddress);
  int    receivedSize;
  fd_set readFd;

  FD_ZERO (&readFd);
  FD_SET(_socketFd, &readFd);

  /* wait forever for data on our socket */
  if ((select(_socketFd+1, &readFd, NULL, NULL, NULL)) < 0)
  {
    PSHELL_ERROR("Error on socket select");
  }

  if (FD_ISSET(_socketFd, &readFd))
  {

    /* read the command request */
    if ((receivedSize = recvfrom(_socketFd,
                                 (char*)_pshellMsg,
                                 _pshellPayloadSize+PSHELL_HEADER_SIZE,
                                 0,
                                 (struct sockaddr *) &_fromIpAddress,
                                 (socklen_t *) &fromAddrLen)) < 0)
    {
      PSHELL_ERROR("Data receive error from remote pshellClient");
    }
    else
    {

      /* if we are currently processing a non-interactive command (via the pshell_runCommand
       * function call), wait a little bit for it to complete before processing an interactive command */
      while (!_isCommandInteractive) sleep(1);

      /* make sure our command is NULL terminated */
      _pshellMsg->payload[receivedSize-PSHELL_HEADER_SIZE] = 0;

      /* good to go, process an interactive command */
      processCommand(_pshellMsg->payload);

    }
  }

}

/******************************************************************************/
/******************************************************************************/
static void receiveUNIX(void)
{
  int    fromAddrLen = sizeof(_fromUnixAddress);
  int    receivedSize;
  fd_set readFd;

  FD_ZERO (&readFd);
  FD_SET(_socketFd, &readFd);

  /* wait forever for data on our socket */
  if ((select(_socketFd+1, &readFd, NULL, NULL, NULL)) < 0)
  {
    PSHELL_ERROR("Error on socket select");
  }

  if (FD_ISSET(_socketFd, &readFd))
  {

    /* read the command request */
    if ((receivedSize = recvfrom(_socketFd,
                                 (char*)_pshellMsg,
                                 _pshellPayloadSize+PSHELL_HEADER_SIZE,
                                 0,
                                 (struct sockaddr *) &_fromUnixAddress,
                                 (socklen_t *) &fromAddrLen)) < 0)
    {
      PSHELL_ERROR("Data receive error from remote pshellClient");
    }
    else
    {

      /* if we are currently processing a non-interactive command (via the pshell_runCommand
       * function call), wait a little bit for it to complete before processing an interactive command */
      while (!_isCommandInteractive) sleep(1);

      /* make sure our command is NULL terminated */
      _pshellMsg->payload[receivedSize-PSHELL_HEADER_SIZE] = 0;

      /* good to go, process an interactive command */
      processCommand(_pshellMsg->payload);

    }
  }

}

/******************************************************************************/
/******************************************************************************/
static void replyUDP(PshellMsg *pshellMsg_)
{
  if ((pshellMsg_->header.respNeeded) &&
      (sendto(_socketFd,
             (char*)pshellMsg_,
             pshell_getLength(pshellMsg_->payload)+PSHELL_HEADER_SIZE+1,
             0,
             (struct sockaddr *) &_fromIpAddress,
             sizeof(_fromIpAddress)) < 0))
  {
    PSHELL_ERROR("Not all data sent to pshellClient");
  }
}

/******************************************************************************/
/******************************************************************************/
static void replyUNIX(PshellMsg *pshellMsg_)
{
  if ((pshellMsg_->header.respNeeded) &&
      (sendto(_socketFd,
             (char*)pshellMsg_,
             pshell_getLength(pshellMsg_->payload)+PSHELL_HEADER_SIZE+1,
             0,
             (struct sockaddr *) &_fromUnixAddress,
             sizeof(_fromUnixAddress)) < 0))
  {
    PSHELL_ERROR("Not all data sent to pshellClient");
  }
}

/******************************************************************************/
/******************************************************************************/
static void reply(PshellMsg *pshellMsg_)
{
  if (_serverType == PSHELL_UDP_SERVER)
  {
    replyUDP(pshellMsg_);
  }
  else if (_serverType == PSHELL_UNIX_SERVER)
  {
    replyUNIX(pshellMsg_);
  }
}

/******************************************************************************/
/******************************************************************************/
static void _printf(const char *format_, ...)
{
  char outputString[300];
  va_list args;
  va_start(args, format_);
  vsprintf(outputString, format_, args);
  va_end(args);
  if (_logFunction == NULL)
  {
    printf("%s", outputString);
  }
  else
  {
    (*_logFunction)(outputString);
  }
}
