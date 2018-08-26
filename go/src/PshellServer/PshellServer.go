package PshellServer

import "encoding/binary"
import "net"
import "fmt"
import "strings"
import "strconv"
import "io/ioutil"
import "os"
import "math"
import "bufio"

/////////////////////////////////////////////////////////////////////////////////
//
// This API provides the Process Specific Embedded Command Line Shell (PSHELL)
// user API functionality.  It provides the ability for a client program to
// register functions that can be invoked via a command line user interface.
// This is the 'go' implementation, there are also corresponding C/C++ and
// Python implementations.
//
// The functions are similar to the prototype of the 'main' in 'go', i.e.
// 'func myFunc([]string), there are several ways to invoke these embedded
// functions based on how the pshell server is configured, which is described
// in documentation further down in this file.
//
// A complete example of the usage of the API can be found in the included 
// demo program file pshellServerDemo.go
//
/////////////////////////////////////////////////////////////////////////////////

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

// These three identifiers that can be used for the hostnameOrIpAddr argument 
// of the startServer call.  PshellServer.ANYHOST will bind the server socket
// to all interfaces of a multi-homed host, PSHELL_ANYBCAST will bind to
// 255.255.255.255, PshellServer.LOCALHOST will bind the server socket to 
// the local loopback address (i.e. 127.0.0.1), note that subnet broadcast 
// it also supported, e.g. x.y.z.255
const (
  ANYHOST = "anyhost"
  ANYBCAST = "anybcast"
  LOCALHOST = "localhost"
)

/////////////////////////////////////////////////////////////////////////////////
//
// global "private" data
//
/////////////////////////////////////////////////////////////////////////////////

// the following enum values are returned by the pshell server
// response to a command request from a control client
const (
  _COMMAND_SUCCESS = 0
  _COMMAND_NOT_FOUND = 1
  _COMMAND_INVALID_ARG_COUNT = 2
)

// msgType codes between interactive client and server
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

// ascii keystroke codes
const (
  _BS = 8
  _TAB = 9
  _LF = 10
  _CR = 13
  _ESC = 27
  _SPACE = 32
  _DEL = 127
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
var _gTitle = "PSHELL"
var _gTcpTitle = "PSHELL"
var _gTcpPrompt = ""
var _gQuitTcp = false
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
var _gTcpConnectSockName = ""
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
var _gTcpSocket *net.TCPListener
var _gConnectFd *net.TCPConn
var _gTcpNegotiate = []byte{0xFF, 0xFB, 0x03, 0xFF, 0xFB, 0x01, 0xFF, 0xFD, 0x03, 0xFF, 0xFD, 0x01}
var _gRecvAddr net.Addr
var _gMaxLength = 0
var _gWheelPos = 0
var _gWheel = "|/-\\"

var _gTabCompletions []string
var _gMaxTabCompletionKeywordLength = 0
var _gMaxCompletionsPerLine = 0
var _gCommandHistory []string
var _gCommandHistoryPos = 0

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
//  set the showUsage parameter to false.
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
func RunCommand(format string, command ...interface{}) {
  runCommand(format, command...)
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
  flush()
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
  wheel(message)
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
  march(message)
}

//
//  Check if the user has asked for help on this command.  Command must be 
//  registered with the showUsage = false option.
//
//    Args:
//        none
//
//    Returns:
//        bool : True if user is requesting command help, false otherwise
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
//        bool : True if substring matches, false otherwise
//
func IsSubString(string1 string, string2 string, minMatchLength int) bool {
  return isSubString(string1, string2, minMatchLength)
}

//
//  This function will parse an argument string of the formats -<key><value> where
//  key is one letter only, i.e. '-t', or <key>=<value> where key can be any length
//  word, i.e. 'timeout', and return a 3-tuple indicating if the arg was parsed
//  correctly, along with the associated key and corresponding value.  An example 
//  of the two valid formats are -t10, timeout=10.
//
//  Args:
//        arg (str) : The argument string to parse
//
//    Returns:
//        bool : True if string parses correctly, i.e. -<key><value> or <key>=<value>
//        str  : The key value found
//        str  : The value associated with the key
//
func GetOption(arg string) (bool, string, string) {
  return (getOption(arg))
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
    _gCommandList = append([]pshellCmd{{command,
                                        usage,
                                        description,
                                        function,
                                        minArgs,
                                        maxArgs,
                                        showUsage}},
                           _gCommandList...)
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
    _gServerName = serverName
    _gServerMode = serverMode
    _gTitle,
    _gBanner,
    _gPrompt,
    _gServerType,
    _gHostnameOrIpAddr,
    _gPort,
    _gTcpTimeout = loadConfigFile(_gServerName,
                                  _gTitle,
                                  _gBanner,
                                  _gPrompt,
                                  serverType,
                                  hostnameOrIpAddr,
                                  port,
                                  _gTcpTimeout)
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
func runCommand(format_ string, command_ ...interface{}) {
  if (_gCommandDispatched == false) {
    command := fmt.Sprintf(format_, command_...)
    _gCommandDispatched = true
    _gCommandInteractive = false
    numMatches := 0
    _gCommandDispatched = true
    _gArgs = strings.Split(strings.TrimSpace(command), " ")
    command = _gArgs[0]
    if (len(_gArgs) > 1) {
      _gArgs = _gArgs[1:]
    } else {
      _gArgs = []string{}
    }
    for _, entry := range _gCommandList {
      if (command == entry.command) {
        _gFoundCommand = entry
        numMatches += 1
      }
    }
    if ((numMatches == 1) && isValidArgCount() && !IsHelp()) {
      _gFoundCommand.function(_gArgs)
    }
    _gCommandDispatched = false
    _gCommandInteractive = true
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func printf(format_ string, message_ ...interface{}) {
  if (_gCommandInteractive == true) {
    if (_gServerType == LOCAL) {
      fmt.Printf(format_, message_...)
    } else {
      // UDP/TCP/Unix (datagramn) server
      _gPshellSendPayload += fmt.Sprintf(format_, message_...)
      if (_gServerType == TCP) {
        flush()
      }
    }
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
func flush() {
  if (_gCommandInteractive == true) {
    if ((_gServerType == UDP) || (_gServerType == UNIX)) {
      reply(getMsgType(_gPshellRcvMsg))
    } else if (_gServerType == TCP) {
      _gConnectFd.Write([]byte(strings.Replace(_gPshellSendPayload,
                                               "\n",
                                               "\r\n",
                                               -1)))
      _gPshellSendPayload = ""
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func wheel(message string) {
  _gWheelPos += 1
  if (message != "") {
    printf("\r%s%c", message, _gWheel[(_gWheelPos)%4])
  } else {
    printf("\r%c", _gWheel[(_gWheelPos)%4])
  }
  flush()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func march(message string) {
  printf(message)
  flush()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showUsage() {
  if (len(_gFoundCommand.usage) > 0) {
    printf("Usage: %s %s\n", _gFoundCommand.command, _gFoundCommand.usage)
  } else {
    printf("Usage: %s\n", _gFoundCommand.command)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func isSubString(string1 string, string2 string, minMatchLength int) bool {
  if (minMatchLength > len(string1)) {
    return false
  } else {
    return strings.HasPrefix(string2, string1)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getOption(arg string) (bool, string, string) {
  if (len(arg) < 3) {
    return false, "", ""
  } else if (arg[0] == '-') {
    return true, arg[:2], arg[2:]
  } else {
    value := strings.Split(arg, "=")
    if (len(value) != 2) {
      return false, "", ""
    } else {
      return true, value[0], value[1]
    }
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
func showWelcome() {
  var server = ""
  // show our welcome screen
  banner := "#  " + _gBanner + "\n"
  // put up our window title banner
  if (_gServerType == LOCAL) {
    Printf("\033]0;%s\007", _gTitle)
    server = fmt.Sprintf("#  Single session LOCAL server: %s[%s]\n",
                         _gServerName,
                         _gServerType)
  } else {
    Printf("\033]0;%s\007", _gTcpTitle)
    server = fmt.Sprintf("#  Single session TCP server: %s[%s]\n",
                         _gServerName,
                         _gTcpConnectSockName)
  }
  maxBorderWidth := math.Max(58, float64(len(banner)-1))
  maxBorderWidth = math.Max(maxBorderWidth, float64(len(server)))+2
  Printf("\n")
  Printf(strings.Repeat("#", int(maxBorderWidth)))
  Printf("\n")
  Printf("#\n")
  Printf(banner)
  Printf("#\n")
  Printf(server)
  Printf("#\n")
  if (_gServerType == LOCAL) {
    Printf("#  Idle session timeout: NONE\n")
  } else {
    Printf("#  Idle session timeout: %d minutes\n", _gTcpTimeout)
  }
  Printf("#\n")
  Printf("#  Type '?' or 'help' at prompt for command summary\n")
  Printf("#  Type '?' or '-h' after command for command usage\n")
  Printf("#\n")
  if (_gServerType == TCP) {
    Printf("#  Full <TAB> completion, up-arrow recall, command\n")
    Printf("#  line editing and command abbreviation supported\n")
  } else {
    Printf("#  Command abbreviation supported\n")
  }
  Printf("#\n")
  Printf(strings.Repeat("#", int(maxBorderWidth)))
  Printf("\n")
  Printf("\n")
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func loadStartupFile() {
  var startupFile1 = ""
  var file []byte
  startupPath := os.Getenv("PSHELL_STARTUP_DIR")
  if (startupPath != "") {
    startupFile1 = startupPath+"/"+_gServerName+".startup"
  }
  startupFile2 := _PSHELL_STARTUP_DIR+"/"+_gServerName+".startup"
  cwd, _ := os.Getwd()
  startupFile3 := cwd+"/"+_gServerName+".startup"
  if _, err := os.Stat(startupFile1); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(startupFile1)
  } else if _, err := os.Stat(startupFile2); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(startupFile2)
  } else if _, err := os.Stat(startupFile3); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(startupFile3)
  } else {
    // file not found, return
    return
  }
  // found a startup file, process it
  lines := strings.Split(string(file), "\n")
  for _, line := range lines {
    // skip comments
    if ((len(line) > 0) && (line[0] != '#')) {
      runCommand(line)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func batch(argv []string) {
  var batchFile = argv[0]
  var batchFile1 = ""
  var file []byte
  batchPath := os.Getenv("PSHELL_BATCH_DIR")
  if (batchPath != "") {
    batchFile1 = batchPath+"/"+batchFile+".batch"
  }
  batchFile2 := _PSHELL_BATCH_DIR+"/"+batchFile+".batch"
  cwd, _ := os.Getwd()
  batchFile3 := cwd+"/"+batchFile+".batch"
  batchFile4 := batchFile
  if _, err := os.Stat(batchFile1); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(batchFile1)
  } else if _, err := os.Stat(batchFile2); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(batchFile2)
  } else if _, err := os.Stat(batchFile3); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(batchFile3)
  } else if _, err := os.Stat(batchFile4); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(batchFile4)
  } else {
    // file not found, return
    printf("ERROR: Could not find batch file: '%s'\n", batchFile)
    return
  }
  // found a batch file, process it
  lines := strings.Split(string(file), "\n")
  for _, line := range lines {
    // skip comments
    if ((len(line) > 0) && (line[0] != '#')) {
      runCommand(line)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func help(argv []string) {
  printf("\n")
  printf("****************************************\n")
  printf("*             COMMAND LIST             *\n")
  printf("****************************************\n")
  printf("\n")
  processQueryCommands1()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func exit(argv []string) {
  if (_gServerType == LOCAL) {
    //local server, exit the process
    os.Exit(0)
  } else if (_gServerType == TCP) {
    _gQuitTcp = true
  }
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
  fmt.Printf("PSHELL_INFO: UDP Server: %s Started On Host: %s, Port: %s\n",
             _gServerName,
             _gHostnameOrIpAddr,
             _gPort)
  // startup our UDP server
  addCommand(batch,
             "batch",
             "run commands from a batch file",
             "<filename>",
             1,
             1,
             true,
             true)
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
  addCommand(batch,
             "batch",
             "run commands from a batch file",
             "<filename>",
             1,
             1,
             true,
             true)
  if (createSocket()) {
    for {
      receiveDGRAM()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runLocalServer() {
  _gPrompt = _gServerName + "[" + _gServerType + "]:" + _gPrompt
  _gTitle = _gTitle + ": " + _gServerName + "[" + _gServerType + "], Mode: INTERACTIVE"
  addCommand(batch, "batch", "run commands from a batch file", "<filename>", 1, 2, true, true)
  addCommand(help, "help", "show all available commands", "", 0, 0, true, true)
  addCommand(exit, "quit", "exit interactive mode", "", 0, 0, true, true)
  addTabCompletions()
  showWelcome()
  reader := bufio.NewReader(os.Stdin)
  for {
    fmt.Print(_gPrompt)
    command, _ := reader.ReadString('\n')
    command = strings.TrimSuffix(command, "\n")
    if (len(command) > 0) {
      processCommand(command)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func acceptConnection() bool {
  _gConnectFd, _ = _gTcpSocket.AcceptTCP()
  _gTcpConnectSockName = strings.Split(_gConnectFd.LocalAddr().String(), ":")[0]
  return (true)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runTCPServer() {
  fmt.Printf("PSHELL_INFO: TCP Server: %s Started On Host: %s, Port: %s\n",
             _gServerName,
             _gHostnameOrIpAddr,
             _gPort)
  _gTcpPrompt = _gServerName + "[" + _gTcpConnectSockName + "]:" + _gPrompt
  _gTcpTitle = _gTitle + ": " + _gServerName + "[" + _gTcpConnectSockName + "], Mode: INTERACTIVE"
  addCommand(batch, "batch", "run commands from a batch file", "<filename>", 1, 1, true, true)
  addCommand(help, "help", "show all available commands", "", 0, 0, true, true)
  addCommand(exit, "quit", "exit interactive mode", "", 0, 0, true, true)
  addTabCompletions()
  // startup our TCP server and accept new connections
  for createSocket() && acceptConnection() {
    _gTcpPrompt = _gServerName + "[" + _gTcpConnectSockName + "]:" + _gPrompt
    _gTcpTitle = _gTitle + ": " + _gServerName + "[" + _gTcpConnectSockName + "], Mode: INTERACTIVE"
    // shutdown original socket to not allow any new connections until we are done with this one
    _gTcpSocket.Close()
    receiveTCP()
    _gConnectFd.Close()
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createSocket() bool {
  var hostnameOrIpAddr = ""
  if (_gHostnameOrIpAddr == ANYHOST) {
    hostnameOrIpAddr = ""
  } else if (_gHostnameOrIpAddr == LOCALHOST) {
    hostnameOrIpAddr = "127.0.0.1"
  } else {
    hostnameOrIpAddr = _gHostnameOrIpAddr
  }
  if (_gServerType == UDP) {
    serverAddr := hostnameOrIpAddr + ":" + _gPort
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
    // Listen for incoming connections
    serverAddr := hostnameOrIpAddr + ":" + _gPort
    tcpAddr, err := net.ResolveTCPAddr("tcp", serverAddr)
    if err == nil {
      _gTcpSocket, err = net.ListenTCP("tcp", tcpAddr)
      return (true)
    } else {
      return (false)
    }
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
func showPrompt(command_ string) {
  if (command_ == "") {
    printf("\r%s", _gTcpPrompt);
  } else {
    printf("\r%s%s", _gTcpPrompt, command_);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addTabCompletions() {
  if (((_gServerType == LOCAL) || (_gServerType == TCP)) &&
       (_gRunning  == true)) {
    for _, entry := range(_gCommandList) {
      addTabCompletion(entry.command)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addTabCompletion(keyword_ string) {
  for _, keyword := range(_gTabCompletions) {
    if (keyword == keyword_) {
      //duplicate keyword found, return
      return
    }
  }
  if (len(keyword_) > _gMaxTabCompletionKeywordLength) {
    _gMaxTabCompletionKeywordLength = len(keyword_)+5
    _gMaxCompletionsPerLine = 80/_gMaxTabCompletionKeywordLength
  }
  _gTabCompletions = append(_gTabCompletions, keyword_)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func findTabCompletions(keyword_ string) []string {
  var matchList []string
  for _, keyword := range(_gTabCompletions) {
    if (isSubString(keyword_, keyword, len(keyword_))) {
      matchList = append(matchList, keyword)
    }
  }
  return (matchList)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func findLongestMatch(matchList_ []string, command_ string) string {
  command := command_
  charPos := len(command_)
  for {
    if (charPos < len(matchList_[0])) {
      char := matchList_[0][charPos]
      for _, keyword := range(matchList_) {
         if ((charPos >= len(keyword)) || (char != keyword[charPos])) {
           return (command)
         }
       }
      command = command + string(char)
      charPos += 1
    } else {
      return (command)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showTabCompletions(completionList_ []string, prompt_ string) {
  if (len(completionList_) > 0) {
    printf("\n")
    totPrinted := 0
    numPrinted := 0
    for _, keyword := range(completionList_) {
      printf("%-*s", _gMaxTabCompletionKeywordLength, keyword)
      numPrinted += 1
      totPrinted += 1
      if ((numPrinted == _gMaxCompletionsPerLine) &&
          (totPrinted < len(completionList_))) {
        printf("\n")
        numPrinted = 0
      }
    }
    printf("\n%s", prompt_)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func clearLine(cursorPos_ int, command_ string) {
  printf("%s%s%s",
         strings.Repeat("\b", cursorPos_),
         strings.Repeat(" ", cursorPos_),
         strings.Repeat("\b", len(command_)))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func beginningOfLine(cursorPos_ int, command_ string) int {
  if (cursorPos_ > 0) {
    cursorPos_ = 0
    printf(strings.Repeat("\b", len(command_)))
  }
  return (cursorPos_)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func endOfLine(cursorPos_ int, command_ string) int {
  if (cursorPos_ < len(command_)) {
    printf(command_[cursorPos_:])
    cursorPos_ = len(command_)
  }
  return cursorPos_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func killLine(cursorPos_ int, command_ string) (int, string) {
  clearLine(cursorPos_, command_)
  command_ = ""
  cursorPos_ = 0
  return cursorPos_, command_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showCommand(command_ string) (int, string) {
  printf(command_)
  return len(command_), command_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getInput(command string,
              keystroke []byte,
              length int,
              cursorPos int,
              tabCount int,
              prompt_ string) (string,
                               bool,
                               bool,
                               int,
                               int) {
  quit := false
  fullCommand := false
  if (keystroke[0] == _CR) {
    // user typed CR, indicate the command is entered and return
    printf("\n")
    tabCount = 0
    if (len(command) > 0) {
      fullCommand = true
      if (len(_gCommandHistory) == 0 ||
          _gCommandHistory[len(_gCommandHistory)-1] != command) {
        _gCommandHistory = append(_gCommandHistory, command)
        _gCommandHistoryPos = len(_gCommandHistory)
      }
    }
  } else if ((length == 1) &&
             (keystroke[0] >= _SPACE) &&
             (keystroke[0] < _DEL)) {
    // printable single character, add it to our command,
    command = command[:cursorPos] + string(keystroke[0]) + command[cursorPos:]
    printf("%s%s",
           command[cursorPos:],
           strings.Repeat("\b", len(command[cursorPos:])-1))
    cursorPos += 1
    tabCount = 0
  } else {
    inEsc := false
    var esc byte = 0
    // non-printable character or escape sequence, process it
    for index := 0; index < length; index++ {
      //fmt.Printf("index[%d], val: %d\n", index, keystroke[index])
      char := keystroke[index]
      if (char != _TAB) {
        // something other than TAB typed, clear out our tabCount
        tabCount = 0
      }
      if (inEsc == true) {
        if (esc == '[') {
          if (char == 'A') {
            // up-arrow key
            if (_gCommandHistoryPos > 0) {
              _gCommandHistoryPos -= 1
              clearLine(cursorPos, command)
              cursorPos, command = showCommand(_gCommandHistory[_gCommandHistoryPos])
            }
            inEsc = false
            esc = 0
          } else if (char == 'B') {
            // down-arrow key
            if (_gCommandHistoryPos < len(_gCommandHistory)-1) {
              _gCommandHistoryPos += 1
              clearLine(cursorPos, command)
              cursorPos, command = showCommand(_gCommandHistory[_gCommandHistoryPos])
            } else {
              // kill whole line
              _gCommandHistoryPos = len(_gCommandHistory)
              cursorPos, command = killLine(cursorPos, command)
            }
            inEsc = false
            esc = 0
          } else if (char == 'C') {
            // right-arrow key
            if (cursorPos < len(command)) {
              printf("%s%s",
                     command[cursorPos:],
                     strings.Repeat("\b", len(command[cursorPos:])-1))
              cursorPos += 1
            }
            inEsc = false
            esc = 0
          } else if (char == 'D') {
            // left-arrow key
            if (cursorPos > 0) {
              cursorPos -= 1
              printf("\b")
            }
            inEsc = false
            esc = 0
          } else if (char == '1') {
            printf("home2")
            cursorPos = beginningOfLine(cursorPos, command)
          //} else if (char == '3') {
          //  print("delete")
          } else if (char == '~') {
            // delete key, delete under cursor
            if (cursorPos < len(command)) {
              printf("%s%s%s",
                     command[cursorPos+1:],
                     " ",
                     strings.Repeat("\b", len(command[cursorPos:])))
              command = command[:cursorPos] + command[cursorPos+1:]
            }
            inEsc = false
            esc = 0
          } else if (char == '4') {
            print("end2")
            cursorPos = endOfLine(cursorPos, command)
          }
        } else if (esc == 'O') {
          if (char == 'H') {
            // home key, go to beginning of line
            cursorPos = beginningOfLine(cursorPos, command)
          } else if (char == 'F') {
            // end key, go to end of line
            cursorPos = endOfLine(cursorPos, command)
          }
          inEsc = false
          esc = 0
        } else if ((char == '[') || (char == 'O')) {
          esc = char
        } else {
          inEsc = false
        }
      } else if (char == 11) {
        // kill to eol
        printf("%s%s",
               strings.Repeat(" ", len(command[cursorPos:]) ),
               strings.Repeat("\b", len(command[cursorPos:])))
        command = command[:cursorPos]
      } else if (char == 21) {
        // kill whole line
        cursorPos, command = killLine(cursorPos, command)
      } else if (char == _ESC) {
        // esc character
        inEsc = true
      } else if ((char == _TAB) &&
                ((len(command) == 0) ||
                 (len(strings.Split(strings.TrimSpace(command), " ")) == 1))) {
        // tab character, print out any completions, we only do tabbing on the first keyword
        tabCount += 1
        if (tabCount == 1) {
          // this tabbing method is a little different than the standard
          // readline or bash shell tabbing, we always trigger on a single
          // tab and always show all the possible completions for any
          // multiple matches
          if (len(command) == 0) {
            // nothing typed, just TAB, show all registered TAB completions
            showTabCompletions(_gTabCompletions, prompt_)
          } else {
            // partial word typed, show all possible completions
            matchList := findTabCompletions(command)
            if (len(matchList) == 1) {
              // only one possible completion, show it
              clearLine(cursorPos, command)
              cursorPos, command = showCommand(matchList[0] + " ")
            } else if (len(matchList) > 1) {
              // multiple possible matches, fill out longest match and
              // then show all other possibilities
              clearLine(cursorPos, command)
              cursorPos, command = showCommand(findLongestMatch(matchList, command))
              showTabCompletions(matchList, prompt_+command)
            }
          }
        }
      } else if (char == _DEL) {
        // backspace delete
        if ((len(command) > 0) && (cursorPos > 0)) {
          printf("%s%s%s%s",
                 "\b",
                 command[cursorPos:],
                 " ",
                 strings.Repeat("\b", len(command[cursorPos:])+1))
          command = command[:cursorPos-1] + command[cursorPos:]
          cursorPos -= 1
        }
      } else if (char == 1) {
        // home, go to beginning of line
        cursorPos = beginningOfLine(cursorPos, command)
      } else if (char == 3) {
        // ctrl-c, raise signal SIGINT to our own process
        //os.kill(os.getpid(), signal.SIGINT)
      } else if (char == 5) {
        // end, go to end of line
        cursorPos = endOfLine(cursorPos, command)
      } else if (char != 9) {
        // don't print out tab if multi keyword command
        //_write("\nchar value: %d" % char)
        //_write("\n"+prompt_)
      }
    }    
  }
  return command, fullCommand, quit, cursorPos, tabCount
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func receiveTCP() {
  var fullCommand bool
  var command string
  var length int
  var cursorPos int
  var tabCount int
  _gConnectFd.Write(_gTcpNegotiate)
  _gConnectFd.Read(_gPshellRcvMsg)
  showWelcome()
  //_gPshellMsg["msgType"] = _gMsgTypes["userCommand"]
  _gQuitTcp = false
  command = ""
  fullCommand = false
  cursorPos = 0
  tabCount = 0
  _gCommandHistory = []string{}
  for (_gQuitTcp == false) {
    if (command == "") {
      showPrompt(command)
    }
    length, _ = _gConnectFd.Read(_gPshellRcvMsg)
    command,
    fullCommand,
    _gQuitTcp,
    cursorPos,
    tabCount = getInput(command,
                        _gPshellRcvMsg,
                        length,
                        cursorPos,
                        tabCount,
                        _gPrompt)
    if ((_gQuitTcp == false) && (fullCommand == true)) {
      processCommand(command)
      command = ""
      fullCommand = false
      cursorPos = 0
    }
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
      help(_gArgs)
      _gCommandDispatched = false
      return
    } else {
      for _, entry := range _gCommandList {
        if (isSubString(command, entry.command, len(command))) {
          _gFoundCommand = entry
          numMatches += 1
        }
      }
    }
    if (numMatches == 0) {
      Printf("PSHELL_ERROR: Command: '%s' not found\n", command)
    } else if (numMatches > 1) {
      Printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n", command)
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
  reply(_COMMAND_COMPLETE)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func reply(response byte) {
  pshellSendMsg := createMessage(response, 
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

