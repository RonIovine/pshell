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
// This is the STUB version of the PshellServer module, to use the actual version,
// set the PshellServer softlink to PshellServer-full, or use the provided utility
// shell script 'setPshellLib' to set the desired softlink and 'showPshellLib' to
// display the current softlink settings.  Note, since go is statically linked, a
// rebuild of the pshellServerDemo.go program is required.
//
package PshellServer

import "fmt"
import "time"
import "strings"

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

// Used to specify the radix extraction format for the getInt function
const (
  RADIX_DEC = 0
  RADIX_HEX = 1
  RADIX_ANY = 2
)

type pshellFunction func([]string)
type logFunction func(string)

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func AddCommand(function pshellFunction,
                command string,
                description string,
                usage string,
                minArgs int,
                maxArgs int,
                showUsage bool) {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func StartServer(serverName string,
                 serverType string,
                 serverMode int,
                 hostnameOrIpAddr string,
                 port string) {
  fmt.Printf("PSHELL_INFO: STUB Server: %s Started\n", serverName)
  if (serverMode == BLOCKING) {
    for {
      time.Sleep(time.Hour*8)
    }
  }
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func CleanupResources() {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func RunCommand(format string, command ...interface{}) {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func SetLogLevel(level int) {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func SetLogFunction(function logFunction) {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func Printf(format string, message ...interface{}) {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func Flush() {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func Wheel(message string) {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func March(message string) {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsHelp() bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func ShowUsage() {
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func Tokenize(string string, delimiter string) (int, []string) {
  return 0, strings.Split(strings.TrimSpace(string), delimiter)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetLength(string string) int {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsEqual(string1 string, string2 string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsEqualNoCase(string1 string, string2 string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsSubString(string1 string, string2 string, minMatchLength int) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsSubStringNoCase(string1 string, string2 string, minMatchLength int) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsFloat(string string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsDec(string string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsHex(string string, needHexPrefix bool) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsAlpha(string string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsNumeric(string string, needHexPrefix bool) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsAlphaNumeric(string1 string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsIpv4Addr(string1 string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func IsIpv4AddrWithNetmask(string1 string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetBool(string string) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetInt(string string, radix int, needHexPrefix bool) int {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetInt64(string string, radix int, needHexPrefix bool) int64 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetInt32(string string, radix int, needHexPrefix bool) int32 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetInt16(string string, radix int, needHexPrefix bool) int16 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetInt8(string string, radix int, needHexPrefix bool) int8 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetUint(string string, radix int, needHexPrefix bool) uint64 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetUint64(string string, radix int, needHexPrefix bool) uint64 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetUint32(string string, radix int, needHexPrefix bool) uint32 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetUint16(string string, radix int, needHexPrefix bool) uint16 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetUint8(string string, radix int, needHexPrefix bool) uint8 {
  return (0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetDouble(string string) float64 {
  return (0.0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetFloat64(string string) float64 {
  return (0.0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetFloat(string string) float32 {
  return (0.0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetFloat32(string string) float32 {
  return (0.0)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetOption(arg string) (bool, string, string) {
  return true, "", ""
}
