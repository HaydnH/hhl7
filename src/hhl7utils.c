/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <math.h>
#include <dirent.h>
#include "hhl7extern.h"


// DEBUG function to show ascii character codes in a buffer, useful for seeing hidden chars
/*
void printChars(char *buf) {
  char ch = 32;
  for (long unsigned int c = 0; c < strlen(buf); c++) {
    ch = 32;
    if (buf[c] > 32 && buf[c] < 127) ch = buf[c];
    printf("%d (%c)\n", (int) buf[c], ch);
  }
}
*/


// Open log file for reading, wrapper for syslog openlog()
void openLog() {
  int logLevel = 6; // Default log level is LOG_INFO (6, see syslog.h)
  if (globalConfig) {
    if (globalConfig->logLevel >= 0) logLevel = globalConfig->logLevel;
  }

  openlog("HHL7", LOG_NDELAY, LOG_USER);
  setlogmask(LOG_UPTO(logLevel));
}


// Close syslog log files, wrapper for syslog closelog() in case we want to add close code
void closeLog() {
  closelog();
}


// Write to the logs, either syslog or stderr if not running as daemon
// See sys/syslog.h for int <-> log level values
void writeLog(int logLvl, char *logStr, int stdErr) {
  static const char logLevels[8][9] = { "[EMERG] ", "[ALERT] ", "[CRIT]  ", "[ERROR] ",
                                        "[WARN]  ", "[NOTIC] ", "[INFO]  ", "[DUBUG] " };
  if (isDaemon == 1) {
    syslog(logLvl, "%s%s", logLevels[logLvl], logStr);
  } else if (stdErr == 1) {
    if (globalConfig) {
      if (logLvl <= globalConfig->logLevel) {
        fprintf(stderr, "%s%s\n", logLevels[logLvl], logStr);
      }
    } else {
      fprintf(stderr, "%s%s\n", logLevels[logLvl], logStr);
    }
  }
}


// Handle an error
void handleError(int logLvl, char *logStr, int exitCode, int exitWeb, int stdErr) {
  writeLog(logLvl, logStr, stdErr);
  if (exitCode >= 0 && webRunning == 1 && exitWeb == 1) exit(exitCode);
  if (exitCode >= 0 && webRunning == 0) exit(exitCode);
}


// Check if file exists (F_OK=0) and is writable (W_OK=2), readable (R_OK=4)
// or exectuable (X_OK=1)
int checkFile(char *fileName, int perms) {
  if (access(fileName, perms) == 0) {
    return(0);
  } else {
    return(1);
  }
}


// Check if we can open a file for reading and return FP, (Modes; r, w etc)
FILE *openFile(char *fileName, char *mode) {
  FILE *fp;
  char errStr[282] = "";

  fp = fopen(fileName, mode);
  if (fp == NULL) {
    sprintf(errStr, "Cannot open file: %s\n", fileName);
    writeLog(LOG_ERR, errStr, 1);
    return(NULL);
  }
  return(fp);
}


// Get the size of a file
long int getFileSize(char *fileName) {
  struct stat st;
  stat(fileName, &st);
  return(st.st_size);
}


// Load a file in to a buffer
void file2buf(char *buf, FILE *fp, long int fsize) {
  char *tbuf = malloc(fsize + 1);

  if (fsize != (long) fread(tbuf, 1, fsize, fp)) {
    free(tbuf);
    tbuf = NULL;
    fprintf(stderr, "ERROR: Cannot read file to buffer\n");
  }
  tbuf[fsize] = '\0';
  strcpy(buf, tbuf);
  free(tbuf);
}


// Return a random number between 2 values to the same number of decimal places as input
void getRand(int lower, int upper, int dp, char *res, int *resInt, float *resF) {
  float tmpF;
  
  // Add 0s to the upper/lower values to allow decimal places
  lower = lower * pow(10, dp);
  if (dp == 0) {
    upper = (upper + 1) * pow(10, dp);
  } else {
    upper = (upper) * pow(10, dp);
  }

  // Create the random number and remove 0s to add decimal places
  tmpF = (float) (rand() % (upper - lower) + lower);
  tmpF = tmpF / pow(10, dp);

  // Return the final result to the correct DPs
  if (dp == 0) {
    *resInt = (int) tmpF;
    if (res) sprintf(res, "%d", *resInt);
    if (resF) *resF = tmpF;
  }  else {
    sprintf(res, "%.*f", dp, tmpF);
    *resF = tmpF;
  }
}


// Get the current time on yyymmddhhmmss.ms+tz format
void timeNow(char *dt, int aMins) {
  time_t t = time(NULL) + (aMins * 60);
  struct tm *tm = localtime(&t);

  strftime(dt, 26, "%Y%m%d%H%M%S", tm);
}


// Strip MLLP parts of packet
void stripMLLP(char *hl7msg) {
  int msgLen = strlen(hl7msg);
  int m = 0, s = 0;
  char tmpBuf[strlen(hl7msg)];

  for (m = 0; m < msgLen; m++) {
    if (hl7msg[m] == (char) 11 || hl7msg[m] == (char) 28) {
      s++;

    } else {
      tmpBuf[m - s] = hl7msg[m];
    }
  }
  tmpBuf[m - s] = '\0';
  strcpy(hl7msg, tmpBuf);
}


// Add MLLP wrapper to packet
// NOTE: No need to realloc hl7msg, the +4b is handled in dblBuf()'s +10b safety
void wrapMLLP(char *hl7msg) {
  if (hl7msg != NULL) {
    char tmpBuf[strlen(hl7msg) + 4];

    // Wrap hl7msg in an ASCII 11 (SOB) & 28 (EOB) and null terminate
    sprintf(tmpBuf, "%c%s%c%c", 11, hl7msg, 28, 13);

    strcpy(hl7msg, tmpBuf);
  }
}


// Find the value of a HL7 segment/field 
int getHL7Field(char *hl7msg, char *seg, int field, char *res) {
  int msgLen = strlen(hl7msg), segLen = strlen(seg);
  int m = 0, s = 0, f = 0, fc = 0, sFound = 0, fFound = 0;
  char sBuf[segLen + 1], errStr[48] = "";
 
  // Decrement field ID by one for MSH special case
  if (strcmp(seg, "MSH") == 0) field = field - 1;

  for (m = 0; m < msgLen; m++) {
    if ((hl7msg[m] == '\r' || hl7msg[m] == '\n') && hl7msg[m-1] != '\\') {
      s = 0;
      fc = 0;

    } else if (s < segLen) {
      sBuf[s] = hl7msg[m];

        if (s == segLen - 1) {
        sBuf[s + 1] = '\0';
        if (strcmp(sBuf, seg) == 0) {
          sFound = 1;
        }
      }
      s++;

    } else if (s == segLen && sFound == 0) {
      continue;
   
    } else if (hl7msg[m] == '|' && hl7msg[m - 1] != '\\') {
      fc++;
      if (sFound == 1 && fc == field) {
        fFound = 1;
      } else if (fFound == 1) {
        res[f] = '\0';
        return(0);

      }
    } else if (fFound == 1) {
      res[f] = hl7msg[m];
      f++;

    }
  }

  sprintf(errStr, "Failed to find field %d in segment %s", field, seg);
  handleError(LOG_WARNING, errStr, -1, 0, 1);
  return(1);
}


// Get number of \n or \r's in a string
long unsigned int numLines(const char *buf) {
  long unsigned int i = 0, c = 0;

  for (i = 0; i < strlen(buf); i++) {
    if (buf[i] == '\n' || buf[i] == '\r') c++;
  }
  return(c);
}


// Update a start and end count for a line in a file
void findLine(char *buf, long int dataSize, int lineNum, int *start, int *lineLen) {
  int i = 0, curLine = 1, startFound = 0, endFound = 0;

  if (lineNum == 1) {
    *start = 0;
    startFound = 1;
  }

  while (startFound + endFound < 2) {
    if (buf[i] == '\n') {
      if (startFound == 0 && curLine == (lineNum - 1)) {
        *start = i + 1;
        startFound = 1;

      } else if (startFound == 1 && endFound == 0) {
        *lineLen = i - *start;
        endFound = 1;

      } else {
        curLine++;

      }
    }

    if (i >= dataSize) {
      startFound = 1;
      endFound = 1;
      *lineLen = dataSize - *start;
    }
    i++;
  }
}


// Convert a HL7 message to a unix format (i.e: /r -> /n)
void hl72unix(char *msg, int onlyPrint) {
  long unsigned int c, ignore = 0;
  char msgUnix[strlen(msg) + 1];
  for (c = 0; c < strlen(msg); c++) {
    if (c == 0 && msg[c] == 13) {
      ignore++;
    } else if (msg[c] == 13) {
      msgUnix[c - ignore] = '\n';
    } else if (msg[c] == 10 || msg[c] == 11 || msg[c] == 28) {
      ignore++;
    } else {
      msgUnix[c - ignore] = msg[c];
    }
  }
  msgUnix[c - ignore] = '\0';

  if (onlyPrint == 1) {
    printf("%s", msgUnix);
  } else {
    strcpy(msg, msgUnix);
  }
}


// Convert a unix HL7 message to a hl7 format (i.e: /n -> /r)
void unix2hl7(char *msg) {
  long unsigned int c, ignore = 0;
  char msgUnix[strlen(msg) + 1];
  for (c = 0; c < strlen(msg); c++) {
    if (msg[c] == 10) {
      msgUnix[c - ignore] = '\r';
    } else if (msg[c] == 13 || msg[c] == 11 || msg[c] == 28) {
      ignore++;
    } else {
      msgUnix[c - ignore] = msg[c];
    }
  }
  msgUnix[c - ignore] = '\0';
  strcpy(msg, msgUnix);
}


// Change a hl7 message to web, \r -> <br />
// NOTE: sendHL72Web() handles additional memory allocation for the added <br />'s
void hl72web(char *msg, int maxSize) {
  char res[maxSize];
  long unsigned int i, strLen = 0;
  res[0] = '\0';

  for (i = 0; i < strlen(msg); i++) {
    if (msg[i] == '\r') {
      res[strLen] = '\0';
      strcat(res, "<br />");
      strLen = strLen + 6;

    } else {
      res[strLen] = msg[i];
      strLen++;
    }
  }

  res[strLen + 1] = '\0';
  strcpy(msg, res);
}


// Find fullpath of a config file
int findConfFile(char *confFile) {
  FILE *fp;
  int p = 0;
  const char *homeDir = getenv("HOME");
  char cFile[3][75] = { "./conf/config.hhl7",
                        "/.config/hhl7/conf/config.hhl7",
                        "/usr/local/hhl7/conf/config.hhl7" };

  if (strlen(homeDir) > 38) {
    handleError(LOG_WARNING, "Linux username longer than allowed (>32 chars)", 0, 0, 1);
    return(2);
  }

  sprintf(cFile[1], "%s/.config/hhl7/conf/config.hhl7", homeDir);

  for (p = 0; p < 3; p++) {
    // If we can open the file for reading, update confFile & return 0
    fp = fopen(cFile[p], "r");
    if (fp != NULL) {
      sprintf(confFile, "%s", cFile[p]);
      fclose(fp);
      return(0);
    }
  }
  return(1); 
}


// Find fullpath of a json template file
FILE *findTemplate(char *fileName, char *tName, int isRespond) {
  FILE *fp;
  int p = 0;
  char errStr[282] = "";
  const char *homeDir = getenv("HOME");
  char tPath[13] = "templates/";
  if (isRespond == 1) sprintf(tPath, "responders/");
  char tPaths[3][18] = { "./", "/.config/hhl7/", "/usr/local/hhl7/" };

  if (isDaemon == 1) {
    sprintf(fileName, "%s%s%s%s", tPaths[2], tPath, tName, ".json");
    fp = fopen(fileName, "r");
    if (fp != NULL) {
      return(fp);
    }

  } else {
    // Create the full file path/name of the found template location
    for (p = 0; p < 3; p++) {
      if (p == 1) {
        if (fileName)
          sprintf(fileName, "%s%s%s%s%s", homeDir, tPaths[p], tPath, tName, ".json");
      } else {
        if (fileName)
          sprintf(fileName, "%s%s%s%s", tPaths[p], tPath, tName, ".json");
      }

      // If we can open the file for reading, return the file pointer
      fp = fopen(fileName, "r");
      if (fp != NULL) {
        return(fp);
      }
    }
  }

  // Error if no template found
  if (fp == NULL) {
    sprintf(errStr, "Failed to find template: %s", tName);
    handleError(LOG_ERR, errStr, 1, 0, 1);
  }

  // We should never get here, but it's included to avoid compiler warnings
  return(fp);
}


// Escape slashes in a buffer
void escapeSlash(char *dest, char *src) {
  long unsigned int s, d = 0;
  for (s = 0; s < strlen(src); s++) {
    if (src[s] == '\0') {
      break;

    } else if (src[s] == '\\') {
      dest[s + d] = '\\';
      d++;
      dest[s + d] = '\\';

    } else {
      dest[s + d] = src[s];
    }
  }

  dest[s + d] = '\0';
}


// Double the memory allocation for a buffer
char *dblBuf(char *buf, int *bufS, int reqS) {
  char *tmpPtr;
  int oldSize = *bufS;

  // While the buffer size is less than the required size, double size + 10b safety
  while (*bufS < reqS) {
    *bufS = *bufS * 2;
  }
  *bufS = *bufS + 10;

  // realloc memory to the new size
  tmpPtr = realloc(buf, *bufS);
  if (tmpPtr == NULL) {
    handleError(LOG_ERR, "dblBuf() failed to allocate memory - server OOM??", 1, 0, 1);
    free(buf);
    *bufS = oldSize;

  } else {
    buf = tmpPtr;
  }
  return(buf);
}


// Check if a string is valid vanilla ascii and correct length
int validStr(char *buf, long unsigned int minL, long unsigned int maxL, int aCheck) {
  long unsigned int i;

  if (strlen(buf) < minL || strlen(buf) > maxL) {
    return(1);

  } else if (aCheck == 1) {
    for (i = 0; i < strlen(buf); i++) {
      if (buf[i] < 32 || buf[i] > 126) return(2);
    }
  }
  return(0);
}
