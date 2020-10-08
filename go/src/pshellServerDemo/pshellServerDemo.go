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
import "math/rand"
import "PshellServer"

// constants used for the advanved parsing date/time stamp range checking
const (
  MAX_YEAR   = 3000
  MAX_MONTH  = 12
  MAX_DAY    = 31
  MAX_HOUR   = 23
  MAX_MINUTE = 59
  MAX_SECOND = 59
)

// dynamic value used for the dunamicOutput function
var dynamicValue string = "0"

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
// simple helloWorld command that just prints out all the passed in arguments
////////////////////////////////////////////////////////////////////////////////
func helloWorld(argv []string) {
  PshellServer.Printf("helloWorld command dispatched:\n")
  for index, arg := range argv {
    PshellServer.Printf("  arg[%d]: %s\n", index, arg)
  }
}

////////////////////////////////////////////////////////////////////////////////
// this command shows an example client keep alive, the PSHELL UDP client has
// a default 5 second timeout, if a command will be known to take longer than
// 5 seconds, it must give some kind of output back to the client, this shows
// the two helper functions created the assist in this, the TCP client does not
// need a keep alive since the TCP protocol itself handles that, but it can be
// handy to show general command progress for commands that take a long time
////////////////////////////////////////////////////////////////////////////////
func keepAlive(argv []string) {
  if (PshellServer.IsHelp()) {
    PshellServer.Printf("\n")
    PshellServer.ShowUsage()
    PshellServer.Printf("\n")
    PshellServer.Printf("Note, this function demonstrates intermediate flushes in a\n")
    PshellServer.Printf("callback command to keep the UDP/UNIX interactive client from\n" )
    PshellServer.Printf("timing out for commands that take longer than the response\n")
    PshellServer.Printf("timeout (default=5 sec).  This is only supported in the 'C'\n")
    PshellServer.Printf("version of the pshell interactive client, the Python version\n")
    PshellServer.Printf("of the interactive client does not support intermediate flushes.\n")
    PshellServer.Printf("\n")
    return
  } else if (argv[0] == "dots") {
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
// this command shows matching the passed command arguments based on substring
// matching rather than matching on the complete exact string, the minimum
// number of characters that must be matched is the last argument to the
// PshellServer.IsSubString function, this must be the minimum number of
// characters necessary to uniquely identify the argument from the complete
// argument list
//
// NOTE: This technique could have been used in the previous example for the
//       "wheel" and "dots" arguments to provide for wildcarding of those
//       arguments.  In the above example, as written, the entire string of
//       "dots" or "wheel" must be enter to be accepted.
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
// this command shows a command that is registered with the "showUsage" flag
// set to "false", the PshellServer will invoke the command when the user types
// a "?" or "-h" rather than automatically giving the registered usage, the
// callback command can then see if the user asked for help (i.e. typed a "?"
// or "-h") by calling PshellServer.IsHelp, the user can then display the
// standard registered usage with the PshellServer.ShowUsage call and then
// give some optional enhanced usage with the PshellServer.Printf call
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
// this function demonstrates the various helper functions that assist in the
// interpretation and conversion of command line arguments
////////////////////////////////////////////////////////////////////////////////
func formatChecking(argv []string) {

  PshellServer.Printf("formatChecking command dispatched:\n")
  if (PshellServer.IsIpv4Addr(argv[0])) {
    PshellServer.Printf("IPv4 address entered: '%s' entered\n", argv[0])
  } else if (PshellServer.IsIpv4AddrWithNetmask(argv[0])) {
    PshellServer.Printf("IPv4 address/netmask entered: '%s' entered\n", argv[0])
  } else if (PshellServer.IsDec(argv[0])) {
    PshellServer.Printf("Decimal arg: %d entered\n", PshellServer.GetInt(argv[0], PshellServer.RADIX_ANY, false))
  } else if (PshellServer.IsHex(argv[0], true)) {
    PshellServer.Printf("Hex arg: 0x%x entered\n", PshellServer.GetInt(argv[0], PshellServer.RADIX_ANY, true))
  } else if (PshellServer.IsAlpha(argv[0])) {
    if (PshellServer.IsEqual(argv[0], "myarg")) {
      PshellServer.Printf("Alphabetic arg: '%s' equal to 'myarg'\n", argv[0])
    } else {
      PshellServer.Printf("Alphabetic arg: '%s' not equal to 'myarg'\n", argv[0])
    }
  } else if (PshellServer.IsAlphaNumeric(argv[0])) {
    if (PshellServer.IsEqual(argv[0], "myarg1")) {
      PshellServer.Printf("Alpha numeric arg: '%s' equal to 'myarg1'\n", argv[0])
    } else {
      PshellServer.Printf("Alpha numeric arg: '%s' not equal to 'myarg1'\n", argv[0])
    }
  } else if (PshellServer.IsFloat(argv[0])) {
    PshellServer.Printf("Float arg: %.2f entered\n", PshellServer.GetFloat(argv[0]))
  } else {
    PshellServer.Printf("Unknown arg format: '%s'\n", argv[0])
  }
}

////////////////////////////////////////////////////////////////////////////////
// function to show advanced command line parsing using the
// PshellServer.Tokenize function
////////////////////////////////////////////////////////////////////////////////
func advancedParsing(argv []string) {

  numDateTokens, date := PshellServer.Tokenize(argv[0], "/")
  numTimeTokens, time := PshellServer.Tokenize(argv[1], ":")

  if ((numDateTokens != 3) || (numTimeTokens != 3)) {
    PshellServer.Printf("ERROR: Improper timestamp format!!\n")
    PshellServer.ShowUsage()
  } else if (!PshellServer.IsDec(date[0]) ||
             (PshellServer.GetInt(date[0], PshellServer.RADIX_ANY, false) > MAX_MONTH)) {
    PshellServer.Printf("ERROR: Invalid month: %s, must be numeric value <= %d\n",
                        date[0],
                        MAX_MONTH)
  } else if (!PshellServer.IsDec(date[1]) ||
             (PshellServer.GetInt(date[1], PshellServer.RADIX_ANY, false) > MAX_DAY)) {
    PshellServer.Printf("ERROR: Invalid day: %s, must be numeric value <= %d\n",
                        date[1],
                        MAX_DAY)
  } else if (!PshellServer.IsDec(date[2]) ||
             (PshellServer.GetInt(date[2], PshellServer.RADIX_ANY, false) > MAX_YEAR)) {
    PshellServer.Printf("ERROR: Invalid year: %s, must be numeric value <= %d\n",
                        date[2],
                        MAX_YEAR)
  } else if (!PshellServer.IsDec(time[0]) ||
             (PshellServer.GetInt(time[0], PshellServer.RADIX_ANY, false) > MAX_HOUR)) {
    PshellServer.Printf("ERROR: Invalid hour: %s, must be numeric value <= %d\n",
                        time[0],
                        MAX_HOUR)
  } else if (!PshellServer.IsDec(time[1]) ||
             (PshellServer.GetInt(time[1], PshellServer.RADIX_ANY, false) > MAX_MINUTE)) {
    PshellServer.Printf("ERROR: Invalid minute: %s, must be numeric value <= %d\n",
                        time[1],
                        MAX_MINUTE)
  } else if (!PshellServer.IsDec(time[2]) ||
             (PshellServer.GetInt(time[2], PshellServer.RADIX_ANY, false) > MAX_SECOND)) {
    PshellServer.Printf("ERROR: Invalid second: %s, must be numeric value <= %d\n",
                        time[2],
                        MAX_SECOND)
  } else {
    PshellServer.Printf("Month  : %s\n", date[0])
    PshellServer.Printf("Day    : %s\n", date[1])
    PshellServer.Printf("Year   : %s\n", date[2])
    PshellServer.Printf("Hour   : %s\n", time[0])
    PshellServer.Printf("Minute : %s\n", time[1])
    PshellServer.Printf("Second : %s\n", time[2])
  }
}

////////////////////////////////////////////////////////////////////////////////
// function to show output value that change frequently, this is used to
// illustrate the command line mode with a repeated rate and an optional clear
// screen between iterations, using command line mode in this way along with a
// function with dynamically changing output information will produce a display
// similar to the familiar "top" display command output
////////////////////////////////////////////////////////////////////////////////
func dynamicOutput(argv []string) {
  if (PshellServer.IsEqual(argv[0], "show")) {
    currTime := time.Now()
    PshellServer.Printf("\n")
    PshellServer.Printf("DYNAMICALLY CHANGING OUTPUT\n")
    PshellServer.Printf("===========================\n")
    PshellServer.Printf("\n")
    PshellServer.Printf("Timestamp ........: %02d:%02d:%02d.%d\n", currTime.Hour(),
                                                                   currTime.Minute(),
                                                                   currTime.Second(),
                                                                   currTime.Nanosecond()/1000)
    PshellServer.Printf("Random Value .....: %d\n", rand.Uint32())
    PshellServer.Printf("Dynamic Value ....: %s\n", dynamicValue)
    PshellServer.Printf("\n")
  } else {
    dynamicValue = argv[0];
  }
}

////////////////////////////////////////////////////////////////////////////////
// function that shows the extraction of arg options using the
// PshellServer.GetOption function,the format of the options are either
// -<option><value> where <option> is a single character option (e.g. -t10),
// or <option>=<value> where <option> is any length character string (e.g.
// timeout=10)
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
  fmt.Printf("\n")
  fmt.Printf("Usage: pshellServerDemo -udp [<port>] | -tcp [<port>] | -unix | -local\n")
  fmt.Printf("\n")
  fmt.Printf("  where:\n")
  fmt.Printf("    -udp   - Multi-session UDP server\n")
  fmt.Printf("    -tcp   - Single session TCP server\n")
  fmt.Printf("    -unix  - Multi-session UNIX domain server\n")
  fmt.Printf("    -local - Local command dispatching server\n")
  fmt.Printf("    <port> - Desired UDP or TCP port, default: %s\n", PSHELL_DEMO_PORT)
  fmt.Printf("\n")
  os.Exit(0)
}

var PSHELL_DEMO_PORT = "8001"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func main() {

  var serverType string
  if (len(os.Args) == 2 || len(os.Args) == 3) {
    if (os.Args[1] == "-udp") {
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
    if len(os.Args) == 3 {
      PSHELL_DEMO_PORT = os.Args[2]
    }
  } else {
    showUsage()
  }

  // register signal handlers so we can do a graceful termination and cleanup any system resources
  registerSignalHandlers()

  // register our callback commands, commands consist of single keyword only
  PshellServer.AddCommand(helloWorld,                            // function
                          "helloWorld",                          // command
                          "command that prints out arguments",   // description
                          "[<arg1> ... <arg20>]",                // usage
                          0,                                     // minArgs
                          20,                                    // maxArgs
                          true)                                  // showUsage on '?'

  PshellServer.AddCommand(keepAlive,                                               // function
                          "keepAlive",                                             // command
                          "command to show client keep-alive ('C' client only)",   // description
                          "dots | bang | pound | wheel",                           // usage
                          1,                                                       // minArgs
                          1,                                                       // maxArgs
                          false)                                                   // showUsage on '?'

  PshellServer.AddCommand(wildcardMatch,                            // function
                          "wildcardMatch",                          // command
                          "command that does a wildcard matching",  // description
                          "<arg>",                                  // usage
                          1,                                        // minArgs
                          1,                                        // maxArgs
                          false)                                    // showUsage on "?"

  PshellServer.AddCommand(enhancedUsage,                   // function
                          "enhancedUsage",                 // command
                          "command with enhanced usage",   // description
                          "<arg1>",                        // usage
                          1,                               // minArgs
                          1,                               // maxArgs
                          false)                           // showUsage on '?'

  PshellServer.AddCommand(formatChecking,                       // function
                          "formatChecking",                     // command
                          "command with arg format checking",   // description
                          "<arg1>",                             // usage
                          1,                                    // minArgs
                          1,                                    // maxArgs
                          true)                                 // showUsage on "?"

  PshellServer.AddCommand(advancedParsing,                                // function
                          "advancedParsing",                              // command
                          "command with advanced command line parsing",   // description
                          "<mm>/<dd>/<yyyy> <hh>:<mm>:<ss>",              // usage
                          2,                                              // minArgs
                          2,                                              // maxArgs
                          true)                                           // showUsage on "?"

  PshellServer.AddCommand(dynamicOutput,                                         // function
                          "dynamicOutput",                                       // command
                          "command with dynamic output for command line mode",   // description
                          "show | <value>",                                      // usage
                          1,                                                     // minArgs
                          1,                                                     // maxArgs
                          true)                                                  // showUsage on "?"

  PshellServer.AddCommand(getOptions,                                  // function
                          "getOptions",                                // command
                          "example of parsing command line options",   // description
                          "<arg1> [<arg2>...<argN>]",                  // usage
                          1,                                           // minArgs
                          20,                                          // maxArgs
                          false)                                       // showUsage on '?'

  // run a registered command from within it's parent process, this can be done before
  // or after the server is started, as long as the command being called is regstered
  PshellServer.RunCommand("helloWorld 1 2 3")

  // Now start our PSHELL server with the PshellServer.StartServer function call.
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

  //
  // NOTE: This server is started up in BLOCKING mode, but if a pshell server is being added
  //       to an existing process that already has a control loop in it's main, it should be
  //       started in NON_BLOCKING mode before the main drops into it's control loop, see the
  //       demo program traceFilterDemo.cc for an example
  //
  PshellServer.StartServer("pshellServerDemo", serverType, PshellServer.BLOCKING, PshellServer.ANYHOST, PSHELL_DEMO_PORT)

  //
  // should never get here because the server is started in BLOCKING mode,
  // but cleanup any pshell system resources as good practice
  //
  PshellServer.CleanupResources()

}

