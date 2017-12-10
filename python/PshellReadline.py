#!/usr/bin/python

# import all our necessary modules
import sys
import tty
import termios

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

def getInput(prompt_):
  sys.stdout.write(prompt_)
  inEsc = False
  esc = NULL
  command = NULL
  cursorPos = 0
  while (True):
    char = __getch()
    #print ord(char)
    if (inEsc == True):
      if (esc == '['):
        if (char == 'A'):
          print "\nup-arrow"
          inEsc = False
          esc = NULL
        elif (char == 'B'):
          print "down-arrow"
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
    elif (ord(char) == 5):
      # end
      if (cursorPos < len(command)):
        sys.stdout.write(command[cursorPos:])
        cursorPos = len(command)
      endif
    else:
      print "char value: %d" % ord(char)
    endif
  endwhile
enddef

def __getch():
  fd = sys.stdin.fileno()
  old_settings = termios.tcgetattr(fd)
  try:
    tty.setraw(fd)
    ch = sys.stdin.read(1)
  finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
  return ch
enddef

