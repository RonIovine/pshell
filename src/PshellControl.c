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
#include <stdarg.h>
#include <pthread.h>

#include <PshellCommon.h>
#include <PshellControl.h>

/* constants */

/*
 * this constant defines the default location to look for the
 * psehll-control.conf configuration file is the PSHELL_CONFIG_DIR
 * env variabel is not set
 */
#ifndef PSHELL_CONFIG_DIR
#define PSHELL_CONFIG_DIR "/etc/pshell"
#endif

#define MAX_UNIX_CLIENTS 1000

/* enums */

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

/*************************
 * private "member" data
 *************************/

struct PshellControl
{
  int socketFd;
  unsigned defaultTimeout;
  ServerType serverType;
  PshellMsg pshellMsg;
  struct sockaddr_in sourceIpAddress;
  struct sockaddr_in destIpAddress;
  struct sockaddr_un sourceUnixAddress;
  struct sockaddr_un destUnixAddress;
};

#define PSHELL_MAX_SERVERS 100
static PshellControl *_control[PSHELL_MAX_SERVERS] = {0};
static PshellLogFunction _logFunction = NULL;
static unsigned _logLevel = PSHELL_LOG_LEVEL_DEFAULT;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *_errorPad = "              ";

/******************************************
 * private "member" function prototypes
 ******************************************/

static PshellControl *getControl(int sid_);
static int createControl(void);
static int sendPshellCommand(PshellControl *control_, const char *command_, unsigned timeoutOverride_);
static bool sendPshellMsg(PshellControl *control_);
static int extractResults(PshellControl *control_, char *results_, int size_);
static bool connectServer(PshellControl *control_, const char *remoteServer_ , unsigned port_, unsigned defaultMsecTimeout_);
static void loadConfigFile(const char *controlName_, char *remoteServer_, unsigned &port_, unsigned &defaultTimeout_);

static void _printf(const char *format_, ...);

/* output display macros */
#define PSHELL_ERROR(format_, ...) if (_logLevel >= PSHELL_LOG_LEVEL_1) {_printf("PSHELL_ERROR: " format_, ##__VA_ARGS__);_printf("\n");}
#define PSHELL_WARNING(format_, ...) if (_logLevel >= PSHELL_LOG_LEVEL_2) {_printf("PSHELL_WARNING: " format_, ##__VA_ARGS__);_printf("\n");}
#define PSHELL_INFO(format_, ...) if (_logLevel >= PSHELL_LOG_LEVEL_2) {_printf("PSHELL_INFO: " format_, ##__VA_ARGS__);_printf("\n");}

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
int pshell_connectServer(const char *controlName_,
                         const char *remoteServer_ ,
                         unsigned port_,
                         unsigned defaultTimeout_)
{
  pthread_mutex_lock(&_mutex);
  int sid;
  char remoteServer[256];
  unsigned port = port_;
  unsigned defaultTimeout = defaultTimeout_;
  if ((sid = createControl()) != PSHELL_INVALID_SID)
  {
    strcpy(remoteServer, remoteServer_);
    loadConfigFile(controlName_, remoteServer, port, defaultTimeout);
    if (!connectServer(_control[sid], remoteServer, port, defaultTimeout))
    {
      free(_control[sid]);
      _control[sid] = NULL;
      sid = PSHELL_INVALID_SID;
    }
  }
  pthread_mutex_unlock(&_mutex);
  return (sid);
}

/******************************************************************************/
/******************************************************************************/
void pshell_disconnectServer(int sid_)
{
  pthread_mutex_lock(&_mutex);
  PshellControl *control;
  if ((control = getControl(sid_)) != NULL)
  {
    if (control->serverType == UNIX)
    {
      unlink(control->sourceUnixAddress.sun_path);
    }
    close(control->socketFd);
    free(control);
    _control[sid_] = NULL;
  }
  pthread_mutex_unlock(&_mutex);
}

/******************************************************************************/
/******************************************************************************/
void pshell_setDefaultTimeout(int sid_, unsigned defaultTimeout_)
{
  pthread_mutex_lock(&_mutex);
  PshellControl *control;
  if ((control = getControl(sid_)) != NULL)
  {
    control->defaultTimeout = defaultTimeout_;
  }
  pthread_mutex_unlock(&_mutex);
}

/******************************************************************************/
/******************************************************************************/
int pshell_sendCommand1(int sid_, const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int retCode= PSHELL_SOCKET_NOT_CONNECTED;
  char command[300];
  PshellControl *control;
  va_list args;
  if ((control = getControl(sid_)) != NULL)
  {
    va_start(args, command_);
    vsprintf(command, command_, args);
    va_end(args);
    control->pshellMsg.header.dataNeeded = false;
    retCode = sendPshellCommand(control, command, control->defaultTimeout);
  }
  pthread_mutex_unlock(&_mutex);  
  return (retCode);
}

/******************************************************************************/
/******************************************************************************/
int pshell_sendCommand2(int sid_, unsigned timeoutOverride_, const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int retCode= PSHELL_SOCKET_NOT_CONNECTED;
  char command[300];
  PshellControl *control;
  va_list args;
  if ((control = getControl(sid_)) != NULL)
  {
    va_start(args, command_);
    vsprintf(command, command_, args);
    va_end(args);
    control->pshellMsg.header.dataNeeded = false;
    retCode = sendPshellCommand(control, command, timeoutOverride_);
  }
  pthread_mutex_unlock(&_mutex);  
  return (retCode);
}

/******************************************************************************/
/******************************************************************************/
int pshell_sendCommand3(int sid_, char *results_, int size_, const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int bytesExtracted = 0;
  char command[300];
  PshellControl *control;
  va_list args;
  if ((control = getControl(sid_)) != NULL)
  {
    va_start(args, command_);
    vsprintf(command, command_, args);
    va_end(args);
    control->pshellMsg.header.dataNeeded = true;
    if (control->defaultTimeout == 0)
    {
      PSHELL_WARNING("Trying to extract data with a 0 wait timeout, no data will be extracted");
    }
    if ((sendPshellCommand(control, command, control->defaultTimeout) == PSHELL_COMMAND_SUCCESS) &&
        (control->defaultTimeout > 0))
    {
      bytesExtracted = extractResults(control, results_, size_);
    }
  }
  pthread_mutex_unlock(&_mutex);  
  return (bytesExtracted);
}

/******************************************************************************/
/******************************************************************************/
int pshell_sendCommand4(int sid_, char *results_, int size_, unsigned timeoutOverride_, const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int bytesExtracted = 0;
  char command[300];
  PshellControl *control;
  va_list args;
  if ((control = getControl(sid_)) != NULL)
  {
    va_start(args, command_);
    vsprintf(command, command_, args);
    va_end(args);
    control->pshellMsg.header.dataNeeded = true;
    if (timeoutOverride_ == 0)
    {
      PSHELL_WARNING("Trying to extract data with a 0 wait timeout, no data will be extracted");
    }
    if ((sendPshellCommand(control, command, timeoutOverride_) == PSHELL_COMMAND_SUCCESS) &&
        (timeoutOverride_ > 0))
    {
      bytesExtracted = extractResults(control, results_, size_);
    }
  }
  pthread_mutex_unlock(&_mutex);  
  return (bytesExtracted);
}

/******************************************************************************/
/******************************************************************************/
const char *pshell_getResultsString(int results_)
{
  switch (results_)
  {
    case PSHELL_COMMAND_SUCCESS:
      return ("PSHELL_COMMAND_SUCCESS");
    case PSHELL_COMMAND_INVALID_ARG_COUNT:
      return ("PSHELL_COMMAND_INVALID_ARG_COUNT");
    case PSHELL_COMMAND_NOT_FOUND:
      return ("PSHELL_COMMAND_NOT_FOUND");
    case PSHELL_SOCKET_SEND_FAILURE:
      return ("PSHELL_SOCKET_SEND_FAILURE");
    case PSHELL_SOCKET_SELECT_FAILURE:
      return ("PSHELL_SOCKET_SELECT_FAILURE");
    case PSHELL_SOCKET_RECEIVE_FAILURE:
      return ("PSHELL_SOCKET_RECEIVE_FAILURE");
    case PSHELL_SOCKET_TIMEOUT:
      return ("PSHELL_SOCKET_TIMEOUT");
    case PSHELL_SOCKET_NOT_CONNECTED:
      return ("PSHELL_SOCKET_NOT_CONNECTED");
    default:
      return ("PSHELL_UNKNOWN_RESULT");    
  }
}

/**************************************
 * private "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
bool connectServer(PshellControl *control_, const char *remoteServer_ , unsigned port_, unsigned defaultTimeout_)
{
  struct hostent *host;
  int retCode = -1;

  control_->defaultTimeout = defaultTimeout_;
  if (port_ > 0)
  {

    /* UDP server must provide a non-0 port number */
    control_->serverType = UDP;
  
    /*
    * we don't care what our source address is because the pshell server
    * will just do a reply to us based on the source address it receives
    * the request from
    */
    control_->sourceIpAddress.sin_family = AF_INET;
    control_->sourceIpAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    control_->sourceIpAddress.sin_port = htons(0);

    /* setup our destination */
    control_->destIpAddress.sin_family = AF_INET;
    control_->destIpAddress.sin_port = htons(port_);

    /* open and bind to our source socket */
    if ((control_->socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      PSHELL_ERROR("Socket create failure");
      return (false);
    }
    else if (bind(control_->socketFd,
                  (struct sockaddr *)&control_->sourceIpAddress,
                  sizeof(control_->sourceIpAddress)) < 0)
    {
      PSHELL_ERROR("Socket bind failure");
      return (false);
    }
    else if (inet_aton(remoteServer_, &control_->destIpAddress.sin_addr) == 0)
    {
      /* they supplied a hostname, see if it is one of our special hostnames */
      if (strcmp(remoteServer_, "localhost") == 0)
      {
        control_->destIpAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
      }
      else if ((host = gethostbyname(remoteServer_)) != NULL)
      {
        memcpy(&control_->destIpAddress.sin_addr.s_addr,
               *host->h_addr_list,
               sizeof(control_->destIpAddress.sin_addr.s_addr));
      }
      else
      {
        PSHELL_ERROR("Could not resolve hostname: %s", remoteServer_);
        return (false);
      }
    }
  }
  else
  {
    
    /* UNIX server does not need a port number */
    control_->serverType = UNIX;
      
    if ((control_->socketFd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
      PSHELL_ERROR("PSHELL_ERROR: Cannot create UNIX socket");
      return (false);
    }

    control_->sourceUnixAddress.sun_family = AF_UNIX;

    /* bind to our source socket */
    for (int i = 0; ((i < MAX_UNIX_CLIENTS) && (retCode < 0)); i++)
    {
      sprintf(control_->sourceUnixAddress.sun_path,
              "%s/pshellControlClient%d",
              PSHELL_UNIX_SOCKET_PATH,
              (rand()%MAX_UNIX_CLIENTS));
      retCode = bind(control_->socketFd,
                     (struct sockaddr *)&control_->sourceUnixAddress,
                     sizeof(control_->sourceUnixAddress));
    }
    
    if (retCode < 0)
    {
      PSHELL_ERROR("PSHELL_ERROR: Cannot bind to UNIX socket: %s", control_->sourceUnixAddress.sun_path);
      return (false);
    }
    /* setup our destination server information */
    control_->destUnixAddress.sun_family = AF_UNIX;
    sprintf(control_->destUnixAddress.sun_path, "%s/%s", PSHELL_UNIX_SOCKET_PATH, remoteServer_);
  }
  return (true);
}

/******************************************************************************/
/******************************************************************************/
int sendPshellCommand(PshellControl *control_, const char *command_, unsigned timeoutOverride_)
{
  fd_set readFd;
  struct timeval timeout;
  int bytesRead = 0;
  int retCode = PSHELL_COMMAND_SUCCESS;

  if (control_ != NULL)
  {
    
    FD_ZERO (&readFd);
    FD_SET(control_->socketFd,&readFd);

    control_->pshellMsg.header.msgType = PSHELL_CONTROL_COMMAND;
    control_->pshellMsg.header.respNeeded = (timeoutOverride_ > 0);
    control_->pshellMsg.payload[0] = 0;
    strncpy(control_->pshellMsg.payload, command_, PSHELL_PAYLOAD_SIZE);
    /* send command request to remote pshell */
    if (!sendPshellMsg(control_))
    {
      retCode = PSHELL_SOCKET_SEND_FAILURE;
    }
    else if (timeoutOverride_ > 0)
    {
      /* they want a response from the remote pshell */
      /* timeout value is in milliSeconds */
      memset(&timeout, 0, sizeof(timeout));
      timeout.tv_sec = timeoutOverride_/1000;
      timeout.tv_usec = (timeoutOverride_%1000)*1000;
      
      if ((select(control_->socketFd+1, &readFd, NULL, NULL, &timeout)) < 0)
      {
        retCode = PSHELL_SOCKET_SELECT_FAILURE;
      }
      else if (FD_ISSET(control_->socketFd, &readFd))
      {
        /* got a response, read it */
        if ((bytesRead = recvfrom(control_->socketFd,
                                  (char *)&control_->pshellMsg,
                                  sizeof(control_->pshellMsg),
                                  0,
                                  (struct sockaddr *)0,
                                  (socklen_t *)0)) < 0)
        {
          retCode = PSHELL_SOCKET_RECEIVE_FAILURE;
        }
        else
        {
          retCode = control_->pshellMsg.header.msgType;
        }
      }
      else
      {
        retCode = PSHELL_SOCKET_TIMEOUT;
      }
    }
  }
  else
  {
    retCode = PSHELL_SOCKET_NOT_CONNECTED;
  }

  if ((strlen(control_->pshellMsg.payload) > 0) &&
      (retCode > PSHELL_COMMAND_SUCCESS) && 
      (retCode < PSHELL_SOCKET_SEND_FAILURE))
  {
    PSHELL_ERROR("Remote pshell command: '%s', %s\n%s%s",
                 command_,
                 pshell_getResultsString(retCode),
                 _errorPad,
                 control_->pshellMsg.payload);
  }
  else if (retCode != PSHELL_COMMAND_SUCCESS)
  {
    PSHELL_ERROR("Remote pshell command: '%s', %s", command_, pshell_getResultsString(retCode));
  }
  else
  {
    PSHELL_INFO("Remote pshell command: '%s', %s", command_, pshell_getResultsString(retCode));
  }
  
  return (retCode);
  
}

/******************************************************************************/
/******************************************************************************/
bool sendPshellMsg(PshellControl *control_)
{
  if (control_->serverType == UDP)
  {
    return ((sendto(control_->socketFd,
                    (char*)&control_->pshellMsg,
                    strlen(control_->pshellMsg.payload)+PSHELL_HEADER_SIZE,
                    0,
                    (struct sockaddr *) &control_->destIpAddress,
                    sizeof(control_->destIpAddress)) >= 0));
  }
  else   /* UNIX server */
  {
    return ((sendto(control_->socketFd,
                    (char*)&control_->pshellMsg,
                    strlen(control_->pshellMsg.payload)+PSHELL_HEADER_SIZE,
                    0,
                    (struct sockaddr *) &control_->destUnixAddress,
                    sizeof(control_->destUnixAddress)) >= 0));
  }
}

/******************************************************************************/
/******************************************************************************/
int extractResults(PshellControl *control_, char *results_, int size_)
{
  int bytesExtracted = 0;
  int payloadSize = strlen(control_->pshellMsg.payload);
  if ((payloadSize > 0) && (results_ != NULL))
  {
    strncpy(results_, control_->pshellMsg.payload, size_);
    bytesExtracted = MIN(payloadSize, size_);
  }
  return (bytesExtracted);
}

/******************************************************************************/
/******************************************************************************/
int createControl(void)
{
  int sid = PSHELL_INVALID_SID;
  int tmpSid;

  /* try to find an open slot in our control array */
  for (unsigned i = 0; i < PSHELL_MAX_SERVERS; i++)
  {
    tmpSid = rand()%PSHELL_MAX_SERVERS;
    if (_control[tmpSid] == NULL)
    {
      sid = tmpSid;
      break;
    }
  }
  if (sid != PSHELL_INVALID_SID)
  {
    if ((_control[sid] = (PshellControl *)calloc(1, sizeof(PshellControl))) == NULL)
    {
      PSHELL_ERROR("Could not allocate memory for new PshellControl entry");
      sid = PSHELL_INVALID_SID;
    }
  }
  return (sid);
}

/******************************************************************************/
/******************************************************************************/
PshellControl *getControl(int sid_)
{
  PshellControl *control = NULL;
  if ((sid_ >= 0) && (sid_ < PSHELL_MAX_SERVERS))
  {
    control = _control[sid_];
  }
  else
  {
    PSHELL_ERROR("Out of range sid: %d, valid range: 0-%d", sid_, PSHELL_MAX_SERVERS);
  }
  return (control);
}

/******************************************************************************/
/******************************************************************************/
static void loadConfigFile(const char *controlName_,
                           char *remoteServer_,
                           unsigned &port_,
                           unsigned &defaultTimeout_)
{
  char configFile[180];
  char cwd[180];
  char *configPath;
  char line[180];
  FILE *fp;
  char *value;
  char *control;
  char *option;
  configFile[0] = '\0';
  bool isUnix = false;
  if ((configPath = getenv("PSHELL_CONFIG_DIR")) != NULL)
  {
    sprintf(configFile, "%s/pshell-control.conf", configPath);
  }

  if ((fp = fopen(configFile, "r")) == NULL)
  {
    /* either the env variable is not found or the file is not found
     * look in our default directory
     */
    sprintf(configFile, "%s/pshell-control.conf", PSHELL_CONFIG_DIR);
    if ((fp = fopen(configFile, "r")) == NULL)
    {
      /* not found in our default directory, look in our CWD */
      getcwd(cwd, sizeof(cwd));
      sprintf(configFile, "%s/pshell-control.conf", cwd);
      fp = fopen(configFile, "r");
    }
  }

  if (fp != NULL)
  {
    while (fgets(line, sizeof(line), fp) != NULL)
    {
      /* look for our server name on every non-commented line */
      if (line[0] != '#')
      {
        if (((value = strstr(line, "=\"")) != NULL) &&
            ((option = strstr(line, ".")) != NULL))
        {
          value[strlen(value)-2] = 0;
          value[0] = 0;
          value += 2;
          option[0] = 0;
          option++;
          control = line;
          if (strcmp(control, controlName_) == 0)
          {
            if (strcmp(option, "udp") == 0)
            {
              strcpy(remoteServer_, value);
            }
            else if (strcmp(option, "unix") == 0)
            {
              strcpy(remoteServer_, value);
              port_ = PSHELL_UNIX_SERVER;
              isUnix = true;
            }
            else if (strcmp(option, "port") == 0)
            {
              /*
               * make this check in case they changed the server
               * from udp to unix and forgot to comment out the
               * port
               */
              if (!isUnix)
              {
                port_ = atoi(value);
              }
            }
            else if (strcmp(option, "timeout") == 0)
            {
              if (strcmp(value, "none") == 0)
              {
                defaultTimeout_ = 0;
              }
              else
              {
                defaultTimeout_ = atoi(value);
              }
            }
          }
        }
      }
    }
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

