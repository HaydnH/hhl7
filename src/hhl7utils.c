/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include "hhl7extern.h"


// DEBUG function to show ascii characters in a buffer, useful for seeing hidden chars
void printChars(char *buf) {
  for (int c =0; c < strlen(buf); c++) {
    printf("%d\n", (int) buf[c]);
  }
}


// TODO - add log files here...
// TODO - grep code for this function and create error code list, currently all fail = 1
// TODO - maybe send error level (warn, error etc) for processsing?
// Handle any error messages
void handleErr(char* message, int eCode, FILE* fStream) {
  if (webRunning == 0) {
    fprintf(fStream, "%s [E: %d]\n", message , eCode);
    if (eCode >= 0) exit(eCode);

  } else {
    // Only send the first web error.
    if (strlen(webErrStr) == 0) {
      sprintf(webErrStr, "%s. [E: %d]", message, eCode);
    }
  }
}


// Check if we can open a file for reading and return FP
FILE *openFile(char* fileName) {
  FILE *fp;
  fp = fopen(fileName, "r");
  if (fp == NULL) {
    fprintf(stderr, "ERROR: Cannot open file: %s\n", fileName);
    exit(1);
  }
  return fp;
}


// Get the size of a file
long int getFileSize(char* fileName) {
  struct stat st;
  stat(fileName, &st);
  return st.st_size;
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


// Get the current time on yyymmddhhmmss.ms+tz format
// TODO - add support for millisenconds and timezone??
void timeNow(char *dt) {
  time_t t =time(NULL);
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
// TODO - is hl7msg a fixed length? May need to realloc it to grow by 3 
void wrapMLLP(char *hl7msg) {
  char tmpBuf[strlen(hl7msg) + 4];

  // Wrap hl7msg in an ASCII 11 (SOB) & 28 (EOB) and null terminate
  sprintf(tmpBuf, "%c%s%c%c%c", 11, hl7msg, 28, 13, '\0');

  strcpy(hl7msg, tmpBuf);
}


// TODO - let rBuf expand if field size > 255
void getHL7Field(char *hl7msg, char *seg, int field, char *res) {
  int msgLen = strlen(hl7msg), segLen = strlen(seg);
  int m = 0, s = 0, f = 0, fc = 0, sFound = 0, fFound = 0;
  char sBuf[segLen + 1];
  //char rBuf[255];
 
  for (m = 0; m < msgLen; m++) {
    if (hl7msg[m] == '\r' && hl7msg[m-1] != '\\') {
      if (sFound == 1) {
        fprintf(stderr, "ERROR: Failed to find field %d in segment %s\n", field, seg);
        exit (1); 
      }
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

    } else if (s == segLen  && sFound == 0) {
      continue;
   
    } else if (hl7msg[m] == '|' && hl7msg[m - 1] != '\\') {
      fc++;
      if (sFound == 1 && fc == field) {
        fFound = 1;
      } else if (fFound == 1) {
        res[f] = '\0';
        break;

      }
    } else if (fFound == 1) {
      res[f] = hl7msg[m];
      f++;

    }
  }
}


// Get number of \n's in a string
int numLines(char *buf) {
  int c = 0;
  for (int i = 0; i < strlen(buf); i++) {
    if (buf[c] == '\n')
      c++;
  }
  return c;
}


// Convert a HL7 message to a unix format (i.e: /r -> /n)
void hl72unix(char *msg) {
  int c, ignore = 0;
  char msgUnix[strlen(msg)+1];
  for (c = 0; c < strlen(msg); c++) {
    if (msg[c] == 13) {
      msgUnix[c - ignore] = '\n';
    } else if (msg[c] == 10 || msg[c] == 11 || msg[c] == 28) {
      ignore++;
    } else {
      msgUnix[c - ignore] = msg[c];
    }
  }
  msgUnix[c - ignore] = '\0';
  strcpy(msg, msgUnix);
  //printf("%s", msgUnix);
}


// Convert a unix HL7 message to a hl7 format (i.e: /n -> /r)
void unix2hl7(char *msg) {
  int c, ignore = 0;
  char msgUnix[strlen(msg)+1];
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
  //printf("%s", msgUnix);
}


// Change a hl7 message to web, \r -> <br />
void hl72web(char *msg) {
  // TODO need to realloc msg AND res once it's malloc'd 
  char res[1224] = "";
  char *tokState;
  char* token = strtok_r(msg, "\r", &tokState);

  while (token != NULL) {
    strcat(res, token);
    strcat(res, "<br />");
    token = strtok_r(NULL, "\r", &tokState);

  }
  strcpy(msg, res);
}


// Find fullpath of a json template file
FILE *findTemplate(char* fileName, char* tName) {
  FILE *fp;
  int p = 0;
  const char *homeDir = getenv("HOME");
  char tPaths[3][33] = { "/.config/hhl7/templates/",
                         "./conf/templates/",
                         "/usr/local/share/hhl7/templates/" };

  // Create the full file path/name of the found template location
  for (p = 0; p < 3; p++) {
    if (p == 0) {
      strcpy(fileName, homeDir);
      strcat(fileName, tPaths[p]);
    } else {
      strcpy(fileName, tPaths[p]);
    }
    strcat(fileName, tName);
    strcat(fileName, ".json");

    // If we can open the file for reading, return the file pointer
    fp = fopen(fileName, "r");
    if (fp != NULL) {
      return fp;
    }
  }

  // Error if no template found
  if (fp == NULL) {
    fprintf(stderr, "ERROR: Cannot find template: %s\n", tName);
    exit(1);
  }

  // We should never get here, but it's included to avoid compiler warnings
  return fp;

}


// Encoding for web certs
char *str2base64(const char *message) {
  const char *lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned long l;
  size_t i, j;
  char *tmp;
  size_t length = strlen (message);

  tmp = malloc(length * 2 + 1);
  if (NULL == tmp)
    return NULL;
  j = 0;
  for (i = 0; i < length; i += 3) {
    l = (((unsigned long) message[i]) << 16)
        | (((i + 1) < length) ? (((unsigned long) message[i + 1]) << 8) : 0)
        | (((i + 2) < length) ? ((unsigned long) message[i + 2]) : 0);

    tmp [j++] = lookup[(l >> 18) & 0x3F];
    tmp [j++] = lookup[(l >> 12) & 0x3F];

    if (i + 1 < length)
      tmp [j++] = lookup[(l >> 6) & 0x3F];
    if (i + 2 < length)
      tmp [j++] = lookup[l & 0x3F];
  }

  if (0 != length % 3)
    tmp [j++] = '=';
  if (1 == length % 3)
    tmp [j++] = '=';

  tmp [j] = 0;

  return tmp;
}


// Escape slashes in a buffer
void escapeSlash(char *dest, char *src) {
  int s, d = 0;
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

  // While the buffer size is less than the required size, double size
  while (*bufS < reqS) {
    *bufS = *bufS * 2;
  }

  // realloc memory to the new size
  tmpPtr = realloc(buf, *bufS);
  if (tmpPtr == NULL) {
    // TODO - add error message on memor allocation failure
    free(buf);
    *bufS = oldSize;

  } else {
    buf = tmpPtr;
  }
  return buf;
}

