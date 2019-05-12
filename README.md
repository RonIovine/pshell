# pshell
**A Lightweight, Process-Specific, Embedded Command Line Shell for C/C++/Python/Go Applications**

This package contains all the necessary code, documentation and examples for
building C/C++/Python/Go applications that incorporate a Process Specific Embedded
Command Line Shell (PSHELL).  The PSHELL library provides a simple, lightweight,
framework and API to embed functions within a C/C++/Python/Go application that can
be invoked either via a separate client program or directly from the within
application itself.

There is also a control API provided by where any external program can invoke another
program's registered pshell functions (only supported for UDP or UNIX pshell servers).
This will provide direct programmatic control of a remote process' pshell functions
without having to fork the calling process to call the 'pshell' command line client
program via the 'system' call.  This provides functionality similar to the familiar
Remote Procedure Call (RPC) mechanism.

The control API can function as a simple control plane IPC mechanism for inter-process
communication and control.  If all inter-process control is implemented as a collection
of pshell commands, a user can get a programmatic IPC mechanism along with manual CLI/Shell
access via the same code base.  There is no need to write separate code for CLI/Shell
processing and control/message event processing.  All inter-process control can then be
driven and tested manually via one of the interactive client programs (pshell or pshellAggregator).

The control API supports both unicast and 'multicast' (not true subscriber based multicast
like IGMP, it's more like sender based aggregated unicast)  messaging paradigms.  It also
supports messaging to broadcast pshell servers (i.e. UDP server running at a subnet
broadcast address, e.g. x.y.z.255).

The Python, 'C', and 'go' versions are consistent with each other at the API level (i.e.
similar functional API, usage, process interaction etc) and fully interoperable with each
other at the protocol level and can be mixed and matched in any combination.

The prototype for the callback functions follow the paradigms of the 'main' for each
language.  A pshell callback function can be thought of as a collection of mini 'mains'
within the given process that is invoked via its registered keyword.  The arguments are
passed into each function just like they would be passed into the 'main' from the host's
command line shell (i.e. bash) for each language as shown below.  See the included demo
programs for language specific examples.

'C' callback:

`void myFunc(int argc, char *argv[])`

Python callback:

`def myFunc(argv)`

'go' callback:

`func myFunc(argv []string)`

Command line pshell functions can also display information back to the interactive clients
via a mechanism similar to the familiar 'printf' as follows:

'C' printf:

`void pshell_printf(const char *format, ...)`

Python printf:

`def printf(string)`

'go' printf:

`func Printf(format string, message ...interface{})`

These functions can be invoked via several methods depending on how the internal PSHELL
server is configured.  The following shows the various PSHELL server types along with their
associated invokation method:

* TCP Server   : Uses standard telnet interactive client to invoke functions
* UDP Server   : Uses included pshell/pshellAggregator interactive client or control API to invoke functions
* UNIX Server  : Uses included pshell/pshellAggregator interactive client or control API to invoke functions
* LOCAL Server : No client program needed, functions invoked directly from within application
                 itself via local command line interactive prompting

The functions are dispatched via its registered command name (keyword), along with 0 or more
command line arguments, similar to command line shell processing.

#### Getting started
There is a short .ppt slide deck that gives a good overview of the framework.  See PSHELL-Framework.ppt

#### Installation
All of the included binaries should work for most modern x86_64 based Linux systems as-is.  They have 
been tested on Mint, Ubuntu, and CentOS.  To install, there is an `install.sh` script provided.  To see 
the usage of the install script, from the top level pshell directory run:

`$ ./install.sh -h` 

The simplest install is a local install, from the top level pshell directory run:

`$ ./install.sh -local`

This will create an environment file, `.pshellrc` that should be sourced in your local shell, it should 
also be added to your shell env file, i.e. `.bashrc`.  This will setup several softlinks and environment
variables that will allow access to the various parts of the framework.

#### Building

For targets other than Linux x86_64, the 'C' and 'go' code will need to be guilt from source.  This 
framework has been successfully built and run on Raspbian/ARM Linux, MAC OSX, and Windows Cygwin.  
To build the 'C' source, a makefile is provided along with a default make config file, `defconfig`.  
To see the make options, just type:

`$ make`

To do a make and local install, run:

`$ make install local=y`

This will compile all the 'C' code and run the above `install.sh` script for a local install.

To build all the 'go' code, you first need to see if you have 'golang' installed on your system, and 
if not, install it with yor appropriate package manager.

To build all the 'go' code, first do a local install or local make install and then 'cd' to the 
'go/src' directory and run:

`$ go install <targetDirectory>`

Where target directory is every sub directory in the 'go/src' directory.  See the following example 
to build the 'PshellControl' 'go' module:

`$ go install PshellControl`

#### Demo programs
There are several demo programs that provide examples of using the various aspects of the framework.
The following sections describes them in order of importance.

##### pshellServerDemo ('C', Python, and 'go')
This is the most important demo program.  It shows how to setup a pshell server to run within any process.
It has examples of pshell callback functions, the registration of those functions within the framework, 
and the starting of the pshell server.  This is all that is needed to add interactive pshell access to a 
given process.  From your shell command line, invoke any of the `pshellServerDemo` programs with the `-h` 
option to see the usage.  All language implementations are fully compatible with all different clients.
Note that in these demo inpmementations, the servers are setup at the end of the `main` in BLOCKING mode.
When retro-fitting an existing application that already has a final control loop in the `main` to keep the
process resident, the server will most likely be setup at the beginning of the `main`, before the final
control loop and in NON_BLOCKING mode.  See the `traceFilterDemo.cc` program for an example of this.

```
$ pshellServerDemo -h

Usage: pshellServerDemo -udp [<port>] | -tcp [<port>] | -unix | -local

  where:
    -udp   - Multi-session UDP server
    -tcp   - Single session TCP server
    -unix  - Multi-session UNIX domain server
    -local - Local command dispatching server
    <port> - Desired UDP or TCP port, default: 6001
``` 
Then invoke the program in the foreground with the desired server type.  For a good example, run 
the program in 4 different windows, each using a different server type.  To connect to the TCP
server type:

`$ telnet localhost 6001`

To connect to the UDP server type:

`$ pshell localhost 6001`

To connect to the Unix server type:

`$ pshell pshellServerDemo`

Note that there are 2 versions of the pshell UDP/Unix client progrmas, `pshell`, which is a compiled
'C' implementation, and `pshell.py`, which is a Python implementation.  Any of those can interface to
any of the `pshellServerDemo` programs for all 3 languages.  The pshell client programs both take a `-h`
to show the usage as follows:

```
$ pshell -h

Usage: pshell -s | [-t<timeout>] {{<hostName> | <ipAddr>} {<portNum> | <serverName>}} | <unixServerName>
                   [{{-c <command> | -f <fileName>} [rate=<seconds>] [repeat=<count>] [clear]}]

  where:

    -s             - show named servers in pshell-client.conf file
    -c             - run command from command line
    -f             - run commands from a batch file
    -t             - change the default server response timeout
    hostName       - hostname of UDP server
    ipAddr         - IP address of UDP server
    portNum        - port number of UDP server
    serverName     - name of UDP server from pshell-client.conf file
    unixServerName - name of UNIX server
    timeout        - response wait timeout in sec (default=5)
    command        - optional command to execute (in double quotes, ex. -c "myCommand arg1 arg2")
    fileName       - optional batch file to execute
    rate           - optional rate to repeat command or batch file (in seconds)
    repeat         - optional repeat count for command or batch file (default=forever)
    clear          - optional clear screen between commands or batch file passes

    NOTE: If no <command> is given, pshell will be started
          up in interactive mode, commands issued in command
          line mode that require arguments must be enclosed 
          in double quotes, commands issued in interactive
          mode that require arguments do not require double
          quotes.

          To get help on a command in command line mode, type
          "<command> ?" or "<command> -h".  To get help in
          interactive mode type 'help' or '?' at the prompt to
          see all available commands, to get help on a single
          command, type '<command> {? | -h}'.  Use TAB completion
          to fill out partial commands and up-arrow to recall
          for command history.
```

##### pshellControlDemo ('C', Python, and 'go')
These demo programs show one process invoking pshell functions in another process using the control API.
This is the RPC-like IPC mechanism.  All 3 implementations take a `-h` to show the usage.  Any of them can
be used to connect to any of the previous `pshellServerDemo` programs and invoke their functions.  The
control demo programs will prompt the user for input for the remote command to invoke, but in real process
to process IPC situation, the IPC commands used in the control API functions will most likely be hard
coded.  The following is the usage of the `pshellControlDemo` programs:

```
$ pshellControlDemo -h

Usage: pshellControlDemo {<hostname> | <ipAddress> | <unixServerName>} {<port> | unix}
                         [-t<timeout>] [-l<logLevel>] [-extract]

  where:
    <hostname>       - hostname of UDP server
    <ipAddress>      - IP address of UDP server
    <unixServerName> - name of UNIX server
    unix             - specifies a UNIX server
    <port>           - port number of UDP server
    <timeout>        - wait timeout for response in mSec (default=100)
    <logLevel>       - log level of control library (0-3, default=3, i.e. all)
    extract          - extract data contents of response (must have non-0 wait timeout)
```

See the full README file for a complete description of all the components, installation, building, and usage.

Note, this package was originally hosted at [code.google.com](https://code.google.com) as
[RDB-Lite](https://code.google.com/p/rdb-lite), it was re-christened as 'pshell' when it was
migrated to this hosting service.
