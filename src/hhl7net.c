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
  char **sendArgs;
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
    return(resp);

  } else if (resp->sendTime <= this->sendTime) {
    resp->next = this;
    return(resp);
    
  } else {
    while (this != NULL) {
      if (this->next == NULL) {
        this->next = resp;
        return(responses);

      } else {
        next = this->next;
        if (next->sendTime >= resp->sendTime) {
          resp->next = next;
          this->next = resp;
          return(responses);

        } else {
          this = this->next;

        }
      }
    }
    return(responses);
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
static int processResponses(int fd, int aTimeout) {
  struct Response *resp;
  time_t tNow = time(NULL), fTime;
  char writeSize[11] = "";
  char resStr[3] = "";
  char *sArg = "";
  int nextResp = -1, respCount = 0, rmCount = 0, argCount = 0, expTime = 900;
  int webRespS = 1024, reqS = 0, webLimit = 200;
  char *webResp = malloc(webRespS);
  webResp[0] = '\0';
  char errStr[256] = "";

  resp = responses;
  if (resp == NULL) return(nextResp);

  char newResp[strlen(resp->rName) + strlen(resp->tName) +
               strlen(resp->sIP) + strlen(resp->sPort) + 293];

  // Use global config variables if they exist
  if (globalConfig) {
    if (globalConfig->rQueueSize >= 0) webLimit = globalConfig->rQueueSize;
    if (globalConfig->rExpiryTime >= 0) expTime = globalConfig->rExpiryTime;
  }

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
      sArg = resp->sendArgs[0];
      while (sArg != NULL) {
        free(resp->sendArgs[argCount]);
        argCount++;
        sArg = resp->sendArgs[argCount];
      }
      free(resp->sendArgs);

      argCount = 0;
      free(resp);
      resp = responses;
      rmCount++;
      continue;

    } else if (tNow >= resp->sendTime && resp->sent == 0) {
      // Send the response to the server
      writeLog(LOG_DEBUG, "Response queue processing, response sent", 0);
      sendTemp(resp->sIP, resp->sPort, resp->tName, 0, 0, 0, resp->argc,
               resp->sendArgs, resStr, aTimeout, 0);

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
    if ((int) strlen(webResp) == 0 && webResp) sprintf(webResp, "%s", "QE");
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
    return(1);

  } else if (portNum < 1024 || portNum > 65535) {
    return(2);

  }
  return(0);
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
int listenACK(int sockfd, char *res, int aTimeout, int pACK) {
  char ackBuf[512] = "", app[12] = "", code[7] = "", aCode[3] = "", errStr[46] = "";
  int recvL = 0, ackT = 3;

  // Get the timeout from global config if it exists
  if (aTimeout > 0 && aTimeout <= 60) {
    ackT = aTimeout;
  } else {
    if (globalConfig) {
      if (globalConfig->ackTimeout > 0 && globalConfig->ackTimeout < 100)
        ackT = globalConfig->ackTimeout;

    } else {
      ackT = 4; // No valid ACK timeout from CLI or conf, fall back to a value of 4 seconds
    }
  }

  // Set ListenACK timeout
  struct timeval tv, *tvp;
  tv.tv_sec = ackT;
  tv.tv_usec = 0;
  tvp = &tv;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, tvp, sizeof tv);

  writeLog(LOG_INFO, "Listening for ACK...", 1);

  // Receive the response from the server and strip MLLP wrapper
  if ((recvL = recv(sockfd, ackBuf, 512, 0)) == -1) {
    handleError(LOG_ERR, "Timeout listening for ACK response", 1, 0, 1);
    return(-2);

  } else {
    stripMLLP(ackBuf);

    // Find the ack response in MSA.1
    getHL7Field(ackBuf, "MSA", 1, aCode);

    // Process the code, either AA, AE, AR, CA, CE or CR.
    if ((char) aCode[0] == 'A') {
      strcpy(app, "Application");
    } else if ((char) aCode[0] == 'C') {
      strcpy(app, "Commit");
    }

    if ((char) aCode[1] == 'A') {
      strcpy(code, "Accept");
    } else if ((char) aCode[1] == 'E') {
      strcpy(code, "Error");
    } else if ((char) aCode[1] == 'R') {
      strcpy(code, "Reject");
    }

    sprintf(errStr, "Server ACK response: %s %s (%s)", app, code, aCode);
    writeLog(LOG_INFO, errStr, 1);

    if (pACK == 1) hl72unix(ackBuf, 1);

    if (res) {
      aCode[2] = '\0';
      strcpy(res, aCode);
    }
  }
  return(recvL);
}


// Send a file over socket
void sendFile(char *sIP, char *sPort, FILE *fp, long int fileSize,
              int aTimeout, int pACK) {
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
  sendPacket(sIP, sPort, fileData, resStr, 0, 0, 0, aTimeout, pACK);
}

// Send a single HL7 message over a socket
int sendPacket(char *sIP, char *sPort, char *hl7Msg, char *resStr, int msgCount,
                  int noSend, int fShowTemplate, int aTimeout, int pACK) {

  int sockfd = -1, retVal = 0;
  char errStr[43] = "";

  // Print the HL7 message if requested
  if (fShowTemplate == 1) {
    hl72unix(hl7Msg, 1);
  }

  if (noSend == 0) {
    // Send the data file to the server
    sprintf(errStr, "Sending HL7 message %d to server", msgCount);
    writeLog(LOG_INFO, errStr, 1);

    // Set the default response code to EE
    if (resStr != NULL) sprintf(resStr, "%s", "EE");

    // Connect to the target server
    sockfd = connectSvr(sIP, sPort);
    if (sockfd >= 0) { 
      // Add MLLP wrapper to this message
      wrapMLLP(hl7Msg);

      // Send the message to the server
      if (send(sockfd, hl7Msg, strlen(hl7Msg), 0) == -1) {
        handleError(LOG_ERR, "Could not send data packet to server", -1, 0, 1);
        return(-1);

      } else {
        retVal = listenACK(sockfd, resStr, aTimeout, pACK);

      }

    } else {
      retVal = -4;
    }

    close(sockfd);
  }

  return(retVal);
}


// Split a packet in to individual messages
int splitPacket(char *sIP, char *sPort, char *hl7Msg, char *resStr, char **resList,
                int noSend, int fShowTemplate, int aTimeout, int pACK) {

  char *curMsg = hl7Msg, *nextMsg = NULL;
  char newMsgStr[6] = "MSH|^";
  int rv = 0, msgCount = 0, lastMsg = 0, reqS = 0, rlSize = 301;
  long unsigned int hl7MsgS = strlen(hl7Msg);

  // Check if there's more than 1 message, if not send the full packet
  if (hl7MsgS >= strlen(newMsgStr)) nextMsg = strstr(hl7Msg + 1, newMsgStr);
  if (nextMsg == NULL) {
    msgCount++;
    rv = sendPacket(sIP, sPort, hl7Msg, resStr, msgCount, noSend,
                       fShowTemplate, aTimeout, pACK);

    if (resList != NULL) strcat(*resList, resStr);

  // If there's multiple messages, split and send each individually
  } else {
    char singleMsg[strlen(hl7Msg + 1)];

    // Loop through each message in the template
    while (nextMsg != NULL) {
      msgCount++;
      hl7MsgS = (int) (nextMsg - curMsg) + 1;

      // Send each sub message from the template
      snprintf(singleMsg, hl7MsgS, "%s", curMsg);

      // TODO - take the worst return code!
      rv = sendPacket(sIP, sPort, singleMsg, resStr, msgCount, noSend,
                         fShowTemplate, 0, pACK);

      if (fShowTemplate == 1) printf("\n");

      // Point to the start and end of the next message (or end of packet)
      curMsg = nextMsg;
      nextMsg = NULL;
      if (hl7MsgS >= strlen(newMsgStr)) nextMsg = strstr(curMsg + 1, newMsgStr);
      if (nextMsg == NULL && lastMsg == 0) {
        nextMsg = hl7Msg + strlen(hl7Msg);
        lastMsg = 1;
      }

      // Add the single message response to the response list
      if (resList != NULL) {
        // TODO - check this size!
        reqS = (rlSize + (3 * msgCount));
        if (reqS > rlSize) *resList = dblBuf(*resList, &rlSize, reqS);
        if (msgCount > 1) strcat(*resList, ",");
        strcat(*resList, resStr);

      }
    }
  }

  // Complete the response list JSON reply
  if (resList != NULL)
    sprintf(*resList + strlen(*resList), "%s%d%s", "\", \"count\":", msgCount, " }");

  return(rv);
}


// Send a json template
void sendTemp(char *sIP, char *sPort, char *tName, int noSend, int fShowTemplate,
              int optind, int argc, char *argv[], char *resStr, int aTimeout, int pACK) {

  FILE *fp;
  int retVal = 0;
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
  char *hl7Msg = calloc(1, hl7MsgS + 1);
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
    splitPacket(sIP, sPort, hl7Msg, resStr, NULL, noSend, fShowTemplate, aTimeout, pACK);

  }

  // Free memory
  free(jsonMsg);
  free(hl7Msg);
  fclose(fp);
}


// Get a random resCode based on command line arguments
static int getResCode(int resType, char *ackList, char *resCode) {
  int resRand = 0, listCount = 0;
  long unsigned int l = 0;

  // Set the default value to AA
  sprintf(resCode, "%s", "AA");

  // -a provided on command line, use default randomisation
  if (resType == 1) {
    getRand(0, 30, 0, NULL, &resRand, NULL);
    if (resRand == 29) {
      sprintf(resCode, "%s", "CE");
    } else if (resRand == 27) {
      sprintf(resCode, "%s", "CR");
    } else if (resRand == 26) {
      sprintf(resCode, "%s", "CA");
    } else if (resRand > 21 && resRand < 26) {
      sprintf(resCode, "%s", "AE");
    } else if (resRand > 16 && resRand < 22) {
      sprintf(resCode, "%s", "AR");
    } else {
      sprintf(resCode, "%s", "AA");
    }
    return(0);

  // -A <ackList> provided, parse list and return code
  } else if (resType == 2) {
    for (l = 0; l < strlen(ackList); l++) {
      if (ackList[l] == ',') listCount++;
    }

    getRand(0, listCount, 0, NULL, &resRand, NULL);

    listCount = 0;
    for (l = 0; l < strlen(ackList); l++) {
      if (ackList[l] == ',') listCount++;
      if (listCount == resRand) break;
    }

    if (strlen(ackList) < l + 2) {
      sprintf(resCode, "%s", "AA");
      return(1);

    } else {
      if (resRand == 0) l--;
      if (ackList[l + 1] == ',') return(1);
      resCode[0] = ackList[l + 1];
      if (ackList[l + 2] == ',') {
        resCode[1] = '\0';
        return(1);
      }
      resCode[1] = ackList[l + 2];
      resCode[2] = '\0';

    }
  }
  return(0);
}


// Send and ACK after receiving a message
int sendACK(int sessfd, char *hl7msg, int resType, char *ackList) {
  char dt[26] = "", cid[201] = "", errStr[256] = "";
  char ackBuf[1024] = "", resCode[3] = "AA";
  char *resCodeP = resCode;
  int writeL = 0;

  // Get current time and control ID of incoming message
  timeNow(dt, 0); 
  getHL7Field(hl7msg, "MSH", 10, cid);
  if (strlen(cid) == 0) sprintf(cid, "%s", "<UNKNOWN>");

  // Create the resCode if required
  if (resType > 0) getResCode(resType, ackList, resCodeP);

  sprintf(ackBuf, "%c%s%s%s%s%s%s%s%s%s%c%c", 0x0B, "MSH|^~\\&|||||", dt, "||ACK|", cid,
                                              "|P|2.4\rMSA|", resCodeP, "|", cid, "|OK\r",
                                              0x1C, 0x0D);

  if ((writeL = write(sessfd, ackBuf, strlen(ackBuf))) == -1) {
    close(sessfd);
    handleError(LOG_ERR, "Failed to send ACK response to server", -1, 0, 1);
    return(-1);

  } else {
    sprintf(errStr, "Message with control ID %s received OK and ACK (%s) sent", cid, resCode);
    writeLog(LOG_INFO, errStr, 1);
  }

  close(sessfd);
  return(writeL);
}


// Check incoming message for responder match and send response
static struct Response *checkResponse(char *msg, char *sIP, char *sPort,
                                      char *tName, int aTimeout) {

  struct Response *respHead = responses;
  struct json_object *resObj = NULL, *jArray = NULL, *matchObj = NULL;
  struct json_object *segStr = NULL, *fldObj = NULL, *valStr = NULL, *exclObj = NULL;
  struct json_object *minObj = NULL, *maxObj = NULL, *sendT = NULL, *respN = NULL;
  int m = 0, mCount = 0, fldInt = 0, exclInt = 0, minT = 0, maxT = 0;
  // TODO - check resFile, fldStr etc length and error handle
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
      return(responses);

    } else {
      sprintf(errStr, "Comparing %s against %s... no match",
              json_object_get_string(valStr), fldStr);
      writeLog(LOG_INFO, errStr, 1);
      return(responses);
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

  struct Response *resp;
  resp = calloc(1, sizeof(struct Response));
  resp->sendArgs = malloc(mCount * sizeof(char *));
  if (resp->sendArgs == NULL) {
    handleError(LOG_WARNING, "Could not allocate memory to create a response", 1, 0, 1);
    free(resp->sendArgs);
    free(resp);
    return(responses);
  }

  for (m = 0; m < mCount; m++) {
    matchObj = json_object_array_get_idx(jArray, m);
    segStr = json_object_object_get(matchObj, "segment");
    json_object_object_get_ex(matchObj, "field", &fldObj);
    fldInt = json_object_get_int(fldObj);
    getHL7Field(msg, (char *) json_object_get_string(segStr), fldInt, fldStr);
    tmpPtr = strdup(fldStr);
    resp->sendArgs[m] = tmpPtr;
  }

  // Get a random value between the min and max times
  int rndNum = 0;
  if (minT != maxT) getRand(minT, maxT, 0, NULL, &rndNum, NULL);

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
             mCount, resp->sendArgs, resStr, aTimeout, 0);
    resp->sent = 1;
    sprintf(resp->resCode, "%s", resStr);

  }

  // Add the response to the queue
  respHead = queueResponse(resp);
  sprintf(errStr, "Response queued, delivery in %ld secs", resp->sendTime - time(NULL));
  writeLog(LOG_INFO, errStr, 1);

  json_object_put(resObj);
  return(respHead);
}


// Handle an incoming message
static struct Response *handleMsg(int sessfd, int fd, char *sIP, char *sPort, int argc,
                                  int optind, char *argv[], int resType, char *ackList,
                                  int aTimeout) {

  struct Response *respHead = responses;
  int readSize = 1024, msgSize = 0, maxSize = readSize + msgSize, rcvSize = 1;
  int webErr = 0;
  int *ms = &maxSize;
  char rcvBuf[readSize + 1];
  memset(rcvBuf, 0, sizeof rcvBuf);
  char *msgBuf = calloc(1, readSize);
  char writeSize[11] = "";
  char errStr[306] = "";
  msgBuf[0] = '\0';
  rcvBuf[0] = '\0';

  // Receive the full message
  while (rcvSize > 0) {
    rcvBuf[0] = '\0';
    rcvSize = read(sessfd, rcvBuf, readSize);

    if (rcvSize == -1) {
      handleError(LOG_ERR, "Failed to read incoming message from server", 1, 0, 1);
      webErr = 1;
    }

    if (rcvSize > 0) {
      msgSize = msgSize + rcvSize;
      if ((msgSize + 1) > maxSize) msgBuf = dblBuf(msgBuf, ms, msgSize);
      strcat(msgBuf, rcvBuf);
      msgBuf[msgSize] = '\0';


      if (rcvBuf[rcvSize - 2] == 28 && rcvBuf[rcvSize - 1] == 13) {
        rcvSize = 0;
      }
    }
  }

  stripMLLP(msgBuf);
  if (sendACK(sessfd, msgBuf, resType, ackList) == -1) webErr = 1;

  // If we're responding, parse each respond template to see if msg matches
  if (argc > 0) {
    for (int i = optind; i < argc; i++) {
      sprintf(errStr, "Checking if incoming message matches responder: %s", argv[i]);
      writeLog(LOG_INFO, errStr, 1);
      respHead = checkResponse(msgBuf, sIP, sPort, argv[i], aTimeout);
      responses = respHead;
    }
  }

  if (webRunning == 1) {
    if (webErr > 0) {
      if (msgBuf)
        sprintf(msgBuf, "ERROR: The backend failed to receive or process a message from the sending server");
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
    printf("\n");
  }

  free(msgBuf);
  close(sessfd);
  return(responses);
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
    return(-1);
  }

  svrfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

  if (svrfd == -1) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't obtain socket address info", 1, 0, 1);
    return(-1);
  }

  int reuseaddr = 1;
  if (setsockopt(svrfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't set socket options", 1, 0, 1);
    return(-1);
  }

  if (bind(svrfd, res->ai_addr, res->ai_addrlen) == -1) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't bind address", 1, 0, 1);
    return(-1);
  }

  if (listen(svrfd, SOMAXCONN)) {
    freeaddrinfo(res);
    handleError(LOG_ERR, "Can't listen for connections", 1, 0, 1);
    return(-1);
  }

  freeaddrinfo(res);
  return(svrfd);
}


// Check the named pipe for response template updates
static int readRespTemps(int respFD, char respTemps[20][256], char *respTempsPtrs[20]) {
  struct json_object *rootObj = NULL, *dataArray = NULL, *dataObj = NULL;
  char readSizeBuf[11];
  int pLen = 1, updated = 0, dataInt = 0, i = 0;
  unsigned long int readSize = 0;

  while (pLen > 0) {
    if ((pLen = read(respFD, readSizeBuf, 11)) > 0) {
      readSize = atoi(readSizeBuf);

      char rBuf[readSize + 1];
      if ((pLen = read(respFD, rBuf, readSize)) > 0) {
        updated = 1;
        rBuf[readSize] = '\0';

        rootObj = json_tokener_parse(rBuf);
        json_object_object_get_ex(rootObj, "templates", &dataArray);

        if (dataArray == NULL) {
          handleError(LOG_ERR, "Invalid response templates list provided", 1, 0, 1);
          json_object_put(rootObj);
          return(1);

        } else {
          dataInt = json_object_array_length(dataArray);

          for (i = 0; i < dataInt; i++) {
            dataObj = json_object_array_get_idx(dataArray, i);
            sprintf(respTemps[i], "%s", json_object_get_string(dataObj));
            respTempsPtrs[i] = respTemps[i];
          }
        }
      }
    }
  }

  if (updated == 1) return(dataInt);
  return(0);
}


// Start listening for incoming messages
int startMsgListener(char *lIP, const char *lPort, char *sIP, char *sPort, int argc,
                     int optind, char *argv[], int resType, char *ackList, int aTimeout) {

  // TODO - malloc instead of limited resp templates
  char respTemps[20][256];
  char *respTempsPtrs[20];
  char errStr[58] = "";
  int svrfd = 0, sessfd = 0, fd = 0, rfd = 0, nextResp = -1, resU = 0, respUpdated = 0;
  time_t lastProcess = time(NULL), now;
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
          if (argc > 0) nextResp = processResponses(fd, aTimeout);
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
        return(-1);
      }

      if (respUpdated == 0) {
        responses = handleMsg(sessfd, fd, sIP, sPort, argc, optind, argv,
                              resType, ackList, aTimeout);
      } else {
        responses = handleMsg(sessfd, fd, sIP, sPort, respUpdated, 0, 
                              respTempsPtrs, resType, ackList, aTimeout);
      }

      close(sessfd);

      if ((now - lastProcess) >= procThrot) {
        if (argc > 0) nextResp = processResponses(fd, aTimeout);
        lastProcess = now;
      } else {
        nextResp = procThrot;
      }
    }
  }

  close(sessfd);
  close(svrfd);
  return(0);
}
