# pshell
**Process Specific Embedded Command Line Shell for C/C++ Applications**

##### INTRODUCTION:

This package contains all the necessary code, documentation and examples for
building C/C++ applications that incorporate a Process Specific Embedded
Command Line Shell (PSHELL).  The PSHELL library provides a simple, lightweight,
framework and API to embed functions within a C/C++ application that can be
invoked either via a separate client program or directly from the within application itself.  The signature for the embedded functions are similar to the 'main' in 'C' as follows:

`void myFunc(int argc, char *argv[]);`

These functions can be invoked in several ways depending on how the internal
PSHELL server is configured.  The following shows the various PSHELL server
types along with their associated invokation method:

* TCP Server   : Uses standard 'telnet' client to invoke functions
* UDP Server   : Uses included 'pshell' client to invoke functions
* UNIX Server  : Uses included 'pshell' client to invoke functions
* LOCAL Server : No client program needed, functions invoked directly from within application itself

The functions are dispatched via its registered command name (keyword), along
with 0 or more command line arguments, similar to command line shell processing.

There is also a control mechanism API provided by where any external program
can make calls into another program that is running a PSHELL that is configured
as a UDP or UNIX server.

This package also provides an integrated interactive dynamic trace filtering mechanism that can be incorporated into any software that uses an existing trace logging system that uses the `__FILE__`, `__LINE__`, `__FUNCTION__`, and `level` paradigm. The trace filtering mechanism does not provide the actual logging service itself, only the filtering mechanism that can be dynamically controlled via the embedded CLI. 

