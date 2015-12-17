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

CC=gcc

FILE_EXT=cc

INCLUDE = -I$(abspath include)

WARNINGS = -Wall

STATIC_OBJ = $(WARNINGS) -c
STATIC_LIB = ar rcs
SHARED_OBJ = $(WARNINGS) -fPIC -c
SHARED_LIB = $(CC) -shared -o

SRC_DIR = src
LIB_DIR = lib
BIN_DIR = bin
UTILS_DIR = utils
DEMO_DIR = demo

PSHELL_FLAGS =
TF_FLAGS = 
TRACE_FLAGS = 
CLIENT_FLAGS =
TRACE_FILTER_DEMO_FLAGS =
TRACE_LOG_DEMO_FLAGS =

PSHELL_SERVER_DEMO_LIBS = -L$(LIB_DIR) -lpshell-server -lpthread 
PSHELL_CONTROL_DEMO_LIBS = -L$(LIB_DIR) -lpshell-control -lpthread 
TRACE_FILTER_DEMO_LIBS = -L$(LIB_DIR) -lpshell-server -lpthread 

VERBOSE = @
LOCAL =
SHELL_ENV_FILE =

#
# create a 'newline' for our error message
#
define nl


endef

#
# if our build-time configuration file, .config, does not exist, copy
# the default deconfig file to .config and proceed with the build
#
TARGET = $(MAKECMDGOALS)
ifneq ($(TARGET), )
  ifeq (,$(wildcard .config))
    $(info Build-time configuration file: '.config' not found)
    $(info Copying 'defconfig' to '.config' for build)
    ifeq (,$(wildcard defconfig))
      $(error ERROR: Could not find file: 'defconfig')
    else
      $(shell cp defconfig .config)
    endif
  endif
endif

#
# pull in our build-time configuration file
#
-include .config

#
# PshellServer objects
#
PSHELL_OBJS = PshellServer.o

#
# PshellClient libraries
#
CLIENT_LIBS = -lnsl

#########################
#
# PshellServer flags
#
#########################

ifdef PSHELL_DEFAULT_TITLE
  PSHELL_FLAGS += -DPSHELL_DEFAULT_TITLE=$(PSHELL_DEFAULT_TITLE)
endif

ifdef PSHELL_DEFAULT_BANNER
  PSHELL_FLAGS += -DPSHELL_DEFAULT_BANNER=$(PSHELL_DEFAULT_BANNER)
endif

ifdef PSHELL_DEFAULT_PROMPT
  PSHELL_FLAGS += -DPSHELL_DEFAULT_PROMPT=$(PSHELL_DEFAULT_PROMPT)
endif

ifdef PSHELL_DEFAULT_TIMEOUT
  PSHELL_FLAGS += -DPSHELL_DEFAULT_TIMEOUT=$(PSHELL_DEFAULT_TIMEOUT)
endif

ifdef PSHELL_INCLUDE_COMMAND_IN_ARGS_LIST
  PSHELL_FLAGS += -DPSHELL_INCLUDE_COMMAND_IN_ARGS_LIST
endif

ifdef PSHELL_CONFIG_DIR
  PSHELL_FLAGS += -DPSHELL_CONFIG_DIR=$(PSHELL_CONFIG_DIR)
endif

ifdef PSHELL_STARTUP_DIR
  PSHELL_FLAGS += -DPSHELL_STARTUP_DIR=$(PSHELL_STARTUP_DIR)
endif

ifdef PSHELL_BATCH_DIR
  PSHELL_FLAGS += -DPSHELL_BATCH_DIR=$(PSHELL_BATCH_DIR)
endif

ifdef PSHELL_READLINE
  PSHELL_FLAGS += -DPSHELL_READLINE
  CLIENT_FLAGS += -DPSHELL_READLINE
  CLIENT_LIBS += -lreadline
  PSHELL_SERVER_DEMO_LIBS += -lreadline
  TRACE_FILTER_DEMO_LIBS += -lreadline
endif

ifdef PSHELL_VSNPRINTF
  PSHELL_FLAGS += -DPSHELL_VSNPRINTF
endif

ifdef PSHELL_PAYLOAD_CHUNK
  PSHELL_FLAGS += -DPSHELL_PAYLOAD_CHUNK=$(PSHELL_PAYLOAD_CHUNK)
endif

ifdef PSHELL_PAYLOAD_GUARDBAND
  PSHELL_FLAGS += -DPSHELL_PAYLOAD_GUARDBAND=$(PSHELL_PAYLOAD_GUARDBAND)
endif

ifdef PSHELL_GROW_PAYLOAD_IN_CHUNKS
  PSHELL_FLAGS += -DPSHELL_GROW_PAYLOAD_IN_CHUNKS
endif

ifdef PSHELL_TOKEN_LIST_CHUNK
  PSHELL_FLAGS += -DPSHELL_TOKEN_LIST_CHUNK=$(PSHELL_TOKEN_LIST_CHUNK)
endif

ifdef PSHELL_TOKEN_CHUNK
  PSHELL_FLAGS += -DPSHELL_TOKEN_CHUNK=$(PSHELL_TOKEN_CHUNK)
endif

ifdef PSHELL_COMMAND_CHUNK
  PSHELL_FLAGS += -DPSHELL_COMMAND_CHUNK=$(PSHELL_COMMAND_CHUNK)
endif

ifdef PSHELL_COPY_ADD_COMMAND_STRINGS
  PSHELL_FLAGS += -DPSHELL_COPY_ADD_COMMAND_STRINGS
endif


ifdef PSHELL_INCLUDE_TRACE_FILTER
  PSHELL_OBJS += TraceFilter.o
  # the integrated trace module is dependent on the trace filter module,
  # so only allow it to be included if the trace filter is also included
  ifdef TF_INTEGRATED_TRACE_LOG
    TF_FLAGS += -DTF_INTEGRATED_TRACE_LOG
    TRACE_FLAGS += -DDYNAMIC_TRACE_FILTER
    TRACE_FILTER_DEMO_FLAGS += -DDYNAMIC_TRACE_FILTER
    PSHELL_OBJS += TraceLog.o
  endif
else
  # make sure we don't include the integrated trace module if we don't
  # have the trace filter module
  TF_INTEGRATED_TRACE_LOG=n
endif

#########################
#
# TraceFilter flags
#
#########################

ifdef TF_FAST_FILENAME_LOOKUP
  TF_FLAGS += -DTF_FAST_FILENAME_LOOKUP
  TRACE_FILTER_DEMO_FLAGS += -DTF_FAST_FILENAME_LOOKUP
endif

ifdef TF_MAX_TRACE_SYMBOLS
  TF_FLAGS += -DTF_MAX_TRACE_SYMBOLS=$(TF_MAX_TRACE_SYMBOLS)
endif

ifdef TF_MAX_LINE_FILTERS
  TF_FLAGS += -DTF_MAX_LINE_FILTERS=$(TF_MAX_LINE_FILTERS)
endif

ifdef TF_MAX_FILE_FILTERS
  TF_FLAGS += -DTF_MAX_FILE_FILTERS=$(TF_MAX_FILE_FILTERS)
endif

ifdef TF_MAX_FUNCTION_FILTERS
  TF_FLAGS += -DTF_MAX_FUNCTION_FILTERS=$(TF_MAX_FUNCTION_FILTERS)
endif

ifdef TF_MAX_THREAD_FILTERS
  TF_FLAGS += -DTF_MAX_THREAD_FILTERS=$(TF_MAX_THREAD_FILTERS)
endif

ifdef TF_USE_COLORS
  TF_FLAGS += -DTF_USE_COLORS
endif

#########################
#
# TraceLog flags
#
#########################

ifdef TRACE_LOG_DISABLED
  TRACE_FILTER_DEMO_FLAGS += -DTRACE_LOG_DISABLED
  TRACE_LOG_DEMO_FLAGS += -DTRACE_LOG_DISABLED
endif

ifeq ($(verbose), y)
  VERBOSE =
endif

#
# make sure only 'root' can run 'make install'
#
USER = $(shell whoami)
ifeq ($(TARGET), install)
  ifeq ($(local), y)
    LOCAL = -local
    ifneq ($(shellEnvFile), )
      SHELL_ENV_FILE = $(shellEnvFile)
    endif
  else ifneq ($(USER), root)
    $(error $(nl)ERROR: User '$(USER)' cannot run 'make install', $(nl)       must be 'root' to do a system install of this package, $(nl)       either run as 'root' or do 'make install local=y')
  endif
endif

.PHONY: demo lib

###############
#
# targets
#
###############

help:
	@echo
	@echo "Usage: make {all | pshell | lib | demo | install | clean} [verbose=y] [local=y [shellEnvFile=<file>]]"
	@echo
	@echo "  where:"
	@echo "    all          - build all components of the pshell package"
	@echo "    pshell       - build the pshell UDP/UNIX stand-alone client program only"
	@echo "    lib          - build the pshell link libraries only (shared, static and stub)"
	@echo "    demo         - build the pshell stand-alone demo programs only"
	@echo "    install      - build and install all pshell components"
	@echo "    clean        - clean all binaries (libs & executables)"
	@echo "    verbose      - print verbose messages from build process"
	@echo "    local        - specify local install (install target only)"
	@echo "    shellEnvFile - shell env file (i.e. .bashrc) to modify for local install"
	@echo
	@echo "  NOTE: The install target option will either install this package on a"
	@echo "        system wide basis or will setup a local install environment.  A system"
	@echo "        install must be done as 'root' and will copy all libraries, binaries,"
	@echo "        include files, conf files, and manpages to their customary system"
	@echo "        locations.  A local install will not copy/move any files.  It will"
	@echo "        only create a pshell env file (.pshellrc) that can be sourced in the"
	@echo "        current shell or can be added to your shell env file (i.e. .bashrc)"
	@echo "        that will allow use of the package features from a local directory."
	@echo
	@echo "        The location of the local install environment will be the directory"
	@echo "        where this script resides."
	@echo

lib:
	@echo "Building libpshell-server-full.a..."
	$(VERBOSE)$(CC) $(INCLUDE) $(PSHELL_FLAGS) $(STATIC_OBJ) $(SRC_DIR)/PshellServer.$(FILE_EXT) -o PshellServer.o
	$(VERBOSE)$(CC) $(INCLUDE) $(TF_FLAGS) $(STATIC_OBJ) $(SRC_DIR)/TraceFilter.$(FILE_EXT) -o TraceFilter.o
	$(VERBOSE)$(CC) $(INCLUDE) $(TRACE_FLAGS) $(STATIC_OBJ) $(SRC_DIR)/TraceLog.$(FILE_EXT) -o TraceLog.o
	$(VERBOSE)$(STATIC_LIB) $(LIB_DIR)/libpshell-server-full.a $(PSHELL_OBJS)
	$(VERBOSE)rm *.o
	
	@echo "Building libpshell-server-full.so..."
	$(VERBOSE)$(CC) $(INCLUDE) $(PSHELL_FLAGS) $(SHARED_OBJ) $(SRC_DIR)/PshellServer.$(FILE_EXT) -o PshellServer.o
	$(VERBOSE)$(CC) $(INCLUDE) $(TF_FLAGS) $(SHARED_OBJ) $(SRC_DIR)/TraceFilter.$(FILE_EXT) -o TraceFilter.o
	$(VERBOSE)$(CC) $(INCLUDE) $(TRACE_FLAGS) $(SHARED_OBJ) $(SRC_DIR)/TraceLog.$(FILE_EXT) -o TraceLog.o
	$(VERBOSE)$(SHARED_LIB) $(LIB_DIR)/libpshell-server-full.so $(PSHELL_OBJS)
	$(VERBOSE)rm *.o
	
	@echo "Building libpshell-server-stub.a..."
	$(VERBOSE)$(CC) $(INCLUDE) $(PSHELL_FLAGS) $(TF_FLAGS) $(STATIC_OBJ) $(SRC_DIR)/PshellStub.$(FILE_EXT) -o PshellStub.o
	$(VERBOSE)$(STATIC_LIB) $(LIB_DIR)/libpshell-server-stub.a PshellStub.o
	$(VERBOSE)rm *.o
	
	@echo "Building libpshell-server-stub.so..."
	$(VERBOSE)$(CC) $(INCLUDE) $(PSHELL_FLAGS) $(TF_FLAGS) $(SHARED_OBJ) $(SRC_DIR)/PshellStub.$(FILE_EXT) -o PshellStub.o
	$(VERBOSE)$(SHARED_LIB) $(LIB_DIR)/libpshell-server-stub.so PshellStub.o
	$(VERBOSE)rm *.o
	
	@echo "Building libpshell-control.a..."
	$(VERBOSE)$(CC) $(INCLUDE) $(PSHELL_FLAGS) $(STATIC_OBJ) $(SRC_DIR)/PshellControl.$(FILE_EXT) -o PshellControl.o
	$(VERBOSE)$(STATIC_LIB) $(LIB_DIR)/libpshell-control.a PshellControl.o
	$(VERBOSE)rm *.o
	
	@echo "Building libpshell-control.so..."
	$(VERBOSE)$(CC) $(INCLUDE) $(PSHELL_FLAGS) $(SHARED_OBJ) $(SRC_DIR)/PshellControl.$(FILE_EXT) -o PshellControl.o
	$(VERBOSE)$(SHARED_LIB) $(LIB_DIR)/libpshell-control.so PshellControl.o
	$(VERBOSE)rm *.o

	@echo "Setting libpshell-server to libpshell-server-full..."
	$(VERBOSE)$(UTILS_DIR)/setPshellLib -full

pshell:
	@echo "Building pshell stand-alone UDP/UNIX client..."
	$(VERBOSE)$(CC) $(INCLUDE) $(WARNINGS) $(CLIENT_FLAGS) $(SRC_DIR)/PshellClient.$(FILE_EXT) $(CLIENT_LIBS) -o $(BIN_DIR)/pshell

demo:
	@echo "Building pshellServerDemo program..."
	$(VERBOSE)$(CC) $(INCLUDE) $(WARNINGS) $(DEMO_DIR)/pshellServerDemo.$(FILE_EXT) $(PSHELL_SERVER_DEMO_LIBS) -o $(BIN_DIR)/pshellServerDemo
	
	@echo "Building pshellControlDemo program..."
	$(VERBOSE)$(CC) $(INCLUDE) $(WARNINGS) $(DEMO_DIR)/pshellControlDemo.$(FILE_EXT) $(PSHELL_CONTROL_DEMO_LIBS) -o $(BIN_DIR)/pshellControlDemo
	
ifeq ($(TF_INTEGRATED_TRACE_LOG),y)
	@echo "Building traceFilterDemo program..."
	$(VERBOSE)$(CC) $(INCLUDE) $(WARNINGS) $(TRACE_FILTER_DEMO_FLAGS) $(DEMO_DIR)/traceFilterDemo.$(FILE_EXT) $(TRACE_FILTER_DEMO_LIBS) -o $(BIN_DIR)/traceFilterDemo
endif

	@echo "Building traceLogDemo program..."
	$(VERBOSE)$(CC) $(INCLUDE) $(WARNINGS) $(TRACE_LOG_DEMO_FLAGS) $(SRC_DIR)/TraceLog.$(FILE_EXT) $(DEMO_DIR)/traceLogDemo.$(FILE_EXT) -o $(BIN_DIR)/traceLogDemo

all: lib pshell demo
	@echo "PSHELL package successfully built..."

install: all
	$(VERBOSE)./install.sh $(LOCAL) $(SHELL_ENV_FILE)

clean:	
	@echo "Cleaning directory bin..."
	$(VERBOSE)rm $(BIN_DIR)/*
	@echo "Cleaning directory lib..."
	$(VERBOSE)rm $(LIB_DIR)/*.so
	$(VERBOSE)rm $(LIB_DIR)/*.a
