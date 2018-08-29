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
// This is an example demo program that uses all the basic features of the PSHELL 
// server Go module.  This program can be run as either a UDP, TCP, UNIX, or 
// local server based on the command line options.  If it is run as a UDP or UNIX 
// based server, you must use the provided stand-alone UDP client program 'pshell' 
// or 'pshell.py' to connect to it, if it is run as a TCP server, you must use
// 'telnet, if it is run as a local server, user command line input is solicited 
// directly from this program, no external client is needed.
//
////////////////////////////////////////////////////////////////////////////////

// import all our necessary modules
import "os"
import "fmt"
import "syscall"
import "time"
import "os/signal"
import "PshellServer"

////////////////////////////////////////////////////////////////////////////////
//
// PSHELL user callback functions, the interface is similar to the "main" in 
// Go, with the argv argument being the argument list and the len and contents
// of argv are the user entered arguments, excluding the actual command itself
// (arguments only).
//
// Use the special 'PshellServer.Printf' function call to display data back to 
// the remote client.  The interface to this function is similar to the standard
// 'fmt.Printf' function in Go.
//
////////////////////////////////////////////////////////////////////////////////
 
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func hello(argv []string) {
  PshellServer.Printf("hello command dispatched:\n")
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
func wildcardMatch(argv []string) {
  if (PshellServer.IsHelp()) {
    PshellServer.Printf("\n")
    PshellServer.ShowUsage()
    PshellServer.Printf("\n")
    PshellServer.Printf("  where valid <args> are:\n")
    PshellServer.Printf("    on\n")
    PshellServer.Printf("    of*f\n")
    PshellServer.Printf("    a*ll\n")
    PshellServer.Printf("    sy*mbols\n")
    PshellServer.Printf("    se*ttings\n")
    PshellServer.Printf("    d*efault\n")
    PshellServer.Printf("\n")
  } else if (PshellServer.IsSubString(argv[0], "on", 2)) {
    PshellServer.Printf("argv 'on' match\n")
  } else if (PshellServer.IsSubString(argv[0], "off", 2)) {
    PshellServer.Printf("argv 'off' match\n")
  } else if (PshellServer.IsSubString(argv[0], "all", 1)) {
    PshellServer.Printf("argv 'all' match\n")
  } else if (PshellServer.IsSubString(argv[0], "symbols", 2)) {
    PshellServer.Printf("argv 'symbols' match\n")
  } else if (PshellServer.IsSubString(argv[0], "settings", 2)) {
    PshellServer.Printf("argv 'settings' match\n")
  } else if (PshellServer.IsSubString(argv[0], "default", 1)) {
    PshellServer.Printf("argv 'default' match\n")
  } else {
    PshellServer.Printf("\n")
    PshellServer.ShowUsage()
    PshellServer.Printf("\n")
    PshellServer.Printf("  where valid <args> are:\n")
    PshellServer.Printf("\n")
    PshellServer.Printf("    on\n")
    PshellServer.Printf("    of*f\n")
    PshellServer.Printf("    a*ll\n")
    PshellServer.Printf("    sy*mbols\n")
    PshellServer.Printf("    se*ttings\n")
    PshellServer.Printf("    d*efault\n")
    PshellServer.Printf("\n")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func keepAlive(argv []string) {
  if (argv[0] == "dots") {
    PshellServer.Printf("marching dots keep alive:\n")
    for i := 0; i < 10; i++ {
      PshellServer.March(".")
      time.Sleep(time.Second)
    }
  } else if (argv[0] == "bang") {
    PshellServer.Printf("marching 'bang' keep alive:\n")
    for i := 0; i < 10; i++ {
      PshellServer.March("!")
      time.Sleep(time.Second)
    }
  } else if (argv[0] == "pound") {
    PshellServer.Printf("marching pound keep alive:\n")
    for i := 0; i < 10; i++ {
      PshellServer.March("#")
      time.Sleep(time.Second)
    }
  } else if (argv[0] == "wheel") {
    PshellServer.Printf("spinning wheel keep alive:\n")
    for i := 0; i < 10; i++ {
      // string is optional, use NULL to omit
      PshellServer.Wheel("optional string: ")
      time.Sleep(time.Second)
    }
  } else {
    PshellServer.ShowUsage()
    return
  }
  PshellServer.Printf("\n")
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
    for index, arg := range(argv) {
      parsed, key, value := PshellServer.GetOption(arg)
      PshellServer.Printf("arg[%d]: '%s', parsed: %v, key: '%s', value: '%s'\n", index, arg, parsed, key, value)
    }
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
  
  PshellServer.AddCommand(wildcardMatch,                            // function
                          "wildcardMatch",                          // command
                          "command that does a wildcard matching",  // description
                          "<arg>",                                  // usage
                          1,                                        // minArgs
                          1,                                        // maxArgs
                          false)                                    // showUsage on "?"

  PshellServer.AddCommand(getOptions,                                  // function
                          "getOptions",                                // command
                          "example of parsing command line options",   // description
                          "<arg1> [<arg2>...<argN>]",                  // usage      
                          1,                                           // minArgs
                          20,                                          // maxArgs
                          false)                                       // showUsage on '?'
  
  // TCP or LOCAL servers don't need a keep-alive, so only add 
  // this command for connectionless datagram type servers
  if ((serverType == PshellServer.UDP) || (serverType == PshellServer.UNIX)) {
    PshellServer.AddCommand(keepAlive,                             // function
                            "keepAlive",                           // command
                            "command to show client keep-alive",   // description
                            "dots | bang | pound | wheel",         // usage
                            1,                                     // minArgs
                            1,                                     // maxArgs
                            false)                                 // showUsage on '?'
  }

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

  PshellServer.StartServer("pshellServerDemo", serverType, PshellServer.BLOCKING, PshellServer.ANYHOST, "7001")

  // should never get here, but cleanup any pshell system resources as good practice
  PshellServer.CleanupResources()

}

