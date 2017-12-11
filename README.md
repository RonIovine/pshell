# pshell
**A Lightweight, Process-Specific, Embedded Command Line Shell for C/C++ Applications**

This package contains all the necessary code, documentation, and examples for
building C/C++ applications that incorporate a Process-Specific Embedded
Command Line Shell (PSHELL).  The PSHELL library provides a simple, lightweight,
framework and API to embed functions within a C/C++ application that can be
invoked either via a separate interactive client program ('telnet' or 'pshell') 
or via direct interaction from within the application itself.

There is also a control mechanism API provided by where any external program can
make calls into another program that is running a PSHELL server (only supported for 
UDP or UNIX pshell servers).  This will provide a direct programmatic control access
mechanism to a remote process' pshell functions without having to fork the calling 
process to call the pshell command line client program via the 'system' call.

Note that there are also Python versions of the PshellServer, PshellControl,
PshellClient, and PshellReadline functionality.  All of the Python modules along 
with corresponding demo programs can be found in the 'python' sub-directory of 
this package.

The prototype for the embedded command line shell functions are similar to the 
'main' in 'C' as follows:

`void myFunc(int argc, char *argv[]);`

Command line shell functions can also display information back to the interactive
clients via a mechanism similar to the familiar 'printf' as follows:

`void pshell_printf(const char *format, ...);`

These functions can be invoked via several methods depending on how the internal PSHELL 
server is configured.  The following shows the various PSHELL server types along with their 
associated invokation method:

* TCP Server   : Uses standard 'telnet' interactive client to invoke functions (C/C++ only)
* UDP Server   : Uses included 'pshell' interactive client or control API to invoke functions
* UNIX Server  : Uses included 'pshell' interactive client or control API to invoke functions
* LOCAL Server : No client program needed, functions invoked directly from within application 
                 itself via local command line interactive prompting

The functions are dispatched via its registered command name (keyword), along with 0 or more
command line arguments, similar to command line shell processing.

This framework also provides the ability to run in non-server, non-interactive mode.  In this
mode, the registered commands can be dispatched via the host's shell command line directly as 
single shot commands via the main registering multi-call binary program.  In this mode, there 
is no interactive user prompting, and control is returned to the calling host's command line 
shell when the command is complete.  This mode also provides the ability to setup softlink 
shortcuts to each internal command and to invoke those commands from the host's command line 
shell directly via the shortcut name  rather than the parent program name, in a manner similar 
[Busybox](https://busybox.net/about.html) functionality.  Note this is not available with the
Python modules.

This package also provides an optional integrated interactive dynamic trace filtering mechanism that 
can be incorporated into any software that uses an existing trace logging system that uses the `__FILE__`, 
`__LINE__`, `__FUNCTION__`, and `level` paradigm.  If this functionality is not desired, it can be
easily compiled out via the build-time config files.

Along with the optional trace filtering mechanism, there is also an optional integrated trace logging
subsystem and API to show the integration of an existing logging system into the dynamic trace filter
API.  The output of this logging system can be controlled via the trace filter pshell CLI mechanism.
This example logging system can also be compiled out via the build-time config files if an existing
logging system is used.

In addition to the infrastructure components, nine demo programs are also provided, two (C and
Python) to show the usage of the pshell server API, two (C and Python) to show the usage of pshell 
control API, two (C and Python) to show the aggergation of multiple pshell servers into a single 
pshell server via the control API, one to show the usage of the trace filter/trace log API, one to 
show the usage of a stand-alone (no filtering) trace log application, and one to show the usage of 
the multi-call binary API.

Finally, a stub module/library is provided that will honor the complete API of the normal pshell
library but with all the underlying functionality stubbed out.  This will allow all the pshell 
functionality to easily be completly removed from an application without requiring any code 
changes or recompilation, only a re-link (for static linking) or restart (when using a shared 
library acessed via a softlink) of the target program is necessary.

See the full README file for a complete description of all the components, installation, building, and usage.

Note, this package was originally hosted at [code.google.com](https://code.google.com) as 
[RDB-Lite](https://code.google.com/p/rdb-lite), it was re-christened as 'pshell' when it was 
migrated to this hosting service.
