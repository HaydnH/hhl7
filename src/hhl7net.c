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


// Connect to server on IP:Port
int connectSvr(char *ip, char *port) {
  int sockfd = -1;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  // TODO - this may need malloc or resize due to variables
  char eBuf[256] = "";

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
    sprintf(eBuf, "ERROR: Can't obtain address info: %s", gai_strerror(rv));
    handleErr(eBuf, 1, stderr);
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
    fprintf(stderr, "INFO:  Connected to server %s on port %s\n", ip, port);
    break;
  }

  if (p == NULL) {
    sprintf(eBuf, "ERROR: Failed to connect to server %s on port %s", ip, port);
    handleErr(eBuf, 1, stderr);
    return(-1);
  }

  freeaddrinfo(servinfo);
  return(sockfd);
}

// Listen for ACK from server
void listenACK(int sockfd, char *res) {
  char ackBuf[256], app[12], code[12], aCode[3];
  int ackErr = 0;
  char eBuf[53];

  if (webRunning == 0) {
    fprintf(stderr, "INFO:  Listening for ACK...\n");
  }

  if (strlen(webErrStr) == 0) {
    // Receive the response from the server and strip MLLP wrapper
    // TODO - change 256: malloc a big buffer, read 256, check if complete ack, expand if needed
    recv(sockfd, ackBuf, 256, 0);
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
      sprintf(eBuf, "ERROR: Client could not understand ACK response: %s", aCode);
      handleErr(eBuf, 1, stderr);

    } else {
      if (webRunning == 0) {
        fprintf(stderr, "INFO:  Server response: %s %s (%s)\n", app, code, aCode);
      }

      if (res) {
        strncpy(res, aCode, 2);
        res[3] = '\0';
      }
    }
    hl72unix(ackBuf);
    // TODO - add option to print ACK response
    //printf("ACK:\n%s\n\n", ackBuf);
  }
}


// Send a file over socket
void sendFile(FILE *fp, long int fileSize, int sockfd) {
  char eBuf[43];
  char fileData[fileSize + 4];
  char resStr[3] = "";

  // Read file to buffer
  size_t newLen = fread(fileData, sizeof(char), fileSize, fp);
  if (ferror(fp) != 0) {
    sprintf(eBuf, "ERROR: Could not read data file to send");
    handleErr(eBuf, 1, stderr);

  } else {
    fileData[newLen++] = '\0';
  }

  fclose(fp);

  // Send the data file to the server
  unix2hl7(fileData);
  wrapMLLP(fileData);
  sendPacket(sockfd, fileData);
  listenACK(sockfd, resStr);
}


// Send a string packet over socket
void sendPacket(int sockfd, char *hl7msg) {
  char eBuf[44];

  if (strlen(webErrStr) == 0) {
    // Send the data file to the server
    fprintf(stderr, "INFO:  Sending HL7 message to server\n");

    if(send(sockfd, hl7msg, strlen(hl7msg), 0)== -1) {
      sprintf(eBuf, "ERROR: Could not send data packet to server");
      handleErr(eBuf, 1, stderr);
    }
  }
}


// Send and ACK after receiving a message
// TODO - rewrite to use json template? Performance vs portability (e.g: FIX)
void sendAck(int sessfd, char *hl7msg) {
  char dt[26] = "", cid[201] = "";
  char eBuf[46];
  char ackBuf[1024];

  // Get current time and control ID of incomming message
  timeNow(dt); 
  // TODO - if we receive an invalid HL7 msg we can't read MSH-9 so should send reject
  getHL7Field(hl7msg, "MSH", 9, cid);

  sprintf(ackBuf, "%c%s%s%s%s%s%s%s%c%c", 0x0B, "MSH|^~\\&|||||", dt, "||ACK|", cid,
                                          "|P|2.4|\rMSA|AA|", cid, "|OK|", 0x1C, 0x0D);

  if (write(sessfd, ackBuf, strlen(ackBuf)) == -1 ) {
    close(sessfd);
    sprintf(eBuf, "ERROR: Failed to send ACK response to server");
    handleErr(eBuf, 1, stderr);

  } else {
    fprintf(stderr, "INFO:  Message with control ID %s received OK and ACK sent\n\n", cid);
  }

  close(sessfd);
}


// Handle an incomming message
static int handleMsg(int sessfd, int fd) {
  // TODO - change this to a malloc
  char buf[1024];
  char eBuf[57];
  char writeSize[11];

  memset(buf, 0, sizeof(buf));
  if ((read(sessfd, buf, sizeof(buf)) - 1) == -1) {
    sprintf(eBuf, "ERROR: Failed to read incomming HL7 message from server");
    handleErr(eBuf, 1, stderr);

  } else {
    stripMLLP(buf);
    sendAck(sessfd, buf);

    if (webRunning == 1) {
      if (strlen(webErrStr) != 0) {
        strcpy(buf, webErrStr);
      }

      sprintf(writeSize, "%d", (int) strlen(buf));
      if (write(fd, writeSize, 11) == -1) {
        sprintf(eBuf, "ERROR: Failed to write to named pipe");
        handleErr(eBuf, 1, stderr);
      }

      if (write(fd, buf, strlen(buf)) == -1) {
        sprintf(eBuf, "ERROR: Failed to write to named pipe");
        handleErr(eBuf, 1, stderr);
      }


    } else {
      hl72unix(buf);
      printf("%s\n", buf);
    }
  }

  close(sessfd);
  return strlen(buf);
}


// Listen for incomming messages
static int createSession(char *ip, char *port) {
  //const char* ip = 0;
  int svrfd, rv;
  struct addrinfo hints, *res = 0;
  char eBuf[256];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol=0;
  hints.ai_flags=AI_PASSIVE|AI_ADDRCONFIG;

  if ((rv = getaddrinfo(ip, port, &hints, &res)) != 0) {
    fprintf(stderr, "ERROR: Can't obtain address info: %s\n", gai_strerror(rv));
    exit(1);
  }

  svrfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

  if (svrfd == -1) {
    sprintf(eBuf, "ERROR: Can't obtain socket address info");
    handleErr(eBuf, 1, stderr);

  }

  int reuseaddr = 1;
  if (setsockopt(svrfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
    sprintf(eBuf, "ERROR: Can't set socket options");
    handleErr(eBuf, 1, stderr);
  }

  if (bind(svrfd, res->ai_addr, res->ai_addrlen) == -1) {
    sprintf(eBuf, "ERROR: Can't bind address");
    handleErr(eBuf, 1, stderr);
  }

  if (listen(svrfd, SOMAXCONN)) {
    sprintf(eBuf, "ERROR: Can't listen for connections");
    handleErr(eBuf, 1, stderr);
  }

  freeaddrinfo(res);
  return svrfd;
}

// TODO - WORKING - when main process exits, listening from web process remains open
// Start listening for incomming messages
int startMsgListener(char *ip, char *port) {
  int svrfd = 0, sessfd = 0;
  int fd = 0;
  char eBuf[256];

  svrfd = createSession(ip, port);

  char *hhl7fifo = "hhl7fifo";
  mkfifo(hhl7fifo, 0666);
  fd = open(hhl7fifo, O_WRONLY | O_NONBLOCK);

  for (;;) {
    sessfd = accept(svrfd, 0, 0);
    if (sessfd == -1) {
      sprintf(eBuf, "ERROR: Can't accept connections");
      handleErr(eBuf, 1, stderr);
    }

    // Make children ignore waiting for parent process before closing
    signal(SIGCHLD, SIG_IGN);

    pid_t pidBefore = getpid();
    pid_t pid=fork();
    if (pid == -1) {
      sprintf(eBuf, "ERROR: Can't fork child process to listen");
      handleErr(eBuf, 1, stderr);

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
  return -1;
}

