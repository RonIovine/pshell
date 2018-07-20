package main

import "PshellControl"
import "fmt"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func main() {
  sid := PshellControl.ConnectServer("controlName", "127.0.0.1", "6002", 2) 
  retCode, results := PshellControl.SendCommand3(sid, "trace show config")
  if (retCode == PshellControl.COMMAND_SUCCESS) {
    fmt.Printf("%s", results)
  }
  fmt.Printf("retCode: %s:\n", PshellControl.GetRetCodeString(retCode))
}

