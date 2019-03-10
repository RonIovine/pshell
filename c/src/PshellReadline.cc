/*******************************************************************************
 *
 * Copyright (c) 2009, Ron Iovine, All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Ron Iovine nor the names of its contributors 
 *       may be used to endorse or promote products derived from this software 
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Ron Iovine ''AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL Ron Iovine BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/select.h>

#include <PshellReadline.h>

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

/*************************
 * private "member" data
 *************************/

/* free and zero (to avoid double-free) */
#define free_z(p) do { if (p) { free(p); (p) = 0; } } while (0)

#define MAX_HISTORY 512
static char *_history[MAX_HISTORY];
static unsigned _historyPos = 0;
static unsigned _numHistory = 0;

#define MAX_TAB_COMPLETIONS 256
static char *_tabMatches[MAX_TAB_COMPLETIONS];
static unsigned _numTabMatches = 0;
static char *_tabCompletions[MAX_TAB_COMPLETIONS];
static unsigned _numTabCompletions = 0;
static unsigned _maxTabCompletionKeywordLength = 0;
static unsigned _maxCompletionsPerLine = 0;
static unsigned _maxMatchKeywordLength = 0;
static unsigned _maxMatchCompletionsPerLine = 0;
static PshellTabStyle _tabStyle = PSHELL_RL_FAST_TAB;
static PshellSerialType _serialType = PSHELL_RL_TTY;
static int _inFd = STDIN_FILENO;
static int _outFd = STDOUT_FILENO;
static int _idleTimeout = PSHELL_RL_IDLE_TIMEOUT_NONE;
static const char *_tcpNegotiate = "\xFF\xFB\x03\xFF\xFB\x01\xFF\xFD\x03\xFF\xFD\x01";

/****************************************
 * private "member" function prototypes
 ****************************************/

static void backspace(unsigned count_ = 1);
static void space(unsigned count_ = 1);
static void newline(unsigned count_ = 1);
static char *stripWhitespace(char *string_);
static unsigned numKeywords(char *command_);
static void findTabCompletions(const char *keyword_);
static char *findLongestMatch(char *command_);
static void showTabCompletions(char *completionList_[],
                               unsigned numCompletions_,
			       unsigned maxCompletionsPerLine_,
			       unsigned maxCompletionLength_,
			       const char* format_, ...);
static void clearLine(unsigned cursorPos_, char *command_);
static void beginningOfLine(unsigned &cursorPos_, const char *command_);
static void endOfLine(unsigned &cursorPos_, const char *command_);
static void killLine(unsigned &cursorPos_, char *command_);
static void killEndOfLine(unsigned cursorPos_, char *command_);
static void deleteUnderCursor(unsigned cursorPos_, char *command_);
static void backspaceDelete(unsigned &cursorPos_, char *command_);
static void addChar(unsigned &cursorPos_, char *command_, char ch_);
static void upArrow(unsigned &cursorPos_, char *command_);
static void downArrow(unsigned &cursorPos_, char *command_);
static void leftArrow(unsigned &cursorPos_);
static void rightArrow(unsigned &cursorPos_, char *command_);
static unsigned showCommand(char *outCommand_, const char* format_, ...);
static void addHistory(char *command_);
static void showHistory(void);
static bool getChar(char &ch);
static bool isNumeric(const char *string_);
#if 0
static void clearHistory(void);
#endif

/**************************************
 * public API "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void pshell_rl_setFileDescriptors(int inFd_, int outFd_, PshellSerialType serialType_, int idleTimeout_)
{
  _inFd = inFd_;
  _outFd = outFd_;
  _serialType = serialType_;
  pshell_rl_setIdleTimeout(idleTimeout_);
  // if a socket serial device, setup for telnet client control
  if (_serialType == PSHELL_RL_SOCKET)
  {
    pshell_rl_writeOutput(_tcpNegotiate);
  }
}

/******************************************************************************/
/******************************************************************************/
void pshell_rl_writeOutput(const char* format_, ...)
{
  char string[PSHELL_RL_MAX_COMMAND_SIZE] = {0};
  char socketString[PSHELL_RL_MAX_COMMAND_SIZE] = {0};
  va_list args;

  va_start(args, format_);
  vsnprintf(string, sizeof(string), format_, args);
  va_end(args);
  if (_serialType == PSHELL_RL_SOCKET)
  {
    // need to convert \n to \r\n
    unsigned j = 0;
    for (unsigned i = 0; i < strlen(string); i++)
    {
      if (string[i] == '\n')
      {
	socketString[j++] = '\r';
      }
      socketString[j++] = string[i];
    }
    write(_outFd, socketString, strlen(socketString));
  }
  else
  {
    write(_outFd, string, strlen(string));
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_rl_getInput(const char *prompt_, char *input_)
{
  bool inEsc = false;
  char esc = '\0';
  unsigned cursorPos = 0;
  unsigned tabCount = 0;
  bool idleSession = false;
  unsigned index = 0;
  char ch;
  input_[0] = 0;
  pshell_rl_writeOutput(prompt_);
  while (true)
  {
    idleSession = getChar(ch);
    // check for idleSession timeout
    if (idleSession == true)
    {
      return (true);
    }
    if (ch != 9)
    {
      // something other than TAB typed, clear out our tabCount
      tabCount = 0;
    }
    if (inEsc == true)
    {
      if (esc == '[')
      {
        if (ch == 'A')
	{
          // up-arrow key
	  upArrow(cursorPos, input_);
          inEsc = false;
          esc = '\0';
	}
        else if (ch == 'B')
	{
          // down-arrow key
	  downArrow(cursorPos, input_);
          inEsc = false;
          esc = '\0';
	}
        else if (ch == 'C')
	{
          // right-arrow key
	  rightArrow(cursorPos, input_);
          inEsc = false;
          esc = '\0';
	}
        else if (ch == 'D')
	{
          // left-arrow key
	  leftArrow(cursorPos);
          inEsc = false;
          esc = '\0';
	}
        else if (ch == '1')
	{
          printf("home2");
          beginningOfLine(cursorPos, input_);
	}
        else if (ch == '3')
	{
          //printf("delete");
        }
        else if (ch == '~')
	{
          // delete key, delete under cursor
	  deleteUnderCursor(cursorPos, input_);
          inEsc = false;
          esc = '\0';
	}
        else if (ch == '4')
	{
          printf("end2");
          endOfLine(cursorPos, input_);
	}
      }
      else if (esc == 'O')
      {
        if (ch == 'H')
	{
          // home key, go to beginning of line
          beginningOfLine(cursorPos, input_);
	}
        else if (ch == 'F')
	{
          // end key, go to end of line
          endOfLine(cursorPos, input_);
	}
        inEsc = false;
        esc = '\0';
      }
      else if ((ch == '[') || (ch == 'O'))
      {
        esc = ch;
      }
      else
      {
        inEsc = false;
      }
    }
    else if ((ch >= 32) && (ch < 127))
    {
      // printable single character, add it to our input_
      addChar(cursorPos, input_, ch);
    }
    else if ((ch == 13) || (ch == 10))
    {
      // carriage return or line feed
      newline();
      if (strlen(input_) > 0)
      {
	if (strcmp(input_, "history") == 0)
	{
          // add input_ to our command history
          addHistory(input_);
	  // we process the history internally
	  showHistory();
	  input_[0] = 0;
	  cursorPos = 0;
	  tabCount = 0;
          pshell_rl_writeOutput(prompt_);
	}
	else if ((strlen(input_) > 1) && (input_[0] == '!'))
	{
	  // they want to recall a specific command in the history, check if it is within range
	  if (isNumeric(&input_[1]))
	  {
	    index = atoi(&input_[1])-1;
	    if (index < _numHistory)
	    {
	      input_[0] = 0;
	      strcpy(input_, _history[index]);
	      addHistory(input_);
	      if (strcmp(_history[index], "history") == 0)
	      {
	        showHistory();
	        input_[0] = 0;
	        cursorPos = 0;
	        tabCount = 0;
                pshell_rl_writeOutput(prompt_);
	      }
	      else
	      {
	        return (false);
	      }
	    }
	    else
	    {
              input_[0] = 0;
              cursorPos = 0;
	      tabCount = 0;
	      pshell_rl_writeOutput("PSHELL_ERROR: History index: %d, out of bounds, range 1-%d\n", index+1, _numHistory);
              pshell_rl_writeOutput(prompt_);
	    }
	  }
	  else
	  {
	    pshell_rl_writeOutput("PSHELL_ERROR: Invalid history index: '%s'\n", &input_[1]);
            pshell_rl_writeOutput(prompt_);
            input_[0] = 0;
            cursorPos = 0;
	    tabCount = 0;
	  }
	}
	else
	{
          // add input_ to our command history
          addHistory(input_);
          // normal input, return no timeout
          return (false);
	}
      }
      else
      {
	// just pressed CR with no input, just give prompt again
        pshell_rl_writeOutput(prompt_);
      }
    }
    else if (ch == 11)
    {
      // kill to eol, CTRL-K
      killEndOfLine(cursorPos, input_);
    }
    else if (ch == 21)
    {
      // kill whole line, CTRL-U
      killLine(cursorPos, input_);
    }
    else if (ch == 27)
    {
      // esc character
      inEsc = true;
    }
    else if ((ch == 9) && ((strlen(input_) == 0) || (numKeywords(input_) == 1)))
    {
      // tab character, print out any completions, we only do tabbing on the first keyword
      tabCount += 1;
      if (_tabStyle == PSHELL_RL_FAST_TAB)
      {
        if (tabCount == 1)
	{
          // this tabbing method is a little different than the standard
          // readline or bash shell tabbing, we always trigger on a single
          // tab and always show all the possible completions for any
          // multiple matches
          if (strlen(input_) == 0)
	  {
            // nothing typed, just TAB, show all registered TAB completions
            showTabCompletions(_tabCompletions, _numTabCompletions, _maxCompletionsPerLine, _maxTabCompletionKeywordLength, prompt_);
	  }
          else
	  {
            // partial word typed, show all possible completions
            findTabCompletions(input_);
            if (_numTabMatches == 1)
	    {
              // only one possible completion, show it
              clearLine(cursorPos, input_);
              cursorPos = showCommand(input_, "%s ", _tabMatches[0]);
	    }
            else if (_numTabMatches > 1)
	    {
              // multiple possible matches, fill out longest match and
              // then show all other possibilities
              clearLine(cursorPos, input_);
              cursorPos = showCommand(input_, findLongestMatch(input_));
              showTabCompletions(_tabMatches, _numTabMatches, _maxMatchKeywordLength, _maxMatchCompletionsPerLine, "%s%s", prompt_, input_);
	    }
	  }
	}
      }
      else  // BASH_TAB
      {
        // this code below implements the more standard readline/bash double tabbing method 
        if (tabCount == 2)
	{
          if (strlen(input_) == 0)
	  {
            // nothing typed, just a double TAB, show all registered TAB completions
            showTabCompletions(_tabCompletions, _numTabCompletions, _maxCompletionsPerLine, _maxTabCompletionKeywordLength, prompt_);
	  }
          else
	  {
            // partial word typed, double TAB, show all possible completions
	    findTabCompletions(input_);
            showTabCompletions(_tabMatches, _numTabMatches, _maxMatchKeywordLength, _maxMatchCompletionsPerLine, "%s%s", prompt_, input_);
	  }
	}
        else if ((tabCount == 1) && (strlen(input_) > 0))
	{
          // partial word typed, single TAB, fill out as much
          //  as we can and show any possible other matches
          findTabCompletions(input_);
          if (_numTabMatches == 1)
	  {
            // we only have one completion, show it
            clearLine(cursorPos, input_);
            cursorPos = showCommand(input_, "%s ", _tabMatches[0]);
	  }
          else if (_numTabMatches > 1)
	  {
            // multiple completions, find the longest match and show up to that
            clearLine(cursorPos, input_);
            cursorPos = showCommand(input_, findLongestMatch(input_));
	  }
	}
      }
    }
    else if (ch == 127)
    {
      // backspace delete
      backspaceDelete(cursorPos, input_);
    }
    else if (ch == 1)
    {
      // home, go to beginning of line
      beginningOfLine(cursorPos, input_);
    }
    else if (ch == 5)
    {
      // end, go to end of line
      endOfLine(cursorPos, input_);
    }
    else if (ch != 9)
    {
      // don't print out tab if multi keyword command
      //pshell_rl_writeOutput("\nchar value: %d" % ch)
      //pshell_rl_writeOutput("\n"+prompt_)
    }
  }
}

/******************************************************************************/
/******************************************************************************/
bool pshell_rl_isSubString(const char *string1_, const char *string2_, unsigned minChars_)
{
  unsigned length;
  if ((string1_ == NULL) && (string2_ == NULL))
  {
    return (true);
  }
  else if ((string1_ != NULL) && (string2_ != NULL))
  {
    if ((length = strlen(string1_)) > strlen(string2_))
    {
      return (false);
    }
    else if (minChars_ == 0)
    {
      return (strncmp(string1_, string2_, strlen(string1_)) == 0);
    }
    else
    {
      return (strncmp(string1_, string2_, MAX(length, minChars_)) == 0);
    }
  }
  else
  {
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
void pshell_rl_addTabCompletion(const char *keyword_)
{
  for (unsigned i = 0; i < _numTabCompletions; i++)
  {
    if (strcmp(_tabCompletions[i], keyword_) == 0)
    {
      // duplicate keyword found, return
      return;
    }
  }
  if (strlen(keyword_) > _maxTabCompletionKeywordLength)
  {
    _maxTabCompletionKeywordLength = strlen(keyword_)+5;
    _maxCompletionsPerLine = 80/_maxTabCompletionKeywordLength;
  }
  _tabCompletions[_numTabCompletions++] = strdup(stripWhitespace((char *)keyword_));
}

/******************************************************************************/
/******************************************************************************/
void pshell_rl_setIdleTimeout(int timeout_)
{
  _idleTimeout = timeout_;
}

/******************************************************************************/
/******************************************************************************/
void pshell_rl_setTabStyle(PshellTabStyle tabStyle_)
{
  _tabStyle = tabStyle_;
}

/************************************
 * private "member" function bodies
 ************************************/

/******************************************************************************/
/******************************************************************************/
static void backspace(unsigned count_)
{
  for (unsigned i = 0; i < count_; i++)
  {
    write(_outFd, "\b", 1);
  }
}

/******************************************************************************/
/******************************************************************************/
static void space(unsigned count_)
{
  for (unsigned i = 0; i < count_; i++)
  {
    write(_outFd, " ", 1);
  }
}

/******************************************************************************/
/******************************************************************************/
static void newline(unsigned count_)
{
  for (unsigned i = 0; i < count_; i++)
  {
    if (_serialType == PSHELL_RL_TTY)
    {
      write(_outFd, "\n", 1);
    }
    else
    {
      write(_outFd, "\r\n", 2);
    }
  }
}

/******************************************************************************/
/******************************************************************************/
static char *stripWhitespace(char *string_)
{
  char *str = string_;

  /* strip leading whitespace */
  for (unsigned i = 0; i < strlen(string_); i++)
  {
    if (!isspace(string_[i]))
    {
      str = &string_[i];
      break;
    }
  }
  if (string_ != str)
  {
    strcpy(string_, str);
  }
  /* strip trailing whitespace */
  for (unsigned i = strlen(string_)-1; ((i >= 0) && isspace(string_[i])); i--)
  {
    string_[i] = '\0';
  }
  return (string_);
}

/******************************************************************************/
/******************************************************************************/
static unsigned numKeywords(char *command_)
{
  unsigned numKeywords = 1;
  stripWhitespace(command_);
  for (unsigned i = 0; i < strlen(command_); i++)
  {
    if (command_[i] == ' ')
    {
      numKeywords++;
    }
  }
  return (numKeywords);
}

/******************************************************************************/
/******************************************************************************/
static void findTabCompletions(const char *keyword_)
{
  _numTabMatches = 0;
  _maxMatchKeywordLength = 0;
  _maxMatchCompletionsPerLine = 0;
  for (unsigned i = 0; i < _numTabCompletions; i++)
  {
    if (pshell_rl_isSubString(keyword_, _tabCompletions[i]))
    {
      _tabMatches[_numTabMatches++] = _tabCompletions[i];
      if (strlen(keyword_) > _maxMatchKeywordLength)
      {
        _maxMatchKeywordLength = strlen(keyword_)+5;
        _maxMatchCompletionsPerLine = 80/_maxMatchKeywordLength;
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
static char *findLongestMatch(char *command_)
{
  char ch;
  unsigned charPos = strlen(command_);
  while (true)
  {
    if (charPos < strlen(_tabMatches[0]))
    {
      ch = _tabMatches[0][charPos];
      for (unsigned i = 0; i < _numTabMatches; i++)
      {
        if ((charPos >= strlen(_tabMatches[i])) || (ch != _tabMatches[i][charPos]))
	{
	  command_[charPos] = 0;
          return (command_);
	}
      }
      command_[charPos++] = ch;
    }
    else
    {
      return (command_);
    }
  }
}

/******************************************************************************/
/******************************************************************************/
static void showTabCompletions(char *completionList_[],
                               unsigned numCompletions_,
			       unsigned maxCompletionsPerLine_,
			       unsigned maxCompletionLength_,
			       const char* format_, ...)
{
  char prompt[PSHELL_RL_MAX_COMMAND_SIZE] = {0};
  va_list args;
  if (_numTabCompletions > 0)
  {
    newline();
    unsigned totPrinted = 0;
    unsigned numPrinted = 0;
    for (unsigned i = 0; i < numCompletions_; i++)
    {
      pshell_rl_writeOutput("%s", completionList_[i]);
      space(maxCompletionLength_-strlen(completionList_[i]));
      numPrinted += 1;
      totPrinted += 1;
      if ((numPrinted == maxCompletionsPerLine_) and (totPrinted < numCompletions_))
      {
        newline();
        numPrinted = 0;
      }
    }
    va_start(args, format_);
    vsnprintf(prompt, sizeof(prompt), format_, args);
    va_end(args);
    pshell_rl_writeOutput("\n%s", prompt);
  }
}

/******************************************************************************/
/******************************************************************************/
static void clearLine(unsigned cursorPos_, char *command_)
{
  backspace(cursorPos_);
  space(strlen(command_));
  backspace(strlen(command_));
}

/******************************************************************************/
/******************************************************************************/
static void beginningOfLine(unsigned &cursorPos_, const char *command_)
{
  if (cursorPos_ > 0)
  {
    cursorPos_ = 0;
    backspace(strlen(command_));
  }
}

/******************************************************************************/
/******************************************************************************/
static void endOfLine(unsigned &cursorPos_, const char *command_)
{
  if (cursorPos_ < strlen(command_))
  {
    pshell_rl_writeOutput(&command_[cursorPos_]);
    cursorPos_ = strlen(command_);
  }
}

/******************************************************************************/
/******************************************************************************/
static void killLine(unsigned &cursorPos_, char *command_)
{
  clearLine(cursorPos_, command_);
  cursorPos_ = 0;
  command_[cursorPos_] = 0;
}

/******************************************************************************/
/******************************************************************************/
static void killEndOfLine(unsigned cursorPos_, char *command_)
{
  space(strlen(&command_[cursorPos_]));
  backspace(strlen(&command_[cursorPos_]));
  command_[cursorPos_] = 0;
}

/******************************************************************************/
/******************************************************************************/
static void deleteUnderCursor(unsigned cursorPos_, char *command_)
{
  if (cursorPos_ < strlen(command_))
  {
    pshell_rl_writeOutput("%s ", &command_[cursorPos_+1]);
    backspace(strlen(&command_[cursorPos_]));
    command_[cursorPos_] = 0;
    sprintf(command_, "%s%s", command_, &command_[cursorPos_+1]);
  }
}

/******************************************************************************/
/******************************************************************************/
static void backspaceDelete(unsigned &cursorPos_, char *command_)
{
  if ((strlen(command_) > 0) && (cursorPos_ > 0))
  {
    backspace();
    pshell_rl_writeOutput("%s ", &command_[cursorPos_]);
    backspace(strlen(&command_[cursorPos_])+1);
    sprintf(&command_[cursorPos_-1], "%s", &command_[cursorPos_]);
    cursorPos_ -= 1;
  }
}

/******************************************************************************/
/******************************************************************************/
static void addChar(unsigned &cursorPos_, char *command_, char ch_)
{
  char newCommand[PSHELL_RL_MAX_COMMAND_SIZE] = {0};
  snprintf(newCommand, cursorPos_+1, "%s", command_);
  sprintf(&newCommand[cursorPos_], "%c%s", ch_, &command_[cursorPos_]);
  strcpy(command_, newCommand);
  pshell_rl_writeOutput(&command_[cursorPos_]);
  backspace(strlen(&command_[cursorPos_])-1);
  cursorPos_ += 1;
}

/******************************************************************************/
/******************************************************************************/
static void upArrow(unsigned &cursorPos_, char *command_)
{
  if (_historyPos > 0)
  {
    _historyPos -= 1;
    clearLine(cursorPos_, command_);
    cursorPos_ = showCommand(command_, _history[_historyPos]);
  }
}

/******************************************************************************/
/******************************************************************************/
static void downArrow(unsigned &cursorPos_, char *command_)
{
  if (_historyPos < _numHistory-1)
  {
    _historyPos += 1;
    clearLine(cursorPos_, command_);
    cursorPos_ = showCommand(command_, _history[_historyPos]);
  }
  else
  {
    // kill whole line
    _historyPos = _numHistory;
    killLine(cursorPos_, command_);
  }
}

/******************************************************************************/
/******************************************************************************/
static void leftArrow(unsigned &cursorPos_)
{
  if (cursorPos_ > 0)
  {
    cursorPos_ -= 1;
    backspace();
  }
}

/******************************************************************************/
/******************************************************************************/
static void rightArrow(unsigned &cursorPos_, char *command_)
{
  if (cursorPos_ < strlen(command_))
  {
    pshell_rl_writeOutput(&command_[cursorPos_]);
    backspace(strlen(&command_[cursorPos_])-1);
    cursorPos_ += 1;
  }
}

/******************************************************************************/
/******************************************************************************/
static unsigned showCommand(char *outCommand_, const char* format_, ...)
{
  char inCommand[PSHELL_RL_MAX_COMMAND_SIZE] = {0};
  va_list args;

  va_start(args, format_);
  vsnprintf(inCommand, sizeof(inCommand), format_, args);
  va_end(args);
  pshell_rl_writeOutput(inCommand);
  strcpy(outCommand_, inCommand);
  return (strlen(outCommand_));
}

/******************************************************************************/
/******************************************************************************/
static void addHistory(char *command_)
{
  unsigned i;

  for (i = 0; i < MAX_HISTORY; i++)
  {
    /* look for the first empty slot */
    if (_history[i] == NULL)
    {
      /* if this is the first entry or the new entry is not
       * the same as the previous one, go ahead and add it
       */
      if ((i == 0) || (strcasecmp(_history[i-1], command_) != 0))
      {
        _history[i] = strdup(command_);
	_numHistory++;
      }
      _historyPos = _numHistory;
      return;
    }
  }

  /* No space found, drop one off the beginning of the list */
  free_z(_history[0]);

  /* move all the other entrys up the list */
  for (i = 0; i < MAX_HISTORY-1; i++)
  {
    _history[i] = _history[i+1];
  }

  /* add the new entry */
  _historyPos = _numHistory;
  _history[MAX_HISTORY-1] = strdup(command_);

}

/******************************************************************************/
/******************************************************************************/
#if 0
static void clearHistory(void)
{
  _numHistory = 0;
  for (unsigned i = 0; i < MAX_HISTORY; i++)
  {
    if (_history[i] != NULL)
    {
      free_z(_history[i]);
    }
  }
}
#endif

/******************************************************************************/
/******************************************************************************/
static void showHistory(void)
{
  for (unsigned i = 0; i < _numHistory; i++)
  {
    pshell_rl_writeOutput("%-3d %s\n", i+1, _history[i]); 
  }
}

/******************************************************************************/
/******************************************************************************/
static bool getChar(char &ch)
{
  //char buf = 0;
  unsigned retCode;
  fd_set readFd;
  bool idleSession = false;
  struct timeval idleTimeout;
  struct termios old = {0};
  ch = 0;
  idleTimeout.tv_sec = _idleTimeout;
  idleTimeout.tv_usec = 0;
  FD_ZERO(&readFd);
  FD_SET(_inFd, &readFd);
  if (_serialType == PSHELL_RL_TTY)
  {
    tcgetattr(0, &old);
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &old);
    if (_idleTimeout > 0)
    {
      if ((retCode = select(_inFd+1, &readFd, NULL, NULL, &idleTimeout)) == 0)
      {
        pshell_rl_writeOutput("\nIdle session timeout\n");
        idleSession = true;
      }
      else if (retCode > 0)
      {
        read(_inFd, &ch, 1);
      }
    }
    else
    {
      read(_inFd, &ch, 1);
    }
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    tcsetattr(0, TCSADRAIN, &old);
  }
  else
  {
    // TCP socket with telnet client
    if (_idleTimeout > 0)
    {
      if ((retCode = select(_inFd+1, &readFd, NULL, NULL, &idleTimeout)) == 0)
      {
        pshell_rl_writeOutput("\nIdle session timeout\n");
        idleSession = true;
      }
      else if (retCode > 0)
      {
        read(_inFd, &ch, 1);
      }
    }
    else
    {
      read(_inFd, &ch, 1);
    }
  }
  return (idleSession);
}

/******************************************************************************/
/******************************************************************************/
static bool isNumeric(const char *string_)
{
  for (unsigned i = 0; i < strlen(string_); i++)
  {
    if (!isdigit(string_[i]))
    {
      return (false);
    }
  }
  return (true);
}
