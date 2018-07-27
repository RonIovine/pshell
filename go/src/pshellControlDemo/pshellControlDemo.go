package main

import "PshellControl"
import "fmt"
import "os"
import "bufio"
import "strconv"
import "strings"

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
  
  sid := PshellControl.ConnectServer("pshellControlDemo", os.Args[1], os.Args[2], timeout) 
  
  command := ""
  scanner := bufio.NewScanner(os.Stdin)
  for (command == "") || !strings.HasPrefix("quit", command) {
    fmt.Print("pshellControlCmd> ")
    scanner.Scan()
    command = scanner.Text()
    if ((len(command) > 0) && !strings.HasPrefix("quit", command)) {
      if (extract == true) {
        retCode, results := PshellControl.SendCommand3(sid, command)
        if (retCode == PshellControl.COMMAND_SUCCESS) {  
          fmt.Printf("%s", results)
        }
        fmt.Printf("retCode: %s\n", PshellControl.GetRetCodeString(retCode))
      } else {
        retCode := PshellControl.SendCommand1(sid, command)
        fmt.Printf("retCode: %s\n", PshellControl.GetRetCodeString(retCode))
      }
    }
  }
  
}

