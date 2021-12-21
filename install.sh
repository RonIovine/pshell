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

if [ $OSTYPE == "cygwin" ]
then
  fileExt=".exe"
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

    # local install, setup some softlinks and create .pshellrc env file
    echo "Setting softlink libpshell-server to libpshell-server-full"
    cd $localDir/c/lib
    rm -f libpshell-server.so
    ln -s libpshell-server-full.so libpshell-server.so
    rm -f libpshell-server.a
    ln -s libpshell-server-full.a libpshell-server.a

    echo "Setting softlink PshellServer.py PshellServer-full.py"
    cd $localDir/python/src
    rm -f PshellServer.py
    ln -s PshellServer-full.py PshellServer.py

    if [ ! -e "$localDir/go/src/PshellServer" ]
    then
      mkdir $localDir/go/src/PshellServer
    fi

    echo "Setting softlink PshellServer.go PshellServer-full.go"
    cd $localDir/go/src/PshellServer
    rm -f $localDir/go/src/PshellServer/PshellServer.go
    ln -s $localDir/go/src/PshellServer-full/PshellServer-full.go $localDir/go/src/PshellServer/PshellServer.go

    echo "Setting softlink PshellServer.a PshellServer-full.a"
    cd $localDir/go/pkg/linux_amd64
    rm -f PshellServer.a
    ln -s PshellServer-full.a PshellServer.a

    echo "Setting up Busybox-like demo softlinks"
    cd $localDir/c/bin
    rm -f advancedParsing*
    ln -s pshellNoServerDemo$fileExt advancedParsing$fileExt
    rm -f enhancedUsage*
    ln -s pshellNoServerDemo$fileExt enhancedUsage$fileExt
    rm -f formatChecking*
    ln -s pshellNoServerDemo$fileExt formatChecking$fileExt
    rm -f getOptions*
    ln -s pshellNoServerDemo$fileExt getOptions$fileExt
    rm -f helloWorld*
    ln -s pshellNoServerDemo$fileExt helloWorld$fileExt
    rm -f wildcardMatch*
    ln -s pshellNoServerDemo$fileExt wildcardMatch$fileExt

    cd $localDir
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
    echo export MANPATH=\$PSHELL_INSTALL/c/man:\$MANPATH >> .pshellrc
    echo export LD_LIBRARY_PATH=\$PSHELL_INSTALL/c/lib:\$LD_LIBRARY_PATH >> .pshellrc
    echo export PYTHONPATH=\$PSHELL_INSTALL/python/src:\$PSHELL_INSTALL/python/demo:\$PYTHONPATH >> .pshellrc
    echo export GOPATH=\$PSHELL_INSTALL/go >> .pshellrc
    echo export PATH=\$PSHELL_INSTALL/c/bin:\$PSHELL_INSTALL/utils:\$PSHELL_INSTALL/python/src:\$PSHELL_INSTALL/python/demo:\$GOPATH/bin:\$PATH >> .pshellrc
    echo "Local install environment setup in '.pshellrc'"
    echo "To source enviromnent in current shell run 'source .pshellrc' at the command line"
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

  localBinDir="c/bin"
  localIncludeDir="c/include"
  localLibDir="c/lib"
  localMan1Dir="c/man/man1"
  localMan3Dir="c/man/man3"
  libDir="/usr/lib"

  echo "Installing pshell files..."
  echo "Installing libs..."

  if [ ! -d $libDir/pshell ]
  then
    echo "Creating lib directory $libDir/pshell"
    mkdir $libDir/pshell
  fi

  echo "Copying libpshell-server-full.so to $libDir/pshell"
  cp -f $localLibDir/libpshell-server-full.so $libDir/pshell/.
  echo "Copying libpshell-server-full.a to $libDir/pshell"
  cp -f $localLibDir/libpshell-server-full.a $libDir/pshell/.
  echo "Copying libpshell-server-stub.so to $libDir/pshell"
  cp -f $localLibDir/libpshell-server-stub.so $libDir/pshell/.
  echo "Copying libpshell-server-stub.a to $libDir/pshell"
  cp -f $localLibDir/libpshell-server-stub.a $libDir/pshell/.
  echo "Copying libpshell-control.so to $libDir/pshell"
  cp -f $localLibDir/libpshell-control.so $libDir/pshell/.
  echo "Copying libpshell-control.a to $libDir/pshell"
  cp -f $localLibDir/libpshell-control.a $libDir/pshell/.
  echo "Copying libpshell-readline.so to $libDir/pshell"
  cp -f $localLibDir/libpshell-readline.so $libDir/pshell/.
  echo "Copying libpshell-readline.a to $libDir/pshell"
  cp -f $localLibDir/libpshell-readline.a $libDir/pshell/.

  echo "Setting softlink $libDir/libpshell-server to $libDir/pshell/libpshell-server-full"
  rm -f $libDir/libpshell-server.so
  ln -s $libDir/pshell/libpshell-server-full.so $libDir/libpshell-server.so

  rm -f $libDir/libpshell-server.a
  ln -s $libDir/pshell/libpshell-server-full.a $libDir/libpshell-server.a

  echo "Setting softlink $libDir/libpshell-control to $libDir/pshell/libpshell-control"
  rm -f $libDir/libpshell-control.so
  ln -s $libDir/pshell/libpshell-control.so $libDir/libpshell-control.so

  rm -f $libDir/libpshell-control.a
  ln -s $libDir/pshell/libpshell-control.a $libDir/libpshell-control.a

  echo "Setting softlink $libDir/libpshell-readline to $libDir/pshell/libpshell-readline"
  rm -f $libDir/libpshell-readline.so
  ln -s $libDir/pshell/libpshell-readline.so $libDir/libpshell-readline.so

  rm -f $libDir/libpshell-readline.a
  ln -s $libDir/pshell/libpshell-readline.a $libDir/libpshell-readline.a

  echo "Installing bins..."
  echo "Copying pshell$fileExt to $binDir"
  cp -f $localBinDir/pshell $binDir/.
  chmod +x $binDir/pshell$fileExt
  echo "Copying pshellAggregator$fileExt to $binDir"
  cp -f $localBinDir/pshellAggregator $binDir/.
  chmod +x $binDir/pshellAggregator$fileExt

  echo "Installing includes..."
  echo "Copying PshellServer.h to $includeDir"
  cp -f $localIncludeDir/PshellServer.h $includeDir/.
  echo "Copying PshellControl.h to $includeDir"
  cp -f $localIncludeDir/PshellControl.h $includeDir/.
  echo "Copying PshellReadline.h to $includeDir"
  cp -f $localIncludeDir/PshellReadline.h $includeDir/.
  echo "Copying TraceFilter.h to $includeDir"
  cp -f $localIncludeDir/TraceFilter.h $includeDir/.
  echo "Copying TraceLog.h to $includeDir"
  cp -f $localIncludeDir/TraceLog.h $includeDir/.

  if [ ! -d $pshellDir ]
  then
    echo "Creating pshell directory $pshellDir"
    mkdir $pshellDir
  fi

  echo "Installing config files..."

  if [ ! -d $configDir ]
  then
    echo "Creating config directory $configDir"
    mkdir $configDir
  fi

  echo "Copying pshell-server.conf to $configDir"
  cp -f config/pshell-server.conf $configDir/.
  echo "Copying pshell-client.conf to $configDir"
  cp -f config/pshell-client.conf $configDir/.
  echo "Copying pshell-control.conf to $configDir"
  cp -f config/pshell-control.conf $configDir/.
  echo "Copying README to $configDir"
  cp -f config/README $configDir/.
  chmod 666 $configDir/*

  echo "Installing batch files..."
  if [ ! -d $batchDir ]
  then
    echo "Creating batch directory $batchDir"
    mkdir $batchDir
  fi

  echo "Copying README to $batchDir"
  cp -f batch/README $batchDir/.
  chmod 666 $batchDir/*

  echo "Installing startup files..."
  if [ ! -d $startupDir ]
  then
    echo "Creating startup directory $startupDir"
    mkdir $startupDir
  fi

  echo "Copying README to $startupDir"
  cp -f startup/README $startupDir/.
  chmod 666 $startupDir/*

  echo "Installing manpages..."
  echo "Copying pshell.1 to $man1Dir"
  cp -f $localMan1Dir/pshell.1 $man1Dir/.
  gzip -f $man1Dir/pshell.1
  echo "Copying PshellControl.3 to $man3Dir"
  cp -f $localMan3Dir/PshellControl.3 $man3Dir/.
  gzip -f $man3Dir/PshellControl.3
  echo "Copying PshellServer.3 to $man3Dir"
  cp -f $localMan3Dir/PshellServer.3 $man3Dir/.
  gzip -f $man3Dir/PshellServer.3
  echo "Copying TraceFilter.3 to $man3Dir"
  cp -f $localMan3Dir/TraceFilter.3 $man3Dir/.
  gzip -f $man3Dir/TraceFilter.3
  echo "Copying TraceLog.3 to $man3Dir"
  cp -f $localMan3Dir/TraceLog.3 $man3Dir/.
  gzip -f $man3Dir/TraceLog.3
fi
