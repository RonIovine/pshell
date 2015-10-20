# pshell
**A Lightweight, Process-Specific, Embedded Command Line Shell for C/C++ Applications**

##### INTRODUCTION:

This package contains all the necessary code, documentation and examples for
building C/C++ applications that incorporate a Process Specific Embedded
Command Line Shell (PSHELL).  The PSHELL library provides a simple, lightweight,
framework and API to embed functions within a C/C++ application that can be
invoked either via a separate interactive client program ('telnet' or 'phsell') 
or via direct interaction from within the application itself.

There is also a control mechanism API provided by where any external program can
make calls into another program that is running a PSHELL (only supported for UDP
or UNIX pshell servers).  This will provide a programmatic control access mechanism
to the registered command line shell functions.  A demo program is also provided to
show the usage of the pshell control API.

The signature for the embedded command line shell functions are similar to the 
'main' in 'C' as follows:

`void myFunc(int argc, char *argv[]);`

Command line shell functions can also display information back to the interactive
clients via a mechanism similar to the familiar 'printf' as follows:

`void pshell_printf(const char *format_, ...);`

These functions can be invoked via several mechanisms depending on how the internal PSHELL 
server is configured.  The following shows the various PSHELL server types along with their 
associated invokation method:

* TCP Server   : Uses standard 'telnet' interactive client to invoke functions
* UDP Server   : Uses included 'pshell' interactive client or control API to invoke functions
* UNIX Server  : Uses included 'pshell' interactive client or control API to invoke functions
* LOCAL Server : No client program needed, functions invoked directly from within application 
                 itself via local command line interactive prompting

The functions are dispatched via its registered command name (keyword), along with 0 or more
command line arguments, similar to command line shell processing.  A demo program is also
provided to show the usage of the pshell server API.

This package also provides an optional integrated interactive dynamic trace filtering mechanism that 
can be incorporated into any software that uses an existing trace logging system that uses the `__FILE__`, 
`__LINE__`, `__FUNCTION__`, and `level` paradigm.  If this functionality is not desired, it can be
easily compiled out via the build-time config files.

Along with the optional trace filtering mechanism, there is also an optional integrated trace logging
subsystem, API, and demo program that is provided to demonstrate the dynamic control of logging via
the trace filter pshell CLI mechanism.  This example logging system can also be compiled out via the
build-time config files if an existing logging system is used.

See the full README file for a complete description of all the components, installation, and usage.

Note, this package was originally hosted at [code.google.com](https://code.google.com) as 
[RDB-Lite](https://code.google.com/p/rdb-lite), it was re-christened as 'pshell' when it was 
migrated to this hosting service.
