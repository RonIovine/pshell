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

type pshellFunction func([]string)

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
func IsSubString(string1 string, string2 string, minMatchLength int) bool {
  return (true)
}

//
// Stub function, set PshellServer.a softlink to PshellServer-full.a for full functionality
//
func GetOption(arg string) (bool, string, string) {
  return true, "", ""
}
