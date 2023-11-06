/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

#define _GNU_SOURCE

#include <microhttpd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <microhttpd.h>
#include <json.h>
#include "hhl7extern.h"
#include "hhl7net.h"
#include "hhl7utils.h"
#include "hhl7webpages.h"
#include "hhl7auth.h"
#include "hhl7json.h"
#include <errno.h>

// TODO - make these variable
#define PORT            8888
#define REALM           "\"Maintenance\""
#define COOKIE_NAME      "hhl7Session"
#define GET             0
#define POST            1
// TODO - allow more space if needed?
#define MAXNAMESIZE     1024
#define MAXANSWERSIZE   50 
#define POSTBUFFERSIZE  65536

// Global variables TODO - review these to see if we can not use globals
// TODO - IMPORTANT - webErrStr needs to go in the session somehow
char webErrStr[256] = "";
int webRunning = 0;
//int isListening = 0;
//int readFD = 0;
//pid_t wpid;


// Connection info
struct connection_info_struct {
  struct MHD_Connection *connection;
  int connectiontype;
  char *answerstring;
  struct MHD_PostProcessor *postprocessor;
  struct Session *session;
};

// Session info
struct Session {
  // Pointer to next session in linked list - TODO - change to hash table?
  struct Session *next;

  // Session id, previous session ID, connection counter, time last active
  char sessID[33];
  char oldSessID[33];
  int aStatus;
  unsigned int rc;
  time_t lastSeen;

  // User settings & data stored in session
  int pcaction;
  char userid[33];
  char sIP[256];
  char sPort[6];
  char lPort[6];
  int isListening;
  int readFD; // File descriptor for named pipe
  pid_t lpid; // Per user/session PID of web listener
};

// Linked list of sessions
static struct Session *sessions;

// Get the session ID from cookie or create a new session
static struct Session *getSession(struct MHD_Connection *connection) {
  struct Session *ret;
  const char *cookie;

  cookie = MHD_lookup_connection_value(connection, MHD_COOKIE_KIND, COOKIE_NAME);

  if (cookie != NULL) {
    // Try to find an existing session
    ret = sessions;
    while (NULL != ret) {
      if (strcmp(cookie, ret->sessID) == 0) {
        break;
      } else if (strcmp(cookie, ret->oldSessID) == 0) {
        break;
      }
      ret = ret->next;
    }
    if (NULL != ret) {
      ret->rc++;
      return ret;
    }
  }
  
  // If no session exists, create a new session
  ret = calloc (1, sizeof (struct Session));
  if (NULL == ret) {
    // TODO - error handle function
    fprintf(stderr, "calloc error");
    return NULL;
  }

  // TODO - create a better ID generation method?
  snprintf (ret->sessID,
            sizeof (ret->sessID),
            "%X%X%X%X",
            (unsigned int) rand (),
            (unsigned int) rand (),
            (unsigned int) rand (),
            (unsigned int) rand ());

  sprintf(ret->oldSessID, "%s", cookie);
  ret->rc++;
  ret->lastSeen = time(NULL);
  ret->next = sessions;
  sessions = ret;
  return ret;
}


// Add a session cookie to the browser
static void addCookie(struct Session *session, struct MHD_Response *response) {
  char cstr[strlen(COOKIE_NAME) + strlen(session->sessID) + 20];
  sprintf(cstr, "%s=%s; SameSite=Strict", COOKIE_NAME, session->sessID);
  if (MHD_add_response_header(response, MHD_HTTP_HEADER_SET_COOKIE, cstr) == MHD_NO) {
    // TODO - error handle
    fprintf (stderr, "Failed to set session cookie header!\n");
  }
}


// Callback function for when a request is completed
static void reqComplete(void *cls, struct MHD_Connection *connection,
                        void **con_cls, enum MHD_RequestTerminationCode toe) {

  struct connection_info_struct *request = *con_cls;
  (void) cls;         // Unused. Silent compiler warning.
  (void) connection;  // Unused. Silent compiler warning.
  (void) toe;         // Unused. Silent compiler warning.

  if (request == NULL)
    return;
  if (request->session != NULL)
    request->session->rc--;
  // TODO - commented out due to seg fault, check for memory leak
  //if (NULL != request->postprocessor)
  //  MHD_destroy_post_processor (request->postprocessor);
  free (request);
}


// Send error message web browser
static enum MHD_Result webErrXHR(struct Session *session,
                                 struct MHD_Connection *connection) {
  enum MHD_Result ret;
  struct MHD_Response *response;

  response = MHD_create_response_from_buffer(strlen(webErrStr), (void *) webErrStr,
             MHD_RESPMEM_MUST_COPY);

  if (!response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  //addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  webErrStr[0] = '\0';
  return ret;
}


static enum MHD_Result requestLogin(struct MHD_Connection *connection) {
  enum MHD_Result ret;
  struct MHD_Response *response;

  response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);

  if (! response)
    return MHD_NO;

  ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, response);
  MHD_destroy_response(response);
  return ret;
}


static enum MHD_Result main_page(struct Session *session,
                                 struct MHD_Connection *connection) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  const char *page = mainPage;

  response = MHD_create_response_from_buffer(strlen(page), (void *) page,
             MHD_RESPMEM_PERSISTENT);

  if (! response) return MHD_NO;

  addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}


static enum MHD_Result getImage(struct Session *session,
                                struct MHD_Connection *connection, const char *url) {

  struct MHD_Response *response;
  unsigned char *buffer = NULL;
  FILE *fp;
  int ret;
  const char *errorstr ="<html><body>An internal server error has occurred!</body></html>";

  // TODO change this to work with /usr/share or ./?
  char *serverURL = malloc(strlen(url) + 2);
  strcpy(serverURL, ".");
  strcat(serverURL, url);

  fp = openFile(serverURL, "r");
  int fSize = getFileSize(serverURL); 

  // Error accessing file
  if (fp == NULL) {
    fclose(fp);
    free(serverURL);

    response = MHD_create_response_from_buffer(strlen (errorstr), 
                                               (void *) errorstr,
                                               MHD_RESPMEM_PERSISTENT);
    if (response) {
      //addCookie(session, response);
      ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
      MHD_destroy_response(response);
      return MHD_YES;

    } else {
      return MHD_NO;
    }

    if (!ret) {
      if (buffer) free(buffer);
    
      response = MHD_create_response_from_buffer(strlen(errorstr),
                                                 (void*) errorstr,
                                                 MHD_RESPMEM_PERSISTENT);

      if (response) {
        // TODO - no cookie needed here, but check everywhere else
        //addCookie(session, response);
        ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        fclose(fp);
        return MHD_YES;    

      } else {
        fclose(fp);
        return MHD_NO;
      }
    }
  }

  // Successful image retrieval
  response = MHD_create_response_from_fd_at_offset64(fSize, fileno(fp), 0);
  MHD_add_response_header(response, "Content-Type", "image/png");
  //addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  free(serverURL);
  return ret;
}


// Get the list of servers that we can send to from the servers config file
static enum MHD_Result getServers(struct Session *session,
                                   struct MHD_Connection *connection,
                                   struct connection_info_struct *con_info) {
  enum MHD_Result ret;
  struct MHD_Response *response;

  // TODO - change svr file location (check for other references to it as well)
  char svrsFile[] = "./conf/servers.hhl7";
  struct json_object *svrsObj = NULL, *svrObj = NULL;

  svrsObj = json_object_from_file(svrsFile);
  if (svrsObj == NULL) {
    // TODO - another error to handle
    printf("Failed to read server config file\n");
    exit(1);
  }

  json_object_object_get_ex(svrsObj, "servers", &svrObj);
  if (svrObj == NULL) {
    // TODO - Another error to pass to web
    printf("Failed to get server object, server file corrupt?\n");
    exit(1);
  }

  const char *svrList = json_object_to_json_string_ext(svrObj, JSON_C_TO_STRING_PLAIN);

  response = MHD_create_response_from_buffer(strlen(svrList), (void *) svrList,
                                             MHD_RESPMEM_MUST_COPY);

  if (!response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  json_object_put(svrObj);
  return ret;
}


// Get users settings from the password file
static enum MHD_Result getSettings(struct Session *session,
                                   struct MHD_Connection *connection,
                                   struct connection_info_struct *con_info) {
  enum MHD_Result ret;
  struct MHD_Response *response;

  // TODO - change pw file location (check for other references to it as well)
  char pwFile[] = "./conf/passwd.hhl7";
  struct json_object *resObj = json_object_new_object();
  struct json_object *pwObj = NULL, *userArray = NULL, *userObj = NULL;
  struct json_object  *uidStr = NULL, *sIP = NULL, *sPort = NULL, *lPort = NULL;
  int uCount = 0, u = 0;

  char *uid = session->userid;

  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    // TODO - another error to handle
    printf("Failed to read password file\n");
    exit(1);
  }

  json_object_object_get_ex(pwObj, "users", &userArray);
  if (userArray == NULL) {
    // Another error to pass to web
    printf("Failed to get user object, passwd file corrupt?\n");
    exit(1);
  }

  // TODO - using a lot of for i in userArrays throughout code, hash table?
  uCount = json_object_array_length(userArray);
  for (u = 0; u < uCount; u++) {
    userObj = json_object_array_get_idx(userArray, u);
    uidStr = json_object_object_get(userObj, "uid");

    if (strcmp(json_object_get_string(uidStr), uid) == 0) {
      sIP = json_object_object_get(userObj, "sIP");
      sPort = json_object_object_get(userObj, "sPort");
      lPort = json_object_object_get(userObj, "lPort");

      // Push the connection settings from passwd file to live session
      sprintf(session->sIP, "%s", json_object_get_string(sIP));
      sprintf(session->sPort, "%s", json_object_get_string(sPort));
      sprintf(session->lPort, "%s", json_object_get_string(lPort));

      json_object_object_add(resObj, "sIP", sIP);
      json_object_object_add(resObj, "sPort", sPort);
      json_object_object_add(resObj, "lPort", lPort);

      break;
    }
  }

  const char *resStr = json_object_to_json_string_ext(resObj, JSON_C_TO_STRING_PLAIN);

  response = MHD_create_response_from_buffer(strlen(resStr), (void *) resStr,
             MHD_RESPMEM_PERSISTENT);

  if (!response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  json_object_put(pwObj);
  return ret;
}


// Get a list of template files and return them as a set of <select> <option>s
static enum MHD_Result getTemplateList(struct Session *session,
                                       struct MHD_Connection *connection) {

  enum MHD_Result ret;
  struct MHD_Response *response;
  DIR *dp;
  FILE *fp = 0;
  struct dirent *file;
  // TODO: Get templates from all possible directories...
  const char *tPath = "/usr/local/share/hhl7/templates/";
  const char *nOpt = "<option value=\"None\">None</option>\n";
  char fName[128], tName[128], fullName[128+strlen(tPath)], *ext; //, *tempOpts;
  char *newPtr;

  // TODO - WORKING - check malloc here
  char *tempOpts = malloc(36);
  if (tempOpts == NULL) {
    // TODO - error handle
    fprintf(stderr, "ERROR: Cannot cannot allocate memory for template list.\n");
    exit(1);
  }

  strcpy(tempOpts, nOpt);

  dp = opendir(tPath); 
  if (dp == NULL) {
    // TODO - error handle
    fprintf(stderr, "ERROR: Cannot open path: %s\n", tPath);
    exit(1);
  }

  while ((file = readdir(dp)) != NULL) {
    ext = 0; 
    if (file->d_type == DT_REG) {
      ext = strrchr(file->d_name, '.');
      if (strcmp(ext, ".json") == 0) {
        // Create a template name and full path to file from filename
        // TODO - long template names (>128) will break - OK? At least error handle
        memcpy(fName, file->d_name, strlen(file->d_name) - strlen(ext));
        fName[strlen(file->d_name) - strlen(ext)] = '\0';
        strcpy(fullName, tPath);
        strcat(fullName, file->d_name);

        // TODO - change to use json library get json from file
        // Read the template and check if it's hidden:
        int fSize = getFileSize(fullName);
        char *jsonMsg = malloc(fSize + 1);
        fp = openFile(fullName, "r");
        readJSONFile(fp, fSize, jsonMsg);

        // If not hidden, add it to the option list
        if (getJSONValue(jsonMsg, 1, "hidden", NULL) != 1) {
          // Get the name of the template, otherwise use filename
          if (getJSONValue(jsonMsg, 2, "name", tName) == -1) {
            strcpy(tName, fName);
          }

          // Increase memory for tempOpts to allow file name etc
          newPtr = realloc(tempOpts, (2*strlen(fName)) + strlen(tempOpts) + 37);
          if (newPtr == NULL) {
            // TODO - error handle
            fprintf(stderr, "ERROR: Can't allocatate memory\n");
          } else {
            tempOpts = newPtr;
          }

          sprintf(tempOpts + strlen(tempOpts), "%s%s%s%s%s", "<option value=\"",
                                               fName, ".json\">", tName, "</option>\n");
        }

        // Free memory
        fclose(fp);
        free(jsonMsg);
      }
    }
  }

  // Succesful template list response
  response = MHD_create_response_from_buffer(strlen(tempOpts), (void *) tempOpts,
             MHD_RESPMEM_MUST_COPY);

  closedir(dp);
  free(tempOpts);

  if (! response) return MHD_NO;

  addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

static enum MHD_Result getTempForm(struct Session *session,
                                   struct MHD_Connection *connection, const char *url) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  FILE *fp;

  int webFormS = 1024;
  int webHL7S = 1024;
  char *webForm = malloc(webFormS);
  char *webHL7 = malloc(webHL7S);
  char *jsonReply = malloc(1);
  webForm[0] = '\0';
  webHL7[0] = '\0';
  jsonReply[0] = '\0';

  // TODO: Get templates from all possible directories...
  char *tPath = "/usr/local/share/hhl7";
  char *fileName = malloc(strlen(tPath) + strlen(url) + 1);

  strcpy(fileName, tPath);
  strcat(fileName, url);

  // TODO - use json from file function
  fp = openFile(fileName, "r");
  int fSize = getFileSize(fileName); 
  char *jsonMsg = malloc(fSize + 1);

  // Read the json template to jsonMsg
  readJSONFile(fp, fSize, jsonMsg);

  // Generate HL7 based on the json template
  parseJSONTemp(jsonMsg, &webHL7, &webHL7S, &webForm, &webFormS, 0, NULL, 1);

  // TODO - rework this... temp fix cos pub time! escapeSlash should malloc
  char tmpBuf[2 * strlen(webHL7) + 1];
  escapeSlash(tmpBuf, webHL7);
  tmpBuf[strlen(tmpBuf)] = '\0';

  // Construct the JSON reply
  // TODO add newPtr in case of memory allocation failure
  jsonReply = realloc(jsonReply, strlen(webForm) + strlen(webHL7) + 53);
  strcpy(jsonReply, "{ \"form\":\"");
  strcat(jsonReply, webForm);
  strcat(jsonReply, "\",\n\"hl7\":\"");
  strcat(jsonReply, tmpBuf);
  strcat(jsonReply, "\" }");

  // Succesful template list response
  response = MHD_create_response_from_buffer(strlen(jsonReply), (void *) jsonReply,
             MHD_RESPMEM_MUST_COPY);

  // Free memory
  fclose(fp);
  free(webForm);
  free(webHL7);
  free(jsonMsg);
  free(jsonReply);

  if (! response) return MHD_NO;
  addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}


static enum MHD_Result sendHL72Web(struct Session *session,
                                   struct MHD_Connection *connection, int fd) {

  enum MHD_Result ret;
  struct MHD_Response *response;
  int fBufS = 2048, rBufS = 512;
  char *fBuf = malloc(fBufS);
  char *rBuf = malloc(rBufS);
  char readSizeBuf[11];
  int p = 0, pLen = 1, maxPkts = 250, readSize = 0;

  if (strlen(webErrStr) > 0) {
    free(fBuf);
    free(rBuf);
    // TODO change to session error handler
    return webErrXHR(session, connection);
  }

  strcpy(fBuf, "event: rcvHL7\ndata: ");

  while (pLen > 0 && p < maxPkts) {
    if ((pLen = read(fd, readSizeBuf, 11)) > 0) {
      readSize = atoi(readSizeBuf);

      if (readSize > rBufS) {
        char *rBufNew = realloc(rBuf, readSize + 1);
        if (rBufNew == NULL) {
          // TODO - error handle
          fprintf(stderr, "ERROR: Can't allocatate memory\n");

        } else {
          rBuf = rBufNew;
          rBufS = readSize;

        }
      }

      if ((pLen = read(fd, rBuf, readSize)) > 0) {
        rBuf[readSize] = '\0';
        hl72web(rBuf);

        if (strlen(fBuf) + strlen(rBuf) + 50 > fBufS) {
          fBufS = strlen(fBuf) + strlen(rBuf) + 50;
          char *fBufNew = realloc(fBuf, fBufS);
          if (fBufNew == NULL) {
            fprintf(stderr, "ERROR: Can't allocatate memory\n");

          } else {
            fBuf = fBufNew;

          }
        }

        strcat(fBuf, rBuf);
        strcat(fBuf, "<br />");
        p++;
      }
    }
  }

  strcat(fBuf, "\nretry:500\n\n");

  if (p == 0) {
    strcpy(fBuf, "event: rcvHL7\ndata: HB\nretry:500\n\n");
  }
  response = MHD_create_response_from_buffer(strlen(fBuf), (void *) fBuf,
             MHD_RESPMEM_MUST_COPY);

  if (!response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/event-stream");
  MHD_add_response_header(response, "Cache-Control", "no-cache");
  addCookie(session, response);

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  // Free memory from buffers
  free(fBuf);
  free(rBuf);

  return ret;
}


// TODO - clean old fifos (when process exits we don't clean them

// Remove the named pipe and kill the listening child process if required
static void cleanSession(struct Session *session) {
  // Remove the listeners named pipe if it exists
  if (session->readFD > 0) {
    char hhl7fifo[21];
    sprintf(hhl7fifo, "%s%d", "/tmp/hhl7fifo.", session->lpid);
    close(session->readFD);
    unlink(hhl7fifo);
  }

  // Stop the listening child process if running
  if (session->lpid > 0) {
    kill(session->lpid, SIGTERM);
    session->lpid = 0;
  }
}


// Stop the backend listening for packets from hl7 server
static enum MHD_Result stopListenWeb(struct Session *session,
                                     struct MHD_Connection *connection) {

  enum MHD_Result ret;
  struct MHD_Response *response;

  cleanSession(session);
  session->isListening = 0;

  // TODO We should probably check the listener actually stopped and retry a few times
  response = MHD_create_response_from_buffer(2, "OK", MHD_RESPMEM_PERSISTENT);

  if (!response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");
  addCookie(session, response);

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}


// Start a HL7 Listener in a forked child process
static void startListenWeb(struct Session *session, struct MHD_Connection *connection) {
  // Make children ignore waiting for parent process before closing
  signal(SIGCHLD, SIG_IGN);

  // Ensure we've stopped the previous listener before starting another
  if (session->lpid > 0) {
    stopListenWeb(session, connection);
  }

  pid_t pidBefore = getpid();
  session->lpid = fork();

  if (session->lpid == 0) {
    // Make the child exit if the  parent exits
    int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
    // TDOD - error handle
    if (r == -1 || getppid() != pidBefore) {
      exit(0);
    }

    // TODO - locahost should be OK for listening, but double check if we need a bind add
    startMsgListener("127.0.0.1", session->lPort);
    _exit(0);

  }

  // Create a named pipe to read from
  char hhl7fifo[21]; 
  sprintf(hhl7fifo, "%s%d", "/tmp/hhl7fifo.", session->lpid);
  mkfifo(hhl7fifo, 0666);
  session->readFD = open(hhl7fifo, O_RDONLY | O_NONBLOCK);
  session->isListening = 1;
  //fcntl(readFD, F_SETPIPE_SZ, 1048576);
}


// TODO - iterate post getting too big, split to functions?
static enum MHD_Result iterate_post(void *coninfo_cls, enum MHD_ValueKind kind,
              const char *key, const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off, size_t size) {

  struct connection_info_struct *con_info = coninfo_cls;

  (void) kind;               /* Unused. Silent compiler warning. */
  (void) filename;           /* Unused. Silent compiler warning. */
  (void) content_type;       /* Unused. Silent compiler warning. */
  (void) transfer_encoding;  /* Unused. Silent compiler warning. */
  (void) off;                /* Unused. Silent compiler warning. */

  int sockfd;
  char resStr[3] = "";
  int aStatus = -1, nStatus = -1;
  char *answerstring;
  answerstring = malloc(MAXANSWERSIZE);
  if (!answerstring) return MHD_NO;

  // A new connection should have already found it's sesssion, but check to be safe
  if (con_info->session == NULL) {
    con_info->session = getSession(con_info->connection);
  }

  if ((size > 0) && (size <= MAXNAMESIZE)) {
    // TODO - if we don't add logging here, change login to avoid pointless else
    // Security check, only these pages are allowed without already being authed
    if (con_info->session->aStatus < 1) {
      if (strcmp(key, "pcaction") == 0 || strcmp(key, "uname") == 0 ||
          strcmp(key, "pword") == 0) {
        // Log?
      } else {
        snprintf(answerstring, 3, "%s", "L0");  // Require a login
        con_info->answerstring = answerstring;
        return MHD_YES;
      }
    }

    if (strcmp (key, "hl7MessageText") == 0 ) {
      char newData[strlen(data) + 5];
      strcpy(newData, data);
      wrapMLLP(newData);

      sockfd = connectSvr(con_info->session->sIP, con_info->session->sPort);
      // TODO - Sock number increases with each message? free? 
      //printf("Debug SOCK: %d\n", sockfd);
 
      if (sockfd >= 0) {
        sendPacket(sockfd, newData, resStr);

        // TODO - Why are we adding newData here? surely just resStr will be fine?
        snprintf(answerstring, MAXANSWERSIZE, resStr, newData);
        con_info->answerstring = answerstring;

      } else {
        // TODO - can't open socket error message - maybe handled by error handler
      }  

    } else if (strcmp(key, "pcaction") == 0 ) {
      // TODO - sanitise username and password
      con_info->session->pcaction = atoi(data);
      // Temporarily list the answer code as DP (partial data) until we receive passwd
      snprintf(answerstring, 3, "%s", "DP");

    } else if (strcmp(key, "uname") == 0 ) {
      // TODO sanitise username and password
      sprintf(con_info->session->userid, "%s", data);

      // Temporarily list the answer code as DP (partial data) until we receive passwd
      snprintf(answerstring, 3, "%s", "DP");

    } else if (strcmp(key, "pword") == 0 ) {
      // Return partial data code if invalid request
      if (strlen(con_info->session->userid) == 0 || con_info->session->pcaction < 1) {
        snprintf(answerstring, 3, "%s", "DP");

      } else {
        // TODO - using aStatus as a variable here when it's also the session status is confusing, change one?
        aStatus = checkAuth(con_info->session->userid, data);

        if (aStatus == 3 && con_info->session->pcaction == 2) {
          nStatus = regNewUser(con_info->session->userid, (char *) data);

          if (nStatus == 0) {
            snprintf(answerstring, 3, "%s", "LC");  // New account created
          } else {
            snprintf(answerstring, 3, "%s", "LF");  // New account creation failure
          }

        } else if (aStatus == 3 && con_info->session->pcaction == 1) {
          snprintf(answerstring, 3, "%s", "LR");  // Reject login, no account

        } else if (aStatus == 2) {
          snprintf(answerstring, 3, "%s", "LR");  // Reject login, wrong password

        } else if (aStatus == 1) {
          snprintf(answerstring, 3, "%s", "LD");  // Account disabled, but login OK

        } else if (aStatus == 0 && con_info->session->pcaction == 3) {
          con_info->session->aStatus = 2;
          snprintf(answerstring, 3, "%s", "DP");

        } else if (aStatus == 0) {
          con_info->session->aStatus = 1;
          snprintf(answerstring, 3, "%s", "LA");  // Accept login

        } else {
          snprintf(answerstring, 3, "%s", "DP");
 
        }
      }

      // TODO - can't overwrite data memory as it's defined as const in libmicrohttpd
      // Overwrite password with nulls for security, we should always compile
      // with -fno-builtin-memset to ensure no memset optimisation
      //memset(data, '\0', strlen(data));
      con_info->answerstring = answerstring;

    } else if (strcmp(key, "npword") == 0 ) {
      // TODO - recheck auth here? Seems unrestrictive on chaging a password
      snprintf(answerstring, 3, "%s", "SX");
      if (con_info->session->aStatus == 2) {
        con_info->session->aStatus = 1;
        if (updatePasswd(con_info->session->userid, data) == 0) {
          snprintf(answerstring, 3, "%s", "OK");  // Password updated succesfully
        }
      }

      // TODO - Overwrite password with nulls for security
      //memset(data, '\0', strlen(data));
      con_info->answerstring = answerstring;

    } else if (strcmp(key, "sIP") == 0 ) {
      if (updatePasswdFile(con_info->session->userid, key, data) == 0) {
        sprintf(con_info->session->sIP, "%s", data);
        snprintf(answerstring, 3, "%s", "OK");  // Save OK

      } else {
        snprintf(answerstring, 3, "%s", "SX");  // Save failed

      }
      con_info->answerstring = answerstring;

    } else if (strcmp(key, "sPort") == 0 ) {
      if (updatePasswdFile(con_info->session->userid, key, data) == 0) {
        sprintf(con_info->session->sPort, "%s", data);
        snprintf(answerstring, 3, "%s", "OK");  // Save OK

      } else {
        snprintf(answerstring, 3, "%s", "SX");  // Save failed

      }
      con_info->answerstring = answerstring;

    } else if (strcmp(key, "lPort") == 0 ) {
      if (lPortUsed(con_info->session->userid, data) == 1) {
        snprintf(answerstring, 3, "%s", "SR");  // Save Rejected (port in use)

      } else if (updatePasswdFile(con_info->session->userid, key, data) == 0) {
        sprintf(con_info->session->lPort, "%s", data);
        stopListenWeb(con_info->session, con_info->connection);
        startListenWeb(con_info->session, con_info->connection);
        snprintf(answerstring, 3, "%s", "OK");  // Save OK

      } else {
        snprintf(answerstring, 3, "%s", "SX");  // Save failed

      }
      con_info->answerstring = answerstring;

    }

  } else {
    // No post data was received, reply with code "D0" (no data)
    strcpy(answerstring, "D0");
    con_info->answerstring = answerstring;
    return MHD_NO;

  }
  return MHD_YES;
}


// TODO - Use this instead of all the individual functions for pages?
static enum MHD_Result sendPage(struct Session *session,
                                 struct MHD_Connection *connection,
                                 const char* connectiontype, const char *page) {

  enum MHD_Result ret;
  struct MHD_Response *response;

  if (strlen(webErrStr) > 0) {
    return webErrXHR(session, connection);
  }

  response = MHD_create_response_from_buffer(strlen(page), (void *) page, MHD_RESPMEM_PERSISTENT);

  if (! response) return MHD_NO;
  if (strcmp(connectiontype, "POST") == 0) {
    MHD_add_response_header(response, "Content-Type", "text/plain");
  }

  if (strcmp(page, "L0") == 0 || strcmp(page, "LR") == 0 ||strcmp(page, "LD") == 0) {
    ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, response);

  } else if (strcmp(page, "DP") == 0) {
    ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);

  } else {
    addCookie(session, response);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  }

  MHD_destroy_response(response);
  return ret;
}


// Logout of the web front end and close backend session
static enum MHD_Result logout(struct Session *session, struct MHD_Connection *connection) {
  enum MHD_Result ret;
  struct MHD_Response *response;

  session->sessID[0] = '\0';

  response = MHD_create_response_from_buffer(2, "LO", MHD_RESPMEM_PERSISTENT);

  if (! response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/plain");
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}


// Debug function used for showing connection values, e.g user-agent
//static int print_out_key (void *cls, enum MHD_ValueKind kind, const char *key,
//               const char *value) {
//  printf ("%s: %s\n", key, value);
//  return MHD_YES;
//}


// TODO - Using the web's listen interface breaks the post interface...
static enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                            const char *url, const char *method,
                                            const char *version, const char *upload_data,
                                            size_t *upload_data_size, void **con_cls) {

  (void) version;           /* Unused. Silence compiler warning. */

  struct connection_info_struct *con_info = *con_cls;
  struct Session *session;

  // Debug - print connection values, e.g user-agent
  //MHD_get_connection_values(connection, MHD_HEADER_KIND, print_out_key, NULL);

  // TODO change this to /use/local/share
  char *imagePath = "/images/";
 
  if (*con_cls == NULL) {
    con_info = malloc(sizeof(struct connection_info_struct));
    if (con_info == NULL) return MHD_NO;
    con_info->connection = connection;
    con_info->session = getSession(connection);
    con_info->answerstring = NULL;

    if (strcmp(method, "POST") == 0) {
      con_info->postprocessor = MHD_create_post_processor(connection, POSTBUFFERSIZE,
                                                     iterate_post, (void *) con_info);

      if (con_info->postprocessor == NULL) {
        free(con_info);
        return MHD_NO;
      }

      con_info->connectiontype = POST;

    } else {
      con_info->connectiontype = GET;
    }

    *con_cls = (void *) con_info;
    return MHD_YES;
  }

  if (con_info->session == NULL) {
    con_info->session = getSession(connection);

    if (con_info->session == NULL) {
      fprintf (stderr, "Failed to setup session for `%s'\n", url);
      return MHD_NO;
    }
  }
  session = con_info->session;
  session->lastSeen = time(NULL);

  if (strcmp(method, "GET") == 0) {
    if (strstr(url, imagePath)) {
      return getImage(session, connection, url);

    } else if (strstr(url, "/templates/")) {
      return getTempForm(session, connection, url);

    } else if (strcmp(url, "/getTemplateList") == 0) {
      // Request login if not logged in
      if (session->aStatus != 1) return requestLogin(connection);
      return getTemplateList(session, connection);

    } else if (strcmp(url, "/getServers") == 0) {
      // Request login if not logged in
      if (session->aStatus != 1) return requestLogin(connection);
      return getServers(session, connection, con_info);

    } else if (strcmp(url, "/getSettings") == 0) {
      // Request login if not logged in
      if (session->aStatus != 1) return requestLogin(connection);
      return getSettings(session, connection, con_info);

    } else if (strcmp(url, "/stopListenHL7") == 0) {
      if (session->aStatus != 1) return requestLogin(connection);
      return stopListenWeb(session, connection);

    } else if (strcmp(url, "/logout") == 0) {
      if (session->aStatus != 1) return requestLogin(connection);
      return logout(session, connection);

    // TODO pull this out to a function?? 
    } else if (strcmp(url, "/listenHL7") == 0) {
      if (session->aStatus != 1) return requestLogin(connection);
      if (session->isListening == 0) {
        startListenWeb(con_info->session, con_info->connection);
      }

      if (session->isListening == 1) {
        return sendHL72Web(session, connection, session->readFD);

      } else {
        session->isListening = 1;
        return MHD_YES;
      }

    } else {
      return main_page(session, connection);
    }
  }

  if (strcmp(method, "POST") == 0) {
    if (*upload_data_size != 0) {
      MHD_post_process(con_info->postprocessor, upload_data, *upload_data_size);
      *upload_data_size = 0;
      return MHD_YES;

    } else if (NULL != con_info->answerstring) {
      return sendPage(session, connection, method, con_info->answerstring);

    }
  }

  // TODO - error page
  return sendPage(session, connection, method, errorPage);
}


// Clear down old sessions
static void expireSessions() {
  struct Session *pos;
  struct Session *prev;
  struct Session *next;
  time_t now;

  now = time(NULL);
  prev = NULL;
  pos = sessions;

  while (NULL != pos) {
    next = pos->next;
    // Expire old sessions after 1 hour
    if (now - pos->lastSeen > 3600) {
      cleanSession(pos);

      if (NULL == prev) {
        sessions = pos->next;
      } else {
        prev->next = next;
      }
      free (pos);

    } else {
      prev = pos;
    }

    pos = next;
  }
}


// Start the web interface
int listenWeb() {
  struct MHD_Daemon *daemon;

  #define SKEY "server.key"
  #define SPEM "server.pem"

  int keySize = getFileSize(SKEY);
  int pemSize = getFileSize(SPEM);
  char key_pem[keySize + 1];
  char cert_pem[pemSize + 1];
  FILE *fpSKEY = openFile(SKEY, "r");
  FILE *fpSPEM = openFile(SPEM, "r");

  file2buf(key_pem, fpSKEY, keySize);
  file2buf(cert_pem, fpSPEM, pemSize);
  fclose(fpSKEY);
  fclose(fpSPEM);

  if ((key_pem == NULL) || (cert_pem == NULL)) {
    fprintf(stderr, "ERROR: The HTTPS key/certificate files could not be read.\n");
    exit(1);
  }

  struct timeval tv;
  struct timeval *tvp;
  fd_set rs;
  fd_set ws;
  fd_set es;
  MHD_socket max;
  MHD_UNSIGNED_LONG_LONG mhd_timeout;

  daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_TURBO |
                            MHD_USE_TLS | MHD_USE_TCP_FASTOPEN,
                            PORT,
                            NULL, NULL,
                            &answer_to_connection, NULL,
                            MHD_OPTION_HTTPS_MEM_KEY, key_pem,
                            MHD_OPTION_HTTPS_MEM_CERT, cert_pem,
                            MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15,
                            MHD_OPTION_NOTIFY_COMPLETED,
                            &reqComplete, NULL,
                            MHD_OPTION_END);

  if (daemon == NULL) {
    fprintf(stderr, "ERROR: Failed to start HTTPS daemon.\n");
    exit(1);

  } else {
    webRunning = 1;
  }


  while (1) {
    expireSessions();

    max = 0;
    FD_ZERO (&rs);
    FD_ZERO (&ws);
    FD_ZERO (&es);

    if (MHD_YES != MHD_get_fdset(daemon, &rs, &ws, &es, &max)) break; // internal error

    if (MHD_get_timeout(daemon, &mhd_timeout) == MHD_YES) {
      tv.tv_sec = mhd_timeout / 1000;
      tv.tv_usec = (mhd_timeout - (tv.tv_sec * 1000)) * 1000;
      tvp = &tv;

    } else {
      tvp = NULL;

    }

    if (select(max + 1, &rs, &ws, &es, tvp) == -1) {
      if (EINTR != errno) {
        fprintf(stderr, "Aborting due to error during select: %s\n", strerror(errno));
      }
      break;
    }

    MHD_run(daemon);
  }

  MHD_stop_daemon(daemon);
  return 0;
}
