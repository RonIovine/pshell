package main

import "PshellServer"
//import "fmt"
//import "strings"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func hello(argv []string) {
  PshellServer.Printf("hello command dispatched-1:\n")
  for index, arg := range argv {
    PshellServer.Printf("  arg[%d]: %s\n", index, arg)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func main() {
  //stringToSplit := "this is a string to split"
  //splitString := strings.Split(stringToSplit, " ")
  //for _, myString := range splitString {
  //  fmt.Printf("len: %d\n", len(myString))
  //}
  
  PshellServer.AddCommand(hello, "hello", "example hello command", "", 0, 0, true) 
  PshellServer.StartServer("pshellServerDemo", PshellServer.UDP, PshellServer.BLOCKING, "127.0.0.1", "7001")
}

