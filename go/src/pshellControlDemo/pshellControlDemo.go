package main

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

