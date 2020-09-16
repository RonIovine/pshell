/////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2009, Ron Iovine, All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Ron Iovine nor the names of its contributors
//       may be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY Ron Iovine ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL Ron Iovine BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
/////////////////////////////////////////////////////////////////////////////////

//
// This API provides the ability for a client program to invoke pshell commands
// that are registered in a remote program that is running a pshell UDP or UNIX
// server.  The format of the command string that is sent to the pshell should
// be in the same usage format that the given command is expecting.  This
// provides a very lightweight way to provide a control mechanism into a program
// that is running a pshell, this is analagous to a remote procedure call (rpc).
//
// This module provides the same functionality as the PshellControl.h API and
// the libpshell-control linkable 'C' library, but implemented as a Go module.
//
// Functions:
//
//   ConnectServer()        -- connect to a remote pshell server
//   DisconnectServer()     -- disconnect from a remote pshell server
//   DisconnectAllServers() -- disconnect from all connected remote pshell servers
//   SetDefaultTimeout()    -- set the default server response timeout
//   AddMulticast()         -- add a command keyword to a multicast group
//   SendMulticast()        -- send a command to a multicast group
//   SendCommand1()         -- send command to server using default timeout, no results extracted
//   SendCommand2()         -- send command to server using timeout override, no results extracted
//   SendCommand3()         -- send command to server using default timeout, results extracted
//   SendCommand4()         -- send command to server using timeout override, results extracted
//   GetResponseString()    -- return the human readable form of one of the command response return codes
//   SetLogLevel()          -- set the internal log level for this module
//   SetLogFunction()       -- register a user function to receive all logs
//
// Integer constants:
//
// Helpful items used for the server response timeout values
//
//   NO_WAIT
//   ONE_MSEC
//   ONE_SEC
//   ONE_MINUTE
//   ONE_HOUR
//
// These are returned from the sendCommandN functions
//
//   COMMAND_SUCCESS
//   COMMAND_NOT_FOUND
//   COMMAND_INVALID_ARG_COUNT
//   SOCKET_SEND_FAILURE
//   SOCKET_SELECT_FAILURE
//   SOCKET_RECEIVE_FAILURE
//   SOCKET_TIMEOUT
//   SOCKET_NOT_CONNECTED
//
// Used if we cannot connect to the local source socket
//
//   INVALID_SID
//
// Constants to let the host program set the internal debug log level,
// if the user of this API does not want to see any internal message
// printed out, set the debug log level to LOG_LEVEL_NONE, the default
// log level is LOG_LEVEL_ALL
//
//   LOG_LEVEL_NONE
//   LOG_LEVEL_ERROR
//   LOG_LEVEL_WARNING
//   LOG_LEVEL_INFO
//   LOG_LEVEL_ALL
//   LOG_LEVEL_DEFAULT
//
// String constants:
//
// Use this when connecting to server running at loopback address
//
//   LOCALHOST
//
// Use this as the "port" identifier for the connectServer
// call when using a UNIX domain server
//
//   UNIX
//
// Specifies if the AddMulticast should add the given command to all specified
// multicast receivers or if all control destinations should receive the given
// multicast command
//
//   MULTICAST_ALL
//
// A complete example of the usage of the API can be found in the included
// demo program pshellControlDemo.go
//
package PshellControl

import "encoding/binary"
import "math/rand"
import "net"
import "time"
import "strings"
import "strconv"
import "io/ioutil"
import "os"
import "syscall"
import "fmt"

/////////////////////////////////////////////////////////////////////////////////
//
// global "public" data, these are used for various parts of the public API
//
/////////////////////////////////////////////////////////////////////////////////

// these enum values are returned by the non-extraction
// based sendCommand1 and sendCommand2 functions
//
// the "COMMAND" enums are returned by the remote pshell server
// and must match their corresponding values in PshellServer.cc
const (
  COMMAND_SUCCESS = 0
  COMMAND_NOT_FOUND = 1
  COMMAND_INVALID_ARG_COUNT = 2
  // the following "SOCKET" enums are generated internally by the sendCommandN functions
  SOCKET_SEND_FAILURE = 3
  SOCKET_SELECT_FAILURE = 4
  SOCKET_RECEIVE_FAILURE = 5
  SOCKET_TIMEOUT = 6
  SOCKET_NOT_CONNECTED = 7
)

// helpful items used for the timeout values
const NO_WAIT = 0
const ONE_MSEC = 1
const ONE_SEC = ONE_MSEC*1000
const ONE_MINUTE = ONE_SEC*60
const ONE_HOUR = ONE_MINUTE*60

// use this when connecting to server running at loopback address
const LOCALHOST = "localhost"

// use this as the "port" identifier for the connectServer call when
// using a UNIX domain server
const UNIX = "unix"

// specifies if the addMulticast should add the given command to all
// destinations or all commands to the specified destinations
const MULTICAST_ALL = "__multicast_all__"

// This is returned on a failure of the ConnectServer function
const INVALID_SID = -1

// constants to let the host program set the internal debug log level,
// if the user of this API does not want to see any internal message
// printed out, set the debug log level to LOG_LEVEL_NONE (0)
const (
  LOG_LEVEL_NONE = 0
  LOG_LEVEL_ERROR = 1
  LOG_LEVEL_WARNING = 2
  LOG_LEVEL_INFO = 3
  LOG_LEVEL_ALL = LOG_LEVEL_INFO
  LOG_LEVEL_DEFAULT = LOG_LEVEL_WARNING
)

/////////////////////////////////////////////////////////////////////////////////
//
// global "private" data
//
/////////////////////////////////////////////////////////////////////////////////

const _UNIX_SOCKET_PATH = "/tmp/.pshell/"
const _LOCK_FILE_EXTENSION = ".lock"
const _NO_RESP_NEEDED = 0
const _NO_DATA_NEEDED = 0
const _RESP_NEEDED = 1
const _DATA_NEEDED = 1
const _RCV_BUFFER_SIZE = 1024*64  // 64k buffer size

const _PSHELL_CONFIG_DIR = "/etc/pshell/config"
const _PSHELL_CONFIG_FILE = "pshell-control.conf"

type logFunction func(string)
var _logLevel = LOG_LEVEL_DEFAULT
var _logFunction logFunction

type pshellControl struct {
  socket net.Conn
  defaultTimeout int
  serverType string
  unixLockFd *os.File
  sourceAddress string
  sendMsg []byte
  recvMsg []byte
  recvSize int
  controlName string
  remoteServer string
}
var _gControlList = []pshellControl{}

type pshellMulticast struct {
  command string
  sidList []int
}
var _gMulticastList = []pshellMulticast{}

var _gPshellControlResponse = map[int]string {
  INVALID_SID:"PSHELL_INVALID_SID",
  COMMAND_SUCCESS:"PSHELL_COMMAND_SUCCESS",
  COMMAND_NOT_FOUND:"PSHELL_COMMAND_NOT_FOUND",
  COMMAND_INVALID_ARG_COUNT:"PSHELL_COMMAND_INVALID_ARG_COUNT",
  SOCKET_SEND_FAILURE:"PSHELL_SOCKET_SEND_FAILURE",
  SOCKET_SELECT_FAILURE:"PSHELL_SOCKET_SELECT_FAILURE",
  SOCKET_RECEIVE_FAILURE:"PSHELL_SOCKET_RECEIVE_FAILURE",
  SOCKET_TIMEOUT:"PSHELL_SOCKET_TIMEOUT",
  SOCKET_NOT_CONNECTED:"PSHELL_SOCKET_NOT_CONNECTED",
}

const (
  _COMMAND_COMPLETE = 8
  _CONTROL_COMMAND = 12
)

/////////////////////////////////
//
// Public functions
//
/////////////////////////////////

//
//  Connect to a pshell server in another process, note that this does
//  not do any handshaking to the remote pshell or maintain a connection
//  state, it meerly sets the internal destination remote pshell server
//  information to use when sending commands via the sendCommandN
//  functions and sets up any resources necessary for the source socket,
//  the timeout value is the number of milliseconds to wait for a response
//  from the remote command in the sendCommandN functions, a timeout
//  value of 0 will not wait for a response, in which case the function
//  will return either SOCKET_NOT_CONNECTED, SOCKET_SEND_FAILURE, or
//  COMMAND_SUCCESS, the timeout value entered in this function will
//  be used as the default timeout for all calls to sendCommandN that
//  do not provide an override timeout value, for a UDP server, the
//  remoteServer must be either a valid hostname or IP address and a
//  valid destination port must be provided, for a UNIX server, only
//  a valid server name must be provided along with the identifier
//  PshellControl.UNIX for the 'port' parameter
//
//  This function returns a Server ID (sid) handle which must be saved and
//  used for all subsequent calls into this module
//
//    Args:
//        controlName (str)    : The logical name of the control server
//        remoteServer (str)   : The server name (UNIX) or hostname/IP address (UDP) of the remote server
//        port (str)           : The UDP port of the remote server
//        defaultTimeout (int) : The default timeout (in msec) for the remote server response
//
//    Returns:
//        int: The ServerId (sid) handle of the connected server or INVALID_SID on failure
//
func ConnectServer(controlName string, remoteServer string, port string, defaultTimeout int) int {
  return (connectServer(controlName, remoteServer, port, defaultTimeout))
}

//
//  Cleanup any resources associated with the server connection, including
//  releasing any temp file handles, closing any local socket handles etc.
//
//    Args:
//        sid (int) : The ServerId as returned from the connectServer call
//
//    Returns:
//        none
//
func DisconnectServer(sid int) {
  disconnectServer(sid)
}

//
//  Use this function to cleanup any resources for all connected servers, this
//  function should be called upon program termination, either in a graceful
//  termination or within an exception signal handler, it is especially important
//  that this be called when a unix server is used since there are associated file
//  handles that need to be cleaned up
//
//    Args:
//        none
//
//    Returns:
//        none
//
func DisconnectAllServers() {
  disconnectAllServers()
}

//
//  Set the default server response timeout that is used in the 'send' commands
//  that don't take a timeout override
//
//    Args:
//        sid (int)            : The ServerId as returned from the connectServer call
//        defaultTimeout (int) : The default timeout (in msec) for the remote server response
//
//    Returns:
//        none
//
func SetDefaultTimeout(sid int, defaultTimeout int) {
  setDefaultTimeout(sid, defaultTimeout)
}

//
// This command will add a controlList of multicast receivers to a multicast
// group, multicast groups are based either on the command, or if the special
// argument PshellControl.MULTICAST_ALL is used, the given controlList will receive
// all multicast commands, the format of the controlList is a CSV formatted list
// of all the desired controlNames (as provided in the first argument of the
// PshellControl.ConnectServer command) that will receive this multicast command
// or if the PshellControl.MULTICAST_ALL is used then all control destinations will
// receive the given multicast command, see examples below
//
// ex 1: multicast command sent to receivers in CSV controlList
//
//   PshellControl.AddMulticast("command", "control1,control2,control3")
//
// ex 2: all multicast commands sent to receivers in CSV controlList
//
//   PshellControl.AddMulticast(PshellControl.MULTICAST_ALL, "control1,control2,control3")
//
// ex 3: multicast command sent to all receivers
//
//   PshellControl.AddMulticast("command", PshellControl.MULTICAST_ALL)
//
// ex 4: all multicast commands sent to all receivers
//
//  PshellControl.AddMulticast(PshellControl.MULTICAST_ALL, PshellControl.MULTICAST_ALL)
//
//    Args:
//        command (str)     : The multicast command that will be distributed to the
//                            following controlList, if the special MULTICAST_ALL
//                            identifier is used, then the controlList will receive
//                            all multicast initiated commands
//        controlList (str) : A CSV formatted list of all the desired controlNames
//                            (as provided in the first argument of the connectServer
//                            command) that will receive this multicast command, if
//                            the special MULTICAST_ALL identifier is used, then
//                            all control destinations will receive the command
//
//    Returns:
//        none
//
func AddMulticast(command string, controlList string) {
  addMulticast(command, controlList)
}

//
//  This command will send a given command to all the registered multicast
//  receivers (i.e. sids) for this multicast group, multicast groups are
//  based on the command's keyword, this function will issue the command as
//  a best effort fire-and-forget command to each receiver in the multicast
//  group, no results will be requested or expected, and no response will be
//  requested or expected
//
//    Args:
//        command (str) : The command to send to the remote server
//
//    Returns:
//        none
//
func SendMulticast(format string, command ...interface{}) {
  sendMulticast(format, command...)
}

//
//  Send a command using the default timeout setup in the connectServer call,
//  if the default timeout is 0, the server will not reply with a response and
//  this function will not wait for one
//
//    Args:
//        sid (int)     : The ServerId as returned from the connectServer call
//        command (str) : The command to send to the remote server
//
//    Returns:
//        int: Return code result of the command:
//               COMMAND_SUCCESS
//               COMMAND_NOT_FOUND
//               COMMAND_INVALID_ARG_COUNT
//               SOCKET_SEND_FAILURE
//               SOCKET_SELECT_FAILURE
//               SOCKET_RECEIVE_FAILURE
//               SOCKET_TIMEOUT
//               SOCKET_NOT_CONNECTED
//
func SendCommand1(sid int, format string, command ...interface{}) int {
  return (sendCommand1(sid, format, command...))
}

//
//  Send a command overriding the default timeout, if the override timeout is 0,
//  the server will not reply with a response and this function will not wait for
//  one
//
//    Args:
//        sid (int)             : The ServerId as returned from the connectServer call
//        timeoutOverride (int) : The server timeout override (in msec) for this command
//        command (str)         : The command to send to the remote server
//
//    Returns:
//        int: Return code result of the command:
//               COMMAND_SUCCESS
//               COMMAND_NOT_FOUND
//               COMMAND_INVALID_ARG_COUNT
//               SOCKET_SEND_FAILURE
//               SOCKET_SELECT_FAILURE
//               SOCKET_RECEIVE_FAILURE
//               SOCKET_TIMEOUT
//               SOCKET_NOT_CONNECTED
//
func SendCommand2(sid int, timeoutOverride int, format string, command ...interface{}) int {
  return (sendCommand2(sid, timeoutOverride, format, command...))
}

//
//  Send a command using the default timeout setup in the connectServer call and
//  return any results received in the payload, if the default timeout is 0, the
//  server will not reply with a response and this function will not wait for one,
//  and no results will be extracted
//
//    Args:
//        sid (int)     : The ServerId as returned from the connectServer call
//        command (str) : The command to send to the remote server
//
//    Returns:
//        str: The human readable results of the command response or NULL
//             if no results or command failure
//        int: Return code result of the command:
//               COMMAND_SUCCESS
//               COMMAND_NOT_FOUND
//               COMMAND_INVALID_ARG_COUNT
//               SOCKET_SEND_FAILURE
//               SOCKET_SELECT_FAILURE
//               SOCKET_RECEIVE_FAILURE
//               SOCKET_TIMEOUT
//               SOCKET_NOT_CONNECTED
//
func SendCommand3(sid int, format string, command ...interface{}) (int, string) {
  return (sendCommand3(sid, format, command...))
}

//
//  Send a command overriding the default timeout and return any results received
//  in the payload, if the timeout override default timeout is 0, the server will
//  not reply with a response and this function will not wait for one, and no
//  results will be extracted
//
//    Args:
//        sid (int)             : The ServerId as returned from the connectServer call
//        timeoutOverride (int) : The server timeout override (in msec) for this command
//        command (str)         : The command to send to the remote server
//
//    Returns:
//        str: The human readable results of the command response or NULL
//             if no results or command failure
//        int: Return code result of the command:
//               COMMAND_SUCCESS
//               COMMAND_NOT_FOUND
//               COMMAND_INVALID_ARG_COUNT
//               SOCKET_SEND_FAILURE
//               SOCKET_SELECT_FAILURE
//               SOCKET_RECEIVE_FAILURE
//               SOCKET_TIMEOUT
//               SOCKET_NOT_CONNECTED
//
func SendCommand4(sid int, timeoutOverride int, format string, command ...interface{}) (int, string) {
  return (sendCommand4(sid, timeoutOverride, format, command...))
}

//
// Returns the string value corresponding to a received response code
//
//  Args:
//      retCode (int) : One of the response code enum constants
//
//  Returns:
//      str: The string representation of the enum value
//
func GetResponseString(retCode int) string {
  return (getResponseString(retCode))
}

//
//  Set the internal log level, valid levels are:
//
//  LOG_LEVEL_ERROR
//  LOG_LEVEL_WARNING
//  LOG_LEVEL_INFO
//  LOG_LEVEL_ALL
//  LOG_LEVEL_DEFAULT
//
//  Where the default is LOG_LEVEL_ALL
//
//    Args:
//        level (int) : The desired log level to set
//
//    Returns:
//        None
//
func SetLogLevel(level int) {
  setLogLevel(level)
}

//
//  Provide a user callback function to send the logs to, this allows an
//  application to get all the logs issued by this module to put in it's
//  own logfile.  If a log function is not set, all internal logs are just
//  sent to the 'print' function.
//
//    Args:
//        function (ptr) : Log callback function
//
//    Returns:
//        None
//
func SetLogFunction(function logFunction) {
  setLogFunction(function)
}

/////////////////////////////////
//
// Private functions
//
/////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func connectServer(controlName_ string, remoteServer_ string, port_ string, defaultTimeout_ int) int {
  // setup our destination socket
  var err error
  var socket net.Conn
  var sourceAddress string
  var unixLockFd *os.File
  cleanupUnixResources()
  sid := getSid(controlName_)
  if (sid == INVALID_SID) {
    remoteServer_, port_, defaultTimeout_ = loadConfigFile(controlName_, remoteServer_, port_, defaultTimeout_)
    if (port_ == UNIX) {
      // UNIX domain socket
      remoteAddr := net.UnixAddr{_UNIX_SOCKET_PATH+remoteServer_, "unixgram"}
      rand := rand.New(rand.NewSource(time.Now().UnixNano()))
      for {
        sourceAddress = _UNIX_SOCKET_PATH+remoteServer_+"-control"+strconv.FormatUint(uint64(rand.Uint32()%1000), 10)
        localAddr := net.UnixAddr{sourceAddress, "unixgram"}
        unixLockFile := sourceAddress+_LOCK_FILE_EXTENSION
        unixLockFd, err = os.Create(unixLockFile)
        if err == nil {
          err = syscall.Flock(int(unixLockFd.Fd()), syscall.LOCK_EX | syscall.LOCK_NB)
          if err == nil {
            socket, err = net.DialUnix("unixgram", &localAddr, &remoteAddr)
            if err == nil {
              break
            }
          }
        }
      }
      _gControlList = append(_gControlList,
                             pshellControl{socket,
                                           defaultTimeout_,
                                           "unix",
                                           unixLockFd,
                                           sourceAddress,                   // unix file handle, used for cleanup
                                           []byte{},                        // sendMsg
                                           make([]byte, _RCV_BUFFER_SIZE),  // recvMsg
                                           0,                               // recvSize
                                           controlName_,
                                           strings.Join([]string{controlName_, "[", remoteServer_, "]"}, "")})
      sid = len(_gControlList)-1
    } else {
      // IP (UDP) domain socket
      remoteAddr, _ := net.ResolveUDPAddr("udp", strings.Join([]string{remoteServer_, ":", port_,}, ""))
      socket, err = net.DialUDP("udp", nil, remoteAddr)
      if (err == nil) {
        _gControlList = append(_gControlList,
                               pshellControl{socket,
                                             defaultTimeout_,
                                             "udp",
                                             unixLockFd,
                                             "",                              // sourceAddress not used for UDP socket
                                             []byte{},                        // sendMsg
                                             make([]byte, _RCV_BUFFER_SIZE),  // recvMsg
                                             0,                               // recvSize
                                             controlName_,
                                             strings.Join([]string{controlName_, "[", remoteServer_, "]"}, "")})

        sid = len(_gControlList)-1
      }
    }
  } else {
    printWarning("Control name: '%s' already exists, must use unique control name", controlName_)
  }
  return (sid)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func cleanupUnixResources() {
  if _, err := os.Stat(_UNIX_SOCKET_PATH); os.IsNotExist(err) {
    os.Mkdir(_UNIX_SOCKET_PATH, 0777)
    os.Chmod(_UNIX_SOCKET_PATH, 0777)
  }
  fileInfo, err := ioutil.ReadDir(_UNIX_SOCKET_PATH)
  if err == nil {
    for _, file := range fileInfo {
      if strings.HasSuffix(file.Name(), _LOCK_FILE_EXTENSION) {
        // try to open lock file
        unixLockFile := file.Name()
        unixSocketFile := strings.Split(unixLockFile, ".")[0]
        unixLockFd, err := os.Open(unixLockFile)
        if err == nil {
          err = syscall.Flock(int(unixLockFd.Fd()), syscall.LOCK_EX | syscall.LOCK_NB)
          // file exists, try to see if another process has it locked
          if err == nil {
            // we got the lock, nobody else has it, ok to clean it up
            os.Remove(unixSocketFile)
            os.Remove(unixLockFile)
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func disconnectServer(sid_ int) {
  if ((sid_ >= 0) && (sid_ < len(_gControlList))) {
    control := _gControlList[sid_]
    if (control.serverType == UNIX) {
      os.Remove(control.sourceAddress)
      os.Remove(control.sourceAddress+_LOCK_FILE_EXTENSION)
    }
  }
  cleanupUnixResources()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func disconnectAllServers() {
  for sid, _ := range(_gControlList) {
    disconnectServer(sid)
  }
  _gControlList = []pshellControl{}
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addMulticast(command_ string, controlList_ string) {
  if (controlList_ == MULTICAST_ALL) {
    for sid, _ := range(_gControlList) {
      addMulticastSid(command_, sid)
    }
  } else {
    controlNames := strings.Split(strings.TrimSpace(controlList_), ",")
    for _, controlName := range(controlNames) {
      sid := getSid(controlName)
      if (sid != INVALID_SID) {
        addMulticastSid(command_, sid)
      } else {
        printWarning("Control name: '%s' not found", controlName)
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getSid(controlName_ string) int {
  for sid, control := range(_gControlList) {
    if (control.controlName == controlName_) {
      return (sid)
    }
  }
  return (INVALID_SID)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addMulticastSid(command_ string, sid_ int) {
  var sidList []int
  multicastFound := false
  for _, multicast := range(_gMulticastList) {
    if (multicast.command == command_) {
      multicastFound = true
      sidList = multicast.sidList
      break
    }
  }
  if (multicastFound == false) {
    // multicast entry not found for this keyword, add a new one
    _gMulticastList = append(_gMulticastList, pshellMulticast{command_, []int{}})
    sidList = _gMulticastList[len(_gMulticastList)-1].sidList
  }
  sidFound := false
  for _, sid := range(sidList) {
    if (sid == sid_) {
      sidFound = true
      break
    }
  }
  if (sidFound == false) {
    // sid not found for this multicast group, add it for this group
    sidList = append(sidList, sid_)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendMulticast(format_ string, command_ ...interface{}) {
  command := fmt.Sprintf(format_, command_...)
  keyword := strings.Split(strings.TrimSpace(command), " ")[0]
  keywordFound := false
  for _, multicast := range(_gMulticastList) {
    if ((multicast.command == MULTICAST_ALL) || (keyword == multicast.command)) {
      keywordFound = true
      for _, sid := range(multicast.sidList) {
        if ((sid >= 0) && (sid < len(_gControlList))) {
          control := _gControlList[sid]
          sendCommand(&control, command, NO_WAIT, _NO_DATA_NEEDED)
        }
      }
    }
  }
  if keywordFound == false {
    printError("Multicast command: '%s', not found", command)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setDefaultTimeout(sid_ int, defaultTimeout_ int) {
  if ((sid_ >= 0) && (sid_ < len(_gControlList))) {
    control := _gControlList[sid_]
    control.defaultTimeout = defaultTimeout_
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getResponseString(retCode_ int) string {
  if (retCode_ < len(_gPshellControlResponse)) {
    return (_gPshellControlResponse[retCode_])
  } else {
    return ("PSHELL_UNKNOWN_RESPONSE")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand1(sid_ int, format_ string, command_ ...interface{}) int {
  if ((sid_ >= 0) && (sid_ < len(_gControlList))) {
    control := _gControlList[sid_]
    return sendCommand(&control, fmt.Sprintf(format_, command_...), control.defaultTimeout, _NO_DATA_NEEDED)
  } else {
    printError("No control defined for sid: %d", sid_)
    return INVALID_SID
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand2(sid_ int, timeoutOverride_ int, format_ string, command_ ...interface{}) int {
  if ((sid_ >= 0) && (sid_ < len(_gControlList))) {
    control := _gControlList[sid_]
    return sendCommand(&control, fmt.Sprintf(format_, command_...), timeoutOverride_, _NO_DATA_NEEDED)
  } else {
    printError("No control defined for sid: %d", sid_)
    return INVALID_SID
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand3(sid_ int, format_ string, command_ ...interface{}) (int, string) {
  if ((sid_ >= 0) && (sid_ < len(_gControlList))) {
    control := _gControlList[sid_]
    return sendCommand(&control, fmt.Sprintf(format_, command_...), control.defaultTimeout, _DATA_NEEDED),
           getPayload(control.recvMsg, control.recvSize)
  } else {
    printError("No control defined for sid: %d", sid_)
    return INVALID_SID, ""
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand4(sid_ int, timeoutOverride_ int, format_ string, command_ ...interface{}) (int, string) {
  if ((sid_ >= 0) && (sid_ < len(_gControlList))) {
    control := _gControlList[sid_]
    return sendCommand(&control, fmt.Sprintf(format_, command_...), timeoutOverride_, _DATA_NEEDED),
           getPayload(control.recvMsg, control.recvSize)
  } else {
    printError("No control defined for sid: %d", sid_)
    return INVALID_SID, ""
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setLogLevel(level_ int) {
  _logLevel = level_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setLogFunction(function_ logFunction) {
  _logFunction = function_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func printError(format_ string, message_ ...interface{}) {
  if _logLevel >= LOG_LEVEL_ERROR {
    printLog("PSHELL_ERROR: "+fmt.Sprintf(format_, message_...)+"\n")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func printWarning(format_ string, message_ ...interface{}) {
  if _logLevel >= LOG_LEVEL_WARNING {
    printLog("PSHELL_WARNING: "+fmt.Sprintf(format_, message_...)+"\n")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func printInfo(format_ string, message_ ...interface{}) {
  if _logLevel >= LOG_LEVEL_INFO {
    printLog("PSHELL_INFO: "+fmt.Sprintf(format_, message_...)+"\n")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func printLog(message_ string) {
  if _logFunction != nil {
    _logFunction(message_)
  } else {
    fmt.Printf(message_)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand(control_ *pshellControl, command_ string, timeout_ int, dataNeeded_ byte) int {
  retCode := COMMAND_SUCCESS
  sendSeqNum := getSeqNum(control_.recvMsg)+1
  if (timeout_ > 0) {
    control_.sendMsg = createMessage(_CONTROL_COMMAND, _RESP_NEEDED, dataNeeded_, sendSeqNum, command_)
  } else {
    // timeout is 0, fire and forget message, do not request a response
    control_.sendMsg = createMessage(_CONTROL_COMMAND, _NO_RESP_NEEDED, dataNeeded_, sendSeqNum, command_)
  }
  _, err := control_.socket.Write(control_.sendMsg)
  if (err == nil) {
    if (timeout_ > NO_WAIT) {
      for {
        control_.socket.SetReadDeadline(time.Now().Add(time.Millisecond*time.Duration(timeout_)))
        var err error
        control_.recvSize, err = control_.socket.Read(control_.recvMsg)
        if (err == nil) {
          retCode = int(getMsgType(control_.recvMsg))
          recvSeqNum := getSeqNum(control_.recvMsg)
          if (sendSeqNum > recvSeqNum) {
            // make sure we have the correct response, this condition can happen if we had
            // a very short timeout for the previous call and missed the response, in which
            // case the response to the previous call will be queued in the socket ahead of
            // our current expected response, when we detect that condition, we read the
            // socket until we either find the correct response or timeout, we toss any previous
            // unmatched responses
            printWarning("Received seqNum: %d, does not match sent seqNum: %d", recvSeqNum, sendSeqNum)
          } else {
            break
          }
        } else {
          retCode = SOCKET_TIMEOUT
        }
      }
    } else if (dataNeeded_ == _DATA_NEEDED) {
      printWarning("Trying to extract data with a 0 wait timeout, no data will be extracted")
    }
    if (retCode == _COMMAND_COMPLETE) {
      retCode = COMMAND_SUCCESS
    }
  } else {
    retCode = SOCKET_SEND_FAILURE
  }

  return retCode
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func loadConfigFile(controlName_ string, remoteServer_ string, port_ string, defaultTimeout_ int) (string, string, int) {
  var configFile1 = ""
  var file []byte
  configPath := os.Getenv("PSHELL_CONFIG_DIR")
  if (configPath != "") {
    configFile1 = configPath+"/"+_PSHELL_CONFIG_FILE
  }
  configFile2 := _PSHELL_CONFIG_DIR+"/"+_PSHELL_CONFIG_FILE
  cwd, _ := os.Getwd()
  configFile3 := cwd+"/"+_PSHELL_CONFIG_FILE
  if _, err := os.Stat(configFile1); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(configFile1)
  } else if _, err := os.Stat(configFile2); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(configFile2)
  } else if _, err := os.Stat(configFile3); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(configFile3)
  } else {
    return remoteServer_, port_, defaultTimeout_
  }
  // found a config file, process it
  isUnix := false
  lines := strings.Split(string(file), "\n")
  for _, line := range lines {
    // skip comments
    if ((len(line) > 0) && (line[0] != '#')) {
      option := strings.Split(line, "=")
      if (len(option) == 2) {
        control := strings.Split(option[0], ".")
        if ((len(control) == 2) && (controlName_ == control[0])) {
          if (strings.ToLower(control[1]) == "udp") {
            remoteServer_ = option[1]
          } else if (strings.ToLower(control[1]) == "unix") {
            remoteServer_ = option[1]
            port_ = "unix"
            isUnix = true
          } else if (strings.ToLower(control[1]) == "port") {
            port_ = option[1]
          } else if (strings.ToLower(control[1]) == "timeout") {
            if (strings.ToLower(option[1]) == "none") {
              defaultTimeout_ = 0
            } else {
              defaultTimeout_, _ = strconv.Atoi(option[1])
            }
          }
        }
      }
    }
  }
  // make this check in case they changed the server
  // from udp to unix and forgot to comment out the
  // port
  if (isUnix) {
    port_ = "unix"
  }
  return remoteServer_, port_, defaultTimeout_
}

////////////////////////////////////////////////////////////////////////////////
//
// PshellMsg datagram message processing functions
//
// A PshellMsg is just a byte slice, there is a small 8 byte header along
// with an ascii byte payload as follows:
//
//   type PshellMsg struct {
//     msgType byte
//     respNeeded byte
//     dataNeeded byte
//     pad byte
//     seqNum uint32
//     payload string
//   }
//
// I did not have any luck serializing this using 'gob' or binary/encoder to
// send 'over-the-wire' as-is, so I am just representing this as a byte slice
// and packing/extracting the header elements myself based on byte offsets,
// everything in the message except the 4 byte seqNum are single bytes, so I
// didn't think this was to bad.  There is probably a correct way to do this
// in 'go', but since I'm new to the language, this was the easiest way I got
// it to work.
//
////////////////////////////////////////////////////////////////////////////////

// create offsets into the byte slice for the various items in the msg header
const (
  _MSG_TYPE_OFFSET = 0
  _RESP_NEEDED_OFFSET = 1
  _DATA_NEEDED_OFFSET = 2
  _SEQ_NUM_OFFSET = 4
  _PAYLOAD_OFFSET = 8
)

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getPayload(message_ []byte, recvSize_ int) string {
  return (string(message_[_PAYLOAD_OFFSET:recvSize_]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getMsgType(message_ []byte) byte {
  return (message_[_MSG_TYPE_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getSeqNum(message_ []byte) uint32 {
  return (binary.BigEndian.Uint32(message_[_SEQ_NUM_OFFSET:]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setSeqNum(message_ []byte, seqNum_ uint32) {
  binary.BigEndian.PutUint32(message_[_SEQ_NUM_OFFSET:], seqNum_)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createMessage(msgType_ byte, respNeeded_ byte, dataNeeded_ byte, seqNum_ uint32, command_ string) []byte {
  message := []byte{msgType_, respNeeded_, dataNeeded_, 0, 0, 0, 0, 0}
  setSeqNum(message, seqNum_)
  message = append(message, []byte(command_)...)
  return (message)
}

