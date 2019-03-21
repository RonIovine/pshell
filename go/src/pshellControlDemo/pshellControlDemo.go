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

package main

////////////////////////////////////////////////////////////////////////////////
//
// This is an example demo program that shows how to use the pshell control
// interface.  This API will allow any external client program to invoke
// functions that are registered within a remote program running a pshell
// server.  This will only control programs that are running either a UDP
// or UNIX pshell server.
//
////////////////////////////////////////////////////////////////////////////////

// import all our necessary modules
import "fmt"
import "os"
import "bufio"
import "strconv"
import "strings"
import "syscall"
import "os/signal"
import "PshellControl"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func signalHandler(signalChan chan os.Signal) {
  for {
    <-signalChan // This line will block until a signal is received
    PshellControl.DisconnectAllServers()
    fmt.Printf("\n")
    os.Exit(0)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func registerSignalHandlers() {
  // register a signal handler so we can cleanup our
  // system resources upon abnormal termination
  signalChan := make(chan os.Signal, 1)
  signal.Notify(signalChan,
                syscall.SIGHUP,       // 1  Hangup (POSIX)
                syscall.SIGINT,       // 2  Interrupt (ANSI)
                syscall.SIGQUIT,      // 3  Quit (POSIX)
                syscall.SIGILL,       // 4  Illegal instruction (ANSI)
                syscall.SIGABRT,      // 6  Abort (ANSI)
                syscall.SIGBUS,       // 7  BUS error (4.2 BSD)
                syscall.SIGFPE,       // 8  Floating-point exception (ANSI)
                syscall.SIGSEGV,      // 11 Segmentation violation (ANSI)
                syscall.SIGPIPE,      // 13 Broken pipe (POSIX)
                syscall.SIGALRM,      // 14 Alarm clock (POSIX)
                syscall.SIGTERM,      // 15 Termination (ANSI)
                syscall.SIGXCPU,      // 24 CPU limit exceeded (4.2 BSD)
                syscall.SIGXFSZ,      // 25 File size limit exceeded (4.2 BSD)
                syscall.SIGSYS)       // 31 Bad system call
  go signalHandler(signalChan)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showUsage() {
  fmt.Printf("\n")
  fmt.Printf("Usage: pshellControlDemo {<hostname> | <ipAddress> | <unixServerName>} {<port> | unix}\n")
  fmt.Printf("                         [-t<timeout>] [-extract]\n")
  fmt.Printf("\n")
  fmt.Printf("  where:\n")
  fmt.Printf("    <hostname>       - hostname of UDP server\n")
  fmt.Printf("    <ipAddress>      - IP address of UDP server\n")
  fmt.Printf("    <unixServerName> - name of UNIX server\n")
  fmt.Printf("    unix             - specifies a UNIX server\n")
  fmt.Printf("    <port>           - port number of UDP server\n")
  fmt.Printf("    <timeout>        - wait timeout for response in mSec (default=100)\n")
  fmt.Printf("    extract          - extract data contents of response (must have non-0 wait timeout)\n")
  fmt.Printf("\n")
  os.Exit(0)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func main() {
  if ((len(os.Args) < 3) || ((len(os.Args)) > 5)) {
    showUsage()
  }

  extract := false
  timeout := 1000

  for _, arg := range os.Args[3:] {
    if (arg == "-t") {
      timeout, _ = strconv.Atoi(arg[2:])
    } else if (arg == "-extract") {
      extract = true
    } else {
      showUsage()
    }
  }

  // register signal handlers so we can do a graceful termination and cleanup any system resources
  registerSignalHandlers()

  sid := PshellControl.ConnectServer("pshellControlDemo", os.Args[1], os.Args[2], timeout)

  if (sid != PshellControl.INVALID_SID) {
    command := ""
    scanner := bufio.NewScanner(os.Stdin)
    fmt.Printf("Enter command or 'q' to quit\n");
    for (command == "") || !strings.HasPrefix("quit", command) {
      fmt.Print("pshellControlCmd> ")
      scanner.Scan()
      command = scanner.Text()
      if ((len(command) > 0) && !strings.HasPrefix("quit", command)) {
        if (extract == true) {
          retCode, results := PshellControl.SendCommand3(sid, command)
          if (retCode == PshellControl.COMMAND_SUCCESS) {
            fmt.Printf("%d bytes extracted, results:\n", len(results))
            fmt.Printf("%s", results)
          }
          fmt.Printf("retCode: %s\n", PshellControl.GetResponseString(retCode))
        } else {
          retCode := PshellControl.SendCommand1(sid, command)
          fmt.Printf("retCode: %s\n", PshellControl.GetResponseString(retCode))
        }
      }
    }
    PshellControl.DisconnectServer(sid)
  }

}

