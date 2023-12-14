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
#include <errno.h>
#include <microhttpd.h>
#include <json.h>
#include "hhl7extern.h"
#include "hhl7json.h"
#include "hhl7net.h"
#include "hhl7utils.h"
#include "hhl7web.h"


// Struct for queued auto responses
struct Response {
  struct Response *next;
  time_t sendTime;
  char sIP[256];
  char sPort[6];
  char *tName;
  int sent;
  char resCode[3];
  int argc;
  char sendArgs[20][256];
  char *sendPtrs[20];
};

// Linked list of responses
static struct Response *responses;


// Debug function to print auto response queue
static void printResponses() {
  struct Response *resp;

  resp = responses;
  while (resp != NULL) {
    printf("R: %ld\n", resp->sendTime);
    resp = resp->next;
  }
}


// Add a response struct to the queue
static struct Response *queueResponse(struct Response *resp) {
  struct Response *this, *next;

  this = responses;
  if (this == NULL) {
    return resp;

  } else if (resp->sendTime <= this->sendTime) {
    resp->next = this;
    return resp;
    
  } else {
    while (this != NULL) {
      if (this->next == NULL) {
        this->next = resp;
        return responses;

      } else {
        next = this->next;
        if (next->sendTime >= resp->sendTime) {
          resp->next = next;
          this->next = resp;
          return responses;

        } else {
          this = this->next;

        }
      }
    }
    return responses;
  }
}


// Process the response queue
static int processResponses() {
printf("processing...\n");
printResponses(responses);
  struct Response *resp;
  time_t tNow = time(NULL); 
  char resStr[3] = "";

  resp = responses;
  while (resp != NULL) {
    // TODO - add expiry time to config file
    // Expire old responses
    if (tNow - resp->sendTime >= 900 && resp->sent == 1) {
      responses = resp->next;
      free(resp);
      resp = responses;
      continue;

    } else if (tNow >= resp->sendTime && resp->sent == 0) {
      // Send the response to the server
      printf("Send...\n");
//printf("%s\n%s\n%s\n%d\n%s\n%s\n\n", resp->sIP, resp->sPort, resp->tName, resp->argc, resp->sendPtrs[0], resp->sendPtrs[1]);
      sendTemp(resp->sIP, resp->sPort, resp->tName, 0, 0, 0, resp->argc,
               resp->sendPtrs, resStr);

      resp->sent = 1;
      //strcpy(resp->resCode, resStr);
      sprintf(resp->resCode, "%s", resStr);

    } else if (resp->sendTime > tNow) {
      return(resp->sendTime - tNow);

    }
    resp = resp->next;
  }
  // TODO - make this a config item
  return 10;
}


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
  // TODO - change 256: malloc a buffer, read 256, check if complete ack, expand if needed
  if ((recvL = recv(sockfd, ackBuf, 256, 0)) == -1) {
    handleError(LOG_ERR, "Timeout listening for ACK response", 1, 0, 1);
    return -1;

  } else {
    close(sockfd);
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
        //strncpy(res, aCode, 2);
        aCode[3] = '\0';
        strcpy(res, aCode);
        res[3] = '\0';
      }
    }
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


// Send a json template
void sendTemp(char *sIP, char *sPort, char *tName, int noSend, int fShowTemplate,
              int optind, int argc, char *argv[], char *resStr) {

  FILE *fp;
  int sockfd;
  char fileName[256] = "";

  // Find the template file
  fp = findTemplate(fileName, tName);
  int fSize = getFileSize(fileName);

  sprintf(infoStr, "Using template file: %s", fileName);
  writeLog(6, infoStr, 1);

  char *jsonMsg = malloc(fSize + 1);
  int hl7MsgS = 1024;
  char *hl7Msg = malloc(hl7MsgS);
  hl7Msg[0] = '\0';

  // Read the json template to jsonMsg
  readJSONFile(fp, fSize, jsonMsg);

  // Generate HL7 based on the json template
  parseJSONTemp(jsonMsg, &hl7Msg, &hl7MsgS, NULL, NULL, argc - optind, argv + optind, 0);

  // Print the HL7 message if requested
  if (fShowTemplate == 1) {
     hl72unix(hl7Msg, 1);
  }

  if (noSend == 0) {
    // Connect to server, send & listen for ack
    sockfd = connectSvr(sIP, sPort);
printf("sockfd: %d\n", sockfd);
    sendPacket(sockfd, hl7Msg, resStr);
  }

  // TODO - free hl7Msg?? 
  // Free memory
  free(jsonMsg);
  fclose(fp);
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
    close(sessfd);
    sprintf(infoStr, "Message with control ID %s received OK and ACK sent", cid);
    writeLog(LOG_INFO, infoStr, 1);
  }

//printf("sessfd, SA: %d\n", sessfd);
//hl72unix(ackBuf, 1);
//printf("Ack: %s\n", ackBuf);

  return writeL;
}


// Check incomming message for responder match and send response
static struct Response *checkResponse(char *msg, char *sIP, char *sPort, char *tName) {
  struct Response *respHead = responses;
  struct json_object *resObj = NULL, *jArray = NULL, *matchObj = NULL;
  struct json_object *segStr = NULL, *fldObj = NULL, *valStr = NULL;
  struct json_object *minObj = NULL, *maxObj = NULL, *sendT = NULL;
  int m = 0, mCount = 0, fldInt = 0, minT = 0, maxT = 0;
  // TODO - check values, malloc?
  char resFile[290], fldStr[256], resStr[3];

  // Define template location
  if (isDaemon == 1) {
    sprintf(resFile, "/usr/local/hhl7/responders/%s.json", tName);
  } else {
    sprintf(resFile, "./responders/%s.json", tName);
  }

  // Read the template file
  resObj = json_object_from_file(resFile);
  if (resObj == NULL) {
    // TODO - use error handler
    printf("Failed to read responder template\n");
    exit(1);
  }

  json_object_object_get_ex(resObj, "matches", &jArray);
  if (jArray == NULL) {
    // TODO - use error handler
    printf("Responder template contains no matches\n");
    exit(1);
  }

  mCount = json_object_array_length(jArray);

  // Check that each mathes item matches, return responses if no match
  for (m = 0; m < mCount; m++) {
    // TODO - error handling
    matchObj = json_object_array_get_idx(jArray, m);
    segStr = json_object_object_get(matchObj, "segment"); 
    json_object_object_get_ex(matchObj, "field", &fldObj);
    fldInt = json_object_get_int(fldObj);
    valStr = json_object_object_get(matchObj, "value");

    getHL7Field(msg, (char *) json_object_get_string(segStr), fldInt, fldStr);

    // TODO - log function
    printf("Comparing %s against %s...\n", json_object_get_string(valStr), fldStr);
    if (strcmp(json_object_get_string(valStr), fldStr) != 0) {
      printf("No Match\n");
      return responses;
    }
  }

  // We've matched a responder, handle the response
  json_object_object_get_ex(resObj, "reponseTimeMin", &minObj);
  minT = json_object_get_int(minObj);
  json_object_object_get_ex(resObj, "reponseTimeMax", &maxObj);
  maxT = json_object_get_int(maxObj);
  sendT = json_object_object_get(resObj, "sendTemplate");

  json_object_object_get_ex(resObj, "sendArgs", &jArray);
  if (jArray == NULL) {
    // TODO - use error handler
    printf("Responder template contains no matches\n");
    exit(1);
  }

  mCount = json_object_array_length(jArray);
  //char sendArgs[mCount][256], *sendPtrs[mCount];

  struct Response *resp;
  resp = calloc(1, sizeof(struct Response));

  for (m = 0; m < mCount; m++) {
    matchObj = json_object_array_get_idx(jArray, m);
    segStr = json_object_object_get(matchObj, "segment");
    json_object_object_get_ex(matchObj, "field", &fldObj);
    fldInt = json_object_get_int(fldObj);
    getHL7Field(msg, (char *) json_object_get_string(segStr), fldInt, fldStr);
    strcpy(resp->sendArgs[m], fldStr);
    resp->sendPtrs[m] = resp->sendArgs[m];

  }

  // Get a random value between the min and max times
  int rndNum = 0;
  if (minT != maxT) getRand(minT, maxT, 0, NULL, &rndNum);

  // Create a response struct for this response
  resp->sendTime = time(NULL) + rndNum;
  strcpy(resp->sIP, sIP);
  strcpy(resp->sPort, sPort);
  resp->tName = (char *) json_object_get_string(sendT);
  resp->sent = 0;
  resp->argc = mCount;

  // If min and max times are 0, send immediately
  if (minT == 0 && maxT == 0) {
    sendTemp(sIP, sPort, (char *) json_object_get_string(sendT), 0, 0, 0,
             mCount, resp->sendPtrs, resStr);
    resp->sent = 1;
    sprintf(resp->resCode, "%s", resStr);

  }

  // Add the response to the queue
  respHead = queueResponse(resp);
  printf("Response queued, delivery in %ld seconds\n", resp->sendTime - time(NULL));

  json_object_put(jArray);
  return respHead;
}


// Handle an incomming message
static struct Response *handleMsg(int sessfd, int fd, char *sIP,
                                  char *sPort, char *tName) {

  struct Response *respHead = responses;
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

          } else if (tName) {
            respHead = checkResponse(msgBuf, sIP, sPort, tName);
            //hl72unix(msgBuf, 1);

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
  return respHead;
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
int startMsgListener(char *lIP, const char *lPort, char *sIP, char *sPort, char *tName) {
  // TODO - move timeout/polling interval nextResponse to config file
  int svrfd = 0, sessfd = 0, fd = 0, nextResp = 10;

  if ((svrfd = createSession(lIP, lPort)) == -1) return -1;

  // Create a named pipe to write to
  char hhl7fifo[21]; 
  sprintf(hhl7fifo, "%s%d", "/tmp/hhl7fifo.", getpid());
  mkfifo(hhl7fifo, 0666);
  fd = open(hhl7fifo, O_WRONLY | O_NONBLOCK);

  while(1) {
    struct timeval tv, *tvp;
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(svrfd, &rs);

    if (nextResp < 0) nextResp = 0;
    tv.tv_sec = nextResp;
    tv.tv_usec = 0;
    tvp = &tv;

    int res = select(svrfd + 1, &rs, NULL, NULL, tvp);
    if (res == -1) {
      if (errno != EINTR) {
        sprintf(infoStr, "Aborting due to error during select: %s", strerror(errno));
        handleError(LOG_ERR, infoStr, 1, 1, 1);
        break;
      }

    } else if (res == 0) {
      if (sIP != NULL) {
        nextResp = processResponses();
        printf("Responses processed, next response in %d seconds\n", nextResp);
      }

    } else {
      sessfd = accept(svrfd, 0, 0);
      if (sessfd == -1) {
        handleError(LOG_ERR, "Can't accept connections", 1, 0, 1);
        return -1;
      }

      responses = handleMsg(sessfd, fd, sIP, sPort, tName);
      printResponses(responses);
      close(sessfd);

      if (sIP != NULL) nextResp = processResponses();
    }
  }

  close(svrfd);
  return 0;
}
