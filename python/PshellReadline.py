#!/usr/bin/python

# import all our necessary modules
import sys
import os
import tty
import termios

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

gTabCompletions = []
gMaxTabCompletionKeywordLength = 0
gMaxCompletionsPerLine = 0
gCommandHistory = []
gCommandHistoryPos = 0

#################################################################################
#################################################################################
def addTabCompletion(keyword_):
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
    gMaxTabCompletionKeywordLength = len(keyword_)+2
    gMaxCompletionsPerLine = 80/gMaxTabCompletionKeywordLength
  endif
  gTabCompletions.append(keyword_.strip())
enddef

#################################################################################
#################################################################################
def getInput(prompt_):
  global gCommandHistory
  global gCommandHistoryPos
  global gTabCompletions
  global gMaxTabCompletionKeywordLength
  global gMaxCompletionsPerLine
  sys.stdout.write(prompt_)
  inEsc = False
  esc = NULL
  command = NULL
  cursorPos = 0
  tabCount = 0
  while (True):
    char = __getChar()
    if (ord(char) != 9):
      tabCont = 0
    endif
    #print ord(char)
    if (inEsc == True):
      if (esc == '['):
        if (char == 'A'):
          # up-arrow
          if (gCommandHistoryPos > 0):
            gCommandHistoryPos -= 1
            sys.stdout.write("\b"*cursorPos + " "*len(command) + "\b"*(len(command)))
            command = gCommandHistory[gCommandHistoryPos]
            sys.stdout.write(command)
            cursorPos = len(command)
          endif
          inEsc = False
          esc = NULL
        elif (char == 'B'):
          if (gCommandHistoryPos < len(gCommandHistory)-1):
            gCommandHistoryPos += 1
            sys.stdout.write("\b"*cursorPos + " "*len(command) + "\b"*(len(command)))
            command = gCommandHistory[gCommandHistoryPos]
            sys.stdout.write(command)
            cursorPos = len(command)
          endif
          inEsc = False
          esc = NULL
        elif (char == 'C'):
          # right arrow
          if (cursorPos < len(command)):
            sys.stdout.write(command[cursorPos:] + "\b"*(len(command[cursorPos:])-1))
            cursorPos += 1
          endif
          inEsc = False
          esc = NULL
        elif (char == 'D'):
          # left arrow
          if (cursorPos > 0):
            cursorPos -= 1
            sys.stdout.write("\b")
          endif
          inEsc = False
          esc = NULL
        elif (char == '1'):
          print "home2"
          if (cursorPos > 0):
            cursorPos = 0
            sys.stdout.write("\b"*len(command))
          endif
        #elif (char == '3'):
        #  print "delete"
        elif (char == '~'):
          # delete under cursor
          if (cursorPos < len(command)):
            sys.stdout.write(command[cursorPos+1:] + " " + "\b"*(len(command[cursorPos:])))
            command = command[:cursorPos] + command[cursorPos+1:]
          endif
          inEsc = False
          esc = NULL
        elif (char == '4'):
          print "end2"
          if (cursorPos < len(command)):
            sys.stdout.write(command[cursorPos:])
            cursorPos = len(command)
          endif
        endif
      elif (esc == 'O'):
        if (char == 'H'):
          # home
          if (cursorPos > 0):
            cursorPos = 0
            sys.stdout.write("\b"*len(command))
          endif
        elif (char == 'F'):
          #end
          if (cursorPos < len(command)):
            sys.stdout.write(command[cursorPos:])
            cursorPos = len(command)
          endif
        endif
        inEsc = False
        esc = NULL
      elif ((char == '[') or (char == 'O')):
        esc = char
      else:
        inEsc = False
      endif
    elif ((ord(char) >= 32) and (ord(char) < 127)):
      # printable single character, add it to our command
      # see if we are in the middle of the string, need to insert differently
      # than when at the beginning or end
      if ((cursorPos > 0) and (cursorPos < len(command))):
        command = command[:cursorPos] + char + command[cursorPos:]
        sys.stdout.write(command[cursorPos:] + "\b"*(len(command[cursorPos:])-1))
      else:
        command = command[:cursorPos] + char + command[cursorPos:]
        sys.stdout.write(command[cursorPos:] + "\b"*(len(command[cursorPos:])-1))
      endif
      cursorPos += 1
    elif (ord(char) == 13):
      # carriage return
      if (len(command) > 0):
        gCommandHistory.append(command)
        gCommandHistoryPos = len(gCommandHistory)
        return (command)
      else:
        sys.stdout.write("\n"+prompt_)
      endif
    elif (ord(char) == 11):
      # kill to eol
      sys.stdout.write(" "*len(command[cursorPos:]) + "\b"*(len(command[cursorPos:])))      
      command = command[:cursorPos]
    elif (ord(char) == 21):
      # kill whole line
      sys.stdout.write("\b"*cursorPos + " "*len(command) + "\b"*(len(command)))
      command = NULL
      cursorPos = 0
    elif (ord(char) == 27):
      # esc character
      inEsc = True
    elif ((ord(char) == 9) and ((len(command) == 0) or (len(command.split()) == 1))):
      # tab character
      tabCount += 1
      if (tabCount == 2):
        if (len(command) == 0):
          sys.stdout.write("\n")
          numPrinted = 0
          for keyword in gTabCompletions:
            sys.stdout.write("%-*s" % (gMaxTabCompletionKeywordLength, keyword))
            numPrinted += 1
            if (numPrinted > gMaxCompletionsPerLine):
              sys.stdout.write("\n")
              numPrinted = 0
            endif
          endfor
          sys.stdout.write("\n"+prompt_)
        else:
          matchFound = False
          for keyword in gTabCompletions:
            if (command in keyword):
              matchFound = True
              break
            enddef
          endfor
          if (matchFound == True):
            sys.stdout.write("\n")
            numPrinted = 0
            for keyword in gTabCompletions:
              if (command in keyword):
                sys.stdout.write("%-*s" % (gMaxTabCompletionKeywordLength, keyword))
                numPrinted += 1
                if (numPrinted > gMaxCompletionsPerLine):
                  sys.stdout.write("\n")
                  numPrinted = 0
                endif
              endif
            endfor
            sys.stdout.write("\n"+prompt_+command)
          endif
        endif
        tabCount = 0
      elif ((tabCount == 1) and (len(command) > 0)):
        numFound = 0
        for keyword in gTabCompletions:
          if (command in keyword):
            numFound += 1
          endif
        endfor
        if (numFound == 1):
          tabCount = 0
          for keyword in gTabCompletions:
            if ((command != keyword) and command in keyword):
              sys.stdout.write("\b"*cursorPos + " "*len(command) + "\b"*(len(command)))
              command = keyword
              sys.stdout.write(command)
              cursorPos = len(command)           
            endif
          endfor
        endif
      elif (len(command) > 0):
        tabCount = 0
      endif
    elif (ord(char) == 127):
      # backspace delete
      if ((len(command) > 0) and (cursorPos > 0)):
        sys.stdout.write("\b" + command[cursorPos:] + " " + "\b"*(len(command[cursorPos:])+1))
        command = command[:cursorPos-1] + command[cursorPos:]
        cursorPos -= 1
      endif
    elif (ord(char) == 1):
      # home
      if (cursorPos > 0):
        cursorPos = 0
        sys.stdout.write("\b"*len(command))
      endif
    elif (ord(char) == 3):
      # ctrl-c, exit program
      print
      sys.exit(0)
    elif (ord(char) == 5):
      # end
      if (cursorPos < len(command)):
        sys.stdout.write(command[cursorPos:])
        cursorPos = len(command)
      endif
    elif (ord(char) != 9):
      # don't print out tab if multi keyword command
      #sys.stdout.write("\nchar value: %d" % ord(char))
      #sys.stdout.write("\n"+prompt_)
      None
    endif
  endwhile
enddef

#################################################################################
#################################################################################
def __getChar():
  inFd = sys.stdin
  old_settings = termios.tcgetattr(inFd)
  try:
    tty.setraw(inFd)
    char = inFd.read(1)
  finally:
    termios.tcsetattr(inFd, termios.TCSADRAIN, old_settings)
  return (char)
enddef

