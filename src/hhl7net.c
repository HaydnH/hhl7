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
  char *rName;
  char *tName;
  // TODO - Consider malloc to allow more than 25 args
  char *sArgs[25];
  int sent;
  char resCode[3];
  int argc;
};

// Linked list of responses
static struct Response *responses;


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


// Add a response to the web list
static void addRespWeb(char *newResp, struct Response *resp, int respCount) {
  char resEvenOdd[7] = "trEven";
  char resClass[17] = "";
  char newResCode[3] = "";
  int resEven = respCount % 2;

  // Provide conditional html/css options to the response queue
  newResp[0] = '\0';
  if (resEven == 1) sprintf(resEvenOdd, "trOdd");
  sprintf(newResCode, "%s", resp->resCode);
  if (strlen(resp->resCode) == 0) {
    sprintf(newResCode, "--");
    sprintf(resClass, " class='tdCtr'");
  } else if (strcmp(newResCode, "AA") == 0 || strcmp(newResCode, "CA") == 0) {
    sprintf(resClass, " class='tdCtrG'");
  } else {
    sprintf(resClass, " class='tdCtrR'");
  }

  sprintf(newResp, "<tr class='%s'><td class='tdCtr'>%d</td><td>%s</td><td>%s</td><td>%s:%s</td><td class='rSendTime'>%ld</td><td class='rSTFmt'></td><td%s>%s</td></tr>", resEvenOdd, respCount + 1, resp->rName, resp->tName, resp->sIP, resp->sPort, resp->sendTime, resClass, newResCode);

}


// Process the response queue
static int processResponses(int fd) {
  struct Response *resp;
  time_t tNow = time(NULL), fTime;
  char writeSize[11] = "";
  char resStr[3] = "";
  char *sArg = "";
  // TODO - add expiry time to config file
  int nextResp = -1, nr = -1, respCount = 0, rmCount = 0, argCount = 0, expTime = 900;
  // TODO - add webLimit to config file? Remove once changed to SSE?
  int webRespS = 1024, reqS = 0, webLimit = 200;
  char *webResp = malloc(webRespS);
  webResp[0] = '\0';
  char errStr[256] = "";

  resp = responses;
  if (resp == NULL) return nextResp;

  char newResp[strlen(resp->rName) + strlen(resp->tName) +
               strlen(resp->sIP) + strlen(resp->sPort) + 293];

  writeLog(LOG_DEBUG, "Processing response queue...", 0);

  sprintf(webResp, "%s", "{ \"data\":\"");

  while (resp != NULL) {
    // Expire old responses
    if (tNow - resp->sendTime >= expTime && resp->sent == 1) {
      writeLog(LOG_DEBUG, "Response queue processing, old response removed", 0);
      responses = resp->next;
      free(resp->tName);
      free(resp->rName);

      // Free all template send arguments
      sArg = resp->sArgs[0];
      while (sArg != NULL) {
        free(resp->sArgs[argCount]);
        argCount++;
        sArg = resp->sArgs[argCount];
      }

      argCount = 0;
      free(resp);
      resp = responses;
      rmCount++;
      continue;

    } else if (tNow >= resp->sendTime && resp->sent == 0) {
      // Send the response to the server
      writeLog(LOG_DEBUG, "Response queue processing, response sent", 0);
      sendTemp(resp->sIP, resp->sPort, resp->tName, 0, 0, 0, resp->argc,
               resp->sArgs, resStr);

      resp->sent = 1;
      sprintf(resp->resCode, "%s", resStr);
      if (webRunning == 1 && respCount <= webLimit) addRespWeb(newResp, resp, respCount);
      respCount++;

    } else if (resp->sendTime > tNow) {
      writeLog(LOG_DEBUG, "Response queue processing, future response added to queue", 0);
      fTime = resp->sendTime - tNow;
      if (nextResp == -1 || fTime < nextResp) nextResp = fTime;
      if (webRunning == 1 && respCount <= webLimit) addRespWeb(newResp, resp, respCount);
      respCount++;
      if (nextResp < 0) nextResp = 0;

    } else if (tNow - resp->sendTime < expTime) {
      writeLog(LOG_DEBUG, "Response queue processing, sent response added to queue", 0);
      if (nextResp == -1 || nextResp > expTime) nextResp = expTime;

      nr = tNow - resp->sendTime;
      if (nextResp == -1 || nr < nextResp) nextResp = nr;

      if (webRunning == 1  && respCount <= webLimit) addRespWeb(newResp, resp, respCount);
      respCount++;

    }

    if (webRunning == 1 && respCount <= webLimit) {
      reqS = strlen(webResp) + strlen(newResp) + 57;
      if (reqS > webRespS) webResp = dblBuf(webResp, &webRespS, reqS);
      strcat(webResp, newResp);
    }
    resp = resp->next;
  }

  sprintf(webResp + strlen(webResp), "\", \"count\": %d, \"max\": %d }",
                                     respCount, webLimit);

  writeLog(LOG_DEBUG, "Response queue processing complete", 0);

  if (webRunning == 1 && (respCount + rmCount) > 0) {
    if ((int) strlen(webResp) == 0) sprintf(webResp, "%s", "QE");
    sprintf(writeSize, "%d", (int) strlen(webResp));

    if (write(fd, writeSize, 11) == -1) {
      sprintf(errStr, "Failed to write to named pipe: %s", strerror(errno));
      handleError(LOG_ERR, errStr, 1, 0, 0);
    }

    if (write(fd, webResp, strlen(webResp)) == -1) {
      sprintf(errStr, "Failed to write to named pipe: %s", strerror(errno));
      handleError(LOG_ERR, errStr, 1, 0, 0);
    }
  }

  webResp[0] = '\0';
  free(webResp);
  if (respCount > 0 && (nextResp == expTime || nextResp== -1)) return(expTime);
  return(nextResp);
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
  int sockfd = -1, rv;
  struct addrinfo hints, *servinfo, *p;
  char errStr[299] = "";

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
    handleError(LOG_ERR, "Can't obtain address info when connecting to server", -1, 0, 1);
    freeaddrinfo(servinfo);
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
    sprintf(errStr, "Connected to server %s on port %s", ip, port);
    writeLog(LOG_INFO, errStr, 1);
    break;
  }

  if (p == NULL) {
    sprintf(errStr, "Failed to connect to server %s on port %s", ip, port);
    handleError(LOG_ERR, errStr, -1, 0, 1);
    freeaddrinfo(servinfo);
    return(-1);
  }

  freeaddrinfo(servinfo);
  return(sockfd);
}


// Listen for ACK from server
int listenACK(int sockfd, char *res) {
  char ackBuf[512] = "", app[12] = "", code[7] = "", aCode[3] = "", errStr[46] = "";
  int ackErr = 0, recvL = 0;

  // TODO Add timeout to config file
  // Set ListenACK timeout
  struct timeval tv, *tvp;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  tvp = &tv;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, tvp, sizeof tv);

  writeLog(LOG_INFO, "Listening for ACK...", 1);

  // Receive the response from the server and strip MLLP wrapper
  if ((recvL = recv(sockfd, ackBuf, 512, 0)) == -1) {
    handleError(LOG_ERR, "Timeout listening for ACK response", 1, 0, 1);
    return -2;

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
      if (res != NULL) sprintf(res, "%s", "EE");
      return -3;

    } else {
      sprintf(errStr, "Server ACK response: %s %s (%s)", app, code, aCode);
      writeLog(LOG_INFO, errStr, 1);

      if (res) {
        aCode[2] = '\0';
        strcpy(res, aCode);
      }
    }
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
  sendPacket(sockfd, fileData, resStr, 0);
}


// Send a string packet over socket
int sendPacket(int sockfd, char *hl7msg, char *resStr, int fShowTemplate) {
  const char msgDelim[6] = "MSH|";
  char tokMsg[strlen(hl7msg) + 5];
  char *curMsg = hl7msg, *nextMsg;
  int msgCount = 0, retVal = 0;
  char errStr[43] = "";

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
    sprintf(errStr, "Sending HL7 message %d to server", msgCount);
    writeLog(LOG_INFO, errStr, 1);

    // Print the HL7 message if requested
    if (fShowTemplate == 1) {
       hl72unix(tokMsg, 1);
    }

    // Add MLLP wrapper to this message
    wrapMLLP(tokMsg);

    // Send the message to the server
    if(send(sockfd, tokMsg, strlen(tokMsg), 0) == -1) {
      if (resStr != NULL) sprintf(resStr, "%s", "EE");
      handleError(LOG_ERR, "Could not send data packet to server", -1, 0, 1);
      return -1;

    } else {
      retVal = listenACK(sockfd, resStr);

    }
  }
  close(sockfd);
  return(retVal);
}


// Send a json template
void sendTemp(char *sIP, char *sPort, char *tName, int noSend, int fShowTemplate,
              int optind, int argc, char *argv[], char *resStr) {

  FILE *fp;
  int sockfd, retVal = 0;
  char fileName[256] = "";
  char errStr[289] = "";

  sprintf(errStr, "Attempting to send template: %s", tName);
  writeLog(LOG_DEBUG, errStr, 0);

  // Find the template file
  fp = findTemplate(fileName, tName, 0);
  int fSize = getFileSize(fileName);

  sprintf(errStr, "Using template file: %s", fileName);
  writeLog(LOG_INFO, errStr, 1);

  char *jsonMsg = malloc(fSize + 1);
  int hl7MsgS = 1024;
  char *hl7Msg = malloc(hl7MsgS);
  hl7Msg[0] = '\0';

  // Read the json template to jsonMsg
  readJSONFile(fp, fSize, jsonMsg);
  writeLog(LOG_DEBUG, "JSON Template read OK", 0);

  // Generate HL7 based on the json template
  retVal = parseJSONTemp(jsonMsg, &hl7Msg, &hl7MsgS, NULL, NULL,
                         argc - optind, argv + optind, 0);

  if (retVal > 0) {
    sprintf(errStr, "Failed to parse JSON template (%s)", tName);
    handleError(LOG_ERR, errStr, 1, 0, 1);

  } else {
    writeLog(LOG_DEBUG, "JSON Template parsed OK", 0);

    if (noSend == 0) {
      // Connect to server, send & listen for ack
      sockfd = connectSvr(sIP, sPort);
      sendPacket(sockfd, hl7Msg, resStr, fShowTemplate);

    } else if (fShowTemplate == 1) {
       hl72unix(hl7Msg, 1);

    }
  }

  // Free memory
  free(jsonMsg);
  free(hl7Msg);
  fclose(fp);
}


// Send and ACK after receiving a message
int sendAck(int sessfd, char *hl7msg) {
  char dt[26] = "", cid[201] = "", errStr[251] = "";
  char ackBuf[1024];
  int writeL = 0;

  // Get current time and control ID of incoming message
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
    sprintf(errStr, "Message with control ID %s received OK and ACK sent", cid);
    writeLog(LOG_INFO, errStr, 1);
  }

  return writeL;
}


// Check incoming message for responder match and send response
static struct Response *checkResponse(char *msg, char *sIP, char *sPort, char *tName) {
  struct Response *respHead = responses;
  struct json_object *resObj = NULL, *jArray = NULL, *matchObj = NULL;
  struct json_object *segStr = NULL, *fldObj = NULL, *valStr = NULL, *exclObj = NULL;
  struct json_object *minObj = NULL, *maxObj = NULL, *sendT = NULL, *respN = NULL;
  int m = 0, mCount = 0, fldInt = 0, exclInt = 0, minT = 0, maxT = 0;
  char resFile[290], fldStr[256], resStr[3] = "", errStr[542] = "";
  char *tmpPtr = NULL;

  // Define template location
  if (isDaemon == 1) {
    sprintf(resFile, "/usr/local/hhl7/responders/%s.json", tName);
  } else {
    findTemplate(resFile, tName, 1);
  }

  // Read the template file
  resObj = json_object_from_file(resFile);
  if (resObj == NULL) {
    handleError(LOG_ERR, "Failed to read responder template", 1, 0, 1);
  }

  json_object_object_get_ex(resObj, "matches", &jArray);
  if (jArray == NULL) {
    handleError(LOG_ERR, "Responder template contains no matches", 1, 0, 1);
  }

  mCount = json_object_array_length(jArray);

  // Check that each matches item matches, return responses if no match
  for (m = 0; m < mCount; m++) {
    // TODO - error handling + implement size checks for values from json
    matchObj = json_object_array_get_idx(jArray, m);
    segStr = json_object_object_get(matchObj, "segment"); 
    json_object_object_get_ex(matchObj, "field", &fldObj);
    fldInt = json_object_get_int(fldObj);
    valStr = json_object_object_get(matchObj, "value");
    json_object_object_get_ex(matchObj, "exclude", &exclObj);
    if (exclObj) {
      exclInt = json_object_get_boolean(exclObj);
    }

    getHL7Field(msg, (char *) json_object_get_string(segStr), fldInt, fldStr);

    // Check if we've mached this match clause, if not return
    if (strcmp(json_object_get_string(valStr), fldStr) == 0 && exclInt == 0) {
      sprintf(errStr, "Comparing %s against %s... matched",
              json_object_get_string(valStr), fldStr);
      writeLog(LOG_INFO, errStr, 1);

    } else if (strcmp(json_object_get_string(valStr), fldStr) != 0 && exclInt == 1) {
      sprintf(errStr, "Comparing %s against %s... exclusion, match",
              json_object_get_string(valStr), fldStr);
      writeLog(LOG_INFO, errStr, 1);

    } else if (strcmp(json_object_get_string(valStr), fldStr) == 0 && exclInt == 1) {
      sprintf(errStr, "Comparing %s against %s... exclusion, no match",
              json_object_get_string(valStr), fldStr);
      writeLog(LOG_INFO, errStr, 1);
      return responses;

    } else {
      sprintf(errStr, "Comparing %s against %s... no match",
              json_object_get_string(valStr), fldStr);
      writeLog(LOG_INFO, errStr, 1);
      return responses;
    }
  }

  // We've matched a responder, handle the response
  json_object_object_get_ex(resObj, "reponseTimeMin", &minObj);
  minT = json_object_get_int(minObj);
  json_object_object_get_ex(resObj, "reponseTimeMax", &maxObj);
  maxT = json_object_get_int(maxObj);
  sendT = json_object_object_get(resObj, "sendTemplate");
  respN = json_object_object_get(resObj, "name");

  json_object_object_get_ex(resObj, "sendArgs", &jArray);
  if (jArray == NULL) {
    handleError(LOG_INFO, "Responder template contains no send arguments", 1, 0, 1);
  }

  mCount = json_object_array_length(jArray);

  if (mCount > 25) {
    handleError(LOG_WARNING, "A responder template created too many arguments (>25)", 1, 0, 1);
    return respHead;
  }

  struct Response *resp;
  resp = calloc(1, sizeof(struct Response));

  for (m = 0; m < mCount; m++) {
    matchObj = json_object_array_get_idx(jArray, m);
    segStr = json_object_object_get(matchObj, "segment");
    json_object_object_get_ex(matchObj, "field", &fldObj);
    fldInt = json_object_get_int(fldObj);
    getHL7Field(msg, (char *) json_object_get_string(segStr), fldInt, fldStr);
    tmpPtr = strdup(fldStr);
    resp->sArgs[m] = tmpPtr;
  }

  // Get a random value between the min and max times
  int rndNum = 0;
  if (minT != maxT) getRand(minT, maxT, 0, NULL, &rndNum);

  // Create a response struct for this response
  resp->sendTime = time(NULL) + rndNum;
  strcpy(resp->sIP, sIP);
  strcpy(resp->sPort, sPort);
  resp->tName = strdup(json_object_get_string(sendT));
  resp->rName = strdup(json_object_get_string(respN));
  resp->sent = 0;
  resp->argc = mCount;

  // If min and max times are 0, send immediately
  if (minT == 0 && maxT == 0) {
    sendTemp(sIP, sPort, (char *) json_object_get_string(sendT), 0, 0, 0,
             mCount, resp->sArgs, resStr);
    resp->sent = 1;
    sprintf(resp->resCode, "%s", resStr);

  }

  // Add the response to the queue
  respHead = queueResponse(resp);
  sprintf(errStr, "Response queued, delivery in %ld secs", resp->sendTime - time(NULL));
  writeLog(LOG_INFO, errStr, 1);

  json_object_put(resObj);
  return respHead;
}


// Handle an incoming message
static struct Response *handleMsg(int sessfd, int fd, char *sIP, char *sPort,
                                  int argc, int optind, char *argv[]) {

  struct Response *respHead = responses;
  int readSize = 512, msgSize = 0, maxSize = readSize + msgSize, rcvSize = 1;
  int msgCount = 0, ignoreNext = 0, webErr = 0;
  int *ms = &maxSize;
  char rcvBuf[readSize + 1];
  char *msgBuf = malloc(maxSize);
  char writeSize[11] = "";
  char errStr[306] = "";
  rcvBuf[0] = '\0';
  msgBuf[0] = '\0';

  while (rcvSize > 0) {
    rcvSize = read(sessfd, rcvBuf, readSize);
    rcvBuf[rcvSize] = '\0';

    if (ignoreNext == 1) {
      ignoreNext = 0;

    } else {
      if (rcvSize == -1 && msgCount == 0) {
        handleError(LOG_ERR, "Failed to read incoming message from server", 1, 0, 1);
        // TODO - check if we're still using webErr
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

          // If we're responding, parse each respond template to see if msg matches
          if (argc > 0) {
            for (int i = optind; i < argc; i++) {
              sprintf(errStr, "Checking if incoming message matches responder: %s",
                      argv[i]);
              writeLog(LOG_INFO, errStr, 1);
              respHead = checkResponse(msgBuf, sIP, sPort, argv[i]);
              responses = respHead;
            }
          }

          if (webRunning == 1) {
            if (webErr > 0) {
              sprintf(msgBuf, "ERROR: The backend failed to receive or process a message from the sending server");
              // TODO  WORKING - rework how errors get sent to web listener
              //  strcpy(msgBuf, webErrStr);
            }

            if (argc <= 0) {
              sprintf(writeSize, "%d", (int) strlen(msgBuf));
              if (write(fd, writeSize, 11) == -1) {
                handleError(LOG_ERR, "ERROR: Failed to write to named pipe", 1, 0, 1);
              }

              if (write(fd, msgBuf, strlen(msgBuf)) == -1) {
                handleError(LOG_ERR, "ERROR: Failed to write to named pipe", 1, 0, 1);
              }
            }

          } else if (argc <= 0) {
            hl72unix(msgBuf, 1);
          }

          // Clean up counters and buffers
          msgSize = 0;
          msgBuf[0] = '\0';
        }
      }
    }

    rcvBuf[0] = '\0';

  }

  free(msgBuf);
  close(sessfd);
  return responses;
}


// Listen for incoming messages
static int createSession(char *ip, const char *port) {
  int svrfd, rv;
  struct addrinfo hints, *res = 0;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol=0;
  hints.ai_flags=AI_PASSIVE|AI_ADDRCONFIG;

  if ((rv = getaddrinfo(ip, port, &hints, &res)) != 0) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't obtain address info when creating session", 1, 0, 1);
    return -1;
  }

  svrfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

  if (svrfd == -1) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't obtain socket address info", 1, 0, 1);
    return -1;
  }

  int reuseaddr = 1;
  if (setsockopt(svrfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't set socket options", 1, 0, 1);
    return -1;
  }

  if (bind(svrfd, res->ai_addr, res->ai_addrlen) == -1) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't bind address", 1, 0, 1);
    return -1;
  }

  if (listen(svrfd, SOMAXCONN)) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't listen for connections", 1, 0, 1);
    return -1;
  }

  freeaddrinfo(res);
  return svrfd;
}


// Check the named pipe for response template updates
static int readRespTemps(int respFD, char respTemps[20][256], char *respTempsPtrs[20]) {
  struct json_object *rootObj = NULL, *dataArray = NULL, *dataObj = NULL;
  char readSizeBuf[11];
  int pLen = 1, readSize = 0, updated = 0, dataInt = 0, i = 0;

  while (pLen > 0) {
    if ((pLen = read(respFD, readSizeBuf, 11)) > 0) {
      readSize = atoi(readSizeBuf);

      char rBuf[readSize + 1];
      if ((pLen = read(respFD, rBuf, readSize)) > 0) {
        updated = 1;
        rBuf[readSize] = '\0';

        // TODO - IMPORTANT - error check! data can come from external and may seg fault
        rootObj = json_tokener_parse(rBuf);
        json_object_object_get_ex(rootObj, "templates", &dataArray);
        dataInt = json_object_array_length(dataArray);

        for (i = 0; i < dataInt; i++) {
          dataObj = json_object_array_get_idx(dataArray, i);
          sprintf(respTemps[i], "%s", json_object_get_string(dataObj));
          respTempsPtrs[i] = respTemps[i];
        }
      }
    }
  }

  if (updated == 1) return dataInt;
  return 0;
}


// Start listening for incoming messages
int startMsgListener(char *lIP, const char *lPort, char *sIP, char *sPort,
                     int argc, int optind, char *argv[]) {
  // TODO - malloc instead of limited resp templates
  char respTemps[20][256];
  char *respTempsPtrs[20];
  char errStr[58] = "";
  // TODO - move timeout/polling interval nextResponse to config file
  int svrfd = 0, sessfd = 0, fd = 0, rfd = 0, nextResp = -1, resU = 0, respUpdated = 0;
  time_t lastProcess = time(NULL), now;
  // TODO add to config file? Maybe remove it once we only send respond updates to web?
  int procThrot = 1;

  writeLog(LOG_INFO, "Listener child process starting up", 0);

  svrfd = createSession(lIP, lPort);

  // Create a named pipe to write to
  if (webRunning == 1) {
    char hhl7fifo[21]; 
    sprintf(hhl7fifo, "%s%d", "/tmp/hhl7fifo.", getpid());
    mkfifo(hhl7fifo, 0666);
    fd = open(hhl7fifo, O_WRONLY);

    // Send FS code to parent process to state the listener has bound it's port
    if (svrfd != -1) {
      if (write(fd, "FS", 2) == -1) {
        handleError(LOG_ERR, "Failed to write to named pipe while starting listener", 1, 1, 0);
      }
    } else {
      return(-1);
    }

    // Switch to non blocking after initial FS message
    fcntl(fd, F_SETFL, O_NONBLOCK);

    char hhl7rfifo[22];
    sprintf(hhl7rfifo, "%s%d", "/tmp/hhl7rfifo.", getpid());
    mkfifo(hhl7rfifo, 0666);
    rfd = open(hhl7rfifo, O_RDONLY | O_NONBLOCK);
    //fcntl(rfd, F_SETPIPE_SZ, 1048576); // Change pipe size, default seems OK

  }

  while(1) {
    now = time(NULL);
    struct timeval tv, *tvp;
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(svrfd, &rs);

    if (nextResp == -1) {
      tvp = NULL;
    } else {
      tv.tv_sec = nextResp;
      tv.tv_usec = 0;
      tvp = &tv;
    }

    int res = select(svrfd + 1, &rs, NULL, NULL, tvp);
    if (res == -1) {
      if (errno != EINTR) {
        handleError(LOG_ERR, "startMsgListener() Failed during select() routine", 1, 1, 1);
        break;
      }

    } else if (res == 0) {
      if (argc > 0) {
        if ((now - lastProcess) >= procThrot) {
          if (argc > 0) nextResp = processResponses(fd);
          lastProcess = now;
        } else {
          nextResp = procThrot;
        }

        if (nextResp == -1) {
          writeLog(LOG_INFO, "Response queue empty, awaiting next received message", 1);
        } else {
          sprintf(errStr, "Responses processed, next process in %d seconds", nextResp);
          writeLog(LOG_INFO, errStr, 1);
        }
      }

    } else {
      if (webRunning == 1) {
        resU = readRespTemps(rfd, respTemps, respTempsPtrs);
        if (resU > 0) respUpdated = resU;
      }

      sessfd = accept(svrfd, 0, 0);
      if (sessfd == -1) {
        close(svrfd);
        handleError(LOG_ERR, "Can't accept connections", 1, 0, 1);
        return -1;
      }

      if (respUpdated == 0) {
        responses = handleMsg(sessfd, fd, sIP, sPort, argc, optind, argv);
      } else {
        responses = handleMsg(sessfd, fd, sIP, sPort, respUpdated, 0, respTempsPtrs);
      }

      close(sessfd);

      if ((now - lastProcess) >= procThrot) {
        if (argc > 0) nextResp = processResponses(fd);
        lastProcess = now;
      } else {
        nextResp = procThrot;
      }
    }
  }

  close(svrfd);
  return 0;
}
