package PshellControl

import "encoding/binary"
import "math/rand"
import "net"
import "time"
import "strings"
import "strconv"
import "io/ioutil"
import "os"
import "fmt"

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

// use this as the "port" identifier for the connectServer call when
// using a UNIX domain server
const UNIX = "unix"

const _UNIX_SOCKET_PATH = "/tmp/"

// This is returned on a failure of the ConnectServer function
const INVALID_SID = -1

const _NO_RESP_NEEDED = 0
const _NO_DATA_NEEDED = 0
const _RESP_NEEDED = 1
const _DATA_NEEDED = 1

const _PSHELL_CONFIG_DIR = "/etc/pshell/config"
const _PSHELL_CONFIG_FILE = "pshell-control.conf"

type pshellControl struct {
  socket net.Conn
  defaultTimeout int
  serverType string
  sourceAddress string
  sendMsg []byte
  recvMsg []byte
  recvSize int
  remoteServer string
}
var gControlList = []pshellControl{}

type pshellMulticast struct {
  sidList []int
  keyword string
}
var gMulticastList = []pshellMulticast{}

var gPshellControlResponse = map[int]string {
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
//  COMMAND_SUCCESS, the timeout value entered in this funcition will
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
//  This command will add a given multicast receiver (i.e. sid) to a multicast 
//  group, multicast groups are either based on the command's keyword, or if 
//  no keyword is supplied, the given sid will receive all multicast commands
//
//    Args:
//        sid (int)     : The ServerId as returned from the connectServer call
//        keyword (str) : The multicast keyword that the sid is associated with
//                        If no keyword is supplied, all multicast commands will
//                        go to the corresponding sid
//
//    Returns:
//        none
//
func AddMulticast(sid int, keyword string) {
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
func SendMulticast(command string) {
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

/////////////////////////////////
//
// Private functions
//
/////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func connectServer(controlName_ string, remoteServer_ string, port_ string, defaultTimeout_ int) int {
  // setup our destination socket
  var retCode error
  var sid = INVALID_SID
  var socket net.Conn
  var sourceAddress string
  remoteServer_, port_, defaultTimeout_ = loadConfigFile(controlName_, remoteServer_, port_, defaultTimeout_)
  if (port_ == UNIX) {
    if _, err := os.Stat(_UNIX_SOCKET_PATH+remoteServer_); !os.IsNotExist(err) {
      // UNIX domain socket
      remoteAddr := net.UnixAddr{_UNIX_SOCKET_PATH+remoteServer_, "unixgram"}
      rand := rand.New(rand.NewSource(time.Now().UnixNano()))
      for {
        sourceAddress = _UNIX_SOCKET_PATH+remoteServer_+strconv.FormatUint(uint64(rand.Uint32()%1000), 10)
        localAddr := net.UnixAddr{sourceAddress, "unixgram"}
        if socket, retCode = net.DialUnix("unixgram", &localAddr, &remoteAddr); retCode == nil {
          break
        }
      }
      gControlList = append(gControlList, 
                            pshellControl{socket,
                                          defaultTimeout_, 
                                          "unix",
                                          sourceAddress,      // unix file handle, used for cleanup
                                          []byte{},           // sendMsg
                                          make([]byte, 2048), // recvMsg
                                          0,                  // recvSize
                                          strings.Join([]string{controlName_, "[", remoteServer_, "]"}, "")})
      sid = len(gControlList)-1
    } else {
      fmt.Printf("PSHELL_ERROR: Could not find unix server: '%s'\n", _UNIX_SOCKET_PATH+remoteServer_)
    }
  } else {
    // IP (UDP) domain socket
    remoteAddr, _ := net.ResolveUDPAddr("udp", strings.Join([]string{remoteServer_, ":", port_,}, ""))
    socket, retCode = net.DialUDP("udp", nil, remoteAddr)
    if (retCode == nil) {
      gControlList = append(gControlList, 
                            pshellControl{socket,
                                          defaultTimeout_, 
                                          "udp",
                                          "",                 // sourceAddress not used for UDP socket
                                          []byte{},           // sendMsg
                                          make([]byte, 2048), // recvMsg
                                          0,                  // recvSize
                                          strings.Join([]string{controlName_, "[", remoteServer_, "]"}, "")})
                                      
      sid = len(gControlList)-1
    }
  }
  return (sid)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func disconnectServer(sid int) {
  if ((sid >= 0) && (sid < len(gControlList))) {
    control := gControlList[sid]
    if (control.serverType == UNIX) {
      os.Remove(control.sourceAddress)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func disconnectAllServers() {
  for sid, _ := range(gControlList) {
    disconnectServer(sid)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setDefaultTimeout(sid_ int, defaultTimeout_ int) {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    control.defaultTimeout = defaultTimeout_
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getResponseString(retCode_ int) string {
  if (retCode_ < len(gPshellControlResponse)) {
    return (gPshellControlResponse[retCode_])
  } else {
    return ("PSHELL_UNKNOWN_RESPONSE")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand1(sid_ int, format_ string, command_ ...interface{}) int {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    return sendCommand(&control, fmt.Sprintf(format_, command_...), control.defaultTimeout, _NO_DATA_NEEDED)
  } else {
    return INVALID_SID
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand2(sid_ int, timeoutOverride_ int, format_ string, command_ ...interface{}) int {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    return sendCommand(&control, fmt.Sprintf(format_, command_...), timeoutOverride_, _NO_DATA_NEEDED)
  } else {
    return INVALID_SID
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand3(sid_ int, format_ string, command_ ...interface{}) (int, string) {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    return sendCommand(&control, fmt.Sprintf(format_, command_...), control.defaultTimeout, _DATA_NEEDED),
           getPayload(control.recvMsg, control.recvSize)
  } else {
    return INVALID_SID, ""
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand4(sid_ int, timeoutOverride_ int, format_ string, command_ ...interface{}) (int, string) {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    return sendCommand(&control, fmt.Sprintf(format_, command_...), timeoutOverride_, _DATA_NEEDED),
           getPayload(control.recvMsg, control.recvSize)
  } else {
    return INVALID_SID, ""
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
    if (timeout_ > 0) {
      for {
        control_.socket.SetReadDeadline(time.Now().Add(time.Second*time.Duration(timeout_)))
        var err error
        control_.recvSize, err = control_.socket.Read(control_.recvMsg)
        if (err == nil) {
          retCode = int(getMsgType(control_.recvMsg))
          recvSeqNum := getSeqNum(control_.recvMsg)
          if (sendSeqNum == recvSeqNum) {
            break
          }
        } else {
          retCode = SOCKET_TIMEOUT
        }
      }
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
// everything in the message except the 4 byte seqNum is bytes, so I didn't
// think this was to bad.  There is probably a correct way to do this in 'go',
// but since I'm new to the language, this was the easiest way I got it to 
// work.
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
func getPayload(message []byte, recvSize int) string {
  return (string(message[_PAYLOAD_OFFSET:recvSize]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getMsgType(message []byte) byte {
  return (message[_MSG_TYPE_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getSeqNum(message []byte) uint32 {
  return (binary.BigEndian.Uint32(message[_SEQ_NUM_OFFSET:]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setSeqNum(message []byte, seqNum uint32) {
  binary.BigEndian.PutUint32(message[_SEQ_NUM_OFFSET:], seqNum)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createMessage(msgType byte, respNeeded byte, dataNeeded byte, seqNum uint32, command string) []byte {
  message := []byte{msgType, respNeeded, dataNeeded, 0, 0, 0, 0, 0}
  setSeqNum(message, seqNum)
  message = append(message, []byte(command)...)
  return (message)
}

