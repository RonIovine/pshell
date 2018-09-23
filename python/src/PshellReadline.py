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

"""
A Python module to provide readline like user input functionality

This API implements a readline like functionality for user input.  This can
work with any character stream based input/output terminal device, i.e. 
keyboard input over a serial tty, or over a TCP/telnet connection.  This module 
will provide up-arrow command history recall, command line editing, and TAB 
completion of registered keywords.  

Functions:

setFileDescriptors() -- set the input/output file descriptors
setIdleTimeout()     -- set the idle session timeout
addTabCompletion()   -- add a keyword to the TAB completion list
setTabStyle()        -- sets the tab behavour style ("fast" or bash)
getInput()           -- get a line of user input from our input file descriptor
isSubString()        -- checks for string1 substring of string2 at position 0
write()              -- write a string to our output file descriptor

Integer constants:

Use for the timeout value when setting the idleSessionTimeout, default=none

IDLE_TIMEOUT_NONE
ONE_SECOND
ONE_MINUTE

String constants:

Valid serial types, TTY is for serial terminal control and defaults to
stdin and stdout, SOCKET uses a serial TCP socket placed in 'telnet'
mode for control via a telnet client, default=tty with stdin/stdout

TTY
SOCKET

Valid TAB style identifiers

Standard bash/readline tabbing method, i.e. initiate via double tabbing

BASH_TAB

Fast tabbing, i.e. initiated via single tabbing, this is the default

FAST_TAB

A complete example of the usage of the API can be found in the included demo 
program pshellReadlineDemo.py
"""

# import all our necessary modules
import sys
import os
import tty
import termios
import select
import signal

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

# fast tabbing, i.e. initiated via single tabbing, this is the default
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
#################################################################################
def setFileDescriptors(inFd, outFd, serialType, idleTimeout = IDLE_TIMEOUT_NONE):
  """
  Set the input and output file descriptors, if this function is not called,
  the default is stdin and stdout.  The file descriptors given to this function
  must be opened and running in raw serial character mode.  The idleTimeout 
  specifies the time to wait for any user activity in the getInput function.
  Use the identifiers PshellReadline.ONE_SECOND and PshellReadline.ONE_MINUTE
  to set this timeout value, e.g. PshellReadline.ONE_MINUTE*5, for serialType
  use the identifiers PshellReadline.TTY and PshellReadline.SOCKET.  For the
  socket identifier, use the file descriptor that is returned from the TCP 
  socket server 'accept' call for both the inFd and outFd.

    Args:
        inFd (int)        : The input file descriptor
        outFd (int)       : The output file descriptor
        serialType (str)  : The serial type (TTY or SOCKET)
        idleTimeout (int) : The idle session timeout

    Returns:
        none
  """
  _setFileDescriptors(inFd, outFd, serialType, idleTimeout)

#################################################################################
#################################################################################
def setIdleTimeout(idleTimeout):
  """
  Set the idle session timeout as described above.  Use the identifiers
  PshellReadline.ONE_SEC and PshellRedline.ONE_MINUTE as follows,
  e.g. PshellReadline.ONE_MINUTE*5 for a 5 minute idleSession timeout.

    Args:
        idleTimeout (int) : The idle session timeout

    Returns:
        none
  """
  _setIdleTimeout(idleTimeout)

#################################################################################
#################################################################################
def write(string):
  """
  Write a string to our output file descriptor

    Args:
        string (str) : The string to write to the out file descriptor

    Returns:
        none
  """
  _write(string)

#################################################################################
#################################################################################
def addTabCompletion(keyword):
  """
  Add a keyword to the TAB completion list.  TAB completion will only be applied
  to the first keyword of a given user typed command

    Args:
        keyword (str) : The keyword to add to the TAB completion list

    Returns:
        none
  """
  _addTabCompletion(keyword)

#################################################################################
#################################################################################
def setTabStyle(tabStyle):
  """
  Set the tabbing method to either be bash/readline style tabbing, i.e. double
  tabbing to initiate and display completions, or "fast" tabbing, where all
  completions and displays are initiated via a single tab only, the default is
  "fast" tabbing, use the identifiers PshellReadline.BASH_TAB and
  PshellReadline.FAST_TAB for the tabStyle

    Args:
        tabStyle (str) : The desired TAB  behavour style (FAST or BASH)

    Returns:
        none
  """
  _setTabStyle(tabStyle)

#################################################################################
#################################################################################
def getInput(prompt):
  """
  Issue the user prompt and return the entered command line value.  This 
  function will return the tuple (command, idleSession).  If the idle session
  timeout is set to IDLE_TIMEOUT_NONE (default), the idleSession will always 
  be false and this function will not return until the user has typed a command
  and pressed return.  Otherwise this function will set the idleSession value
  to true and return if no user activity is detected for the idleSessionTimeout 
  period

    Args:
        prompt (str) : The user prompt to display

    Returns:
        str  : The user typed command
        bool : True for idleSession timeout, False otherwise
  """
  return (_getInput(prompt))

#################################################################################
#################################################################################
def isSubString(string1, string2, minMatchLength = 0):
  """
  This function will return True if string1 is a substring of string2 at 
  position 0.  If the minMatchLength is 0, then it will compare up to the
  length of string1.  If the minMatchLength > 0, it will require a minimum
  of that many characters to match.  A string that is longer than the min
  match length must still match for the remaining characters, e.g. with a
  minMatchLength of 2, 'q' will not match 'quit', but 'qu', 'qui' or 'quit'
  will match, 'quix' or 'uit' will not match.  This function is useful for 
  wildcard matching.

    Args:
        string1 (str)        : The substring
        string2 (str)        : The target string
        minMatchLength (int) : The minimum required characters match

    Returns:
        bool : True if substring matches, False otherwise
  """
  return (_isSubString(string1, string2, minMatchLength))

#################################################################################
#
# "private" functions and data
#
# Users of this module should never access any of these "private" items directly,
# these are meant to hide the implementation from the presentation of the public
# API above
#
#################################################################################

#################################################################################
#################################################################################
def _setFileDescriptors(inFd_, outFd_, serialType_, idleTimeout_):
  global _gInFd
  global _gOutFd
  global _gTcpNegotiate
  global _gSerialType
  global _gCommandHistory
  _gInFd = inFd_
  _gOutFd = outFd_
  _gSerialType = serialType_
  _setIdleTimeout(idleTimeout_)
  _gCommandHistory = []
  # if a socket serial device, setup for telnet client control
  if (_gSerialType == SOCKET):
    _write(_gTcpNegotiate)
    _gInFd.recv(len(_gTcpNegotiate))

#################################################################################
#################################################################################
def _setIdleTimeout(idleTimeout_):
  global _gIdleTimeout
  _gIdleTimeout = idleTimeout_

#################################################################################
#################################################################################
def _addTabCompletion(keyword_):
  global _gTabCompletions
  global _gMaxTabCompletionKeywordLength
  global _gMaxCompletionsPerLine
  for keyword in _gTabCompletions:
    if (keyword == keyword_):
      # duplicate keyword found, return
      return
  if (len(keyword_) > _gMaxTabCompletionKeywordLength):
    _gMaxTabCompletionKeywordLength = len(keyword_)+5
    _gMaxCompletionsPerLine = 80/_gMaxTabCompletionKeywordLength
  _gTabCompletions.append(keyword_.strip())

#################################################################################
#################################################################################
def _isSubString(string1_, string2_, minMatchLength_):
  if (string1_ == ""):
    return (False)
  elif (len(string1_) > len(string2_)):
    return (False)
  elif (minMatchLength_ == 0):
    return (string1_ == string2_[:len(string1_)])
  elif (len(string1_) > minMatchLength_):
    return (string1_ == string2_[:len(string1_)])
  else:
    return (string1_[:minMatchLength_] == string2_[:minMatchLength_])

#################################################################################
#################################################################################
def _findTabCompletions(keyword_):
  global _gTabCompletions
  matchList = []
  for keyword in _gTabCompletions:
    if (isSubString(keyword_, keyword)):
      matchList.append(keyword)
  return (matchList)

#################################################################################
#################################################################################
def _findLongestMatch(matchList_, command_):
  string = command_
  charPos = len(command_)
  while (True):
    if (charPos < len(matchList_[0])):
      char = matchList_[0][charPos]
      for keyword in matchList_:
         if ((charPos >= len(keyword)) or (char != keyword[charPos])):
           return (string)
      string += char
      charPos += 1
    else:
      return (string)

#################################################################################
#################################################################################
def _showTabCompletions(completionList_, prompt_):
  global _gMaxTabCompletionKeywordLength
  global _gMaxCompletionsPerLine
  if (len(completionList_) > 0):
    _write("\n")
    totPrinted = 0
    numPrinted = 0
    for keyword in completionList_:
      _write("%-*s" % (_gMaxTabCompletionKeywordLength, keyword))
      numPrinted += 1
      totPrinted += 1
      if ((numPrinted == _gMaxCompletionsPerLine) and (totPrinted < len(completionList_))):
        _write("\n")
        numPrinted = 0
    _write("\n"+prompt_)

#################################################################################
#################################################################################
def _setTabStyle(tabStyle_):
  global _gTabStyle
  _gTabStyle = tabStyle_

#################################################################################
#################################################################################
def _clearLine(cursorPos_, command_):
  _write("\b"*cursorPos_ + " "*len(command_) + "\b"*(len(command_)))

#################################################################################
#################################################################################
def _beginningOfLine(cursorPos_, command_):
  if (cursorPos_ > 0):
    cursorPos_ = 0
    _write("\b"*len(command_))
  return (cursorPos_)

#################################################################################
#################################################################################
def _endOfLine(cursorPos_, command_):
  if (cursorPos_ < len(command_)):
    _write(command_[cursorPos_:])
    cursorPos_ = len(command_)
  return (cursorPos_)

#################################################################################
#################################################################################
def _killLine(cursorPos_, command_):
  _clearLine(cursorPos_, command_)
  command_ = ""
  cursorPos_ = 0
  return (cursorPos_, command_)

#################################################################################
#################################################################################
def _showCommand(command_):
  _write(command_)
  return (len(command_), command_)

#################################################################################
#################################################################################
def _getInput(prompt_):
  global _gCommandHistory
  global _gCommandHistoryPos
  global _gTabCompletions
  global _gTabStyle
  global _gOutFd
  inEsc = False
  esc = ""
  command = ""
  cursorPos = 0
  tabCount = 0
  _write(prompt_)
  while (True):
    (char, idleSession) = _getChar()
    # check for idleSession timeout
    if (idleSession == True):
      return (command, True)
    if (ord(char) != 9):
      # something other than TAB typed, clear out our tabCount
      tabCount = 0
    #print(ord(char))
    if (inEsc == True):
      if (esc == '['):
        if (char == 'A'):
          # up-arrow key
          if (_gCommandHistoryPos > 0):
            _gCommandHistoryPos -= 1
            _clearLine(cursorPos, command)
            (cursorPos, command) = _showCommand(_gCommandHistory[_gCommandHistoryPos])
          inEsc = False
          esc = ""
        elif (char == 'B'):
          # down-arrow key
          if (_gCommandHistoryPos < len(_gCommandHistory)-1):
            _gCommandHistoryPos += 1
            _clearLine(cursorPos, command)
            (cursorPos, command) = _showCommand(_gCommandHistory[_gCommandHistoryPos])
          else:
            # kill whole line
            _gCommandHistoryPos = len(_gCommandHistory)
            (cursorPos, command) = _killLine(cursorPos, command)
          inEsc = False
          esc = ""
        elif (char == 'C'):
          # right-arrow key
          if (cursorPos < len(command)):
            _write(command[cursorPos:] + "\b"*(len(command[cursorPos:])-1))
            cursorPos += 1
          inEsc = False
          esc = ""
        elif (char == 'D'):
          # left-arrow key
          if (cursorPos > 0):
            cursorPos -= 1
            _write("\b")
          inEsc = False
          esc = ""
        elif (char == '1'):
          print("home2")
          cursorPos = _beginningOfLine(cursorPos, command)
        #elif (char == '3'):
        #  print("delete")
        elif (char == '~'):
          # delete key, delete under cursor
          if (cursorPos < len(command)):
            _write(command[cursorPos+1:] + " " + "\b"*(len(command[cursorPos:])))
            command = command[:cursorPos] + command[cursorPos+1:]
          inEsc = False
          esc = ""
        elif (char == '4'):
          print("end2")
          cursorPos = _endOfLine(cursorPos, command)
      elif (esc == 'O'):
        if (char == 'H'):
          # home key, go to beginning of line
          cursorPos = _beginningOfLine(cursorPos, command)
        elif (char == 'F'):
          # end key, go to end of line
          cursorPos = _endOfLine(cursorPos, command)
        inEsc = False
        esc = ""
      elif ((char == '[') or (char == 'O')):
        esc = char
      else:
        inEsc = False
    elif ((ord(char) >= 32) and (ord(char) < 127)):
      # printable single character, add it to our command,
      command = command[:cursorPos] + char + command[cursorPos:]
      _write(command[cursorPos:] + "\b"*(len(command[cursorPos:])-1))
      cursorPos += 1
    elif (ord(char) == 13):
      # carriage return
      _write("\n")
      if (len(command) > 0):
        # add command to our command history
        if (len(_gCommandHistory) == 0 or (_gCommandHistory[-1] != command)):
          _gCommandHistory.append(command)
          _gCommandHistoryPos = len(_gCommandHistory)
        # return command, no idleSession timeout
        return (command.strip(), False)
      else:
        _write(prompt_)
    elif (ord(char) == 11):
      # kill to eol
      _write(" "*len(command[cursorPos:]) + "\b"*(len(command[cursorPos:])))      
      command = command[:cursorPos]
    elif (ord(char) == 21):
      # kill whole line
      (cursorPos, command) = _killLine(cursorPos, command)
    elif (ord(char) == 27):
      # esc character
      inEsc = True
    elif ((ord(char) == 9) and ((len(command) == 0) or (len(command.split()) == 1))):
      # tab character, print out any completions, we only do tabbing on the first keyword
      tabCount += 1
      if (_gTabStyle == FAST_TAB):
        if (tabCount == 1):
          # this tabbing method is a little different than the standard
          # readline or bash shell tabbing, we always trigger on a single
          # tab and always show all the possible completions for any
          # multiple matches
          if (len(command) == 0):
            # nothing typed, just TAB, show all registered TAB completions
            _showTabCompletions(_gTabCompletions, prompt_)
          else:
            # partial word typed, show all possible completions
            matchList = _findTabCompletions(command)
            if (len(matchList) == 1):
              # only one possible completion, show it
              _clearLine(cursorPos, command)
              (cursorPos, command) = _showCommand(matchList[0] + " ")
            elif (len(matchList) > 1):
              # multiple possible matches, fill out longest match and
              # then show all other possibilities
              _clearLine(cursorPos, command)
              (cursorPos, command) = _showCommand(_findLongestMatch(matchList, command))
              _showTabCompletions(matchList, prompt_+command)
      else:  # BASH_TAB
        # this code below implements the more standard readline/bash double tabbing method 
        if (tabCount == 2):
          if (len(command) == 0):
            # nothing typed, just a double TAB, show all registered TAB completions
            _showTabCompletions(_gTabCompletions, prompt_)
          else:
            # partial word typed, double TAB, show all possible completions
            _showTabCompletions(_findTabCompletions(command), prompt_+command)
        elif ((tabCount == 1) and (len(command) > 0)):
          # partial word typed, single TAB, fill out as much
          #  as we can and show any possible other matches
          matchList = _findTabCompletions(command)
          if (len(matchList) == 1):
            # we only have one completion, show it
            _clearLine(cursorPos, command)
            (cursorPos, command) = _showCommand(matchList[0] + " ")
          elif (len(matchList) > 1):
            # multiple completions, find the longest match and show up to that
            _clearLine(cursorPos, command)
            (cursorPos, command) = _showCommand(_findLongestMatch(matchList, command))
    elif (ord(char) == 127):
      # backspace delete
      if ((len(command) > 0) and (cursorPos > 0)):
        _write("\b" + command[cursorPos:] + " " + "\b"*(len(command[cursorPos:])+1))
        command = command[:cursorPos-1] + command[cursorPos:]
        cursorPos -= 1
    elif (ord(char) == 1):
      # home, go to beginning of line
      cursorPos = _beginningOfLine(cursorPos, command)
    elif ((ord(char) == 3) and (_gSerialType == TTY)):
      # ctrl-c, raise signal SIGINT to our own process
      os.kill(os.getpid(), signal.SIGINT)
    elif (ord(char) == 5):
      # end, go to end of line
      cursorPos = _endOfLine(cursorPos, command)
    elif (ord(char) != 9):
      # don't print out tab if multi keyword command
      #_write("\nchar value: %d" % ord(char))
      #_write("\n"+prompt_)
      None

#################################################################################
#################################################################################
def _write(string_):
  global _gOutFd
  global _gSerialType
  string_ = string_.replace("\n", "\r\n")
  if (_gSerialType == TTY):
    # serial terminal control
    _gOutFd.write(string_)
    _gOutFd.flush()
  else:
    # TCP socket with telnet client
    _gOutFd.send(string_)

#################################################################################
#################################################################################
def _getChar():
  global _gInFd
  global _gSerialType
  global _gIdleTimeout
  char = ""
  if (_gSerialType == TTY):
    # serial terminal control
    oldSettings = termios.tcgetattr(_gInFd)
    try:
      tty.setraw(_gInFd, termios.TCSADRAIN)
      if (_gIdleTimeout > 0):
        inputready, outputready, exceptready = select.select([_gInFd], [], [], _gIdleTimeout)
        if (len(inputready) > 0):
          char = _gInFd.read(1)
        else:
          _write("\r\nIdle session timeout\r\n");
          return (char, True)
      else:
        char = _gInFd.read(1)
    finally:
      termios.tcsetattr(_gInFd, termios.TCSADRAIN, oldSettings)
  else:
    # TCP socket with telnet client
    if (_gIdleTimeout > 0):
      inputready, outputready, exceptready = select.select([_gInFd], [], [], _gIdleTimeout)
      if (len(inputready) > 0):
        char = _gInFd.recv(1)
      else:
        _write("\nIdle session timeout\n");
        return (char, True)
    else:
      char = _gInFd.recv(1)
  # return char, no idle timeout
  return (char, False)

#################################################################################
#
# global "private" data
#
#################################################################################

_gTcpNegotiate = 'FFFB03FFFB01FFFD03FFFD01'.decode('hex')
_gIdleTimeout = IDLE_TIMEOUT_NONE
_gSerialType = TTY
_gInFd = sys.stdin
_gOutFd = sys.stdout
_gTabCompletions = []
_gMaxTabCompletionKeywordLength = 0
_gMaxCompletionsPerLine = 0
_gCommandHistory = []
_gCommandHistoryPos = 0
_gTabStyle = FAST_TAB
