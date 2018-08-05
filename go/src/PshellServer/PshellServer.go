package PshellServer

import "encoding/binary"
import "net"
import "fmt"
import "strings"
import "strconv"
import "io/ioutil"
import "os"

/////////////////////////////////////////////////////////////////////////////////
//
// global "public" data, these are used for various parts of the public API
//
/////////////////////////////////////////////////////////////////////////////////

// Valid server types, UDP/UNIX servers require the 'pshell' or 'pshell.py'
// client programs, TCP servers require a 'telnet' client, local servers
// require no client (all user interaction done directly with server running
// in the parent host program)
const (
  UDP = "udp"
  TCP = "tcp"
  UNIX = "unix"
  LOCAL = "local"
)

// These are the identifiers for the serverMode.  BLOCKING wil never return 
// control to the caller of startServer, NON_BLOCKING will spawn a thread to 
// run the server and will return control to the caller of startServer
const (
  BLOCKING = 0
  NON_BLOCKING = 1
)

/////////////////////////////////////////////////////////////////////////////////
//
// global "private" data
//
/////////////////////////////////////////////////////////////////////////////////

// the following enum values are returned by the non-extraction
// based sendCommand1 and sendCommand2 functions
//
// the following "COMMAND" enums are returned by the remote pshell server
// and must match their corresponding values in PshellServer.cc
// msgType return codes for control client
const (
  _COMMAND_SUCCESS = 0
  _COMMAND_NOT_FOUND = 1
  _COMMAND_INVALID_ARG_COUNT = 2
)

// msgType codes between client and server
const (
  _QUERY_VERSION = 1
  _QUERY_PAYLOAD_SIZE = 2
  _QUERY_NAME = 3
  _QUERY_COMMANDS1 = 4
  _QUERY_COMMANDS2 = 5
  _UPDATE_PAYLOAD_SIZE = 6
  _USER_COMMAND = 7
  _COMMAND_COMPLETE = 8
  _QUERY_BANNER = 9
  _QUERY_TITLE = 10
  _QUERY_PROMPT = 11
  _CONTROL_COMMAND = 12
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

const _PSHELL_CONFIG_DIR = "/etc/pshell/config"
const _PSHELL_STARTUP_DIR = "/etc/pshell/startup"
const _PSHELL_BATCH_DIR = "/etc/pshell/batch"
const _PSHELL_CONFIG_FILE = "pshell-server.conf"

var _gPrompt = "PSHELL> "
var _gTitle = "PSHELL: "
var _gBanner = "PSHELL: Process Specific Embedded Command Line Shell"
var _gServerVersion = "1"
var _gPshellMsgPayloadLength = 2048
var _gPshellMsgHeaderLength = 8
var _gServerType = UDP
var _gServerName = "None"
var _gServerMode = BLOCKING
var _gHostnameOrIpAddr = "None"
var _gPort = "0"
var _gTcpTimeout = 10  // minutes
var _gRunning = false
var _gCommandDispatched = false
var _gCommandInteractive = true
var _gArgs []string
var _gFoundCommand pshellCmd

var _gCommandList = []pshellCmd{}
var _gPshellRcvMsg = make([]byte, _gPshellMsgPayloadLength)
var _gPshellSendPayload = ""
var _gUdpSocket *net.UDPConn
var _gUnixSocket *net.UnixConn
var _gUnixSocketPath = "/tmp/"
var _gUnixSourceAddress string
var _gTcpSocket *net.TCPConn
var _gRecvAddr net.Addr
var _gMaxLength = 0

/////////////////////////////////
//
// Public functions
//
/////////////////////////////////

//
//  Register callback commands to our PSHELL server.  If the command takes no
//  arguments, the default parameters can be provided.  If the command takes
//  an exact number of parameters, set minArgs and maxArgs to be the same.  If
//  the user wants the callback function to handle all help initiated usage,
//  set the showUsage parameter to False.
//
//    Args:
//        function (ptr)    : User callback function
//        command (str)     : Command to dispatch the function (single keyword only)
//        description (str) : One line description of command
//        usage (str)       : One line command usage (Unix style preferred)
//        minArgs (int)     : Minimum number of required arguments
//        maxArgs (int)     : Maximum number of required arguments
//        showUsage (bool)  : Show registered usage on a '?' or '-h'
//
//    Returns:
//        none
//
func AddCommand(function pshellFunction, command string, description string, usage string, minArgs int, maxArgs int, showUsage bool) {
  addCommand(function, command, description, usage, minArgs, maxArgs, showUsage, false)
}

//
//  Start our PSHELL server, if serverType is UNIX or LOCAL, the default
//  parameters can be used, and will be ignored if provided.  All of these
//  parameters except serverMode can be overridden on a per serverName
//  basis via the pshell-server.conf config file.  All commands in the
//  <serverName>.startup file will be executed in this function at server
//  startup time.
//
//    Args:
//        serverName (str)       : Logical name of the Pshell server
//        serverType (str)       : Desired server type (UDP, UNIX, TCP, LOCAL)
//        serverMode (str)       : Desired server mode (BLOCKING, NON_BLOCKING)
//        hostnameOrIpAddr (str) : Hostname or IP address to run server on
//        port (int)             : Port number to run server on (UDP or TCP only)
//
//    Returns:
//        none
//
func StartServer(serverName string, serverType string, serverMode int, hostnameOrIpAddr string, port string) {
  startServer(serverName, serverType, serverMode, hostnameOrIpAddr, port)
}

//
//  Cleanup and release any system resources claimed by this module.  This includes
//  any open socked handles, file descriptors, or system 'tmp' files.  This should
//  be called at program exit time as well as any signal exception handler that
//  results in a program termination.
//
//    Args:
//        none
//
//    Returns:
//        none
//
func CleanupResources() {
  cleanupResources()
}

//
//  Run a registered command from within its parent process
//
//    Args:
//        command (str) : Registered callback command to run
//
//    Returns:
//        none
//
func RunCommand(command string) {
  runCommand(command)
}

////////////////////////////////////////////////////////////////////////////////
//
// The following public functions should only be called from within a 
// registered callback function
//
////////////////////////////////////////////////////////////////////////////////

//
//  Display data back to the remote client
//
//    Args:
//        string (str)   : Message to display to the client user
//        newline (bool) : Automatically add a newline to message
//
//    Returns:
//        none
//
func Printf(format string, message ...interface{}) {
  printf(format, message...)
}

//
//  Flush the reply (i.e. display) buffer back to the remote client
//
//    Args:
//        none
//
//    Returns:
//        none
//
func Flush() {
}

//
//  Spinning ascii wheel keep alive, user string string is optional
//
//    Args:
//        message (str) : String to display before the spinning wheel
//
//    Returns:
//        none
//
func Wheel(message string) {
}

//
//  March a message or character keep alive across the screen
//
//    Args:
//        message (str) : String or char to march across the screen
//
//    Returns:
//        none
//
func March(message string) {
}

//
//  Check if the user has asked for help on this command.  Command must be 
//  registered with the showUsage = False option.
//
//    Args:
//        none
//
//    Returns:
//        bool : True if user is requesting command help, False otherwise
//
func IsHelp() bool {
  return (isHelp())
}

//
//  Show the command's registered usage
//
//    Args:
//        none
//
//    Returns:
//        none
//
func ShowUsage() {
  showUsage()
}

//
//  This function will return True if string1 is a substring of string2 at 
//  position 0.  If the minMatchLength is 0, then it will compare up to the
//  length of string1.  If the minMatchLength > 0, it will require a minimum
//  of that many characters to match.  A string that is longer than the min
//  match length must still match for the remaining charactes, e.g. with a
//  minMatchLength of 2, 'q' will not match 'quit', but 'qu', 'qui' or 'quit'
//  will match, 'quix' will not match.  This function is useful for wildcard
//  matching.
//
//    Args:
//        string1 (str)        : The substring
//        string2 (str)        : The target string
//        minMatchLength (int) : The minimum required characters match
//
//    Returns:
//        bool : True if substring matches, False otherwise
//
func IsSubString(string1 string, string2 string, minMatchLength int) bool {
  return true
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
                showUsage bool,
                prepend bool) {  
                  
  // see if we have a NULL command name 
  if ((command == "") || (len(command) == 0)) {
    fmt.Printf("PSHELL_ERROR: NULL command name, command not added\n")
    return
  }

  // see if we have a NULL description 
  if ((description == "") || (len(description) == 0)) {
    fmt.Printf("PSHELL_ERROR: NULL description, command: '%s' not added\n", command)
    return
  }

  // see if we have a NULL function
  if (function == nil) {
    fmt.Printf("PSHELL_ERROR: NULL function, command: '%s' not added\n", command)
    return
  }

  // if they provided no usage for a function with arguments
  if (((maxArgs > 0) || (minArgs > 0)) && ((usage == "") || (len(usage) == 0))) {
    fmt.Printf("PSHELL_ERROR: NULL usage for command that takes arguments, command: '%s' not added\n", command)
    return
  }

  // see if their minArgs is greater than their maxArgs
  if (minArgs > maxArgs) {
    fmt.Printf("PSHELL_ERROR: minArgs: %d is greater than maxArgs: %d, command: '%s' not added\n", minArgs, maxArgs, command)
    return
  }
    
  // see if it is a duplicate command
  for _, entry := range _gCommandList {
    if (entry.command == command) {
      // command name already exists, don't add it again
      fmt.Printf("PSHELL_ERROR: Command: %s already exists, not adding command\n", command)
      return
    }
  }
      
  // everything ok, good to add command
  
  if (len(command) > _gMaxLength) {
    _gMaxLength = len(command)
  }
    
  if (prepend == true) {
    _gCommandList = append(_gCommandList, 
                           pshellCmd{command, 
                                     usage,
                                     description, 
                                     function,
                                     minArgs, 
                                     maxArgs,
                                     showUsage})
  } else {
    _gCommandList = append(_gCommandList, 
                           pshellCmd{command, 
                                     usage,
                                     description, 
                                     function,
                                     minArgs, 
                                     maxArgs,
                                     showUsage})
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func startServer(serverName string,
                 serverType string,
                 serverMode int,
                 hostnameOrIpAddr string, 
                 port string) {
  if (_gRunning == false) {
    _gServerType = serverType
    _gServerMode = serverMode
    _gTitle, _gBanner, _gPrompt, _gServerType, _gHostnameOrIpAddr, _gPort, _gTcpTimeout =
      loadConfigFile(_gServerName, _gTitle, _gBanner, _gPrompt, serverType, hostnameOrIpAddr, port, _gTcpTimeout)
    loadStartupFile()  
    _gRunning = true
    if (_gServerMode == BLOCKING) {
      runServer()
    } else {
      go runServer()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func cleanupResources() {
  if _gServerType == UNIX {
    os.Remove(_gUnixSourceAddress)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runCommand(command string) {
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func printf(format_ string, message_ ...interface{}) {
  if (_gCommandInteractive == true) {
    _gPshellSendPayload += fmt.Sprintf(format_, message_...)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func isHelp() bool {
  return ((len(_gArgs) == 1) &&
          ((_gArgs[0] == "?") ||
           (_gArgs[0] == "-h") ||
           (_gArgs[0] == "--h") ||
           (_gArgs[0] == "-help") ||
           (_gArgs[0] == "--help")))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showUsage() {
  if (len(_gFoundCommand.usage) > 0) {
    Printf("Usage: %s %s\n", _gFoundCommand.command, _gFoundCommand.usage)
  } else {
    Printf("Usage: %s\n", _gFoundCommand.command)
  }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func loadConfigFile(serverName string,
                    title string,
                    banner string,
                    prompt string,
                    serverType string,
                    hostnameOrIpAddr string,
                    port string,
                    tcpTimeout int) (string,
                                     string,
                                     string,
                                     string,
                                     string,
                                     string,
                                     int) {
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
    return title, banner, prompt, serverType, hostnameOrIpAddr, port, tcpTimeout
  }
  // found a config file, process it
  lines := strings.Split(string(file), "\n")
  for _, line := range lines {
    // skip comments
    if ((len(line) > 0) && (line[0] != '#')) {
      value := strings.Split(line, "=")
      if (len(value) == 2) {
        option := strings.Split(value[0], ".")
        if ((len(option) == 2) && (serverName == option[0])) {
          if (strings.ToLower(option[1]) == "title") {
            title = value[1]
          } else if (strings.ToLower(option[1]) == "banner") {
            banner = value[1]
          } else if (strings.ToLower(option[1]) == "prompt") {
            prompt = value[1]
          } else if (strings.ToLower(option[1]) == "host") {
            hostnameOrIpAddr = value[1]
          } else if (strings.ToLower(option[1]) == "port") {
            port = value[1]
          } else if (strings.ToLower(option[1]) == "type") {
            if ((strings.ToLower(value[1]) == UDP) ||
                (strings.ToLower(value[1]) == TCP) ||
                (strings.ToLower(value[1]) == UNIX) ||
                (strings.ToLower(value[1]) == LOCAL)) {
              serverType = value[1]
            }
          } else if (strings.ToLower(option[1]) == "timeout") {
            tcpTimeout, _ = strconv.Atoi(value[1])
          }
        }
      }
    }
  }
  return title, banner, prompt, serverType, hostnameOrIpAddr, port, tcpTimeout
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func loadStartupFile() {
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runServer() {
  if (_gServerType == UDP) {
    runUDPServer()
  } else if (_gServerType == TCP) {
    runTCPServer()
  } else if (_gServerType == UNIX) {
    runUNIXServer()
  } else {  // local server 
    runLocalServer()
  }
}
 
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runUDPServer() {
  fmt.Printf("PSHELL_INFO: UDP Server: %s Started On Host: %s, Port: %s\n", _gServerName, _gHostnameOrIpAddr, _gPort)
  // startup our UDP server
  //addCommand(_batch, "batch", "run commands from a batch file", "<filename>", 1, 1, True, True)
  if (createSocket()) {
    for {
      receiveDGRAM()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runUNIXServer() {
  fmt.Printf("PSHELL_INFO: UNIX Server: %s Started\n", _gServerName)
  // startup our UDP server
  //addCommand(_batch, "batch", "run commands from a batch file", "<filename>", 1, 1, True, True)
  if (createSocket()) {
    for {
      receiveDGRAM()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runLocalServer() {
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runTCPServer() {
  fmt.Printf("PSHELL_INFO: TCP Server: %s Started On Host: %s, Port: %s\n", _gServerName, _gHostnameOrIpAddr, _gPort)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createSocket() bool {
  if (_gServerType == UDP) {
    serverAddr := _gHostnameOrIpAddr + ":" + _gPort
    udpAddr, err := net.ResolveUDPAddr("udp", serverAddr)
    if err == nil {
      _gUdpSocket, err = net.ListenUDP("udp", udpAddr)
      return (true)
    } else {
      return (false)
    }
  } else if (_gServerType == UNIX) {
    _gUnixSourceAddress = _gUnixSocketPath + _gServerName
    os.Remove(_gUnixSourceAddress)
    unixAddr, err := net.ResolveUnixAddr("unixgram", _gUnixSourceAddress)
    if err == nil {
      _gUnixSocket, err = net.ListenUnixgram("unixgram", unixAddr)
      return (true)
    } else {
      return (false)
    }
    return (true)
  } else if (_gServerType == TCP) {
    return (true)
  } else {
    return (false)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func receiveDGRAM() {
  var err error
  var recvSize int
  if (_gServerType == UDP) {
    recvSize, _gRecvAddr, err = _gUdpSocket.ReadFrom(_gPshellRcvMsg)
  } else if (_gServerType == UNIX) {
    recvSize, _gRecvAddr, err = _gUnixSocket.ReadFrom(_gPshellRcvMsg)
  }
  if (err == nil) {
    processCommand(getPayload(_gPshellRcvMsg, recvSize))
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func isValidArgCount() bool {
  return ((len(_gArgs) >= _gFoundCommand.minArgs) &&
          (len(_gArgs) <= _gFoundCommand.maxArgs))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryVersion() {
  Printf("%s", _gServerVersion)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryPayloadSize() {
  Printf("%d", _gPshellMsgPayloadLength)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryName() {
  Printf(_gServerName)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryTitle() {
  Printf(_gTitle)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryBanner() {
  Printf(_gBanner)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryPrompt() {
  Printf(_gPrompt)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryCommands1() {
  for _, command := range _gCommandList {
    Printf("%-*s  -  %s\n", _gMaxLength, command.command, command.description)
  }
  Printf("\n")
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryCommands2() {
  for _, command := range _gCommandList {
    Printf("%s%s", command.command, "/")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processCommand(command string) {
  if (getMsgType(_gPshellRcvMsg) == _QUERY_VERSION) {
    processQueryVersion()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_PAYLOAD_SIZE) {
    processQueryPayloadSize()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_NAME) {
    processQueryName()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_TITLE) {
    processQueryTitle()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_BANNER) {
    processQueryBanner()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_PROMPT) {
    processQueryPrompt()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_COMMANDS1) {
    processQueryCommands1()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_COMMANDS2) {
    processQueryCommands2()
  } else {
    _gCommandDispatched = true
    _gArgs = strings.Split(strings.TrimSpace(command), " ")
    command := _gArgs[0]
    if (len(_gArgs) > 1) {
      _gArgs = _gArgs[1:]
    } else {
      _gArgs = []string{}
    }
    numMatches := 0
    if ((command == "?") || (command == "help")) {
      //help(_gArgs)
      _gCommandDispatched = false
      return
    } else {
      for _, entry := range _gCommandList {
        if (command == entry.command) {
          _gFoundCommand = entry
          numMatches += 1
        }
      }
    }
    if (numMatches == 0) {
      fmt.Printf("PSHELL_ERROR: Command: '%s' not found", command)
    } else if (numMatches > 1) {
      fmt.Printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'", command)
    } else {
      if (IsHelp()) {
        if (_gFoundCommand.showUsage == true) {
          ShowUsage()          
        } else {
          _gFoundCommand.function(_gArgs)
        }
      } else if (!isValidArgCount()) {
        ShowUsage()
      } else {
        _gFoundCommand.function(_gArgs)
      }
    }
  }
  _gCommandDispatched = false
  reply()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func reply() {
  pshellSendMsg := createMessage(_COMMAND_COMPLETE, 
                                 getRespNeeded(_gPshellRcvMsg), 
                                 getDataNeeded(_gPshellRcvMsg), 
                                 getSeqNum(_gPshellRcvMsg), 
                                 _gPshellSendPayload)
  if (_gServerType == UDP) {
    _gUdpSocket.WriteTo(pshellSendMsg, _gRecvAddr)
  } else if (_gServerType == UNIX) {
    _gUnixSocket.WriteTo(pshellSendMsg, _gRecvAddr)
  }
  _gPshellSendPayload = ""
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
func setMsgType(message []byte, msgType byte) {
  message[_MSG_TYPE_OFFSET] = msgType
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getRespNeeded(message []byte) byte {
  return (message[_RESP_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setRespNeeded(message []byte, respNeeded byte) {
  message[_RESP_NEEDED_OFFSET] = respNeeded
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getDataNeeded(message []byte) byte {
  return (message[_DATA_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setDataNeeded(message []byte, dataNeeded byte) {
  message[_DATA_NEEDED_OFFSET] = dataNeeded
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

