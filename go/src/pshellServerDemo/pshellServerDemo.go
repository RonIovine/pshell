package main

import "PshellServer"
//import "fmt"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func hello(argv []string) {
  PshellServer.Printf("hello command dispatched:\n")
  for index, arg := range argv {
    PshellServer.Printf("  arg[%d]: %s\n", index, arg)
  }
  PshellServer.Printf("hello command dispatched:\n")
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func main() {
  PshellServer.AddCommand(hello, "hello", "example hello command", "", 0, 0, true) 
  PshellServer.StartServer("pshellServerDemo", PshellServer.UDP, PshellServer.BLOCKING, "127.0.0.1", "7001")
}

