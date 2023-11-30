/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <getopt.h>
#include <time.h>
#include <microhttpd.h>
#include "hhl7extern.h"
#include "hhl7net.h"
#include "hhl7utils.h"
#include "hhl7web.h"


// Check if a port is valid (ret 0=valid, 1=string too long, 2=out of range)
int validPort(char *port) {
  int portLen = strlen(port);
  int portNum = atoi(port);

  if (portLen < 4 || portLen > 5) {
    return 1;

  } else if (portNum < 1024 || portNum > 65535) {
    return 2;

  }
  return 0;
}


// Connect to server on IP:Port
int connectSvr(char *ip, char *port) {
  int sockfd = -1;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  // TODO - this may need malloc or resize due to variables

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
    sprintf(infoStr, "Can't obtain address info: %s", gai_strerror(rv));
    handleError(LOG_ERR, infoStr, -1, 0, 1);
    return(-1);
  }

  // Loop through results and try to connect
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
      p->ai_protocol)) == -1) {
      continue;
    }
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      continue;
    }
    sprintf(infoStr, "Connected to server %s on port %s", ip, port);
    writeLog(LOG_INFO, infoStr, 1);
    break;
  }

  if (p == NULL) {
    sprintf(infoStr, "Failed to connect to server %s on port %s", ip, port);
    handleError(LOG_ERR, infoStr, -1, 0, 1);
    return(-1);
  }

  freeaddrinfo(servinfo);
  return(sockfd);
}


// Listen for ACK from server
int listenACK(int sockfd, char *res) {
  char ackBuf[256], app[12], code[12], aCode[3];
  int ackErr = 0, recvL = 0;

  // TODO Add timeout to config file
  // Set ListenACK timeout
  struct timeval tv, *tvp;
  tv.tv_sec = 3;
  tv.tv_usec = 0;
  tvp = &tv;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, tvp, sizeof tv);

  writeLog(LOG_INFO, "Listening for ACK...", 1);

  // Receive the response from the server and strip MLLP wrapper
  // TODO - change 256: malloc a big buffer, read 256, check if complete ack, expand if needed
  if ((recvL = recv(sockfd, ackBuf, 256, 0)) == -1) {
    handleError(LOG_ERR, "Timeout listening for ACK response", 1, 0, 1);
    return -1;

  } else {
    stripMLLP(ackBuf);
    ackBuf[strlen(ackBuf) - 1] = '\0';

    // Find the ack response in MSA.1
    getHL7Field(ackBuf, "MSA", 1, aCode);

    // Process the code, either AA, AE, AR, CA, CE or CR.
    if ((char) aCode[0] == 'A') {
      strcpy(app, "Application");
    } else if ((char) aCode[0] == 'C') {
      strcpy(app, "Commit");
    } else {
      ackErr = 1;
    }

    if ((char) aCode[1] == 'A') {
      strcpy(code, "Accept");
    } else if ((char) aCode[1] == 'E') {
      strcpy(code, "Error");
    } else if ((char) aCode[1] == 'R') {
      strcpy(code, "Reject");
    } else {
      ackErr = 2;
    }

    // Print error or response
    if (ackErr > 0) {
      handleError(LOG_ERR, "Could not understand ACK response", 1, 0, 1);
      sprintf(res, "%s", "EE");
      return -1;

    } else {
      sprintf(infoStr, "Server ACK response: %s %s (%s)", app, code, aCode);
      writeLog(LOG_INFO, infoStr, 1);

      if (res) {
        strncpy(res, aCode, 2);
        res[3] = '\0';
      }
    }
    hl72unix(ackBuf, 0);
    // TODO - add option to print ACK response
    //printf("ACK:\n%s\n\n", ackBuf);

  }
  return recvL;
}


// Send a file over socket
void sendFile(FILE *fp, long int fileSize, int sockfd) {
  char fileData[fileSize + 4];
  char resStr[3] = "";

  // Read file to buffer
  size_t newLen = fread(fileData, sizeof(char), fileSize, fp);
  if (ferror(fp) != 0) {
    handleError(LOG_ERR, "Could not read data file to send", 1, 0, 1);

  } else {
    fileData[newLen++] = '\0';
  }

  fclose(fp);

  // Send the data file to the server
  unix2hl7(fileData);
  sendPacket(sockfd, fileData, resStr);
}


// Send a string packet over socket
int sendPacket(int sockfd, char *hl7msg, char *resStr) {
  const char msgDelim[6] = "MSH|";
  char tokMsg[strlen(hl7msg) + 5];
  char *curMsg = hl7msg, *nextMsg;
  int msgCount = 0;
 
  while (curMsg != NULL) {
    msgCount++;
    nextMsg = strstr(curMsg + 1, msgDelim);

    if (nextMsg == NULL) { 
      sprintf(tokMsg, "%s", curMsg);
      curMsg = NULL;
    } else {
      snprintf(tokMsg, strlen(curMsg) - strlen(nextMsg), "%s", curMsg);
      curMsg = nextMsg;
    }

    // Send the data file to the server
    sprintf(infoStr, "Sending HL7 message %d to server", msgCount);
    writeLog(LOG_INFO, infoStr, 1);

    // Add MLLP wrappre to this message
    wrapMLLP(tokMsg);

    // Send the message to the server
    if(send(sockfd, tokMsg, strlen(tokMsg), 0) == -1) {
      handleError(LOG_ERR, "Could not send data packet to server", -1, 0, 1);
      return -1;

    } else {
      return listenACK(sockfd, resStr);

    }
  }
  return 0;
}


// Send and ACK after receiving a message
// TODO - rewrite to use json template? Performance vs portability (e.g: FIX)
int sendAck(int sessfd, char *hl7msg) {
  // TODO - control id seems long at 201 characters, check hl7 spec for max length
  char dt[26] = "", cid[201] = "";
  char ackBuf[1024];
  int writeL = 0;

  // Get current time and control ID of incomming message
  timeNow(dt, 0); 
  getHL7Field(hl7msg, "MSH", 9, cid);
  if (strlen(cid) == 0) sprintf(cid, "%s", "<UNKNOWN>");

  sprintf(ackBuf, "%c%s%s%s%s%s%s%s%c%c", 0x0B, "MSH|^~\\&|||||", dt, "||ACK|", cid,
                                          "|P|2.4|\rMSA|AA|", cid, "|OK|", 0x1C, 0x0D);

  if ((writeL = write(sessfd, ackBuf, strlen(ackBuf))) == -1) {
    close(sessfd);
    handleError(LOG_ERR, "Failed to send ACK response to server", -1, 0, 1);
    return -1;

  } else {
    sprintf(infoStr, "Message with control ID %s received OK and ACK sent", cid);
    writeLog(LOG_INFO, infoStr, 1);
  }
  return writeL;
}


// Handle an incomming message
static void handleMsg(int sessfd, int fd) {
  int readSize = 512, msgSize = 0, maxSize = readSize + msgSize, rcvSize = 1;
  int msgCount = 0, ignoreNext = 0, webErr = 0;
  int *ms = &maxSize;
  char rcvBuf[readSize+1];
  char *msgBuf = malloc(maxSize);
  char writeSize[11];
  msgBuf[0] = '\0';

  while (rcvSize > 0) {
    rcvSize = read(sessfd, rcvBuf, readSize);
    rcvBuf[rcvSize] = '\0';

    if (ignoreNext == 1) {
      ignoreNext = 0;

    } else {
      if (rcvSize == -1 && msgCount == 0) {
        handleError(LOG_ERR, "Failed to read incomming message from server", 1, 0, 1);
        webErr = 1;
      }

      if (rcvSize > 0 && rcvSize <= readSize) {
        msgSize = msgSize + rcvSize;
        if ((msgSize + 1) > maxSize) msgBuf = dblBuf(msgBuf, ms, msgSize);
        sprintf(msgBuf, "%s%s", msgBuf, rcvBuf);

        // Full hl7 message received, handle the msg
        if (rcvBuf[rcvSize - 2] == 28 || rcvBuf[rcvSize - 1] == 28) {
          // If the EOB character is the last in the message, there is still a CR to
          // receive which we can ignore
          if (rcvBuf[rcvSize - 1] == 28) ignoreNext = 1;
        
          stripMLLP(msgBuf);
          if (sendAck(sessfd, msgBuf) == -1) webErr = 1;
          msgCount++;

          if (webRunning == 1) {
            if (webErr > 0) {
              sprintf(msgBuf, "ERROR: The backend failed to receive or process a message from the sending server");
              // TODO  WORKING - rework how errors get sent to web listener
              //  strcpy(msgBuf, webErrStr);
            }

            sprintf(writeSize, "%d", (int) strlen(msgBuf));
            if (write(fd, writeSize, 11) == -1) {
              handleError(LOG_ERR, "ERROR: Failed to write to named pipe", 1, 0, 1);
            }

            if (write(fd, msgBuf, strlen(msgBuf)) == -1) {
              handleError(LOG_ERR, "ERROR: Failed to write to named pipe", 1, 0, 1);
            }

          } else {
             hl72unix(msgBuf, 1);
          }

          // Clean up counters and buffers
          msgSize = 0;
          msgBuf[0] = '\0';
        }
      }
    }

    rcvBuf[rcvSize - 1] = '\0';
    rcvBuf[0] = '\0';

  }

  free(msgBuf);
  close(sessfd);
}


// Listen for incomming messages
static int createSession(char *ip, const char *port) {
  int svrfd, rv;
  struct addrinfo hints, *res = 0;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol=0;
  hints.ai_flags=AI_PASSIVE|AI_ADDRCONFIG;

  if ((rv = getaddrinfo(ip, port, &hints, &res)) != 0) {
    sprintf(infoStr, "Can't obtain address info: %s", gai_strerror(rv));
    handleError(LOG_ERR, infoStr, 1, 0, 1);
    return -1;
  }

  svrfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

  if (svrfd == -1) {
    handleError(LOG_ERR, "Can't obtain socket address info", 1, 0, 1);
    return -1;
  }

  int reuseaddr = 1;
  if (setsockopt(svrfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
    handleError(LOG_ERR, "Can't set socket options", 1, 0, 1);
    return -1;
  }

  if (bind(svrfd, res->ai_addr, res->ai_addrlen) == -1) {
    handleError(LOG_ERR, "Can't bind address", 1, 0, 1);
    return -1;
  }

  if (listen(svrfd, SOMAXCONN)) {
    handleError(LOG_ERR, "Can't listen for connections", 1, 0, 1);
    return -1;
  }

  freeaddrinfo(res);
  return svrfd;
}


// Start listening for incomming messages
int startMsgListener(char *ip, const char *port) {
  int svrfd = 0, sessfd = 0;
  int fd = 0;

  if ((svrfd = createSession(ip, port)) == -1) return -1;

  // Create a named pipe to write to
  char hhl7fifo[21]; 
  sprintf(hhl7fifo, "%s%d", "/tmp/hhl7fifo.", getpid());
  mkfifo(hhl7fifo, 0666);
  fd = open(hhl7fifo, O_WRONLY | O_NONBLOCK);

  for (;;) {
    sessfd = accept(svrfd, 0, 0);
    if (sessfd == -1) {
      handleError(LOG_ERR, "Can't accept connections", 1, 0, 1);
      return -1;
    }

    // Make children ignore waiting for parent process before closing
    signal(SIGCHLD, SIG_IGN);

    pid_t pidBefore = getpid();
    pid_t pid=fork();
    if (pid == -1) {
      handleError(LOG_ERR, "Can't fork a child process for listener", 1, 0, 1);
      return -1;

    }  else if (pid == 0) {
      // Check for parent exiting and repeat
      int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
      if (r == -1 || getppid() != pidBefore) {
        exit(0);
      }

      handleMsg(sessfd, fd);
      close(sessfd);
      _exit(0);

    } else {
      close(sessfd);

    }
  }
  return 0;
}
