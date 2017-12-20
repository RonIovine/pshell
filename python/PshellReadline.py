#!/usr/bin/python

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

#################################################################################
#
# This API implements a readline like functionality for user input.  This can
# work with any character stream based input/output terminal device, i.e. 
# keyboard input over a serial tty, or over a TCP/telnet connection.  This module 
# will provide up-arrow command history recall, command line editing, and TAB 
# completion of registered keywords.  
#
# A complete example of the usage of the API can be found in the included demo 
# program pshellReadlineDemo.py
#
#################################################################################

# import all our necessary modules
import sys
import os
import tty
import termios
import select

# dummy variables so we can create pseudo end block indicators, add these 
# identifiers to your list of python keywords in your editor to get syntax 
# highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

#################################################################################
#
# global "public" data, these are used for various parts of the public API
#
#################################################################################

# valid serial types, TTY is for serial terminal control and defaults to
# stdin and stdout, SOCKET uses a serial TCP socket placed in 'telnet'
# mode for control via a telnet client, default=tty with stdin/stdout
TTY = "tty"
SOCKET = "socket"

# use for the timeout value when setting the idleSessionTimeout, default=none
IDLE_TIMEOUT_NONE = 0
ONE_SECOND = 1
ONE_MINUTE = ONE_SECOND*60

# standard bash/readline tabbing method, i.e. initiate via double tabbing
BASH_TAB = "bash"
# fast tabbing, i.e.  initiated via single tabbing, this is the default
FAST_TAB = "fast"

#################################################################################
#
# "public" API functions
#
# Users of this module should only access functionality via these "public"
# methods.  This is broken up into "public" and "private" sections for 
# readability and to not expose the implementation in the API definition
#
#################################################################################

#################################################################################
#
# setFileDescriptors:
#
# Set the input and output file descriptors, if this function is not called,
# the default is stdin and stdout.  The file descriptors given to this function
# must be opened and running in raw serial character mode.  The idleTimeout 
# specifies the time to wait for any user activity in the getInput function.
# Use the identifiers PshellReadline.ONE_SECOND and PshellReadline.ONE_MINUTE
# to set this timeout value, e.g. PshellReadline.ONE_MINUTE*5, for serialType
# use the identifiers PshellReadline.TTY and PshellReadline.SOCKET.  For the
# socket identifier, use the file descriptor that is returned from the TCP 
# socket server 'accept' call for both the inFd and outFd.
#
#################################################################################
def setFileDescriptors(inFd_, outFd_, serialType_, idleTimeout_ = IDLE_TIMEOUT_NONE):
  __setFileDescriptors(inFd_, outFd_, serialType_, idleTimeout_)
enddef

#################################################################################
#
# setIdleTimeout:
#
# Set the idle session timeout as described above.  Use the identifiers
# PshellReadline.ONE_SEC and PshellRedline.ONE_MINUTE as follows,
# e.g. PshellReadline.ONE_MINUTE*5 for a 5 minute idleSession timeout.
#
#################################################################################
def setIdleTimeout(idleTimeout_):
  __setIdleTimeout(idleTimeout_)
enddef

#################################################################################
#
# write:
#
# Write a string to our output file descriptor
#
#################################################################################
def write(string_):
  __write(string_)
enddef

#################################################################################
#
# addTabCompletion:
#
# Add a keyword to the TAB completion list.  TAB completion will only be applied
# to the first keyword of a given user typed command
#
#################################################################################
def addTabCompletion(keyword_):
  __addTabCompletion(keyword_)
enddef

#################################################################################
#
# setTabStyle:
#
# Set the tabbing method to either be bash/readline style tabbing, i.e. double
# tabbing to initiate and display completions, or "fast" tabbing, where all
# completions and displays are initiated via a single tab only, the default is
# "fast" tabbing, use the identifiers PshellReadline.BASH_TAB and
# PshellReadline.FAST_TAB for the tabStyle
#
#################################################################################
def setTabStyle(tabStyle_):
  __setTabStyle(tabStyle_)
enddef

#################################################################################
#
# getInput:
#
# Issue the user prompt and return the entered command line value.  This 
# function will return the tuple (command, idleSession).  If the idle session
# timeout is set to IDLE_TIMEOUT_NONE (default), the idleSession will always 
# be false and this function will not return until the user has typed a command
# and pressed return.  Otherwise this function will set the idleSession value
# to true and return if no user activity is detected for the idleSessionTimeout 
# period
#
#################################################################################
def getInput(prompt_):
  return (__getInput(prompt_))
enddef

#################################################################################
#
# isSubString:
#
# This function will return True if string1 is a substring of string2 at 
# position 0.  If the minMatchLength is 0, then it will compare up to the
# length of string1.  If the minMatchLength > 0, it will require a minimum
# of that many characters to match.  A string that is longer than the min
# match length must still match for the remaining charactes, e.g. with a
# minMatchLength of 2, 'q' will not match 'quit', but 'qu', 'qui' or 'quit'
# will match, 'quix' or 'uit' will not match.  This function is useful for 
# wildcard matching.
#
#################################################################################
def isSubString(string1_, string2_, minMatchLength_ = 0):
  return (__isSubString(string1_, string2_, minMatchLength_))
enddef

#################################################################################
#
# "private" functions and data
#
# Users of this module should never access any of these "private" items directly,
# these are meant to hide the implementation from the presentation of the public
# API
#
#################################################################################

#################################################################################
#################################################################################
def __setFileDescriptors(inFd_, outFd_, serialType_, idleTimeout_):
  global gInFd
  global gOutFd
  global gTcpNegotiate
  global gSerialType
  gInFd = inFd_
  gOutFd = outFd_
  gSerialType = serialType_
  __setIdleTimeout(idleTimeout_)
  # if a socket serial device, setup for telnet client control
  if (gSerialType == SOCKET):
    __write(gTcpNegotiate)
    gInFd.recv(len(gTcpNegotiate))
  endif
enddef

#################################################################################
#################################################################################
def __setIdleTimeout(idleTimeout_):
  global gIdleTimeout
  gIdleTimeout = idleTimeout_
enddef

#################################################################################
#################################################################################
def __addTabCompletion(keyword_):
  global gTabCompletions
  global gMaxTabCompletionKeywordLength
  global gMaxCompletionsPerLine
  for keyword in gTabCompletions:
    if (keyword == keyword_):
      # duplicate keyword found, return
      return
    endif
  endfor
  if (len(keyword_) > gMaxTabCompletionKeywordLength):
    gMaxTabCompletionKeywordLength = len(keyword_)+3
    gMaxCompletionsPerLine = 80/gMaxTabCompletionKeywordLength
  endif
  gTabCompletions.append(keyword_.strip())
enddef

#################################################################################
#################################################################################
def __isSubString(string1_, string2_, minMatchLength_):
  if (string1_ == NULL):
    return (False)
  elif (len(string1_) > len(string2_)):
    return (False)
  elif (minMatchLength_ == 0):
    return (string1_ == string2_[:len(string1_)])
  elif (len(string1_) > minMatchLength_):
    return (string1_ == string2_[:len(string1_)])
  else:
    return (string1_[:minMatchLength_] == string2_[:minMatchLength_])
  endif
enddef

#################################################################################
#################################################################################
def __findTabCompletions(keyword_):
  global gTabCompletions
  matchList = []
  for keyword in gTabCompletions:
    if (isSubString(keyword_, keyword)):
      matchList.append(keyword)
    endif
  endfor
  return (matchList)
enddef

#################################################################################
#################################################################################
def __findLongestMatch(matchList_, command_):
  string = command_
  charPos = len(command_)
  while (True):
    if (charPos < len(matchList_[0])):
      char = matchList_[0][charPos]
      for keyword in matchList_:
         if ((charPos >= len(keyword)) or (char != keyword[charPos])):
           return (string)
         endif
      endfor
      string += char
      charPos += 1
    else:
      return (string)
    endif
  endwhile
enddef

#################################################################################
#################################################################################
def __showTabCompletions(completionList_, prompt_):
  global gMaxTabCompletionKeywordLength
  global gMaxCompletionsPerLine
  if (len(completionList_) > 0):
    __write("\n")
    numPrinted = 0
    for keyword in completionList_:
      __write("%-*s" % (gMaxTabCompletionKeywordLength, keyword))
      numPrinted += 1
      if ((numPrinted == gMaxCompletionsPerLine) and (numPrinted < len(completionList_))):
        __write("\n")
        numPrinted = 0
      endif
    endfor
    __write("\n"+prompt_)
  endif
enddef

#################################################################################
#################################################################################
def __setTabStyle(tabStyle_):
  global gTabStyle
  gTabStyle = tabStyle_
enddef

#################################################################################
#################################################################################
def __clearLine(cursorPos_, command_):
  __write("\b"*cursorPos_ + " "*len(command_) + "\b"*(len(command_)))
enddef

#################################################################################
#################################################################################
def __beginningOfLine(cursorPos_, command_):
  if (cursorPos_ > 0):
    cursorPos_ = 0
    __write("\b"*len(command_))
  endif
  return (cursorPos_)
enddef

#################################################################################
#################################################################################
def __endOfLine(cursorPos_, command_):
  if (cursorPos_ < len(command_)):
    __write(command_[cursorPos_:])
    cursorPos_ = len(command_)
  endif
  return (cursorPos_)
enddef

#################################################################################
#################################################################################
def __killLine(cursorPos_, command_):
  __clearLine(cursorPos_, command_)
  command_ = NULL
  cursorPos_ = 0
  return (cursorPos_, command_)
enddef

#################################################################################
#################################################################################
def __showCommand(command_):
  __write(command_)
  return (len(command_), command_)
enddef

#################################################################################
#################################################################################
def __getInput(prompt_):
  global gCommandHistory
  global gCommandHistoryPos
  global gTabCompletions
  global gTabStyle
  global gOutFd
  inEsc = False
  esc = NULL
  command = NULL
  cursorPos = 0
  tabCount = 0
  tabCompletions = []
  __write(prompt_)
  while (True):
    (char, idleSession) = __getChar()
    # check for idleSession timeout
    if (idleSession == True):
      return (command, True)
    endif
    if (ord(char) != 9):
      # something other than TAB typed, clear out our tabCount
      tabCount = 0
    endif
    #print ord(char)
    if (inEsc == True):
      if (esc == '['):
        if (char == 'A'):
          # up-arrow key
          if (gCommandHistoryPos > 0):
            gCommandHistoryPos -= 1
            __clearLine(cursorPos, command)
            (cursorPos, command) = __showCommand(gCommandHistory[gCommandHistoryPos])
          endif
          inEsc = False
          esc = NULL
        elif (char == 'B'):
          # down-arrow key
          if (gCommandHistoryPos < len(gCommandHistory)-1):
            gCommandHistoryPos += 1
            __clearLine(cursorPos, command)
            (cursorPos, command) = __showCommand(gCommandHistory[gCommandHistoryPos])
          else:
            # kill whole line
            gCommandHistoryPos = len(gCommandHistory)
            (cursorPos, command) = __killLine(cursorPos, command)
          endif
          inEsc = False
          esc = NULL
        elif (char == 'C'):
          # right-arrow key
          if (cursorPos < len(command)):
            __write(command[cursorPos:] + "\b"*(len(command[cursorPos:])-1))
            cursorPos += 1
          endif
          inEsc = False
          esc = NULL
        elif (char == 'D'):
          # left-arrow key
          if (cursorPos > 0):
            cursorPos -= 1
            __write("\b")
          endif
          inEsc = False
          esc = NULL
        elif (char == '1'):
          print "home2"
          cursorPos = __beginningOfLine(cursorPos, command)
        #elif (char == '3'):
        #  print "delete"
        elif (char == '~'):
          # delete key, delete under cursor
          if (cursorPos < len(command)):
            __write(command[cursorPos+1:] + " " + "\b"*(len(command[cursorPos:])))
            command = command[:cursorPos] + command[cursorPos+1:]
          endif
          inEsc = False
          esc = NULL
        elif (char == '4'):
          print "end2"
          cursorPos = __endOfLine(cursorPos, command)
        endif
      elif (esc == 'O'):
        if (char == 'H'):
          # home key, go to beginning of line
          cursorPos = __beginningOfLine(cursorPos, command)
        elif (char == 'F'):
          # end key, go to end of line
          cursorPos = __endOfLine(cursorPos, command)
        endif
        inEsc = False
        esc = NULL
      elif ((char == '[') or (char == 'O')):
        esc = char
      else:
        inEsc = False
      endif
    elif ((ord(char) >= 32) and (ord(char) < 127)):
      # printable single character, add it to our command,
      command = command[:cursorPos] + char + command[cursorPos:]
      __write(command[cursorPos:] + "\b"*(len(command[cursorPos:])-1))
      cursorPos += 1
    elif (ord(char) == 13):
      # carriage return
      __write("\n")
      if (len(command) > 0):
        # add command to our command history
        gCommandHistory.append(command)
        gCommandHistoryPos = len(gCommandHistory)
        # return command, no idleSession timeout
        return (command.strip(), False)
      else:
        __write(prompt_)
      endif
    elif (ord(char) == 11):
      # kill to eol
      __write(" "*len(command[cursorPos:]) + "\b"*(len(command[cursorPos:])))      
      command = command[:cursorPos]
    elif (ord(char) == 21):
      # kill whole line
      (cursorPos, command) = __killLine(cursorPos, command)
    elif (ord(char) == 27):
      # esc character
      inEsc = True
    elif ((ord(char) == 9) and ((len(command) == 0) or (len(command.split()) == 1))):
      # tab character, print out any completions, we only do tabbing on the first keyword
      tabCount += 1
      if (gTabStyle == FAST_TAB):
        if (tabCount == 1):
          # this tabbing method is a little different than the standard
          # readline or bash shell tabbing, we always trigger on a single
          # tab and always show all the possible completions for any
          # multiple matches
          if (len(command) == 0):
            # nothing typed, just TAB, show all registered TAB completions
            __showTabCompletions(gTabCompletions, prompt_)
          else:
            # partial word typed, double TAB, show all possible completions
            matchList = __findTabCompletions(command)
            if (len(matchList) == 1):
              # only one possible completion, show it
              __clearLine(cursorPos, command)
              (cursorPos, command) = __showCommand(matchList[0] + " ")
            elif (len(matchList) > 1):
              # multiple possible matches, fill out longest match and
              # then show all other possibilities
              __clearLine(cursorPos, command)
              (cursorPos, command) = __showCommand(__findLongestMatch(matchList, command))
              __showTabCompletions(matchList, prompt_+command)
            endif
          endif
        endif
      else:  # BASH_TAB
        # this code below implements the more standard readline/bash double tabbing method 
        if (tabCount == 2):
          if (len(command) == 0):
            # nothing typed, just a double TAB, show all registered TAB completions
            __showTabCompletions(gTabCompletions, prompt_)
          else:
            # partial word typed, double TAB, show all possible completions
            __showTabCompletions(__findTabCompletions(command), prompt_+command)
            tabCount = 0
          endif
        elif ((tabCount == 1) and (len(command) > 0)):
          # partial word typed, single TAB, fill out as much
          #  as we can and show any possible other matches
          matchList = __findTabCompletions(command)
          if (len(matchList) == 1):
            # we only have one completion, show it
            tabCount = 0
            __clearLine(cursorPos, command)
            (cursorPos, command) = __showCommand(matchList[0] + " ")
          elif (len(matchList) > 1):
            # multiple completions, find the longest match and show up to that
            __clearLine(cursorPos, command)
            (cursorPos, command) = __showCommand(__findLongestMatch(matchList, command))
          endif
        elif (len(command) > 0):
          # TAB count > 2 with command typed, reset TAB count
          tabCount = 0
        endif
      endif
    elif (ord(char) == 127):
      # backspace delete
      if ((len(command) > 0) and (cursorPos > 0)):
        __write("\b" + command[cursorPos:] + " " + "\b"*(len(command[cursorPos:])+1))
        command = command[:cursorPos-1] + command[cursorPos:]
        cursorPos -= 1
      endif
    elif (ord(char) == 1):
      # home, go to beginning of line
      cursorPos = __beginningOfLine(cursorPos, command)
    elif (ord(char) == 3):
      # ctrl-c, exit program
      print
      sys.exit(0)
    elif (ord(char) == 5):
      # end, go to end of line
      cursorPos = __endOfLine(cursorPos, command)
    elif (ord(char) != 9):
      # don't print out tab if multi keyword command
      #__write("\nchar value: %d" % ord(char))
      #__write("\n"+prompt_)
      None
    endif
  endwhile
enddef

#################################################################################
#################################################################################
def __write(string_):
  global gOutFd
  global gSerialType
  if (gSerialType == TTY):
    # serial terminal control
    gOutFd.write(string_)
    gOutFd.flush()
  else:
    # TCP socket with telnet client
    string = NULL
    for char in string_:
      # need to insert carriage return every place we find a newline
      if (char == "\n"):
        string += "\r\n"
      else:
        string += char
      endif
    endfor
    gOutFd.send(string)
  endif
enddef

#################################################################################
#################################################################################
def __getChar():
  global gInFd
  global gSerialType
  global gIdleTimeout
  char = NULL
  if (gSerialType == TTY):
    # serial terminal control
    oldSettings = termios.tcgetattr(gInFd)
    try:
      tty.setraw(gInFd)
      if (gIdleTimeout > 0):
        inputready, outputready, exceptready = select.select([gInFd], [], [], gIdleTimeout)
        if (len(inputready) > 0):
          char = gInFd.read(1)
        else:
          __write("\r\nIdle session timeout\r\n");
          return (char, True)
        endif
      else:
        char = gInFd.read(1)
      endif
    finally:
      termios.tcsetattr(gInFd, termios.TCSADRAIN, oldSettings)
  else:
    # TCP socket with telnet client
    if (gIdleTimeout > 0):
      inputready, outputready, exceptready = select.select([gInFd], [], [], gIdleTimeout)
      if (len(inputready) > 0):
        char = gInFd.recv(1)
      else:
        __write("\nIdle session timeout\n");
        return (char, True)
      endif
    else:
      char = gInFd.recv(1)
    endif
  endif
  # return char, no idle timeout
  return (char, False)
enddef

#################################################################################
#
# global "private" data
#
#################################################################################

# python does not have a native null string identifier, so create one
NULL = ""

gTcpNegotiate = 'FFFB03FFFB01FFFD03FFFD01'.decode('hex')
gIdleTimeout = IDLE_TIMEOUT_NONE
gSerialType = TTY
gInFd = sys.stdin
gOutFd = sys.stdout
gTabCompletions = []
gMaxTabCompletionKeywordLength = 0
gMaxCompletionsPerLine = 0
gCommandHistory = []
gCommandHistoryPos = 0
gTabStyle = FAST_TAB
