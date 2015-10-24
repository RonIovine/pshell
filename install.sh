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
if [ ! -e "$localDir/install.sh" ]
then
  echo "ERROR: This script must be run from same directory where it resides"
  exit 1
fi

if [ $# -ge 1 ]
then
  if [ "$1" != "-local" -o $# -gt 2 ]
  then
    echo
    echo "Usage: install.sh [-local [<shellEnvFile>]]"
    echo
    echo "  This install script will either install this package on a system"
    echo "  wide basis or will setup a local install environment.  A system"
    echo "  install must be done as 'root' and will copy all libraries, binaries,"
    echo "  include files, conf files, and manpages to their customary system"
    echo "  locations.  A local install will not copy/move any files.  It will"
    echo "  only create a pshell env file (.pshellrc) that can be sourced in the"
    echo "  current shell or can be added to your shell env file (i.e. .bashrc)"
    echo "  that will allow use of the package features from a local directory."
    echo
    echo "  The location of the local install environment will be the directory"
    echo "  where this script resides."
    echo
    echo "  where:"
    echo "    local        - install locally, omit for system install"
    echo "    shellEnvFile - name of shell environment file to modify"
    echo "                   (e.g. full path to your .bashrc)"
    echo
    exit 1
  else
    # local install, create .pshellrc env file
    echo "Setting softlink libpshell-server to libpshell-server-full"
    if [ -e "lib/libpshell-server.so" ]
    then
      rm -f lib/libpshell-server.so
    fi
    ln -s $localDir/lib/libpshell-server-full.so lib/libpshell-server.so
    if [ -e "lib/libpshell-server.a" ]
    then
      rm -f lib/libpshell-server.a
    fi
    ln -s $localDir/lib/libpshell-server-full.a lib/libpshell-server.a
    rm -f .pshellrc
    echo "#" >> .pshellrc
    echo "# Local PSHELL install env file" >> .pshellrc
    echo "#" >> .pshellrc
    echo "# This env file can be sourced in the current command shell with 'source .pshellrc'" >> .pshellrc
    echo "# or can be sourced for all shells by adding 'source $localDir/.pshellrc' to your '.bashrc'" >> .pshellrc
    echo "#" >> .pshellrc
    echo export PSHELL_INSTALL=$localDir >> .pshellrc
    echo export PSHELL_BATCH_DIR=\$PSHELL_INSTALL/batch >> .pshellrc
    echo export PSHELL_STARTUP_DIR=\$PSHELL_INSTALL/startup >> .pshellrc
    echo export PSHELL_CONFIG_DIR=\$PSHELL_INSTALL/config >> .pshellrc
    echo export PATH=\$PSHELL_INSTALL/bin:\$PSHELL_INSTALL/utils:\$PATH >> .pshellrc
    echo export MANPATH=\$PSHELL_INSTALL/man:\$MANPATH >> .pshellrc
    echo export LD_LIBRARY_PATH=\$PSHELL_INSTALL/lib:\$LD_LIBRARY_PATH >> .pshellrc
    echo "Local install environment setup in '.pshellrc'"
    echo "To source enviromnent in current shell run 'source .pshellrc'"
    if [ $# -eq 2 ]
    then
      shellEnvFile=$2
      if [ ! -e "$shellEnvFile" ]
      then
        echo "ERROR: Shell env file: $shellEnvFile, not found"
        echo "To source enviromnent in all shells add 'source $localDir/.pshellrc' to your '.bashrc'"
      else
        echo "#" >> $shellEnvFile
        echo "# Setup local PSHELL install environment" >> $shellEnvFile
        echo "#" >> $shellEnvFile
        echo source $localDir/.pshellrc >> $shellEnvFile
      fi
    else
      echo "To source enviromnent in all shells add 'source $localDir/.pshellrc' to your '.bashrc'"
    fi
  fi
else
  # system wide install
  if [ $(whoami) != "root" ]
  then
    echo "ERROR: Must be 'root' to do system install of this package,"
    echo "       run 'install.sh -local' for local install or 'install.sh -h'"
    echo "       for usage"
    exit 1
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
  
  echo "Installing pshell files..."
  echo "Installing libs..."
  
  if [ ! -d $libDir/pshell ]
  then
    echo "Creating lib directory: $libDir"
    mkdir $libDir/pshell
  fi
  
  echo "Copying libpshell-server-full.so to $libDir/pshell"
  cp lib/libpshell-server-full.so $libDir/pshell/.
  echo "Copying libpshell-server-full.a to $libDir/pshell"
  cp lib/libpshell-server-full.a $libDir/pshell/.
  echo "Copying libpshell-server-stub.so to $libDir/pshell"
  cp lib/libpshell-server-stub.so $libDir/pshell/.
  echo "Copying libpshell-server-stub.a to $libDir/pshell"
  cp lib/libpshell-server-stub.a $libDir/pshell/.
  echo "Copying libpshell-control.so to $libDir/pshell"
  cp lib/libpshell-control.so $libDir/pshell/.
  echo "Copying libpshell-control.a to $libDir/pshell"
  cp lib/libpshell-control.a $libDir/pshell/.
  
  echo "Setting softlink $libDir/libpshell-server to $libDir/pshell/libpshell-server-full"
  if [ -e "$libDir/libpshell-server.so" ]
  then
    rm -f $libDir/libpshell-server.so
  fi
  ln -s $libDir/pshell/libpshell-server-full.so $libDir/libpshell-server.so
  
  if [ -e "$libDir/libpshell-server.a" ]
  then
    rm -f $libDir/libpshell-server.a
  fi
  ln -s $libDir/pshell/libpshell-server-full.a $libDir/libpshell-server.a

  echo "Setting softlink $libDir/libpshell-control to $libDir/pshell/libpshell-control"
  if [ -e "$libDir/libpshell-control.so" ]
  then
    rm -f $libDir/libpshell-control.so
  fi
  ln -s $libDir/pshell/libpshell-control.so $libDir/libpshell-control.so
  
  echo "Installing bins..."
  echo "Copying pshell to $binDir"
  cp bin/pshell $binDir/.
  chmod +x $binDir/pshell
  
  echo "Installing includes..."
  echo "Copying PshellServer.h to $includeDir"
  cp include/PshellServer.h $includeDir/.
  echo "Copying PshellControl.h to $includeDir"
  cp include/PshellControl.h $includeDir/.
  echo "Copying TraceFilter.h to $includeDir"
  cp include/TraceFilter.h $includeDir/.
  echo "Copying TraceLog.h to $includeDir"
  cp include/TraceLog.h $includeDir/.
  
  if [ ! -d $pshellDir ]
  then
    echo "Creating pshell directory: $pshellDir"
    mkdir $pshellDir
  fi

  echo "Installing conf files..."

  if [ ! -d $configDir ]
  then
    echo "Creating conf directory: $configDir"
    mkdir $configDir
  fi
  
  echo "Copying pshell-server.conf to $configDir"
  cp config/pshell-server.conf $configDir/.
  echo "Copying pshell-client.conf to $configDir"
  cp config/pshell-client.conf $configDir/.
  echo "Copying pshell-control.conf to $configDir"
  cp config/pshell-control.conf $configDir/.
  echo "Copying README to $configDir"
  cp config/README $configDir/.
  chmod 666 $configDir/*
  
  echo "Installing batch files..."
  if [ ! -d $batchDir ]
  then
    echo "Creating batch directory: $batchDir"
    mkdir $batchDir
  fi
  
  #echo "Copying pshellServerDemo.batch to $batchDir"
  #cp batch/pshellServerDemo.batch $batchDir/.
  #echo "Copying traceFilterDemo.batch to $batchDir"
  #cp batch/traceFilterDemo.batch $batchDir/.
  echo "Copying README to $batchDir"
  cp batch/README $batchDir/.
  chmod 666 $batchDir/*
  
  echo "Installing startup files..."
  if [ ! -d $startupDir ]
  then
    echo "Creating startup directory: $startupDir"
    mkdir $startupDir
  fi
  
  #echo "Copying pshellServerDemo.startup to $startupDir"
  #cp startup/pshellServerDemo.startup $startupDir/.
  #echo "Copying traceFilterDemo.startup to $startupDir"
  #cp startup/traceFilterDemo.startup $startupDir/.
  echo "Copying README to $startupDir"
  cp startup/README $startupDir/.
  chmod 666 $startupDir/*
  
  echo "Installing manpages..."
  echo "Copying pshell.1 to $man1Dir"
  cp man/man1/pshell.1 $man1Dir/.
  gzip -f $man1Dir/pshell.1
  echo "Copying PshellControl.3 to $man3Dir"
  cp man/man3/PshellControl.3 $man3Dir/.
  gzip -f $man3Dir/PshellControl.3
  echo "Copying PshellServer.3 to $man3Dir"
  cp man/man3/PshellServer.3 $man3Dir/.
  gzip -f $man3Dir/PshellServer.3
  echo "Copying TraceFilter.3 to $man3Dir"
  cp man/man3/TraceFilter.3 $man3Dir/.
  gzip -f $man3Dir/TraceFilter.3
  echo "Copying TraceLog.3 to $man3Dir"
  cp man/man3/TraceLog.3 $man3Dir/.
  gzip -f $man3Dir/TraceLog.3
fi
