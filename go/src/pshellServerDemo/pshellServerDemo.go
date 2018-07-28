package main

import "os"
import "fmt"
//import "strings"
import "PshellServer"

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
func world(argv []string) {
  PshellServer.Printf("world command dispatched:\n")
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func enhancedUsage(argv []string) {
  // see if the user asked for help
  if (PshellServer.IsHelp()) {
    // show standard usage 
    PshellServer.ShowUsage()
    // give some enhanced usage 
    PshellServer.Printf("Enhanced usage here...\n")
  } else {
    // do normal function processing 
    PshellServer.Printf("enhancedUsage command dispatched:\n")
    for index, arg := range argv {
      PshellServer.Printf("  argv[%d]: '%s'\n", index, arg)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func keepAlive(argv []string) {
/*
  if (argv[0] == "dots"):
    PshellServer.printf("marching dots keep alive:")
    for i in range(1,10):
      PshellServer.march(".")
      time.sleep(1)
  elif (argv[0] == "bang"):
    PshellServer.printf("marching 'bang' keep alive:");
    for  i in range(1,10):
      PshellServer.march("!")
      time.sleep(1)
  elif (argv[0] == "pound"):
    PshellServer.printf("marching pound keep alive:");
    for  i in range(1,10):
      PshellServer.march("#")
      time.sleep(1)
  elif (argv[0] == "wheel"):
    PshellServer.printf("spinning wheel keep alive:")
    for  i in range(1,10):
      // string is optional, use NULL to omit
      PshellServer.wheel("optional string: ")
      time.sleep(1)
  else:
    PshellServer.showUsage()
    return
  PshellServer.printf()
*/
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getOptions(argv []string) {
  if (PshellServer.IsHelp()) {
    PshellServer.Printf("\n")
    PshellServer.ShowUsage()
    PshellServer.Printf("\n")
    PshellServer.Printf("  where:\n")
    PshellServer.Printf("    arg - agrument of the format -<key><value> or <key>=<value>\n")
    PshellServer.Printf("\n")
    PshellServer.Printf("  For the first form, the <key> must be a single character, e.g. -t10\n")
    PshellServer.Printf("  for the second form, <key> can be any length, e.g. timeout=10\n")
    PshellServer.Printf("\n")
  } else {
    //for index, arg in enumerate(argv):
    //  (parsed, key, value) = PshellServer.getOption(arg)
    //  PshellServer.printf("arg[%d]: '%s', parsed: %s, key: '%s', value: '%s'" % (index, arg, parsed, key, value))
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showUsage() {
  fmt.Printf("Usage: pshellServerDemo -udp | -tcp | -unix | -local\n")
  os.Exit(0)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func main() {

  var serverType string
  if (len(os.Args) != 2) {
    showUsage()
  } else if (os.Args[1] == "-udp") {
    serverType = PshellServer.UDP
  } else if (os.Args[1] == "-unix") {
    serverType = PshellServer.UNIX
  } else if (os.Args[1] == "-tcp") {
    serverType = PshellServer.TCP
  } else if (os.Args[1] == "-local") {
    serverType = PshellServer.LOCAL
  } else {
    showUsage()
  }
  
  PshellServer.AddCommand(hello,                           // function
                          "hello",                         // command
                          "example hello command",         // description
                          "[<arg1> ... <arg20>]",          // usage          
                          0,                               // minArgs
                          20,                              // maxArgs
                          true)                            // showUsage on '?'
  
  PshellServer.AddCommand(world,                         // function
                          "world",                       // command
                          "world command description",   // description
                          "",                            // usage      
                          0,                             // minArgs
                          0,                             // maxArgs
                          true)                          // showUsage on '?'
  
  PshellServer.AddCommand(enhancedUsage,                   // function
                          "enhancedUsage",                 // command
                          "command with enhanced usage",   // description
                          "<arg1>",                        // usage      
                          1,                               // minArgs
                          1,                               // maxArgs
                          false)                           // showUsage on '?'
  
  PshellServer.AddCommand(getOptions,                                  // function
                          "getOptions",                                // command
                          "example of parsing command line options",   // description
                          "<arg1> [<arg2>...<argN>]",                  // usage      
                          1,                                           // minArgs
                          20,                                          // maxArgs
                          false)                                       // showUsage on '?'
  
  PshellServer.StartServer("pshellServerDemo", serverType, PshellServer.BLOCKING, "127.0.0.1", "7001")
}

