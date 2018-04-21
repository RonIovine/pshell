#!/bin/bash

#################################################################################
# 
# Copyright (c) 2009, Ron Iovine, All rights reserved.  
#  
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Ron Iovine nor the names of its contributors 
#       may be used to endorse or promote products derived from this software 
#       without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY Ron Iovine ''AS IS'' AND ANY EXPRESS OR 
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
# IN NO EVENT SHALL Ron Iovine BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
# POSSIBILITY OF SUCH DAMAGE. 
#
#################################################################################

localDir=$(pwd)
if [ ! -e "$localDir/uninstall.sh" ]
then
  echo "ERROR: This script must be run from same directory where it resides"
  exit 1
fi

if [ $# -ge 1 ]
then

  echo
  echo "Usage: uninstall.sh"
  echo
  echo "  This uninstall script will remove all pshell related files that were"
  echo "  installed via a system wide install.  This script must be run  as 'root'."
  echo
  exit 1
  
else

  # system wide uninstall
  if [ $(whoami) != "root" ]
  then
    echo "ERROR: Must be 'root' to do system uninstall of this package"
    exit 1
  fi

  if [ $OSTYPE == "cygwin" ]
  then
    fileExt=".exe"
  fi

  binDir="/usr/bin"
  includeDir="/usr/include"
  pshellDir="/etc/pshell"
  configDir="/etc/pshell/config"
  batchDir="/etc/pshell/batch"
  startupDir="/etc/pshell/startup"
  man1Dir="/usr/share/man/man1"
  man3Dir="/usr/share/man/man3"
  
  if [ -d "/usr/lib64" ]
  then
    libDir="/usr/lib64"
  else
    libDir="/usr/lib"
  fi
  
  echo "Removing pshell files..."
  
  echo "Removing libs..."
  
  echo "Removing lib directory $libDir/pshell"
  rm -rf $libDir/pshell
  
  echo "Removing softlink $libDir/libpshell-server"
  rm -f $libDir/libpshell-server.so
  rm -f $libDir/libpshell-server.a
  
  echo "Removing softlink $libDir/libpshell-control"
  rm -f $libDir/libpshell-control.so
  
  echo "Removing pshell$fileExt from $binDir"
  rm -f $binDir/pshell$fileExt
  
  echo "Removing pshellAggregator$fileExt from $binDir"
  rm -f $binDir/pshellAggregator$fileExt
  
  echo "Removing includes..."
  
  echo "Removing PshellServer.h from $includeDir"
  rm -f $includeDir/PshellServer.h
  
  echo "Removing PshellControl.h from $includeDir"
  rm -f $includeDir/PshellControl.h
  
  echo "Removing TraceFilter.h from $includeDir"
  rm -f $includeDir/TraceFilter.h
  
  echo "Removing TraceLog.h from $includeDir"
  rm -f $includeDir/TraceLog.h
  
  echo "Removing pshell directory $pshellDir"
  rm -rf $pshellDir

  echo "Removing manpages..."
  
  echo "Removing pshell.1 from $man1Dir"
  rm -f $man1Dir/pshell.1.gz
  
  echo "Removing PshellControl.3 from $man3Dir"
  rm -f $man3Dir/PshellControl.3.gz
  
  echo "Removing PshellServer.3 from $man3Dir"
  rm -f $man3Dir/PshellServer.3.gz
  
  echo "Removing TraceFilter.3 from $man3Dir"
  rm -f $man3Dir/TraceFilter.3.gz
  
  echo "Removing TraceLog.3 from $man3Dir"
  rm -f $man3Dir/TraceLog.3.gz
  
fi
