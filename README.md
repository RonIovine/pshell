# pshell
**Process Specific Embedded Command Line Shell for C/C++ Applications**

##### INTRODUCTION:

This package contains all the necessary code, documentation and examples for
building C/C++ applications that incorporate a Process Specific Embedded
Command Line Shell (PSHELL).  The PSHELL library provides a simple, lightweight,
framework and API to embed functions within a C/C++ application that can be
invoked either via a separate interactive client program ('telnet' or 'phsell') 
or directly from the within application itself.

There is also a control mechanism API provided by where any external program
can make calls into another program that is running a PSHELL (only supported for
UDP or UNIX pshell servers).  This will provide a programmatic control access 
mechanism to the registered command line shell functions.

The signature for the embedded functions are similar to the 'main' in 'C' as follows:

`void myFunc(int argc, char *argv[]);`

These functions can be invoked via several mechanisms ways depending on how the internal PSHELL 
server is configured.  The following shows the various PSHELL server types along with their 
associated invokation method:

* TCP Server   : Uses standard 'telnet' interactive client to invoke functions
* UDP Server   : Uses included 'pshell' interactive client or control API nto invoke functions
* UNIX Server  : Uses included 'pshell' interactive client or control API to invoke functions
* LOCAL Server : No client program needed, functions invoked directly from within application 
                 itself via local command line ineractive prompting

The functions are dispatched via its registered command name (keyword), along
with 0 or more command line arguments, similar to command line shell processing.

This package also provides an optional integrated interactive dynamic trace filtering mechanism that can be incorporated into any software that uses an existing trace logging system that uses the `__FILE__`, `__LINE__`, `__FUNCTION__`, and `level` paradigm. The trace filtering mechanism does not provide the actual logging service itself, only the filtering mechanism that can be dynamically controlled via the embedded CLI. 

See the full README file for a complete description of all the components, installation, and usage.

Note, this package was originally hosted at [code.google.com](https://code.google.com) as [RDB-Lite](https://code.google.com/p/rdb-lite), it was re-christened as 'pshell' when it was migrated to this hosting service.
