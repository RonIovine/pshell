package main

import "os"
import "fmt"
import "syscall"
import "os/signal"
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
func signalHandler(signalChan chan os.Signal) {
	for {
		<-signalChan // This line will block until a signal is received
    PshellServer.CleanupResources()
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

  // register signal handlers so we can do a graceful termination and cleanup any system resources
  registerSignalHandlers()
  
  // register our callback commands, commands consist of single keyword only
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
  
  // run a registered command from within it's parent process, this can be done before
  // or after the server is started, as long as the command being called is regstered
  PshellServer.RunCommand("hello 1 2 3")

  // Now start our PSHELL server with the pshell_startServer function call.
  //
  // The 1st argument is our serverName (i.e. "pshellServerDemo").
  //
  // The 2nd argument specifies the type of PSHELL server, the four valid values are:
  // 
  //   PshellServer.UDP   - Server runs as a multi-session UDP based server.  This requires
  //                        the special stand-alone command line UDP/UNIX client program
  //                        'pshell'.  This server has no timeout for idle client sessions.
  //                        It can be also be controlled programatically via an external
  //                        program running the PshellControl API and library.
  //   PshellServer.UNIX  - Server runs as a multi-session UNIX based server.  This requires
  //                        the special stand-alone command line UDP/UNIX client program
  //                        'pshell'.  This server has no timeout for idle client sessions.
  //                        It can be also be controlled programatically via an external
  //                        program running the PshellControl API and library.
  //   PshellServer.TCP   - Server runs as a single session TCP based server with a 10 minute
  //                        idle client session timeout.  The TCP server can be connected to
  //                        using a standard 'telnet' based client.
  //   PshellServer.LOCAL - Server solicits it's own command input via the system command line
  //                        shell, there is no access via a separate client program, when the
  //                        user input is terminated via the 'quit' command, the program is
  //                        exited.
  //
  // The 3rd argument is the server mode, the two valid values are:
  //
  //   PshellServer.NON_BLOCKING - A separate thread will be created to process user input, when
  //                               this function is called as non-blocking, the function will return
  //                               control to the calling context.
  //   PshellServer.BLOCKING     - No thread is created, all processing of user input is done within
  //                               this function call, it will never return control to the calling context.
  // 
  // The 4th and 5th arguments are only used for a UDP or TCP server, for a LOCAL or
  // UNIX server they are ignored.
  // 
  // For the 4th argument, a valid IP address or hostname can be used.  There are also 3 special
  // "hostname" type identifiers defined as follows:
  //
  //   localhost - the loopback address (i.e. 127.0.0.1)
  //   myhost    - the hostname assigned to this host, this is usually defined in the
  //               /etc/hosts file and is assigned to the default interface
  //   anyhost   - all interfaces on a multi-homed host (including loopback)
  //
  // Finally, the 5th argument is the desired port number.
  //
  // All of these arguments (except the server name and mode, i.e. args 1 & 3) can be overridden 
  // via the 'pshell-server.conf' file on a per-server basis.

  PshellServer.StartServer("pshellServerDemo", serverType, PshellServer.BLOCKING, "127.0.0.1", "7001")

  // should never get here, but cleanup any pshell system resources as good practice
  PshellServer.CleanupResources()

}

