/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

// Function prototypes
void openLog();
void closeLog();
void writeLog(int logLvl, char *logStr, int stdErr);
void handleError(int logLvl, char *logStr, int exitCode, int exitWeb, int stdErr);
int checkFile(char *fileName, int perms);
FILE *openFile(char *fileName, char *mode);
long int getFileSize(char *fileName);
void file2buf(char *buf, FILE *fp, int fsize);
void getRand(int lower, int upper, int dp, char *res, int *resI);
void timeNow(char *dt, int aMins);
void stripMLLP(char *hl7msg);
void wrapMLLP(char *hl7msg);
int getHL7Field(char *hl7msg, char *seg, int field, char *res);
void hl72unix(char *msg, int onlyPrint);
void hl72web(char *msg);
void unix2hl7(char *msg);
FILE *findTemplate(char *fileName, char *tName, int isRespond);
char *str2base64(const char *message);
void escapeSlash(char *dest, char *src);
void printChars(char *buf);
char *dblBuf(char *buf, int *bufS, int reqS);
void emptyFifo(int fd);
