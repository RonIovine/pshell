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

#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <PshellCommon.h>
#include <PshellServer.h>

/* constants */

/*
 * change the default window title bar, welcome banner, and prompt to suit,
 * your product/application, note that these can also be changed on a per-server
 * basis at startup time via the pshell-server.conf file
 */

/* the full window title format is 'title: shellName[ipAddress]' */
static const char *_defaultTitle  = "PSHELL";
/* this is the first line of the welcome banner */
static const char *_defaultBanner = "PSHELL: Process Specific Embedded Command Line Shell";
/* the full prompt format is 'shellName[ipAddress]:prompt' */
static const char *_defaultPrompt = "PSHELL> ";
/* the default idle session timeout for TCP sessions in minutes */
static int _defaultIdleTimeout = 10;

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
#ifndef PSHELL_CONFIG_DIR
#define PSHELL_CONFIG_DIR "/etc/pshell"
#endif

/*
 * This is the size of the transfer buffer used to communicate between the
 * pshell server and client.  This is the initial buffer size as well as the
 * chunk size the buffer will grow by in the event of a buffer overflow when
 * the server compile flag PSHELL_GROW_PAYLOAD_IN_CHUNKS is set.
 *
 * If that compile flag is NOT set, the behavour of the transfer buffer upon
 * an overflow is dependent on the setting of the VSNPRINTF compile flag as
 * described below:
 *
 * VSNPRINTF is set:
 *
 * The buffer will grow in the EXACT amount needed in the event of an overflow
 * (note, this is only for glibc 2.1 and higher, for glibc 2.0, we grow by the
 * chunk size because in the 2.0 'C' library the "vsnprintf" function does not
 * support returning the overflow size).
 *
 * VSNPRINTF is NOT set:

 * The buffer will be flushed to the client upon overflow.  Note that care must
 * be taken when setting the PSHELL_PAYLOAD_GUARDBAND value when operating in this
 * mode.
 */

#ifndef PSHELL_PAYLOAD_CHUNK
#define PSHELL_PAYLOAD_CHUNK 4000   /* bytes */
#endif

/*
 * if we are not using "vsnprintf" to format and populate the transfer buffer,
 * we must specify a guardband to detect a potential buffer overflow condition,
 * this value must be smaller than the above payload chunk size, but must be
 * larger than the maximum expected data size for and given SINGLE "pshell_printf"
 * call, otherwise we risk an undetected buffer overflow condition and subsequent
 * memory corruption
 */

#ifndef VSNPRINTF
#ifndef PSHELL_PAYLOAD_GUARDBAND
#define PSHELL_PAYLOAD_GUARDBAND 400   /* bytes */
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
enum PshellControlResults
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
};

/*
 * structure used to tokenize command line arguments, this is
 * setup as a pseudo "class" in which only the "public" data
 * member (PshellTokens) is returned to the caller, while the "private"
 * data members (maxTokens, string) are just kept for internal
 * token management
 */

struct PshellTokens_c
{
  PshellTokens _public;  /* returned to the caller of "pshell_tokenize" */
  unsigned maxTokens;  /* used for internal token management */
  char *string;        /* used for internal token management */
};

/*************************
 * private "member" data
 *************************/

/*
 * coding convention is leading underscore for "member" data,
 * trailing underscore for function arguments, and no leading
 * or trailing underscores for local stack variables
 */

/* LOCAL server data */

#ifdef READLINE
bool _commandFound;
int _matchLength;
unsigned _commandPos;
bool _completionEnabled;
#endif

/* UDP/UNIX server data */

static struct sockaddr_in _fromIpAddress;
static struct sockaddr_un _fromUnixAddress;
static const char *_wheel = "|/-\\";
static unsigned _wheelPos = 0;
static bool _isControlCommand = false;

/* TCP server data */

#define PSHELL_MAX_HISTORY 256
#define PSHELL_MAX_LINE_WORDS 128
#define PSHELL_MAX_LINE_LENGTH 180
#define PSHELL_MAX_SCREEN_WIDTH 80

#define CR  "\r"
#define LF  "\n"
#define BS  "\b"
#define BEL "\a"

#ifndef CTRL
#define CTRL(c) (c - '@')
#endif

/* free and zero (to avoid double-free) */
#define free_z(p) do { if (p) { free(p); (p) = 0; } } while (0)

#define PSHELL_MAX_TOKENS 64
struct Tokens
{
  char *tokens[PSHELL_MAX_TOKENS];
  int numTokens;
};

static char *_history[PSHELL_MAX_HISTORY];
static bool _showPrompt;
static FILE *_clientFd = stdout;
static int _connectFd;
static struct timeval _idleTimeout;
static bool _quit = false;
static char _interactivePrompt[80];
static unsigned _maxTabCommandLength = 0;
static unsigned _maxTabColumns = 0;

/* common data (for UDP, TCP and LOCAL servers */

#ifdef PSHELL_INCLUDE_COMMAND_IN_ARGS_LIST
#define PSHELL_BASE_ARG_OFFSET 0
#else
#define PSHELL_BASE_ARG_OFFSET 1
#endif

static bool _isRunning = false;
static bool _isCommandDispatched = false;
static bool _isCommandInteractive = true;

static PshellLogFunction _logFunction = NULL;
static unsigned _logLevel = PSHELL_LOG_LEVEL_DEFAULT;

static int _socketFd;
static struct sockaddr_in _localIpAddress;
static struct sockaddr_un _localUnixAddress;

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
static unsigned _maxCommandLength = 0;

static PshellTokens _dummyTokens;
static PshellTokens_c *_tokenList = NULL;
static unsigned _maxTokenLists = 0;
static unsigned _numTokenLists = 0;
static int _argc;
static char **_argv;

/****************************************
 * private "member" function prototypes
 ****************************************/

/* LOCAL server functions */

static void runLocalServer(void);
static void stripWhitespace(char *string_);
static void processBatchFile(char *filename_, unsigned rate_, unsigned repeat_, bool clear_);
static void clearScreen(void);

#ifdef READLINE
char **commandCompletion (const char *command_, int start_, int end_);
char *commandGenerator (const char *command_, int state_);
#endif

/* UDP/UNIX server functions */

static void runUDPServer(void);
static void receiveUDP(void);
static void replyUDP(PshellMsg *pshellMsg_);
static void runUNIXServer(void);
static void receiveUNIX(void);
static void replyUNIX(PshellMsg *pshellMsg_);
static void processQueryVersion(void);
static void processQueryPayloadSize(void);
static void processQueryName(void);
static void processQueryIpAddress(void);
static void processQueryTitle(void);
static void processQueryBanner(void);
static void processQueryPrompt(void);
static void processQueryCommands1(void);
static void processQueryCommands2(void);
static void reply(PshellMsg *pshellMsg_);

/* TCP server functions */

static void runTCPServer(void);
static void receiveTCP(void);
static void writeSocket(const char *string_, unsigned length_);
static int findCompletions(char *command_, char *completions_[]);
static void clearLine(char *command_, int length_, int cursor_);
static void addHistory(char *command_);
static void clearHistory(void);
static void showPrompt(char *command_);
static void tokenize(char *string_, struct Tokens *tokens_, const char *delimiter_);
static void addHistory(char *command_);

/* common functions (TCP and LOCAL servers) */

static void showWelcome(void);

/* common functions (UDP and TCP servers) */

static int getIpAddress(const char *interface_, char *ipAddress_);
static bool createSocket(void);

/* common functions (UDP, TCP and LOCAL servers) */

static void loadConfigFile(void);
static bool checkForWhitespace(const char *string_);
static char *createArgs(char *command_);
static unsigned findCommand(char *command_);
static void processCommand(char *command_);
static bool allocateTokens(void);
static void cleanupTokens(void);
static void *serverThread(void*);
static void runServer(void);

/* native callback commands (TCP and LOCAL server only) */

static void help(int argc, char *argv[]);
static void quit(int argc, char *argv[]);

/* native callback commands (LOCAL server only) */

static void batch(int argc, char *argv[]);

/* output display macros */
static void _printf(const char *format_, ...);
#define PSHELL_ERROR(format_, ...) if (_logLevel >= PSHELL_LOG_LEVEL_1) {_printf("PSHELL_ERROR: " format_, ##__VA_ARGS__);_printf("\n");}
#define PSHELL_WARNING(format_, ...) if (_logLevel >= PSHELL_LOG_LEVEL_2) {_printf("PSHELL_WARNING: " format_, ##__VA_ARGS__);_printf("\n");}
#define PSHELL_INFO(format_, ...) if (_logLevel >= PSHELL_LOG_LEVEL_3) {_printf("PSHELL_INFO: " format_, ##__VA_ARGS__);_printf("\n");}

/**************************************
 * public API "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void pshell_setLogLevel(unsigned level_)
{
  _logLevel = level_;
}

/******************************************************************************/
/******************************************************************************/
void pshell_registerLogFunction(PshellLogFunction logFunction_)
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

  /* perform some command validation */

  /* see if we have a NULL command name */
  if (command_ == NULL)
  {
    PSHELL_ERROR("NULL command name, command not added");
    return;
  }

  /* see if we have a NULL description */
  if (description_ == NULL)
  {
    PSHELL_ERROR("NULL description, command: '%s' not added", command_);
    return;
  }

  /* see if we have a NULL function */
  if (function_ == NULL)
  {
    PSHELL_ERROR("NULL function, command: '%s' not added", command_);
    return;
  }

  /* see if they provided a usage for a function with no arguments */
  if ((minArgs_ == 0) && (maxArgs_ == 0) && (usage_ != NULL))
  {
    PSHELL_ERROR("Usage provided for function that takes no arguments, command: '%s' not added",
                 command_);
    return;
  }

  /* see if they provided no usage for a function with arguments */
  if (((maxArgs_ > 0) || (minArgs_ > 0)) && (usage_ == NULL))
  {
    PSHELL_ERROR("NULL usage for command that takes arguments, command: '%s' not added",
                 command_);
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
    return;
  }

  /* see if we have a whitespace in the command name */
  if (checkForWhitespace(command_))
  {
    PSHELL_ERROR("Whitespace found, command: '%s' not added", command_);
    return;
  }

  /*
   * see if the command name is a duplicate of one of the
   * native UDP interactive commands, only give a warning because
   * we can still use the command in command line mode
   */
  if (((_serverType == PSHELL_UDP_SERVER) || (_serverType == PSHELL_UNIX_SERVER)) &&
      (pshell_isEqual(command_, "help") ||
       pshell_isEqual(command_, "quit") ||
       pshell_isEqual(command_, "batch")))
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
      return;
    }

    /* check for duplicate function pointers */
    if (_commandTable[entry].function == function_)
    {
      PSHELL_ERROR("Duplicate function found, command: '%s' not added", command_);
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
      return;
    }
    /* success, update our pointer and allocated size */
    _commandTableSize += PSHELL_COMMAND_CHUNK;
    _commandTable = (PshellCmd*)ptr;
  }

#ifdef PSHELL_COPY_ADD_COMMAND_STRINGS

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
      free(_commandTable[_numCommands].command);
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
    free(_commandTable[_numCommands].command);
    free(_commandTable[_numCommands].usage);
    return;
  }

#else

  /* just set the pointers but don't clone the strings, this will use less memory */
  _commandTable[_numCommands].command = command_;
  _commandTable[_numCommands].description = description_;
  _commandTable[_numCommands].usage = usage_;

#endif

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
  _numCommands++;

  /* figure out our max command length so the help display can be aligned */
  if (pshell_getLength(command_) > _maxCommandLength)
  {
    _maxCommandLength = pshell_getLength(command_);
  }

}

/******************************************************************************/
/******************************************************************************/
void pshell_runCommand(const char *command_, ...)
{
  char *commandName;
  char fullCommand[256];
  char command[256];
  /* only dispatch command if we are not already in the middle of
   * dispatching an interactive command
   */
  if (!_isCommandDispatched)
  {
    /* create the command fro our varargs */
    va_list args;
    va_start(args, command_);
    vsprintf(command, command_, args);
    va_end(args);
    /* set this to false so we can short circuit any calls to pshell_printf since
     * there is no client side program to receive the output */
    _isCommandInteractive = false;
    /* set this to true so we can tokenize our command line with pshell_tokenize */
    _isCommandDispatched = true;
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
    pshell_printf("\r%s%c", string_, _wheel[(_wheelPos++)%sizeof(_wheel)]) :
    pshell_printf("\r%c", _wheel[(_wheelPos++)%sizeof(_wheel)]);
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
  char *printPos;
  char *newline;

  /* if we called a command non-interactively, just bail because there is no
   * client side program (either pshell or telnet) to receive the output
   */
  if (_isCommandInteractive)
  {
    if ((_serverType == PSHELL_UDP_SERVER) || (_serverType == PSHELL_UNIX_SERVER))
    {
      /* don't do any intermediate flushed if this command was from a control client */
      if (!_isControlCommand)
      {
        reply(_pshellMsg);
        /* clear out buffer for next time */
        _pshellMsg->payload[0] = 0;
      }
    }
    else  /* TCP or local server */
    {

      printPos = _pshellMsg->payload;

      do
      {
        /* for every newline we need to insert a CR/LF in its place */
        if ((newline = strchr(printPos, '\n')) != NULL)
        {
          *newline++ = 0;
          fprintf(_clientFd, "%s\r\n", printPos);
        }
        else
        {
          fprintf(_clientFd, "%s", printPos);
        }
        printPos = newline;
      } while (printPos);

      /* clear out buffer for next time */
      _pshellMsg->payload[0] = '\0';

    }
  }
}

/******************************************************************************/
/******************************************************************************/
void pshell_printf(const char *format_, ...)
{

  /* if we called a command non-interactively, just bail because there is no
   * pshell client side program to receive the output
   */
  if (!_isCommandInteractive || !_isRunning)
  {
    return;
  }

  va_list args;
  int offset = pshell_getLength(_pshellMsg->payload);
  char *address = &_pshellMsg->payload[offset];

#ifndef VSNPRINTF

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

#else  /* ifndef VSNPRINTF */

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
#endif  /* ifndef VSNPRINTF */

  /* if we are running a TCP server, flush the output to the socket */
  if ((_serverType == PSHELL_TCP_SERVER) || (_serverType == PSHELL_LOCAL_SERVER))
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
  PshellCmd helpCmd;
  PshellCmd quitCmd;
  PshellCmd batchCmd;
  int numNativeCommands = 2;
  int i;

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
  else if ((serverType_ != PSHELL_LOCAL_SERVER) && ((port_ == 0) || (hostnameOrIpAddr_ == NULL)))
  {
    PSHELL_ERROR("Non-local server, must supply valid IP/hostname and port");
  }
  else if (!_isRunning)
  {

    /* allocate our transfer buffer */
    if ((_pshellMsg = (PshellMsg*)malloc(_pshellPayloadSize+PSHELL_HEADER_SIZE)) != NULL)
    {

      /* make sure we are only invoked once */
      _isRunning = true;
      strcpy(_serverName, serverName_);
      if (hostnameOrIpAddr_ != NULL)
      {
        strcpy(_hostnameOrIpAddr, hostnameOrIpAddr_);
      }
      strcpy(_title, _defaultTitle);
      strcpy(_banner, _defaultBanner);
      strcpy(_prompt, _defaultPrompt);
      _port = port_;
      _serverType = serverType_;
      _serverMode = serverMode_;
      _dummyTokens.numTokens = 0;

      /* load any config file */
      loadConfigFile();

      if ((_serverType != PSHELL_UDP_SERVER) && (_serverType != PSHELL_UNIX_SERVER))
      {
        /*
         * add our two built in commands for the TCP/LOCAL server,
         * for the UDP/UNIX server, these are implemented in the
         * stand-alone 'pshell' client program
         */
        pshell_addCommand(quit,
                          "quit",
                          "exit interactive mode",
                          NULL,
                          0,
                          0,
                          true);

        pshell_addCommand(help,
                          "help",
                          "show all available commands",
                          NULL,
                          0,
                          0,
                          true);

        /* move these commands to be first in the command list */

        /* save off the command info */
        helpCmd.function = _commandTable[_numCommands-1].function;
        helpCmd.command = _commandTable[_numCommands-1].command;
        helpCmd.usage = _commandTable[_numCommands-1].usage;
        helpCmd.description = _commandTable[_numCommands-1].description;
        helpCmd.minArgs = _commandTable[_numCommands-1].minArgs;
        helpCmd.maxArgs = _commandTable[_numCommands-1].maxArgs;
        helpCmd.showUsage = _commandTable[_numCommands-1].showUsage;

        quitCmd.function = _commandTable[_numCommands-2].function;
        quitCmd.command = _commandTable[_numCommands-2].command;
        quitCmd.usage = _commandTable[_numCommands-2].usage;
        quitCmd.description = _commandTable[_numCommands-2].description;
        quitCmd.minArgs = _commandTable[_numCommands-2].minArgs;
        quitCmd.maxArgs = _commandTable[_numCommands-2].maxArgs;
        quitCmd.showUsage = _commandTable[_numCommands-2].showUsage;

        if (_serverType == PSHELL_LOCAL_SERVER)
        {
          numNativeCommands = 3;
          pshell_addCommand(batch,
                          "batch",
                          "run commands from a batch file",
                          "<filename> [rate=<seconds] [repeat=<count>] [clear]",
                          1,
                          4,
                          false);
          batchCmd.function = _commandTable[_numCommands-1].function;
          batchCmd.command = _commandTable[_numCommands-1].command;
          batchCmd.usage = _commandTable[_numCommands-1].usage;
          batchCmd.description = _commandTable[_numCommands-1].description;
          batchCmd.minArgs = _commandTable[_numCommands-1].minArgs;
          batchCmd.maxArgs = _commandTable[_numCommands-1].maxArgs;
          batchCmd.showUsage = _commandTable[_numCommands-1].showUsage;
        }

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
        }

        /* restore the saved command info to positions 0 and 1 in the list */
        _commandTable[0].function = quitCmd.function;
        _commandTable[0].command = quitCmd.command;
        _commandTable[0].usage = quitCmd.usage;
        _commandTable[0].description = quitCmd.description;
        _commandTable[0].minArgs = quitCmd.minArgs;
        _commandTable[0].maxArgs = quitCmd.maxArgs;
        _commandTable[0].showUsage = quitCmd.showUsage;

        _commandTable[1].function = helpCmd.function;
        _commandTable[1].command = helpCmd.command;
        _commandTable[1].usage = helpCmd.usage;
        _commandTable[1].description = helpCmd.description;
        _commandTable[1].minArgs = helpCmd.minArgs;
        _commandTable[1].maxArgs = helpCmd.maxArgs;
        _commandTable[1].showUsage = helpCmd.showUsage;

        if (_serverType == PSHELL_LOCAL_SERVER)
        {
          _commandTable[2].function = batchCmd.function;
          _commandTable[2].command = batchCmd.command;
          _commandTable[2].usage = batchCmd.usage;
          _commandTable[2].description = batchCmd.description;
          _commandTable[2].minArgs = batchCmd.minArgs;
          _commandTable[2].maxArgs = batchCmd.maxArgs;
          _commandTable[2].showUsage = batchCmd.showUsage;
        }

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
    PSHELL_ERROR("pshellServer is already running");
  }

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
      if ((ptr = realloc(_tokenList, (_maxTokenLists+PSHELL_TOKEN_LIST_CHUNK)*sizeof(PshellTokens_c))) == NULL)
      {
        PSHELL_ERROR("Could not allocate memory to expand token list, size: %d",
                     (int)((_maxTokenLists+1)*sizeof(PshellTokens)));
        return (&_dummyTokens);
      }
      _tokenList = (PshellTokens_c*)ptr;
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
  const char *str;
  if ((strlen(string1_) < minChars_) || (strlen(string1_) > strlen(string2_)))
  {
    return (false);
  }
  else if (((str = strstr(string2_, string1_)) != NULL) && (str == string2_))
  {
    return (true);
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
bool pshell_isNumeric(const char *string_)
{
  return (pshell_isDec(string_) || pshell_isHex(string_));
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
bool pshell_isHex(const char *string_)
{
  unsigned i;
  if (pshell_getLength(string_) > 2)
  {
    if ((string_[0] == '0') && (tolower(string_[1]) == 'x'))
    {
      for (i = 2; i < pshell_getLength(string_); i++)
      {
        if (!isxdigit(string_[i]))
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
  else
  {
    return (false);
  }
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

/**********************************************
 * command line agrument conversion functions
 **********************************************/

/******************************************************************************/
/******************************************************************************/
long pshell_getLong(const char *string_)
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
    return (0);
  }
}

/******************************************************************************/
/******************************************************************************/
unsigned long pshell_getUnsignedLong(const char *string_)
{
  if (pshell_isDec(string_))
  {
    return (strtoul(string_, (char **)NULL, 10));
  }
  else if (pshell_isHex(string_))
  {
    return (strtoul(string_, (char **)NULL, 16));
  }
  else
  {
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
    return (0.0);
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_getOption(const char *string_, char *option_, char *value_)
{
  PshellTokens *option = pshell_tokenize(string_, "=");
  /* if 'option_' is NULL, we set the it to point to the option name
   * and always extract the value, if 'option_' is not NULL, we only
   * extract the value if the option matches the one they are looking
   * for, the format of the option can have two different forms,
   * -<option><value> type option where <option> is a single character
   * identifier or <option>=<value> where <option> can be any length
   * character string
   */
  if (strlen(option_) == 0)
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
      // they are looking for a <option>=<value> type option */
      if (option->numTokens == 2)
      {
        strcpy(option_, option->tokens[0]);
        strcpy(value_, option->tokens[1]);
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
      // assume they are looking for a <option>=<value> type option */
      if ((option->numTokens == 2) && pshell_isEqual(option->tokens[0], option_))
      {
        strcpy(value_, option->tokens[1]);
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
  return (pshell_isEqual(string_, "true"));
}

/******************************************************************************/
/******************************************************************************/
float pshell_getFloat(const char *string_)
{
  return ((float)pshell_getDouble(string_));
}

/******************************************************************************/
/******************************************************************************/
int pshell_getInt(const char *string_)
{
  return ((int)pshell_getLong(string_));
}

/******************************************************************************/
/******************************************************************************/
short pshell_getShort(const char *string_)
{
  return ((short)pshell_getLong(string_));
}

/******************************************************************************/
/******************************************************************************/
char pshell_getChar(const char *string_)
{
  return ((char)pshell_getLong(string_));
}

/******************************************************************************/
/******************************************************************************/
unsigned pshell_getUnsigned(const char *string_)
{
  return ((unsigned)pshell_getUnsignedLong(string_));
}

/******************************************************************************/
/******************************************************************************/
unsigned short pshell_getUnsignedShort(const char *string_)
{
  return ((unsigned short)pshell_getUnsignedLong(string_));
}

/******************************************************************************/
/******************************************************************************/
unsigned char pshell_getUnsignedChar(const char *string_)
{
  return ((unsigned char)pshell_getUnsignedLong(string_));
}

/************************************
 * private "member" function bodies
 ************************************/

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
  char filename[80];
  char *batchPath;
  unsigned count = 0;

  strcpy(filename, filename_);
  if ((batchPath = getenv("PSHELL_BATCH_DIR")) != NULL)
  {
    sprintf(batchFile, "%s/%s", batchPath, filename_);
  }
  if ((fp = fopen(batchFile, "r")) != NULL)
  {
    while ((repeat_ == 0) || (count < repeat_))
    {
      if (repeat_ != 0)
      {
        pshell_printf("\033]0;%s: %s[%s], Mode: BATCH[%s], Rate: %d SEC, Iteration: %d of %d\007", _title, _serverName, _ipAddress, filename, rate_, count+1, repeat_);
      }
      else
      {
        pshell_printf("\033]0;%s: %s[%s], Mode: BATCH[%s], Rate: %d SEC, Iteration: %d\007", _title, _serverName, _ipAddress, filename, rate_, count+1);
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
  else
  {
    PSHELL_ERROR("Could not open batch file: '%s'", batchFile);
  }
}

/******************************************************************************/
/******************************************************************************/
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

  while ((_commandPos < _numCommands) && (!_commandFound))
  {

    /* get our command string */
    command = _commandTable[_commandPos++].command;

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
static void stripWhitespace(char *string_)
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
static void runUDPServer(void)
{
  PSHELL_INFO("UDP Server: %s Started On Host: %s, Port: %d",
              _serverName,
              _hostnameOrIpAddr,
              _port);
  /* startup our UDP server */
  if (createSocket())
  {
    for (;;)
    {
      receiveUDP();
    }
  }
}

/******************************************************************************/
/******************************************************************************/
static void runTCPServer(void)
{
  PSHELL_INFO("TCP Server: %s Started On Host: %s, Port: %d",
              _serverName,
              _hostnameOrIpAddr,
              _port);
  /* startup our TCP server and accept new connections */
  while (createSocket() && (_connectFd = accept(_socketFd, NULL, 0)) > 0)
  {
    /* shutdown original socket to not allow any new connections until we are done with this one */
    shutdown(_socketFd, SHUT_RDWR);
    receiveTCP();
    shutdown(_connectFd, SHUT_RDWR);
  }
}

/******************************************************************************/
/******************************************************************************/
static void runUNIXServer(void)
{
  PSHELL_INFO("UNIX Server: %s Started", _serverName);
  /* startup our UNIX server */
  if (createSocket())
  {
    for (;;)
    {
      receiveUNIX();
    }
  }
}

/******************************************************************************/
/******************************************************************************/
static void runLocalServer(void)
{
  char inputLine[180];
  strcpy(_ipAddress, "local");
  sprintf(_interactivePrompt, "%s[%s]:%s", _serverName, _ipAddress, _prompt);
  showWelcome();

#ifdef READLINE
  char *commandLine;
  /* register our TAB completion function */
  rl_attempted_completion_function = commandCompletion;
#endif

  while (!_quit)
  {
    inputLine[0] = 0;
    _pshellMsg->header.msgType = PSHELL_USER_COMMAND;
    while (strlen(inputLine) == 0)
    {
#ifdef READLINE
      commandLine = NULL;
      _commandFound = false;
      commandLine = readline(_interactivePrompt);
      if (commandLine && *commandLine)
      {
        add_history(commandLine);
        strcpy(inputLine, commandLine);
        stripWhitespace(inputLine);
      }
      free(commandLine);
#else
      fprintf(stdout, "%s", _interactivePrompt);
      fflush(stdout);
      fgets(inputLine, sizeof(inputLine), stdin);
      stripWhitespace(inputLine);
#endif
    }
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
static void writeSocket(const char *string_, unsigned length_)
{
  if (length_ > 0)
  {
    write(_connectFd, string_, length_);
  }
  else
  {
    write(_connectFd, string_, strlen(string_));
  }
}

/******************************************************************************/
/******************************************************************************/
static void showWelcome(void)
{
  char sessionInfo[256];
  sprintf(sessionInfo, "Single session %s server: %s[%s]", ((_serverType == PSHELL_TCP_SERVER) ? "TCP" : "LOCAL"), _serverName, _ipAddress);
  unsigned maxLength = MAX(strlen(_banner), strlen(sessionInfo))+3;
  /* set our terminal title bar */
  pshell_printf("\033]0;%s: %s[%s], Mode: INTERACTIVE\007", _title, _serverName, _ipAddress);
  /* show our welcome screen */
  pshell_printf("\n");
  PSHELL_PRINT_WELCOME_BORDER(pshell_printf, maxLength);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  %s\n", PSHELL_WELCOME_BORDER, _banner);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  %s\n", PSHELL_WELCOME_BORDER, sessionInfo);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  if (_serverType == PSHELL_TCP_SERVER)
  {
    pshell_printf("%s  Idle session timeout: %d minutes\n", PSHELL_WELCOME_BORDER, _defaultIdleTimeout);
  }
  else
  {
    pshell_printf("%s  Idle session timeout: NONE\n", PSHELL_WELCOME_BORDER);
  }
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  Type '?' or 'help' at prompt for command summary\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s  Type '?' or '-h' after command for command usage\n", PSHELL_WELCOME_BORDER);
  pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  if (_serverType == PSHELL_TCP_SERVER)
  {
    pshell_printf("%s  Full <TAB> completion, up-arrow recall, command\n", PSHELL_WELCOME_BORDER);
    pshell_printf("%s  line editing and command abbreviation supported\n", PSHELL_WELCOME_BORDER);
    pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
  }
  else  /* local server build with readline library */
  {
#ifdef READLINE
    pshell_printf("%s  Full <TAB> completion, up-arrow recall, command\n", PSHELL_WELCOME_BORDER);
    pshell_printf("%s  line editing and command abbreviation supported\n", PSHELL_WELCOME_BORDER);
    pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
#else
    pshell_printf("%s  Command abbreviation supported\n", PSHELL_WELCOME_BORDER);
    pshell_printf("%s\n", PSHELL_WELCOME_BORDER);
#endif
  }
  PSHELL_PRINT_WELCOME_BORDER(pshell_printf, maxLength);
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
static void quit(int argc, char *argv[])
{
  _quit = true;
  if (_serverType == PSHELL_LOCAL_SERVER)
  {
    exit(0);
  }
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
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
static void batch(int argc, char *argv[])
{
  unsigned rate = 0;
  unsigned repeat = 1;
  bool clear = false;
  PshellTokens *argAndValue;
  if (pshell_isHelp())
  {
    pshell_printf("\n");
    pshell_showUsage();
    pshell_printf("\n");
    pshell_printf("  where:\n");
    pshell_printf("    filename - name of batch file to run\n");
    pshell_printf("    rate     - rate in seconds to repeat batch file (default=0)\n");
    pshell_printf("    repeat   - number of times to repeat command or 'forever' (default=1)\n");
    pshell_printf("    clear    - clear the screen between batch file runs\n");
    pshell_printf("\n");
  }
  else
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
}

/******************************************************************************/
/******************************************************************************/
static void addHistory(char *command_)
{
  int i;

  for (i = 0; i < PSHELL_MAX_HISTORY; i++)
  {
    /* look for the first empty slot */
    if (_history[i] == NULL)
    {
      /* if this is the first entry or the new entry is not
       * the same as the previous one, go ahead and add it
       */
      if ((i == 0) || (strcasecmp(_history[i-1], command_) != 0))
      {
        _history[i] = strdup(command_);
      }
      return;
    }
  }

  /* No space found, drop one off the beginning of the list */
  free_z(_history[0]);

  /* move all the other entrys up the list */
  for (i = 0; i < PSHELL_MAX_HISTORY-1; i++)
  {
    _history[i] = _history[i+1];
  }

  /* add the new entry */
  _history[PSHELL_MAX_HISTORY-1] = strdup(command_);

}

/******************************************************************************/
/******************************************************************************/
static void clearHistory(void)
{
  int i;
  for (i = 0; i < PSHELL_MAX_HISTORY; i++)
  {
    if (_history[i] != NULL)
    {
      free_z(_history[i]);
    }
  }
}

/******************************************************************************/
/******************************************************************************/
static void tokenize(char *string_, struct Tokens *tokens_, const char *delimiter_)
{
  char *str;
  tokens_->numTokens = 0;
  if ((str = strtok(string_, delimiter_)) != NULL)
  {
    tokens_->tokens[tokens_->numTokens++] = str;
    while ((str = strtok(NULL, delimiter_)) != NULL)
    {
      if (tokens_->numTokens < PSHELL_MAX_TOKENS)
      {
        tokens_->tokens[tokens_->numTokens++] = str;
      }
      else
      {
        break;
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
static int findCompletions(char *command_, char *completions_[])
{
  struct Tokens tokens;
  unsigned i;
  int numFound = 0;

  _maxTabCommandLength = 0;
  tokenize(command_, &tokens, " ");

  if (tokens.numTokens > 0)
  {
    for (i = 0; i < _numCommands; i++)
    {
      if (strncasecmp(_commandTable[i].command, tokens.tokens[0], strlen(tokens.tokens[0])) == 0)
      {
        completions_[numFound++] = (char *)_commandTable[i].command;
        if (strlen(_commandTable[i].command) > _maxTabCommandLength)
        {
          _maxTabCommandLength = strlen(_commandTable[i].command);
        }
      }
    }
  }
  else
  {
    for (i = 0; i < _numCommands; i++)
    {
      completions_[numFound++] = (char *)_commandTable[i].command;
      if (strlen(_commandTable[i].command) > _maxTabCommandLength)
      {
        _maxTabCommandLength = strlen(_commandTable[i].command);
      }
    }
  }

  _maxTabColumns = PSHELL_MAX_SCREEN_WIDTH/(_maxTabCommandLength+2);

  return (numFound);

}

/******************************************************************************/
/******************************************************************************/
static void clearLine(char *command_, int length_, int cursor_)
{
  int i;
  if (cursor_ < length_) for (i = 0; i < (length_-cursor_); i++) writeSocket(" ", 0);
  for (i = 0; i < length_; i++) command_[i] = '\b';
  for (; i < length_ * 2; i++) command_[i] = ' ';
  for (; i < length_ * 3; i++) command_[i] = '\b';
  writeSocket(command_, i);
  memset(command_, 0, i);
}

/******************************************************************************/
/******************************************************************************/
static void showPrompt(char *command_)
{
  if (command_ == NULL)
  {
    pshell_printf("\r%s", _interactivePrompt);
  }
  else
  {
    pshell_printf("\r%s%s", _interactivePrompt, command_);
  }
}

/******************************************************************************/
/******************************************************************************/
static void receiveTCP(void)
{
  unsigned char character;
  int i;
  int numRead;
  int position;
  bool historyFound;
  int length;
  int isTelnetOption = 0;
  int esc = 0;
  int cursor = 0;
  int lastChar;
  int nextChar;
  int back;
  int retCode;
  signed int inHistory;
  fd_set readFd;
  bool insertMode = true;
  char *completions[PSHELL_MAX_LINE_WORDS];
  int numCompletions;
  char command[PSHELL_MAX_LINE_LENGTH];
  const char *negotiate = "\xFF\xFB\x03\xFF\xFB\x01\xFF\xFD\x03\xFF\xFD\x01";

  clearHistory();
  writeSocket(negotiate, 0);

  if (!(_clientFd = fdopen(_connectFd, "w+")))
  {
    return;
  }

  setbuf(_clientFd, NULL);

  /* print out our welcome banner */
  showWelcome();

  _quit = false;
  while (!_quit)
  {

    _pshellMsg->header.msgType = PSHELL_USER_COMMAND;
    memset(command, 0, PSHELL_MAX_LINE_LENGTH);
    inHistory = 0;
    lastChar = 0;
    length = 0;
    cursor = 0;

    _showPrompt = true;

    while (1)
    {

      if (_showPrompt)
      {
        showPrompt(command);
        if (cursor < length)
        {
          numRead = length-cursor;
          while (numRead--)
          {
            writeSocket("\b", 0);
          }
        }
        _showPrompt = false;
      }

      FD_ZERO(&readFd);
      FD_SET(_connectFd, &readFd);

      /* Set the default idle timeout to 10 minutes */
      _idleTimeout.tv_sec = _defaultIdleTimeout*60;
      _idleTimeout.tv_usec = 0;

      if ((retCode = select(_connectFd+1, &readFd, NULL, NULL, &_idleTimeout)) < 0)
      {
        /* select error */
        if (errno == EINTR)
        {
          continue;
        }
        PSHELL_ERROR("Socket select error");
        length = -1;
        break;
      }
      else if (retCode == 0)
      {
        pshell_printf("\nIdle session timeout");
        return;
      }

      if ((numRead = read(_connectFd, &character, 1)) < 0)
      {
        if (errno == EINTR)
        {
          continue;
        }
        PSHELL_ERROR("Socket read error");
        length = -1;
        break;
      }

      if (numRead == 0)
      {
        length = -1;
        break;
      }

      if (character == 255 && !isTelnetOption)
      {
        isTelnetOption++;
        continue;
      }

      if (isTelnetOption)
      {
        if (character >= 251 && character <= 254)
        {
          isTelnetOption = character;
          continue;
        }
        if (character != 255)
        {
          isTelnetOption = 0;
          continue;
        }
        isTelnetOption = 0;
      }

      /* handle ANSI arrows */
      if (esc)
      {
        if (esc == '[')
        {
          /* remap to readline control codes */
          switch (character)
          {
            case 'A': /* Up */
              character = CTRL('P');
              break;
            case 'B': /* Down */
              character = CTRL('N');
              break;
            case 'C': /* Right */
              character = CTRL('F');
              break;
            case 'D': /* Left */
              character = CTRL('B');
              break;
            default:
              character = 0;
              break;
          }
          esc = 0;
        }
        else
        {
          esc = (character == '[') ? character : 0;
          continue;
        }
      }

      if ((character == 0) || (character == '\n'))
      {
        continue;
      }

      if (character == '\r')
      {
        writeSocket("\r\n", 0);
        break;
      }

      if (character == 27)
      {
        esc = 1;
        continue;
      }

      if (character == CTRL('C'))
      {
        writeSocket("\a", 0);
        continue;
      }

      /* back word, backspace/delete */
      if (character == CTRL('W') || character == CTRL('H') || character == 0x7f)
      {
        back = 0;

        if (character == CTRL('W')) /* word */
        {
          nextChar = cursor;
          if ((length == 0) || (cursor == 0))
          {
            continue;
          }

          while (nextChar && (command[nextChar-1] == ' '))
          {
            nextChar--;
            back++;
          }

          while (nextChar && (command[nextChar-1] != ' '))
          {
            nextChar--;
            back++;
          }
        }
        else   /* char */
        {
          if ((length == 0) || (cursor == 0))
          {
            writeSocket("\a", 0);
            continue;
          }
          back = 1;
        }

        if (back)
        {
          while (back--)
          {
            if (length == cursor)
            {
              command[--cursor] = 0;
              writeSocket("\b \b", 0);
            }
            else
            {
              cursor--;
              for (i = cursor; i <= length; i++)
              {
                command[i] = command[i+1];
              }
              writeSocket("\b", 0);
              writeSocket(command+cursor, strlen(command+cursor));
              writeSocket(" ", 0);
              for (i = 0; i <= (int)strlen(command+cursor); i++)
              {
                writeSocket("\b", 0);
              }
            }
            length--;
          }
          continue;
        }
      }

      /* redraw */
      if (character == CTRL('L'))
      {
        showPrompt(command);
        for (i = 0; i < length-cursor; i++)
        {
          writeSocket("\b", 0);
        }
        continue;
      }

      /* clear line */
      if (character == CTRL('U'))
      {
        clearLine(command, length, cursor);
        length = cursor = 0;
        continue;
      }

      /* kill to EOL */
      if (character == CTRL('K'))
      {
        if (cursor == length)
        {
          continue;
        }

        for (position = cursor; position < length; position++)
        {
          writeSocket(" ", 0);
        }

        for (position = cursor; position < length; position++)
        {
          writeSocket("\b", 0);
        }

        memset(command + cursor, 0, length-cursor);
        length = cursor;
        continue;
      }

      /* TAB completion */
      if (character == CTRL('I'))
      {
        if (cursor != length)
        {
          continue;
        }

        if ((numCompletions = findCompletions(command, completions)) == 0)
        {
          writeSocket("\a", 0);
        }
        else if (numCompletions == 1)
        {
          /* Single completion */
          for (; length > 0; length--, cursor--)
          {
            if ((command[length-1] == ' ') || (command[length-1] == '|'))
            {
              break;
            }
            writeSocket("\b", 0);
          }
          strcpy((command+length), completions[0]);
          length += strlen(completions[0]);
          command[length++] = ' ';
          cursor = length;
          writeSocket(completions[0], 0);
          writeSocket(" ", 0);
        }
        else if (lastChar == CTRL('I'))
        {
          /* double tab */
          pshell_printf("\n");
          for (i = 0; i < numCompletions; i++)
          {
            pshell_printf("%-*s  ", _maxTabCommandLength, completions[i]);
            if ((((i+1)%_maxTabColumns) == 0) || (i == numCompletions-1))
            {
              pshell_printf("\n");
            }
          }
          _showPrompt = true;
        }
        else
        {
          /* More than one completion */
          lastChar = character;
          writeSocket("\a", 0);
        }
        continue;
      }

      /* history */
      if (character == CTRL('P') || character == CTRL('N'))
      {
        historyFound = false;
        if (character == CTRL('P')) /* Up */
        {
          inHistory--;
          if (inHistory < 0)
          {
            for (inHistory = PSHELL_MAX_HISTORY-1; inHistory >= 0; inHistory--)
            {
              if (_history[inHistory])
              {
                historyFound = true;
                break;
              }
            }
          }
          else
          {
            historyFound = (_history[inHistory] != NULL);
          }
        }
        else /* Down */
        {
          inHistory++;
          if ((inHistory >= PSHELL_MAX_HISTORY) || !_history[inHistory])
          {
            for (i = 0; i < PSHELL_MAX_HISTORY; i++)
            {
              if (_history[i])
              {
                inHistory = i;
                historyFound = true;
                break;
              }
            }
          }
          else
          {
            historyFound = (_history[inHistory] != NULL);
          }
        }
        if (historyFound && _history[inHistory])
        {
          /* Show history item */
          clearLine(command, length, cursor);
          memset(command, 0, PSHELL_MAX_LINE_LENGTH);
          strncpy(command, _history[inHistory], PSHELL_MAX_LINE_LENGTH-1);
          length = cursor = strlen(command);
          writeSocket(command, 0);
        }
        continue;
      }

      /* left/right cursor motion */
      if (character == CTRL('B') || character == CTRL('F'))
      {
        if (character == CTRL('B')) /* Left */
        {
          if (cursor)
          {
            writeSocket("\b", 0);
            cursor--;
          }
        }
        else /* Right */
        {
          if (cursor < length)
          {
            writeSocket(&command[cursor], 0);
            cursor++;
          }
        }
        continue;
      }

      /* start of line */
      if (character == CTRL('A'))
      {
        if (cursor)
        {
          showPrompt(NULL);
          cursor = 0;
        }
        continue;
      }

      /* end of line */
      if (character == CTRL('E'))
      {
        if (cursor < length)
        {
          writeSocket(&command[cursor], length-cursor);
          cursor = length;
        }
        continue;
      }

      /* normal character typed */
      if (cursor == length)
      {
        /* append to end of line */
        command[cursor] = character;
        if (length < PSHELL_MAX_LINE_LENGTH-1)
        {
          length++;
          cursor++;
        }
        else
        {
          writeSocket("\a", 0);
          continue;
        }
      }
      else
      {
        /* Middle of text */
        if (insertMode)
        {
          /* Move everything one character to the right */
          if (length >= PSHELL_MAX_LINE_LENGTH-2)
          {
            length--;
          }
          for (i = length; i >= cursor; i--)
          {
            command[i+1] = command[i];
          }
          /* Write what we've just added */
          command[cursor] = character;
          writeSocket(&command[cursor], length-cursor+1);
          for (i = 0; i < (length-cursor+1); i++)
          {
            writeSocket("\b", 0);
          }
          length++;
        }
        else
        {
          command[cursor] = character;
        }
        cursor++;
      }
      writeSocket((char *)&character, 1);
      lastChar = character;
    }

    if (length > 0)
    {
      addHistory(command);
      processCommand(command);
    }
    else if (length < 0)
    {
      break;
    }

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
static void loadConfigFile(void)
{
  PshellTokens *config;
  PshellTokens *value;
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
        config = pshell_tokenize(line, "=");
        if (config->numTokens == 2)
        {
          item = pshell_tokenize(config->tokens[0], ".");
          value = pshell_tokenize(config->tokens[1], "\"");
          if ((item->numTokens == 2) &&
              (value->numTokens == 2) &&
              pshell_isEqual(item->tokens[0], _serverName))
          {
            if (pshell_isEqual(item->tokens[1], "title"))
            {
              strcpy(_title, value->tokens[0]);
            }
            else if (pshell_isEqual(item->tokens[1], "banner"))
            {
              strcpy(_banner, value->tokens[0]);
            }
            else if (pshell_isEqual(item->tokens[1], "prompt"))
            {
              strcpy(_prompt, value->tokens[0]);
            }
            else if (pshell_isEqual(item->tokens[1], "host"))
            {
              strcpy(_hostnameOrIpAddr, value->tokens[0]);
            }
            else if (pshell_isEqual(item->tokens[1], "port") &&
                     pshell_isNumeric(value->tokens[0]))
            {
              _port = pshell_getUnsigned(value->tokens[0]);
            }
            else if (pshell_isEqual(item->tokens[1], "timeout") &&
                     pshell_isNumeric(value->tokens[0]))
            {
              _defaultIdleTimeout = pshell_getInt(value->tokens[0]);
            }
            else if (pshell_isEqual(item->tokens[1], "type"))
            {
              if (pshell_isEqualNoCase(value->tokens[0], "UDP"))
              {
                _serverType = PSHELL_UDP_SERVER;
              }
              else if (pshell_isEqualNoCase(value->tokens[0], "UNIX"))
              {
                _serverType = PSHELL_UNIX_SERVER;
              }
              else if (pshell_isEqualNoCase(value->tokens[0], "TCP"))
              {
                _serverType = PSHELL_TCP_SERVER;
              }
              else if (pshell_isEqualNoCase(value->tokens[0], "LOCAL"))
              {
                _serverType = PSHELL_LOCAL_SERVER;
              }
            }
            else if (pshell_isEqual(item->tokens[1], "mode"))
            {
              if (pshell_isEqualNoCase(value->tokens[0], "BLOCKING"))
              {
                _serverMode = PSHELL_BLOCKING;
              }
              else if (pshell_isEqualNoCase(value->tokens[0], "NON_BLOCKING"))
              {
                _serverMode = PSHELL_NON_BLOCKING;
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
    _localUnixAddress.sun_family = AF_UNIX;
    sprintf(_localUnixAddress.sun_path, "%s/%s", PSHELL_UNIX_SOCKET_PATH, _serverName);
    unlink(_localUnixAddress.sun_path);

    /* bind to our source socket */
    if (bind(_socketFd,
             (struct sockaddr *) &_localUnixAddress,
             sizeof(_localUnixAddress)) < 0)
    {
      PSHELL_ERROR("Cannot bind to UNIX socket: %s", _localUnixAddress.sun_path);
      return (false);
    }
    strcpy(_ipAddress, "unix");
  }
  else
  {
    _localIpAddress.sin_family = AF_INET;
    _localIpAddress.sin_port = htons(_port);

    /* see if they supplied an IP address */
    if (inet_aton(_hostnameOrIpAddr, &_localIpAddress.sin_addr) == 0)
    {
      strcpy(requestedHost, _hostnameOrIpAddr);
      if (pshell_isEqual(requestedHost, "anyhost"))
      {
        _localIpAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        getIpAddress("eth0", _ipAddress);
      }
      else if (pshell_isEqual(requestedHost, "localhost"))
      {
        _localIpAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
        strcpy(_ipAddress, "127.0.0.1");
      }
      else
      {
        /* see if they are requesting our local host address */
        if (pshell_isEqual(requestedHost, "myhost"))
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

    /* bind to our source socket */
    if (bind(_socketFd,
             (struct sockaddr *) &_localIpAddress,
             sizeof(_localIpAddress)) < 0)
    {
      PSHELL_ERROR("Cannot bind to socket: address: %s, port: %d",
                   inet_ntoa(_localIpAddress.sin_addr),
                     ntohs(_localIpAddress.sin_port));
      return (false);
    }

    if ((_serverType == PSHELL_TCP_SERVER) && (listen(_socketFd, 1) < 0))
    {
      PSHELL_ERROR("Cannot listen on socket");
      return (false);
    }

  }

  sprintf(_interactivePrompt, "%s[%s]:%s", _serverName, _ipAddress, _prompt);

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
static void processQueryIpAddress(void)
{
  pshell_printf("%s", _ipAddress);
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
 * called when pshell client runs in interactive mode and
 * was built with the -DREADLINE compile time flag, in
 * that case the client needs the command list for its
 * TAB completion function, it is much easier to parse
 * a list of just the command names with a simple delimeter
 * rather than try to parse the output that is returned
 * from the processQueryCommands1 function in order to
 * pick out just the command names
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

  _argc = 0;
  if (args->numTokens > 0)
  {
    command = args->tokens[0];
    _argc = args->numTokens-PSHELL_BASE_ARG_OFFSET;
    _argv = &args->tokens[PSHELL_BASE_ARG_OFFSET];
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
  _foundCommand = NULL;
  if (pshell_isEqual(command_, "?"))
  {
    /* the position of the "help" command  */
    _foundCommand = &_commandTable[1];
    numMatches = 1;
  }
  else
  {
    for (entry = 0; entry < _numCommands; entry++)
    {
      /* see if we have a match */
      if (pshell_isSubString(command_, _commandTable[entry].command, strlen(command_)))
      {
        /* set our _foundCommand pointer */
        _foundCommand = &_commandTable[entry];
        numMatches++;
      }
    }
  }
  return (numMatches);
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
                                     * create args" error for the non-user commands */
  unsigned payloadSize = _pshellPayloadSize;  /* save off current size in case it changes
                                             * and we need to grab more memory, UDP server only */
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
        _foundCommand->function(_argc, _argv);
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
     * if the caller does not any data back, we set this value to
     * false which will short circuit any calls to pshell_printf
     */
    _isCommandInteractive = _pshellMsg->header.dataNeeded;
    /* set this to true so we can tokenize our command line with pshell_tokenize */
    _isCommandDispatched = true;
    /* set the to true so we prevent any intermediate flushed back to the control cliennt */
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
        _foundCommand->function(_argc, _argv);
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
  else if (_pshellMsg->header.msgType == PSHELL_QUERY_IP_ADDRESS)
  {
    processQueryIpAddress();
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

  if ((_pshellMsg->header.msgType == PSHELL_CONTROL_COMMAND) &&
      (strlen(_pshellMsg->payload) > 0))
  {
    /* take out the extra newline for the remote control client */
    _pshellMsg->payload[strlen(_pshellMsg->payload)-1] = 0;
  }
  
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

      /* if we are not in interactive mode, wait a little bit and try again */
      while (!_isCommandInteractive)
      {
        sleep(1);
      }
      /* make sure our command is NULL terminated */
      _pshellMsg->payload[receivedSize-PSHELL_HEADER_SIZE] = 0;

      /* process the command and issue the reply */
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

      /* if we are not in interactive mode, wait a little bit and try again */
      while (!_isCommandInteractive)
      {
        sleep(1);
      }
      /* make sure our command is NULL terminated */
      _pshellMsg->payload[receivedSize-PSHELL_HEADER_SIZE] = 0;

      /* process the command and issue the reply */
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
  else if ((_serverType == PSHELL_UNIX_SERVER))
  {
    replyUNIX(pshellMsg_);
  }
}

/******************************************************************************/
/******************************************************************************/
void _printf(const char *format_, ...)
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
