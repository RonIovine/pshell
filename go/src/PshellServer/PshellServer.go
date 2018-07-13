package PshellServer

import "encoding/binary"
//import "net"
//import "fmt"
//import "time"
//import "strings"

// the following enum values are returned by the non-extraction
// based sendCommand1 and sendCommand2 functions
//
// the following "COMMAND" enums are returned by the remote pshell server
// and must match their corresponding values in PshellServer.cc
// msgType return codes for control client
const (
  COMMAND_SUCCESS = 0
  COMMAND_NOT_FOUND = 1
  COMMAND_INVALID_ARG_COUNT = 2
)

// msgType codes between client and server
const (
  QUERY_VERSION = 1
  QUERY_PAYLOAD_SIZE = 2
  QUERY_NAME = 3
  QUERY_COMMANDS1 = 4
  QUERY_COMMANDS2 = 5
  UPDATE_PAYLOAD_SIZE = 6
  USER_COMMAND = 7
  COMMAND_COMPLETE = 8
  QUERY_BANNER = 9
  QUERY_TITLE = 10
  QUERY_PROMPT = 11
  CONTROL_COMMAND = 12
)

type pshellFunction func([]string)

type pshellCmd struct {
  command string
  usage string
  description string
  function pshellFunction
  minArgs int
  maxArgs int
  showUsage bool
}

var gCommandList = []pshellCmd{}

/////////////////////////////////
//
// Public functions
//
/////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func AddCommand(function pshellFunction, 
                command string, 
                description string, 
                usage string, 
                minArgs int, 
                maxArgs int, 
                showUsage bool) {
  addCommand(function, command, description, usage, minArgs, maxArgs, showUsage)
}

/////////////////////////////////
//
// Private functions
//
/////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addCommand(function pshellFunction, 
                command string, 
                description string, 
                usage string, 
                minArgs int, 
                maxArgs int, 
                showUsage bool) {
  gCommandList = append(gCommandList, 
                        pshellCmd{command, 
                                  usage,
                                  description, 
                                  function,
                                  minArgs, 
                                  maxArgs,
                                  showUsage})
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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getPayload(message []byte) []byte {
  return (message[PAYLOAD_OFFSET:])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setPayload(message []byte, payload string) {
  copy(message[PAYLOAD_OFFSET:], []byte(payload))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func clearPayload(message []byte) {
  message[PAYLOAD_OFFSET] = 0
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getMsgType(message []byte) byte {
  return (message[MSG_TYPE_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setMsgType(message []byte, msgType byte) {
  message[MSG_TYPE_OFFSET] = msgType
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getRespNeeded(message []byte) byte {
  return (message[RESP_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setRespNeeded(message []byte, respNeeded byte) {
  message[RESP_NEEDED_OFFSET] = respNeeded
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getDataNeeded(message []byte) byte {
  return (message[DATA_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setDataNeeded(message []byte, dataNeeded byte) {
  message[DATA_NEEDED_OFFSET] = dataNeeded
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

