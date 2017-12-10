#!/usr/bin/python

# import all our necessary modules
import sys
import PshellReadline

# dummy variables so we can create pseudo end block indicators, add these identifiers to your
# list of python keywords in your editor to get syntax highlighting on these identifiers, sorry Guido
enddef = endif = endwhile = endfor = None

# python does not have a native null string identifier, so create one
NULL = ""

command = "xxx"
while (command not in "quit"):
  command = PshellReadline.getInput("prompt> ")
  print "\ncommand: '%s'" % command
endwhile
