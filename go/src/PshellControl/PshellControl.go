package PshellControl

import "encoding/binary"
import "net"
import "time"
import "strings"
//import "fmt"

// the following enum values are returned by the non-extraction
// based sendCommand1 and sendCommand2 functions
//
// the following "COMMAND" enums are returned by the remote pshell server
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

const INVALID_SID = -1

const NO_RESP_NEEDED = 0
const NO_DATA_NEEDED = 0
const RESP_NEEDED = 1
const DATA_NEEDED = 1

type pshellControl struct {
  socket net.Conn
  defaultTimeout int
  serverType string
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

var gRetCodes = map[int]string {
  INVALID_SID:"INVALID_SID",
  COMMAND_SUCCESS:"COMMAND_SUCCESS",
  COMMAND_NOT_FOUND:"COMMAND_NOT_FOUND",
  COMMAND_INVALID_ARG_COUNT:"COMMAND_INVALID_ARG_COUNT",
  SOCKET_SEND_FAILURE:"SOCKET_SEND_FAILURE",
  SOCKET_SELECT_FAILURE:"SOCKET_SELECT_FAILURE",
  SOCKET_RECEIVE_FAILURE:"SOCKET_RECEIVE_FAILURE",
  SOCKET_TIMEOUT:"SOCKET_TIMEOUT",
  SOCKET_NOT_CONNECTED:"SOCKET_NOT_CONNECTED",
}

/////////////////////////////////
//
// Public functions
//
/////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

func ConnectServer(controlName_ string, remoteServer_ string, port_ string, defaultTimeout_ int) int {
  return (connectServer(controlName_, remoteServer_, port_, defaultTimeout_))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func SendCommand1(sid_ int, command_ string) int {
  return (sendCommand1(sid_, command_))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func SendCommand2(sid_ int, timeoutOverride_ int, command_ string) int {
  return (sendCommand2(sid_, timeoutOverride_, command_))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func SendCommand3(sid_ int, command_ string) (int, string) {
  return (sendCommand3(sid_, command_))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func SendCommand4(sid_ int, timeoutOverride_ int, command_ string) (int, string) {
  return (sendCommand4(sid_, timeoutOverride_, command_))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func GetRetCodeString(retCode_ int) string {
  return gRetCodes[retCode_]
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
  udpSocket, retCode := net.Dial("udp", strings.Join([]string{remoteServer_, ":", port_,}, ""))
  if (retCode == nil) {
    gControlList = append(gControlList, 
                          pshellControl{udpSocket,
                                        defaultTimeout_, 
                                        "udp", 
                                        []byte{},           // sendMsg
                                        make([]byte, 2048), // recvMsg
                                        0,                  // recvSize
                                        strings.Join([]string{controlName_, "[", remoteServer_, "]"}, "")})
    return len(gControlList)-1
  } else {
    return INVALID_SID
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand1(sid_ int, command_ string) int {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    return sendCommand(&control, command_, control.defaultTimeout, NO_DATA_NEEDED)
  } else {
    return INVALID_SID
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand2(sid_ int, timeoutOverride_ int, command_ string) int {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    return sendCommand(&control, command_, timeoutOverride_, NO_DATA_NEEDED)
  } else {
    return INVALID_SID
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand3(sid_ int, command_ string) (int, string) {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    return sendCommand(&control, command_, control.defaultTimeout, DATA_NEEDED),
           getPayload(control.recvMsg, control.recvSize)
  } else {
    return INVALID_SID, ""
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func sendCommand4(sid_ int, timeoutOverride_ int, command_ string) (int, string) {
  if ((sid_ >= 0) && (sid_ < len(gControlList))) {
    control := gControlList[sid_]
    return sendCommand(&control, command_, timeoutOverride_, DATA_NEEDED),
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
    control_.sendMsg = createMessage(CONTROL_COMMAND, RESP_NEEDED, dataNeeded_, sendSeqNum, command_)
  } else {
    // timeout is 0, fire and forget message, do not request a response
    control_.sendMsg = createMessage(CONTROL_COMMAND, NO_RESP_NEEDED, dataNeeded_, sendSeqNum, command_)
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
  } else {
    retCode = SOCKET_SEND_FAILURE
  }
  
  return retCode
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
  MSG_TYPE_OFFSET = 0
  RESP_NEEDED_OFFSET = 1
  DATA_NEEDED_OFFSET = 2
  SEQ_NUM_OFFSET = 4
  PAYLOAD_OFFSET = 8
)

const (
  CONTROL_COMMAND = 12
)

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getPayload(message []byte, recvSize int) string {
  return (string(message[PAYLOAD_OFFSET:recvSize]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getMsgType(message []byte) byte {
  return (message[MSG_TYPE_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getSeqNum(message []byte) uint32 {
  return (binary.BigEndian.Uint32(message[SEQ_NUM_OFFSET:]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setSeqNum(message []byte, seqNum uint32) {
  binary.BigEndian.PutUint32(message[SEQ_NUM_OFFSET:], seqNum)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createMessage(msgType byte, respNeeded byte, dataNeeded byte, seqNum uint32, command string) []byte {
  message := []byte{msgType, respNeeded, dataNeeded, 0, 0, 0, 0, 0}
  setSeqNum(message, seqNum)
  message = append(message, []byte(command)...)
  return (message)
}

