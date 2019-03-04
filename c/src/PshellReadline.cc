#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/select.h>

enum TabStyle
{
  FAST_TAB,
  BASH_TAB 
};

enum SerialType
{
  TTY,
  SOCKET
};

#define IDLE_TIMEOUT_NONE 0
#define ONE_SECOND 1
#define ONE_MINUTE ONE_SECOND*60


#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

/* free and zero (to avoid double-free) */
#define free_z(p) do { if (p) { free(p); (p) = 0; } } while (0)

#define MAX_COMMAND_SIZE 256
static char _command[MAX_COMMAND_SIZE] = {0};

#define MAX_TOKENS 32

#define PSHELL_MAX_HISTORY 256
static char *_history[PSHELL_MAX_HISTORY];
int _historyPos = 0;
int _numHistory = 0;

#define MAX_TAB_COMPLETIONS 256
static char *_tabMatches[MAX_TAB_COMPLETIONS];
int _numTabMatches = 0;
static char *_tabCompletions[MAX_TAB_COMPLETIONS];
int _numTabCompletions = 0;
int _maxTabCompletionKeywordLength = 0;
int _maxCompletionsPerLine = 0;
TabStyle _tabStyle = FAST_TAB;
SerialType _serialType = TTY;
int _inFd = STDIN_FILENO;
int _outFd = STDOUT_FILENO;
int _idleTimeout = 0;

int showCommand(char *outCommand_, const char* format_, ...);

/******************************************************************************/
/******************************************************************************/
void writeString(const char* format_, ...)
{
  char string[MAX_COMMAND_SIZE] = {0};
  va_list args;

  va_start(args, format_);
  vsnprintf(string, sizeof(string), format_, args);
  va_end(args);
  
  write(_outFd, string, strlen(string));
}

/******************************************************************************/
/******************************************************************************/
void backspace(int count_ = 1)
{
  for (int i = 0; i < count_; i++)
  {
    write(_outFd, "\b", 1);
  }
}

/******************************************************************************/
/******************************************************************************/
void space(int count_ = 1)
{
  for (int i = 0; i < count_; i++)
  {
    write(_outFd, " ", 1);
  }
}

/******************************************************************************/
/******************************************************************************/
void newline(int count_ = 1)
{
  for (int i = 0; i < count_; i++)
  {
    write(_outFd, "\n", 1);
  }
}

/******************************************************************************/
/******************************************************************************/
char *stripWhitespace(char *string_)
{
  int i;
  char *str = string_;

  /* strip leading whitespace */
  for (i = 0; i < (int)strlen(string_); i++)
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
  for (i = strlen(string_)-1; ((i >= 0) && isspace(string_[i])); i--)
  {
    string_[i] = '\0';
  }
  return (string_);
}

/******************************************************************************/
/******************************************************************************/
int numKeywords(char *command_)
{
  int numKeywords = 1;
  stripWhitespace(command_);
  for (int i = 0; i < strlen(command_); i++)
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
bool isSubString(const char *string1_, const char *string2_, unsigned minChars_ = 0)
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
void addTabCompletion(const char *keyword_)
{
  for (int i = 0; i < _numTabCompletions; i++)
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
  _tabCompletions[_numTabCompletions++] = stripWhitespace((char *)keyword_);
}

/******************************************************************************/
/******************************************************************************/
void findTabCompletions(const char *keyword_)
{
  _numTabMatches = 0;
  for (int i = 0; i < _numTabCompletions; i++)
  {
    if (isSubString(keyword_, _tabCompletions[i]))
    {
      _tabMatches[_numTabMatches++] = _tabCompletions[i];
    }
  }
}

/******************************************************************************/
/******************************************************************************/
char *findLongestMatch(char *command_)
{
  char ch;
  int charPos = strlen(command_);
  while (true)
  {
    if (charPos < strlen(_tabMatches[0]))
    {
      ch = _tabMatches[0][charPos];
      for (int i = 0; i < _numTabMatches; i++)
      {
        if ((charPos >= strlen(_tabMatches[i])) || (ch != _tabMatches[i][charPos]))
	{
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
void showTabCompletions(char *completionList_[], int numCompletions_, const char* format_, ...)
{
  char prompt[MAX_COMMAND_SIZE] = {0};
  va_list args;
  if (_numTabCompletions > 0)
  {
    newline();
    int totPrinted = 0;
    int numPrinted = 0;
    for (int i = 0; i < numCompletions_; i++)
    {
      writeString("%s", (_maxTabCompletionKeywordLength, completionList_[i]));
      space(_maxTabCompletionKeywordLength-strlen(completionList_[i]));
      numPrinted += 1;
      totPrinted += 1;
      if ((numPrinted == _maxCompletionsPerLine) and (totPrinted < numCompletions_))
      {
        newline();
        numPrinted = 0;
      }
    }
    va_start(args, format_);
    vsnprintf(prompt, sizeof(prompt), format_, args);
    va_end(args);
    writeString("\n%s", prompt);
  }
}

/******************************************************************************/
/******************************************************************************/
void clearLine(int cursorPos_, char *command_)
{
  backspace(cursorPos_);
  space(strlen(command_));
  backspace(strlen(command_));
}

/******************************************************************************/
/******************************************************************************/
void beginningOfLine(int &cursorPos_, const char *command_)
{
  if (cursorPos_ > 0)
  {
    cursorPos_ = 0;
    backspace(strlen(command_));
  }
}

/******************************************************************************/
/******************************************************************************/
void endOfLine(int &cursorPos_, const char *command_)
{
  if (cursorPos_ < strlen(command_))
  {
    writeString(&command_[cursorPos_]);
    cursorPos_ = strlen(command_);
  }
}

/******************************************************************************/
/******************************************************************************/
void killLine(int &cursorPos_, char *command_)
{
  clearLine(cursorPos_, command_);
  cursorPos_ = 0;
  command_[cursorPos_] = 0;
}

/******************************************************************************/
/******************************************************************************/
void killEndOfLine(int cursorPos_, char *command_)
{
  space(strlen(&command_[cursorPos_]));
  backspace(strlen(&command_[cursorPos_]));
  command_[cursorPos_] = 0;
}

/******************************************************************************/
/******************************************************************************/
void deleteUnderCursor(int cursorPos_, char *command_)
{
  if (cursorPos_ < strlen(command_))
  {
    writeString("%s ", &command_[cursorPos_+1]);
    backspace(strlen(&command_[cursorPos_]));
    command_[cursorPos_] = 0;
    sprintf(command_, "%s%s", command_, &command_[cursorPos_+1]);
  }
}

/******************************************************************************/
/******************************************************************************/
void backspaceDelete(int &cursorPos_, char *command_)
{
  if ((strlen(command_) > 0) && (cursorPos_ > 0))
  {
    backspace();
    writeString("%s ", &command_[cursorPos_]);
    backspace(strlen(&command_[cursorPos_])+1);
    sprintf(&command_[cursorPos_-1], "%s", &command_[cursorPos_]);
    cursorPos_ -= 1;
  }
}

/******************************************************************************/
/******************************************************************************/
void addChar(int &cursorPos_, char *command_, char ch_)
{
  char newCommand[MAX_COMMAND_SIZE] = {0};
  snprintf(newCommand, cursorPos_+1, "%s", command_);
  sprintf(&newCommand[cursorPos_], "%c%s", ch_, &command_[cursorPos_]);
  strcpy(command_, newCommand);
  writeString(&command_[cursorPos_]);
  backspace(strlen(&command_[cursorPos_])-1);
  cursorPos_ += 1;
}

/******************************************************************************/
/******************************************************************************/
void upArrow(int &cursorPos_, char *command_)
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
void downArrow(int &cursorPos_, char *command_)
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
void leftArrow(int &cursorPos_)
{
  if (cursorPos_ > 0)
  {
    cursorPos_ -= 1;
    backspace();
  }
}

/******************************************************************************/
/******************************************************************************/
void rightArrow(int &cursorPos_, char *command_)
{
  if (cursorPos_ < strlen(command_))
  {
    writeString(&command_[cursorPos_]);
    backspace(strlen(&command_[cursorPos_])-1);
    cursorPos_ += 1;
  }
}

/******************************************************************************/
/******************************************************************************/
int showCommand(char *outCommand_, const char* format_, ...)
{
  char inCommand[MAX_COMMAND_SIZE] = {0};
  va_list args;

  va_start(args, format_);
  vsnprintf(inCommand, sizeof(inCommand), format_, args);
  va_end(args);
  writeString(inCommand);
  strcpy(outCommand_, inCommand);
  return (strlen(outCommand_));
}

/******************************************************************************/
/******************************************************************************/
static void addHistory(char *command_)
{
  int i;

  for (i = 0; i < PSHELL_MAX_HISTORY; i++)
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
	_numHistory ++;
      }
      _historyPos = _numHistory;
      return;
    }
  }

  /* No space found, drop one off the beginning of the list */
  free_z(_history[0]);

  /* move all the other entrys up the list */
  for (i = 0; i < PSHELL_MAX_HISTORY-1; i++)
  {
    _history[i] = _history[i+1];
  }

  /* add the new entry */
  _historyPos = _numHistory;
  _history[PSHELL_MAX_HISTORY-1] = strdup(command_);

}

/******************************************************************************/
/******************************************************************************/
static void clearHistory(void)
{
  int i;
  _numHistory = 0;
  for (i = 0; i < PSHELL_MAX_HISTORY; i++)
  {
    if (_history[i] != NULL)
    {
      free_z(_history[i]);
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void setIdleSessionTimeout(int timeout_)
{
  _idleTimeout = timeout_;
}

/******************************************************************************/
/******************************************************************************/
char getChar(void)
{
  char buf = 0;
  int retCode;
  fd_set readFd;
  struct timeval idleTimeout;
  struct termios old = {0};
  tcgetattr(0, &old);
  old.c_lflag &= ~ICANON;
  old.c_lflag &= ~ECHO;
  old.c_cc[VMIN] = 1;
  old.c_cc[VTIME] = 0;
  tcsetattr(0, TCSANOW, &old);
  if (_idleTimeout > 0)
  {
    idleTimeout.tv_sec = _idleTimeout;
    idleTimeout.tv_usec = 0;
    FD_ZERO(&readFd);
    FD_SET(_inFd, &readFd);

    if ((retCode = select(_inFd+1, &readFd, NULL, NULL, &idleTimeout)) == 0)
    {
      printf("Idle session timeout\n");
    }
    else if (retCode > 0)
    {
      read(_inFd, &buf, 1);
    }
  }
  else
  {
    read(_inFd, &buf, 1);
  }
  old.c_lflag |= ICANON;
  old.c_lflag |= ECHO;
  tcsetattr(0, TCSADRAIN, &old);
  return (buf);
}

/******************************************************************************/
/******************************************************************************/
char *getInput(const char *prompt_)
{
  bool inEsc = false;
  char esc = '\0';
  char *command = _command;
  int cursorPos = 0;
  int tabCount = 0;
  char ch;
  command[0] = 0;
  writeString(prompt_);
  while (true)
  {
    ch = getChar();
    if (ch != 9)
    {
      // something other than TAB typed, clear out our tabCount
      tabCount = 0;
    }
    //print(ch)
    if (inEsc == true)
    {
      if (esc == '[')
      {
        if (ch == 'A')
	{
          // up-arrow key
	  upArrow(cursorPos, command);
          inEsc = false;
          esc = '\0';
	}
        else if (ch == 'B')
	{
          // down-arrow key
	  downArrow(cursorPos, command);
          inEsc = false;
          esc = '\0';
	}
        else if (ch == 'C')
	{
          // right-arrow key
	  rightArrow(cursorPos, command);
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
          beginningOfLine(cursorPos, command);
	}
        else if (ch == '3')
	{
          //printf("delete");
        }
        else if (ch == '~')
	{
          // delete key, delete under cursor
	  deleteUnderCursor(cursorPos, command);
          inEsc = false;
          esc = '\0';
	}
        else if (ch == '4')
	{
          printf("end2");
          endOfLine(cursorPos, command);
	}
      }
      else if (esc == 'O')
      {
        if (ch == 'H')
	{
          // home key, go to beginning of line
          beginningOfLine(cursorPos, command);
	}
        else if (ch == 'F')
	{
          // end key, go to end of line
          endOfLine(cursorPos, command);
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
      // printable single character, add it to our command
      addChar(cursorPos, command, ch);
    }
    else if ((ch == 13) || (ch == 10))
    {
      // carriage return or line feed
      newline();
      if (strlen(command) > 0)
      {
        // add command to our command history
	addHistory(command);
        // return command
        return (stripWhitespace(command));
      }
      else
      {
        writeString(prompt_);
      }
    }
    else if (ch == 11)
    {
      // kill to eol, CTRL-K
      killEndOfLine(cursorPos, command);
    }
    else if (ch == 21)
    {
      // kill whole line, CTRL-U
      killLine(cursorPos, command);
    }
    else if (ch == 27)
    {
      // esc character
      inEsc = true;
    }
    else if ((ch == 9) && ((strlen(command) == 0) || (numKeywords(command) == 1)))
    {
      // tab character, print out any completions, we only do tabbing on the first keyword
      tabCount += 1;
      if (_tabStyle == FAST_TAB)
      {
        if (tabCount == 1)
	{
          // this tabbing method is a little different than the standard
          // readline or bash shell tabbing, we always trigger on a single
          // tab and always show all the possible completions for any
          // multiple matches
          if (strlen(command) == 0)
	  {
            // nothing typed, just TAB, show all registered TAB completions
            showTabCompletions(_tabCompletions, _numTabCompletions, prompt_);
	  }
          else
	  {
            // partial word typed, show all possible completions
            findTabCompletions(command);
            if (_numTabMatches == 1)
	    {
              // only one possible completion, show it
              clearLine(cursorPos, command);
              cursorPos = showCommand(command, "%s ", _tabMatches[0]);
	    }
            else if (_numTabMatches > 1)
	    {
              // multiple possible matches, fill out longest match and
              // then show all other possibilities
              clearLine(cursorPos, command);
              cursorPos = showCommand(command, findLongestMatch(command));
              showTabCompletions(_tabMatches, _numTabMatches, "%s%s", prompt_, command);
	    }
	  }
	}
      }
      else  // BASH_TAB
      {
        // this code below implements the more standard readline/bash double tabbing method 
        if (tabCount == 2)
	{
          if (strlen(command) == 0)
	  {
            // nothing typed, just a double TAB, show all registered TAB completions
            showTabCompletions(_tabCompletions, _numTabCompletions, prompt_);
	  }
          else
	  {
            // partial word typed, double TAB, show all possible completions
	    findTabCompletions(command);
            showTabCompletions(_tabMatches, _numTabMatches, "%s%s", prompt_, command);
	  }
	}
        else if ((tabCount == 1) && (strlen(command) > 0))
	{
          // partial word typed, single TAB, fill out as much
          //  as we can and show any possible other matches
          findTabCompletions(command);
          if (_numTabMatches == 1)
	  {
            // we only have one completion, show it
            clearLine(cursorPos, command);
            cursorPos = showCommand(command, "%s ", _tabMatches[0]);
	  }
          else if (_numTabMatches > 1)
	  {
            // multiple completions, find the longest match and show up to that
            clearLine(cursorPos, command);
            cursorPos = showCommand(command, findLongestMatch(command));
	  }
	}
      }
    }
    else if (ch == 127)
    {
      // backspace delete
      backspaceDelete(cursorPos, command);
    }
    else if (ch == 1)
    {
      // home, go to beginning of line
      beginningOfLine(cursorPos, command);
    }
    else if (ch == 5)
    {
      // end, go to end of line
      endOfLine(cursorPos, command);
    }
    else if (ch != 9)
    {
      // don't print out tab if multi keyword command
      //writeString("\nchar value: %d" % ch)
      //writeString("\n"+prompt_)
    }
  }
}

/******************************************************************************/
/******************************************************************************/
int main(int argc, char *argv[])
{
  //#if 0
  char *input = NULL;
  char ch;
  addTabCompletion("quit");
  addTabCompletion("help");
  addTabCompletion("hello");
  addTabCompletion("world");
  addTabCompletion("enhancedUsage");
  addTabCompletion("keepAlive");
  addTabCompletion("pshellAggregatorDemo");
  addTabCompletion("pshellControlDemo");
  addTabCompletion("pshellReadlineDemo");
  addTabCompletion("pshellServerDemo");
  addTabCompletion("myComm");
  addTabCompletion("myCommand123");
  addTabCompletion("myCommand456");
  addTabCompletion("myCommand789");

  setIdleSessionTimeout(ONE_SECOND*5);
  
  while (!isSubString(input, "quit"))
  {
    //ch = getChar();
    //printf("char: %d\n", int(ch));
    input = getInput("prompt> ");
    printf("input: '%s'\n", input);
  }
  //#endif
  return (0);
}
