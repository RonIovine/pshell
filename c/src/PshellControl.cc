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
 * env variable is not set
 */
#ifdef PSHELL_CONFIG_DIR
#undef PSHELL_CONFIG_DIR
#define PSHELL_CONFIG_DIR STR(PSHELL_CONFIG_DIR)
#else
#define PSHELL_CONFIG_DIR "/etc/pshell/config"
#endif

#define MAX_UNIX_CLIENTS 1000

#define PSHELL_MAX_SERVERS 100

#define MAX_MULTICAST_GROUPS 100

#define MAX_STRING_SIZE 300

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
  char remoteServer[MAX_STRING_SIZE];
};

struct PshellMulticast
{
  int numSids;
  int sidList[PSHELL_MAX_SERVERS];
  char keyword[MAX_STRING_SIZE];
};

struct PshellMulticastList
{
  int numGroups;
  PshellMulticast groups[MAX_MULTICAST_GROUPS];
};

static PshellMulticastList _multicastList = {0};

static PshellControl *_control[PSHELL_MAX_SERVERS] = {0};
static PshellLogFunction _logFunction = NULL;
static unsigned _logLevel = PSHELL_CONTROL_LOG_LEVEL_DEFAULT;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *_errorPad = "              ";

/******************************************
 * private "member" function prototypes
 ******************************************/

static PshellControl *getControl(int sid_);
static int createControl(void);
static int sendPshellCommand(PshellControl *control_, int commandType_, const char *command_, unsigned timeoutOverride_);
static bool sendPshellMsg(PshellControl *control_);
static int extractResults(PshellControl *control_, char *results_, int size_);
static bool connectServer(PshellControl *control_, const char *remoteServer_ , unsigned port_, unsigned defaultMsecTimeout_);
static void loadConfigFile(const char *controlName_, char *remoteServer_, unsigned &port_, unsigned &defaultTimeout_);

static void _printf(const char *format_, ...);

/* output display macros */
#define PSHELL_ERROR(format_, ...) if (_logLevel >= PSHELL_CONTROL_LOG_LEVEL_ERROR) {_printf("PSHELL_ERROR: " format_, ##__VA_ARGS__);_printf("\n");}
#define PSHELL_WARNING(format_, ...) if (_logLevel >= PSHELL_CONTROL_LOG_LEVEL_WARNING) {_printf("PSHELL_WARNING: " format_, ##__VA_ARGS__);_printf("\n");}
#define PSHELL_INFO(format_, ...) if (_logLevel >= PSHELL_CONTROL_LOG_LEVEL_INFO) {_printf("PSHELL_INFO: " format_, ##__VA_ARGS__);_printf("\n");}

/**************************************
 * public API "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void pshell_setControlLogLevel(unsigned level_)
{
  _logLevel = level_;
}

/******************************************************************************/
/******************************************************************************/
void pshell_registerControlLogFunction(PshellLogFunction logFunction_)
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
  char remoteServer[MAX_STRING_SIZE];
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
    else
    {
      sprintf(_control[sid]->remoteServer,  "%s[%s]", controlName_, remoteServer);
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
void pshell_disconnectAllServers(void)
{
  for (int sid = 0; sid < PSHELL_MAX_SERVERS; sid++)
  {
    pshell_disconnectServer(sid);
  }
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
void pshell_extractCommands(int sid_, char *results_, int size_)
{
  pthread_mutex_lock(&_mutex);
  PshellControl *control;
  int retCode;
  if ((results_ != NULL) && (control = getControl(sid_)) != NULL)
  {
    control->pshellMsg.header.msgType =  PSHELL_QUERY_COMMANDS1;
    control->pshellMsg.header.dataNeeded = true;
    control->pshellMsg.payload[0] = 0;
    if ((retCode = sendPshellCommand(control, PSHELL_QUERY_COMMANDS1, "query commands", PSHELL_ONE_SEC*5)) == PSHELL_COMMAND_SUCCESS)
    {
      results_[0] = 0;
      snprintf(&results_[strlen(results_)], (size_-strlen(results_)), "\n");
      for (unsigned i = 0; i < strlen(control->remoteServer)+22; i++) snprintf(&results_[strlen(results_)], (size_-strlen(results_)), "*");
      snprintf(&results_[strlen(results_)], (size_-strlen(results_)), "\n");
      snprintf(&results_[strlen(results_)], (size_-strlen(results_)), "*   COMMAND LIST: %s", control->remoteServer);
      for (unsigned i = 0; i < 3; i++) snprintf(&results_[strlen(results_)], (size_-strlen(results_)), " ");;
      snprintf(&results_[strlen(results_)], (size_-strlen(results_)), "*\n");
      for (unsigned i = 0; i < strlen(control->remoteServer)+22; i++) snprintf(&results_[strlen(results_)], (size_-strlen(results_)), "*");
      snprintf(&results_[strlen(results_)], (size_-strlen(results_)), "\n");
      snprintf(&results_[strlen(results_)], (size_-strlen(results_)), "\n");
      extractResults(control, &results_[strlen(results_)], (size_-strlen(results_)));
    }
  }
  pthread_mutex_unlock(&_mutex);
}

/******************************************************************************/
/******************************************************************************/
void pshell_addMulticast(int sid_, const char *keyword_)
{
  pthread_mutex_lock(&_mutex);
  int groupIndex = _multicastList.numGroups;
  for (int i = 0; i < _multicastList.numGroups; i++)
  {
    if (strcmp(_multicastList.groups[i].keyword, keyword_) == 0)
    {
      /* we found an existing multicast group, break out */
      groupIndex = i;
      break;
    }
  }
  if (groupIndex < MAX_MULTICAST_GROUPS)
  {
    /* group found, now see if we have a new unique sid for this group */
    int sidIndex = _multicastList.groups[groupIndex].numSids;
    for (int i = 0; i < _multicastList.groups[groupIndex].numSids; i++)
    {
      if (_multicastList.groups[groupIndex].sidList[i] == sid_)
      {
        sidIndex = i;
        break;
      }
    }
    if (sidIndex < PSHELL_MAX_SERVERS)
    {      
      /* see if we have a new multicast group (keyword) */
      if (groupIndex == _multicastList.numGroups)
      {
        /* new keyword (group), add the new entry */
        strcpy(_multicastList.groups[_multicastList.numGroups++].keyword, keyword_);
        PSHELL_INFO("Adding new multicast group keyword: '%s', numGroups: %d", keyword_, _multicastList.numGroups);
      }
      /* see if we have a new sid for this group */
      if (sidIndex == _multicastList.groups[groupIndex].numSids)
      {      
        /* new sid for this group, add it */
        _multicastList.groups[groupIndex].sidList[_multicastList.groups[groupIndex].numSids++] = sid_;
        PSHELL_INFO("Adding new multicast group sid: %d, keyword: '%s', numGroups: %d, numSids: %d", 
                    sid_,
                    keyword_, 
                    _multicastList.numGroups,
                    _multicastList.groups[groupIndex].numSids);
      }
    }
    else
    {
      PSHELL_ERROR("Max servers: %d exceeded for multicast group: '%s'", PSHELL_MAX_SERVERS, _multicastList.groups[groupIndex].keyword);
    }
  }
  else
  {
    PSHELL_ERROR("Max multicast groups: %d exceeded", MAX_MULTICAST_GROUPS);
  }
  pthread_mutex_unlock(&_mutex);
}

/******************************************************************************/
/******************************************************************************/
void pshell_sendMulticast(const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int bytesFormatted = 0;
  char command[MAX_STRING_SIZE];
  PshellControl *control;
  char *keyword;
  va_list args;
  va_start(args, command_);
  bytesFormatted = vsnprintf(command, sizeof(command), command_, args);
  va_end(args);
  if (bytesFormatted < (int)sizeof(command))
  {
    for (int group = 0; group < _multicastList.numGroups; group++)
    {
      if ((strcmp(_multicastList.groups[group].keyword, PSHELL_MULTICAST_ALL) == 0) || 
          (((keyword = strstr(command, _multicastList.groups[group].keyword)) != NULL) && (keyword == command)))
      {
        /* we found a match, send command to all of our sids */
        for (int sid = 0; sid < _multicastList.groups[group].numSids; sid++)
        {
          if ((control = getControl(_multicastList.groups[group].sidList[sid])) != NULL)
          {
            /* 
             * since we are sending to multiple servers, we don't want any data 
             * back and we don't even want any response, we just do fire-and-forget
             */
            control->pshellMsg.header.dataNeeded = false;
            control->pshellMsg.header.respNeeded = false;
            sendPshellCommand(control, PSHELL_CONTROL_COMMAND, command, PSHELL_NO_WAIT);
            PSHELL_INFO("Sending multicast command: '%s' to sid: %d", command, _multicastList.groups[group].sidList[sid]);
          }
        }
      }
    }
  }
  else
  {
    PSHELL_WARNING("Command truncated: '%s', length exceeds %d bytes, %d bytes needed, command not sent", command, sizeof(command), bytesFormatted);
  }
  pthread_mutex_unlock(&_mutex);  
}

/******************************************************************************/
/******************************************************************************/
int pshell_sendCommand1(int sid_, const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int retCode = PSHELL_SOCKET_NOT_CONNECTED;
  int bytesFormatted = 0;
  char command[MAX_STRING_SIZE];
  PshellControl *control;
  va_list args;
  if ((control = getControl(sid_)) != NULL)
  {
    va_start(args, command_);
    bytesFormatted = vsnprintf(command, sizeof(command), command_, args);
    va_end(args);
    if (bytesFormatted < (int)sizeof(command))
    {
      control->pshellMsg.header.dataNeeded = false;
      retCode = sendPshellCommand(control, PSHELL_CONTROL_COMMAND, command, control->defaultTimeout);
    }
    else
    {
      PSHELL_WARNING("Command truncated: '%s', length exceeds %d bytes, %d bytes needed, command not sent", command, sizeof(command), bytesFormatted);
    }
  }
  pthread_mutex_unlock(&_mutex);  
  return (retCode);
}

/******************************************************************************/
/******************************************************************************/
int pshell_sendCommand2(int sid_, unsigned timeoutOverride_, const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int retCode = PSHELL_SOCKET_NOT_CONNECTED;
  int bytesFormatted = 0;
  char command[MAX_STRING_SIZE];
  PshellControl *control;
  va_list args;
  if ((control = getControl(sid_)) != NULL)
  {
    va_start(args, command_);
    bytesFormatted = vsnprintf(command, sizeof(command), command_, args);
    va_end(args);
    if (bytesFormatted < (int)sizeof(command))
    {
      control->pshellMsg.header.dataNeeded = false;
      retCode = sendPshellCommand(control, PSHELL_CONTROL_COMMAND, command, timeoutOverride_);
    }
    else
    {
      PSHELL_WARNING("Command truncated: '%s', length exceeds %d bytes, %d bytes needed, command not sent", command, sizeof(command), bytesFormatted);
    }
  }
  pthread_mutex_unlock(&_mutex);  
  return (retCode);
}

/******************************************************************************/
/******************************************************************************/
int pshell_sendCommand3(int sid_, char *results_, int size_, const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int retCode = PSHELL_SOCKET_NOT_CONNECTED;
  int bytesFormatted = 0;
  char command[MAX_STRING_SIZE];
  PshellControl *control;
  va_list args;
  if (results_ != NULL)
  {
    results_[0] = 0;
  }
  if ((control = getControl(sid_)) != NULL)
  {
    va_start(args, command_);
    bytesFormatted = vsnprintf(command, sizeof(command), command_, args);
    va_end(args);
    if (bytesFormatted < (int)sizeof(command))
    {
      if (!(control->pshellMsg.header.dataNeeded = (control->defaultTimeout != PSHELL_NO_WAIT)))
      {
        PSHELL_WARNING("Trying to extract data with a 0 wait timeout, no data will be extracted");
      }
      if (((retCode = sendPshellCommand(control, PSHELL_CONTROL_COMMAND, command, control->defaultTimeout)) == PSHELL_COMMAND_SUCCESS) &&
           (control->pshellMsg.header.dataNeeded))
      {
        extractResults(control, results_, size_);
      }
    }
    else
    {
      PSHELL_WARNING("Command truncated: '%s', length exceeds %d bytes, %d bytes needed, command not sent", command, sizeof(command), bytesFormatted);
    }
  }
  pthread_mutex_unlock(&_mutex);
  return (retCode);
}

/******************************************************************************/
/******************************************************************************/
int pshell_sendCommand4(int sid_, char *results_, int size_, unsigned timeoutOverride_, const char *command_, ...)
{
  pthread_mutex_lock(&_mutex);
  int retCode = PSHELL_SOCKET_NOT_CONNECTED;
  int bytesFormatted = 0;
  char command[MAX_STRING_SIZE];
  PshellControl *control;
  va_list args;
  if (results_ != NULL)
  {
    results_[0] = 0;
  }
  if ((control = getControl(sid_)) != NULL)
  {
    va_start(args, command_);
    bytesFormatted = vsnprintf(command, sizeof(command), command_, args);
    va_end(args);
    if (bytesFormatted < (int)sizeof(command))
    {
      if (!(control->pshellMsg.header.dataNeeded = (timeoutOverride_ != PSHELL_NO_WAIT)))
      {
        PSHELL_WARNING("Trying to extract data with a 0 wait timeout, no data will be extracted");
      }
      if (((retCode = sendPshellCommand(control, PSHELL_CONTROL_COMMAND, command, timeoutOverride_)) == PSHELL_COMMAND_SUCCESS) &&
           (control->pshellMsg.header.dataNeeded))
      {
        extractResults(control, results_, size_);
      }
    }
    else
    {
      PSHELL_WARNING("Command truncated: '%s', length exceeds %d bytes, %d bytes needed, command not sent", command, sizeof(command), bytesFormatted);
    }
  }
  pthread_mutex_unlock(&_mutex);  
  return (retCode);
}

/******************************************************************************/
/******************************************************************************/
const char *pshell_getResponseString(int results_)
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
      return ("PSHELL_UNKNOWN_RESPONSE");    
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
  if (port_ == PSHELL_UNIX_CONTROL)
  {

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
  else
  {    
    
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
      if (strcmp(remoteServer_, PSHELL_LOCALHOST) == 0)
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
  return (true);
}

/******************************************************************************/
/******************************************************************************/
int sendPshellCommand(PshellControl *control_, int commandType_, const char *command_, unsigned timeoutOverride_)
{
  fd_set readFd;
  struct timeval timeout;
  int bytesRead = 0;
  uint32_t seqNum;
  int retCode = PSHELL_COMMAND_SUCCESS;

  if (control_ != NULL)
  {
    
    FD_ZERO (&readFd);
    FD_SET(control_->socketFd,&readFd);

    control_->pshellMsg.header.msgType = commandType_;
    control_->pshellMsg.header.seqNum++;
    seqNum = control_->pshellMsg.header.seqNum;
    control_->pshellMsg.header.respNeeded = (timeoutOverride_ != PSHELL_NO_WAIT);
    control_->pshellMsg.payload[0] = 0;
    strncpy(control_->pshellMsg.payload, command_, PSHELL_PAYLOAD_SIZE);
    /* send command request to remote pshell */
    if (!sendPshellMsg(control_))
    {
      retCode = PSHELL_SOCKET_SEND_FAILURE;
    }
    else if (timeoutOverride_ != PSHELL_NO_WAIT)
    {
      /* they want a response from the remote pshell */
      /* timeout value is in milliSeconds */
      memset(&timeout, 0, sizeof(timeout));
      timeout.tv_sec = timeoutOverride_/1000;
      timeout.tv_usec = (timeoutOverride_%1000)*1000;
      
      /* loop in case we have any stale responses we need to flush from the socket */
      while (true)
      {
        if ((select(control_->socketFd+1, &readFd, NULL, NULL, &timeout)) < 0)
        {
          retCode = PSHELL_SOCKET_SELECT_FAILURE;
          break;
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
            break;
          }
          else if (seqNum > control_->pshellMsg.header.seqNum)
          {
            /* 
             * make sure we have the correct response, this condition can happen if we had 
             * a very short timeout for the previous call and missed the response, in which 
             * case the response to the previous call will be queued in the socket ahead of 
             * our current expected response, when we detect that condition, we read the 
             * socket until we either find the correct response or timeout, we toss any previous 
             * unmatched responses
             */
            PSHELL_WARNING("Received seqNum: %d, does not match sent seqNum: %d", control_->pshellMsg.header.seqNum, seqNum)
          }
          else
          {
            retCode = control_->pshellMsg.header.msgType;
            break;
          }
        }
        else
        {
          retCode = PSHELL_SOCKET_TIMEOUT;
          break;
        }
      }
      control_->pshellMsg.header.seqNum = seqNum;
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
                 pshell_getResponseString(retCode),
                 _errorPad,
                 control_->pshellMsg.payload);
  }
  else if ((retCode != PSHELL_COMMAND_SUCCESS) && (retCode != PSHELL_COMMAND_COMPLETE))
  {
    PSHELL_ERROR("Remote pshell command: '%s', %s", command_, pshell_getResponseString(retCode));
  }
  else
  {
    PSHELL_INFO("Remote pshell command: '%s', %s", command_, pshell_getResponseString(retCode));
    retCode = PSHELL_COMMAND_SUCCESS;
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
    results_[0] = 0;
    strncpy(results_, control_->pshellMsg.payload, size_);
    bytesExtracted = MIN(payloadSize, size_);
  }
  if ((bytesExtracted == size_) && (results_[bytesExtracted-1] != 0))
  {
    // make sure we NULL terminate if we have truncated
    results_[bytesExtracted-1] = 0;
    PSHELL_WARNING("Truncating results, size: %d, too small for returned payload: %d", size_, payloadSize);
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
    PSHELL_ERROR("Out of range sid: %d, valid range: 0-%d", sid_, PSHELL_MAX_SERVERS-1);
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
  char configFile[MAX_STRING_SIZE];
  char cwd[MAX_STRING_SIZE];
  char *configPath;
  char line[MAX_STRING_SIZE];
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
        line[strlen(line)-1] = '\0';
        if (((value = strstr(line, "=")) != NULL) &&
            ((option = strstr(line, ".")) != NULL))
        {
          value[0] = 0;
          value++;
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
              port_ = PSHELL_UNIX_CONTROL;
              isUnix = true;
            }
            else if (strcmp(option, "port") == 0)
            {
              port_ = atoi(value);
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
    /*
     * make this check in case they changed the server
     * from udp to unix and forgot to comment out the
     * port
     */
     if (isUnix)
     {
       port_ = PSHELL_UNIX_CONTROL;
     }
  }
}

/******************************************************************************/
/******************************************************************************/
void _printf(const char *format_, ...)
{
  char outputString[MAX_STRING_SIZE];
  int bytesFormatted = 0;
  va_list args;
  va_start(args, format_);
  bytesFormatted = vsnprintf(outputString, sizeof(outputString), format_, args);
  va_end(args);
  if (bytesFormatted < (int)sizeof(outputString))
  {
    if (_logFunction == NULL)
    {
      printf("%s", outputString);
    }
    else
    {
      (*_logFunction)(outputString);
    }
  }
  else
  {
    printf("PSHELL_WARNING: Output truncated: '%s', length exceeds %d bytes, %d bytes needed\n", outputString, (int)sizeof(outputString), bytesFormatted);
  }
}

