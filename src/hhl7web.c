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
#include <syslog.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <microhttpd.h>
#include <json.h>
#include <dirent.h>
#include <fcntl.h>
#include "hhl7extern.h"
#include "hhl7net.h"
#include "hhl7utils.h"
#include "hhl7webpages.h"
#include "hhl7auth.h"
#include "hhl7json.h"
#include <errno.h>

#define REALM           "\"Maintenance\""
#define COOKIE_NAME      "hhl7Session"
#define GET             0
#define POST            1
#define MAXANSWERSIZE   3
#define POSTBUFFERSIZE  65536


// Global variables
int webRunning = 0;


// Connection info
struct connection_info_struct {
  struct MHD_Connection *connection;
  int connectiontype;
  char answerstring[3];
  char *poststring;
  struct MHD_PostProcessor *postprocessor;
  struct Session *session;
};


// Session info
struct Session {
  struct Session *next;

  // Session id, previous session ID, connection counter, time last active
  char sessID[33];
  char oldSessID[33];
  int shortID;
  int aStatus;
  unsigned int rc;
  time_t lastSeen;

  // User settings & data stored in session
  int pcaction;
  char userid[33];
  char sIP[256];
  char sPort[6];
  char lPort[6];
  int ackTimeout;
  int webTimeout;
  int isListening;
  int readFD; // File descriptor for listen named pipe
  int respFD; // File descriptor for listen named pipe
  pid_t lpid; // Per user/session PID of web listener
};


// Linked list of sessions
static struct Session *sessions;


// Get the session ID from cookie or create a new session
static struct Session *getSession(struct MHD_Connection *connection) {
  struct Session *ret;
  const char *cookie;
  int maxShortID = 0, baseSession = 0;
  char errStr[122] = "";

  if (connection) {
    cookie = MHD_lookup_connection_value(connection, MHD_COOKIE_KIND, COOKIE_NAME);

    if (cookie != NULL) {
      sprintf(errStr, "Retrieved session from cookie: %s", cookie);
      writeLog(LOG_DEBUG, errStr, 0);

      // Try to find an existing session
      ret = sessions;
      while (ret != NULL) {
        if (ret->shortID > maxShortID) maxShortID = ret->shortID;
        if (strcmp(cookie, ret->sessID) == 0) {
          break;
        } else if (strcmp(cookie, ret->oldSessID) == 0) {
          break;
        }
        ret = ret->next;
      }

      if (ret != NULL) {
        sprintf(errStr, "[S: %03d] Session matched to an active session: %s",
                        ret->shortID, ret->sessID);
        writeLog(LOG_DEBUG, errStr, 0);

        ret->rc++;
        return(ret);
      }
    }

  } else {
    baseSession = 1;
  }

  // If no session exists, create a new session
  ret = calloc(1, sizeof (struct Session));
  if (ret == NULL) {
    handleError(LOG_ERR, "Could not allocate memory to create session ID, out of memory?", -1, 0, 0);
    return(NULL);
  }

  // Generate a session ID
  snprintf (ret->sessID,
            sizeof (ret->sessID),
            "%X%X%X%X",
            (unsigned int) rand (),
            (unsigned int) rand (),
            (unsigned int) rand (),
            (unsigned int) rand ());

  if (baseSession == 0) {
    sprintf(ret->oldSessID, "%s", cookie);
  } else {
    sprintf(ret->oldSessID, "%s", "BaseSession");
  }

  ret->shortID = maxShortID + 1;
  ret->rc++;
  ret->lastSeen = time(NULL);
  ret->next = sessions;
  sessions = ret;

  sprintf(errStr, "[S: %03d] No active session found for: %s, created: %s",
                  ret->shortID, ret->oldSessID, ret->sessID);
  writeLog(LOG_INFO, errStr, 0);

  return(ret);
}


// Add a session cookie to the browser
static void addCookie(struct Session *session, struct MHD_Response *response) {
  char errStr[73] = "";
  char cstr[strlen(COOKIE_NAME) + strlen(session->sessID) + 20];
  sprintf(cstr, "%s=%s; SameSite=Strict", COOKIE_NAME, session->sessID);

  if (MHD_add_response_header(response, MHD_HTTP_HEADER_SET_COOKIE, cstr) == MHD_NO) {
    sprintf(errStr, "[S: %03d] Failed to set session cookie header", session->shortID);
    handleError(LOG_WARNING, errStr, -1, 0, 0);

  } else {
    sprintf(errStr, "[S: %03d] Cookie session header added: %s",
                    session->shortID, session->sessID);
    handleError(LOG_DEBUG, errStr, -1, 0, 0);

  }
}


// Callback function for when a request is completed
static void reqComplete(void *cls, struct MHD_Connection *connection,
                        void **con_cls, enum MHD_RequestTerminationCode toe) {

  struct connection_info_struct *request = *con_cls;
  (void) cls;         // Unused. Silent compiler warning.
  (void) connection;  // Unused. Silent compiler warning.
  (void) toe;         // Unused. Silent compiler warning.

  if (request == NULL) return;
  if (request->session != NULL) request->session->rc--;

  if (request->connectiontype == POST) {
    MHD_destroy_post_processor(request->postprocessor);
  }
  free(request);
  request = NULL;
}


// Send a 401 unauth'd to the client to request a login
static enum MHD_Result requestLogin(struct Session *session,
                                    struct MHD_Connection *connection, const char *url) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  char errStr[300] = "";

  response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);

  if (! response)
    return(MHD_NO);

  ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, response);
  sprintf(errStr, "[S: %03d][401] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);

  MHD_destroy_response(response);
  return(ret);
}


static enum MHD_Result main_page(struct Session *session,
                                 struct MHD_Connection *connection, const char *url) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  const char *page = mainPage;
  char errStr[300] = "";

  response = MHD_create_response_from_buffer(strlen(page), (void *) page,
             MHD_RESPMEM_PERSISTENT);

  if (! response) return(MHD_NO);

  addCookie(session, response);

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);

  MHD_destroy_response(response);
  return(ret);
}


static enum MHD_Result getImage(struct Session *session,
                                struct MHD_Connection *connection, const char *url) {

  struct MHD_Response *response;
  unsigned char *buffer = NULL;
  FILE *fp;
  int ret;
  const char *errorstr ="<html><body>An internal server error has occurred!</body></html>";
  char errStr[300] = "";

  char serverURL[strlen(url) + 17];
  if (isDaemon == 1) {
    sprintf(serverURL, "%s%s", "/usr/local/hhl7", url);
  } else {
    sprintf(serverURL, "%c%s", '.', url);
  }

  fp = openFile(serverURL, "r");
  int fSize = getFileSize(serverURL); 

  // Error accessing file
  if (fp == NULL) {
    response = MHD_create_response_from_buffer(strlen (errorstr), 
                                               (void *) errorstr,
                                               MHD_RESPMEM_PERSISTENT);
    if (response) {
      ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
      sprintf(errStr, "[S: %03d][404] GET: %s", session->shortID, url);
      writeLog(LOG_WARNING, errStr, 0);

      MHD_destroy_response(response);
      return(MHD_YES);

    } else {
      return(MHD_NO);
    }

    if (!ret) {
      if (buffer) free(buffer);
    
      response = MHD_create_response_from_buffer(strlen(errorstr),
                                                 (void*) errorstr,
                                                 MHD_RESPMEM_PERSISTENT);

      if (response) {
        ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        sprintf(errStr, "[S: %03d][501] GET: %s", session->shortID, url);
        writeLog(LOG_WARNING, errStr, 0);

        MHD_destroy_response(response);
        fclose(fp);
        return(MHD_YES);    

      } else {
        fclose(fp);
        return(MHD_NO);
      }
    }
  }

  // Successful image retrieval
  response = MHD_create_response_from_fd_at_offset64(fSize, fileno(fp), 0);
  MHD_add_response_header(response, "Content-Type", "image/png");

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);

  MHD_destroy_response(response);
  return(ret);
}


// Check if a server is valid in the server file and retun it's address
static int checkServerName(struct Session *session, char *sName, char *sAddr) {
  struct json_object *svrsObj = NULL, *svrArray = NULL, *svrObj = NULL;
  struct json_object *nameObj = NULL, *addrObj = NULL;
  int sCount = 0, s = 0;
  char errStr[80] = "";

  // Define server file location
  char svrsFile[34];
  if (isDaemon == 1) {
    sprintf(svrsFile, "%s", "/usr/local/hhl7/conf/servers.hhl7");
  } else {
    sprintf(svrsFile, "%s", "./conf/servers.hhl7");
  }

  svrsObj = json_object_from_file(svrsFile);
  if (svrsObj == NULL) {
    sprintf(errStr, "[S: %03d] Failed to read server config file", session->shortID);
    handleError(LOG_ERR, errStr, 1, 0, 1);
    return(1);
  }

  json_object_object_get_ex(svrsObj, "servers", &svrArray);
  if (svrArray == NULL) {
    sprintf(errStr, "[S: %03d] Failed to get server object, server file corrupt?",
                    session->shortID);
    handleError(LOG_ERR, errStr, 1, 0, 1);
    json_object_put(svrsObj);
    return(1);
  }

  sCount = json_object_array_length(svrArray);

  for (s = 0; s < sCount; s++) {
    svrObj = json_object_array_get_idx(svrArray, s);
    nameObj = json_object_object_get(svrObj, "displayName"); 
    addrObj = json_object_object_get(svrObj, "address");

    if (nameObj == NULL || addrObj == NULL) {
      sprintf(errStr, "[S: %03d] Failed to name or address for server, server file corrupt?",
                      session->shortID);
      handleError(LOG_ERR, errStr, 1, 0, 1);
      json_object_put(svrsObj);
      return(1);
    }

    if (strcmp(json_object_get_string(nameObj), sName) == 0) {
      sprintf(sAddr, "%s", json_object_get_string(addrObj));
      json_object_put(svrsObj);
      return(0);
    }
  }

  json_object_put(svrsObj);
  return(1);
}


// Get the list of servers that we can send to from the servers config file
static enum MHD_Result getServers(struct Session *session,
                                  struct MHD_Connection *connection,
                                  struct connection_info_struct *con_info,
                                  const char *url) {

  (void) con_info;      /* Unused. Silence compiler warning. */

  enum MHD_Result ret;
  struct MHD_Response *response;
  struct json_object *svrsObj = NULL, *svrObj = NULL;
  char errStr[300] = "";

  // Define server file location
  char svrsFile[34];
  if (isDaemon == 1) {
    sprintf(svrsFile, "%s", "/usr/local/hhl7/conf/servers.hhl7");
  } else {
    sprintf(svrsFile, "%s", "./conf/servers.hhl7");
  }

  svrsObj = json_object_from_file(svrsFile);
  if (svrsObj == NULL) {
    sprintf(errStr, "[S: %03d] Failed to read server config file", session->shortID);
    handleError(LOG_ERR, errStr, 1, 0, 1);
  }

  json_object_object_get_ex(svrsObj, "servers", &svrObj);
  if (svrObj == NULL) {
    sprintf(errStr, "[S: %03d] Failed to get server object, server file corrupt?",
                    session->shortID);
    handleError(LOG_ERR, errStr, 1, 0, 1);
  }

  const char *svrList = json_object_to_json_string_ext(svrObj, JSON_C_TO_STRING_PLAIN);

  response = MHD_create_response_from_buffer(strlen(svrList), (void *) svrList,
                                             MHD_RESPMEM_MUST_COPY);

  if (!response) {
    json_object_put(svrsObj);
    return(MHD_NO);
  }

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  //addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);

  MHD_destroy_response(response);

  json_object_put(svrsObj);
  return(ret);
}


// Get users settings from the password file
static enum MHD_Result getSettings(struct Session *session,
                                   struct MHD_Connection *connection,
                                   struct connection_info_struct *con_info,
                                   const char *url) {

  (void) con_info;      /* Unused. Silence compiler warning. */

  enum MHD_Result ret;
  struct MHD_Response *response;

  // Define passwd file location
  char pwFile[34];
  if (isDaemon == 1) {
    sprintf(pwFile, "%s", "/usr/local/hhl7/conf/passwd.hhl7");
  } else {
    sprintf(pwFile, "%s", "./conf/passwd.hhl7");
  }

  //struct json_object *resObj = json_object_new_object();
  struct json_object *pwObj = NULL, *userArray = NULL, *userObj = NULL;
  struct json_object  *uidStr = NULL, *sIP = NULL, *sPort = NULL, *lPort = NULL;
  struct json_object  *ackTObj = NULL, *webTObj = NULL;
  char setStr[350] = "";
  char errStr[300] = "";
  int uCount = 0, u = 0;
  int ackT = 3, webT = 500, ackTc = 0, webTc = 0;

  char *uid = session->userid;

  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    sprintf(errStr, "[S: %03d] Failed to read passwd file", session->shortID);
    handleError(LOG_ERR, errStr, 1, 0, 1);
  }

  json_object_object_get_ex(pwObj, "users", &userArray);
  if (userArray == NULL) {
    sprintf(errStr, "[S: %03d] Failed to get user object, passwd file corrupt?",
                    session->shortID);
    handleError(LOG_ERR, errStr, 1, 0, 1);
  }

  // TODO - check for missing uid, sIP, sPort, lPort objects and error handle
  // it *should* be ok as all are internally handled, but...

  // Loop through users and get their settings
  uCount = json_object_array_length(userArray);
  for (u = 0; u < uCount; u++) {
    userObj = json_object_array_get_idx(userArray, u);
    uidStr = json_object_object_get(userObj, "uid");

    if (strcmp(json_object_get_string(uidStr), uid) == 0) {
      sIP = json_object_object_get(userObj, "sIP");
      sPort = json_object_object_get(userObj, "sPort");
      lPort = json_object_object_get(userObj, "lPort");
      ackTObj = json_object_object_get(userObj, "ackTimeout");
      webTObj = json_object_object_get(userObj, "webTimeout");
      ackTc = json_object_get_int(ackTObj);
      webTc = json_object_get_int(webTObj);

      // Push the connection settings from passwd file to live session
      sprintf(session->sPort, "%s", json_object_get_string(sPort));
      sprintf(session->lPort, "%s", json_object_get_string(lPort));
      sprintf(session->sIP, "%s", json_object_get_string(sIP));

      // Determine if the user has custom timeouts or if they use the defaults
      session->ackTimeout = 3;
      session->webTimeout = 500;

      if (ackTc > 0 && ackTc < 100) {
        session->ackTimeout = ackTc;
        ackT = ackTc;
      } else {
        if (globalConfig) {
          if (globalConfig->ackTimeout > 0 && globalConfig->ackTimeout < 100) {
            session->ackTimeout = globalConfig->ackTimeout;
            ackT = globalConfig->ackTimeout;
          }
        }
      }

      if (webTc >= 50 && webTc <= 5000) {
        session->webTimeout = webTc;
        webT = webTc;
      } else {
        if (globalConfig) {
          if (globalConfig->webTimeout >= 50 && globalConfig->webTimeout <= 5000) {
            session->webTimeout = globalConfig->webTimeout;
            webT = globalConfig->webTimeout;
          }
        }
      }

      // Check if the url overrides the server settings
      char sAddr[256] = "";
      char *svrPtr = strstr(url, "/server/");
      if (svrPtr) {
        if (checkServerName(session, svrPtr + 8, sAddr) == 0) {
          sprintf(session->sIP, "%s", sAddr);
        }    
      }

      sprintf(setStr, "{\"sIP\":\"%s\",\"sPort\":\"%s\",\"lPort\":\"%s\",\"ackT\":%d,\"webT\":%d,\"resQS\":%d}", session->sIP, session->sPort, session->lPort, ackT, webT, globalConfig->rQueueSize);

      break;
    }
  }

  response = MHD_create_response_from_buffer(strlen(setStr), (void *) setStr,
             MHD_RESPMEM_MUST_COPY);

  if (!response) {
    json_object_put(pwObj);
    return(MHD_NO);
  }

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  //addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);

  MHD_destroy_response(response);
  json_object_put(pwObj);
  return(ret);
}


// TODO - consider moving next 2 functions to hhl7utils
// Free a file list created with scandir
void freeFileList(struct dirent **fileList, int fCount) {
  while (fCount--) {
    free(fileList[fCount]);
  }
  free(fileList);
}


// Reverses an alphasort to descending order, for use with scandir
int descAlphasort(const struct dirent **a, const struct dirent **b) {
  return alphasort(b, a);
}


// Get a list of template files and return them as a set of <select> <option>s
static enum MHD_Result getTemplateList(struct Session *session,
                                       struct MHD_Connection *connection,
                                       const char *url, int respond) {

  enum MHD_Result ret;
  struct MHD_Response *response;
  FILE *fp = 0;
  struct dirent **fileList, *file;
  char tPath[strlen(url) + 13];
  char fExt[6] = "";
  const char *nOpt = "<option value=\"None\">None</option>\n";
  // TODO - do these need increasing? Trye large number pf paths etc
  char fName[128] = "", tName[128] = "", fullName[141 + strlen(url)], *ext;
  char *newPtr = NULL;
  char errStr[300] = "";
  int fCount = 0, tmpErr = 0;

  char *dirOpts = malloc(39);
  char *tempOpts = malloc(1);
  if (dirOpts == NULL || tempOpts == NULL) {
    sprintf(errStr, "[S: %03d] Cannot cannot allocate memory for template list",
                    session->shortID);
    handleError(LOG_ERR, errStr, 1, 0, 1);
    return(MHD_NO);
  }
  dirOpts[0] = '\0';
  tempOpts[0] = '\0';

  if (respond == 0) {
    sprintf(tPath, "%s", "/usr/local/hhl7/templates/");
    if (strlen(url) > 17) sprintf(tPath, "%s%s", "/usr/local/hhl7/templates/", url + 17);
    sprintf(fExt, "%s", ".json");
    strcpy(dirOpts, nOpt);

  } else {
    sprintf(tPath, "%s", "/usr/local/hhl7/responders/");

  }

  fCount = scandir(tPath, &fileList, NULL, descAlphasort);
  if (fCount < 0) {
    free(dirOpts);
    free(tempOpts);
    free(fileList);

    sprintf(errStr, "[S: %03d] Error reading template directory",
                    session->shortID);
    handleError(LOG_ERR, errStr, 1, 0, 1);
    return(MHD_NO);
  }

  while (fCount--) {
    file = fileList[fCount];
    ext = 0; 
    if (file->d_type == DT_DIR) {
      if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
        // Increase memory for dirOpts to allow file name etc
        newPtr = realloc(dirOpts, (2*strlen(file->d_name)) + strlen(dirOpts) + 52);
        if (newPtr == NULL) {
          freeFileList(fileList, fCount);
          free(dirOpts);
          free(tempOpts);
          sprintf(errStr, "[S: %03d] Can't realloc memory for template list (dirOpts)",
                          session->shortID);
          handleError(LOG_ERR, errStr, 1, 0, 1);
          return(MHD_NO);

        } else {
          dirOpts = newPtr;
        }

        sprintf(dirOpts + strlen(dirOpts), "%s%s%s%s%s", "<option data-dir=\"true\" value=\"",
                                           file->d_name, "\">&raquo; ",
                                           file->d_name, "</option>\n");

      }

    } if (file->d_type == DT_REG) {
      ext = strrchr(file->d_name, '.');
      if (strcmp(ext, ".json") == 0) {
        // Create a template name and full path to file from filename
        memcpy(fName, file->d_name, strlen(file->d_name) - strlen(ext));
        fName[strlen(file->d_name) - strlen(ext)] = '\0';

        strcpy(fullName, tPath);
        strcat(fullName, file->d_name);

        // Read the template and check if it's hidden:
        int fSize = getFileSize(fullName);
        char *jsonMsg = malloc(fSize + 1);
        fp = openFile(fullName, "r");
        tmpErr = readJSONFile(fp, fSize, jsonMsg);

        if (tmpErr > 0) {
          sprintf(errStr, "Could not read JSON template: %s", fullName);
          writeLog(LOG_WARNING, errStr, 0);

        } else {
          // If not hidden, add it to the option list
          if (getJSONValue(jsonMsg, 1, "hidden", NULL) != 1) {
            // Get the name of the template, otherwise use filename
            if (getJSONValue(jsonMsg, 2, "name", tName) == -1) {
              strcpy(tName, fName);
            }

            // Increase memory for tempOpts to allow file name etc
            newPtr = realloc(tempOpts, strlen(fName) + strlen(tName) + strlen(tempOpts) + 38);
            if (newPtr == NULL) {
              freeFileList(fileList, fCount);
              free(dirOpts);
              free(tempOpts);
              sprintf(errStr, "[S: %03d] Can't realloc memory for template list (tempopts)",
                              session->shortID);
              handleError(LOG_ERR, errStr, 1, 0, 1);
              return(MHD_NO);

            } else {
              tempOpts = newPtr;
            }

            sprintf(tempOpts + strlen(tempOpts), "%s%s%s%s%s%s", "<option value=\"",
                                                 fName, fExt, "\">", tName, "</option>\n");
          }
        }

        // Free memory
        fclose(fp);
        free(jsonMsg);
      }
    }
    free(fileList[fCount]);
  }
  free(fileList);

  // Create a full template list of dirs + files
  newPtr = realloc(dirOpts, strlen(dirOpts) + strlen(tempOpts) + 1);
  if (newPtr == NULL) {
    freeFileList(fileList, fCount);
    free(dirOpts);
    free(tempOpts);
    sprintf(errStr, "[S: %03d] Can't realloc memory for template list (dirOpts)",
                    session->shortID); 
    handleError(LOG_ERR, errStr, 1, 0, 1);
    return(MHD_NO);

  } else {
    dirOpts = newPtr;
  }
  sprintf(dirOpts + strlen(dirOpts), "%s", tempOpts);

  // Succesful template list response
  response = MHD_create_response_from_buffer(strlen(dirOpts), (void *) dirOpts,
             MHD_RESPMEM_MUST_COPY);

  free(dirOpts);
  free(tempOpts);

  if (!response) return(MHD_NO);

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);

  MHD_destroy_response(response);
  return(ret);
}


static enum MHD_Result getTempForm(struct Session *session,
                                   struct MHD_Connection *connection, const char *url) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  FILE *fp;
  int retVal = 0, descSize = 1;
  struct json_object *rootObj = NULL, *descObj = NULL;
  char errStr[300] = "";

  int webFormS = 1024;
  int webHL7S = 1024;
  char *webForm = malloc(webFormS);
  char *webHL7 = malloc(webHL7S);
  char *jsonReply = malloc(3);
  webForm[0] = '\0';
  jsonReply[0] = '\0';
  sprintf(webHL7, "%s", "<div>");

  char *tPath = "/usr/local/hhl7";
  char fileName[strlen(tPath) + strlen(url) + 1];
  sprintf(fileName, "%s%s", tPath, url);

  fp = openFile(fileName, "r");
  int fSize = getFileSize(fileName); 
  char *jsonMsg = malloc(fSize + 1);

  // Read the json template to jsonMsg
  readJSONFile(fp, fSize, jsonMsg);

  // Get the description string if it exists
  rootObj = json_tokener_parse(jsonMsg);
  json_object_object_get_ex(rootObj, "description", &descObj);
  if (descObj != NULL) descSize = strlen(json_object_get_string(descObj)) + 1;
  char descStr[descSize];
  descStr[0] = '\0';
  if (descObj != NULL) sprintf(descStr, "%s", json_object_get_string(descObj));

  // Generate HL7 based on the json template
  retVal = parseJSONTemp(jsonMsg, &webHL7, &webHL7S, &webForm, &webFormS, 0, NULL, 1);

  if (retVal > 0) {
    handleError(LOG_WARNING, "Failed to parse JSON template", 1, 0, 0);
    sprintf(jsonReply, "%s", "TX");

  } else {
    char tmpBuf[2 * strlen(webHL7) + 1];
    escapeSlash(tmpBuf, webHL7);

    // Construct the JSON reply
    jsonReply = realloc(jsonReply, strlen(webForm) + strlen(webHL7) + strlen(descStr) + 65);
    if (jsonReply == NULL) {
      handleError(LOG_ERR, "Failed to allocate memory when creating temp form", 1, 0, 1);
      fclose(fp);
      free(webForm);
      free(webHL7);
      free(jsonMsg);
      free(jsonReply);
      json_object_put(rootObj);     

      return(MHD_NO);
    }

    sprintf(jsonReply, "{ \"form\":\"%s\",\n\"hl7\":\"%s\",\n\"desc\":\"%s\" }",
                       webForm, tmpBuf, descStr);

  }

  response = MHD_create_response_from_buffer(strlen(jsonReply), (void *) jsonReply,
             MHD_RESPMEM_MUST_COPY);

  // Free memory
  fclose(fp);
  free(webForm);
  free(webHL7);
  free(jsonMsg);
  free(jsonReply);
  json_object_put(rootObj);

  if (!response) return(MHD_NO);
  //addCookie(session, response);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);

  MHD_destroy_response(response);
  return(ret);
}


// Send a HL7 message from the Listener to the web client
static enum MHD_Result sendHL72Web(struct Session *session,
                                   struct MHD_Connection *connection, int fd,
                                   const char *url, char *retCode) {

  (void) url;           /* Unused. Silence compiler warning. */

  enum MHD_Result ret;
  struct MHD_Response *response;
  int fBufS = 2048, rBufS = 512, mBufS = 0;
  char *fBuf = malloc(fBufS);
  char *rBuf = malloc(rBufS);
  char *fBufNew = NULL, *rBufNew = NULL;
  char readSizeBuf[11];
  int p = 0, pLen = 1, maxPkts = 250, readSize = 0;
  char errStr[65] = "";

  strcpy(fBuf, "event: rcvHL7\ndata: ");

  if (strlen(retCode) == 0) {
    while (pLen > 0 && p < maxPkts) {
      mBufS = 0;
      if ((pLen = read(fd, readSizeBuf, 11)) > 0) {
        readSize = atoi(readSizeBuf);

      mBufS = (2 * readSize) + 1;
        if (mBufS > rBufS) {
          rBufNew = realloc(rBuf, mBufS);
          if (rBufNew == NULL) {
            sprintf(errStr, "[S: %03d] Can't realloc memory sendHL72Web, rBuf",
                            session->shortID);
            handleError(LOG_ERR, errStr, 1, 0, 1);

          } else {
            rBuf = rBufNew;

          }
        }

        if ((pLen = read(fd, rBuf, readSize)) > 0) {
         rBuf[readSize] = '\0';
          hl72web(rBuf, mBufS);

          mBufS = strlen(rBuf) + strlen(fBuf) + 50;
          if (mBufS > fBufS) {
            fBufNew = realloc(fBuf, mBufS);
            if (fBufNew == NULL) {
              sprintf(errStr, "[S: %03d] Can't realloc memory sendHL72Web, fBuf",
                              session->shortID);
              handleError(LOG_ERR, errStr, 1, 0, 1);

            } else {
              fBuf = fBufNew;
              fBufS = mBufS;

            }
          }


          strcat(fBuf, rBuf);
          rBuf[0] = '\0';
          p++;

        }
      }
    }

  } else {
    strcat(fBuf, retCode);
    p = 1;
  }

  strcat(fBuf, "\nretry:500\n\n");

  if (p == 0) {
    strcpy(fBuf, "event: rcvHL7\ndata: HB\nretry:500\n\n");
  }
  response = MHD_create_response_from_buffer(strlen(fBuf), (void *) fBuf,
             MHD_RESPMEM_MUST_COPY);

  if (!response) return(MHD_NO);

  MHD_add_response_header(response, "Content-Type", "text/event-stream");
  MHD_add_response_header(response, "Cache-Control", "no-cache");
  //addCookie(session, response);

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  if (p == 0) {
    sprintf(errStr, "[S: %03d][200] Web listener sent heart beat", session->shortID);
    writeLog(LOG_DEBUG, errStr, 0);
  } else {
    sprintf(errStr, "[S: %03d][200] Web listener sent %ld byte packet",
                    session->shortID, strlen(fBuf));
    writeLog(LOG_INFO, errStr, 0);
  }

  MHD_destroy_response(response);

  // Free memory from buffers
  free(fBuf);
  free(rBuf);

  return(ret);
}


// Remove the named pipe and kill the listening child process if required
static void cleanSession(struct Session *session) {
  // Remove the listeners named pipe if it exists
  if (session->readFD > 0) {
    char hhl7fifo[21];
    sprintf(hhl7fifo, "%s%d", "/tmp/hhl7fifo.", session->lpid);
    close(session->readFD);
    session->readFD = -1;
    unlink(hhl7fifo);
  }

  if (session->respFD > 0) {
    char hhl7rfifo[21];
    sprintf(hhl7rfifo, "%s%d", "/tmp/hhl7rfifo.", session->lpid);
    close(session->respFD);
    session->respFD = -1;
    unlink(hhl7rfifo);
  }

  // Stop the listening child process if running
  if (session->lpid > 0) {
    kill(session->lpid, SIGTERM);
    session->lpid = 0;
  }
}


// Clean all web sessions
void cleanAllSessions() {
  struct Session *pos;
  struct Session *next;

  pos = sessions;
  while (pos != NULL) {
    next = pos->next;
    cleanSession(pos);
    pos = next;

  }
}


// Get a list of responses via a fifo
static enum MHD_Result getRespQueue(struct Session *session,
                                    struct MHD_Connection *connection, const char *url) {

  (void) url;           /* Unused. Silence compiler warning. */

  enum MHD_Result ret;
  struct MHD_Response *response;
  int rBufS = 512;
  char *rBuf = malloc(rBufS);
  rBuf[0] = '\0';
  int pLen = 1, readSize = 0;
  int fd = session->readFD;
  char readSizeBuf[11];
  char errStr[50] = "";

  while (pLen > 0) {
    if ((pLen = read(fd, readSizeBuf, 11)) > 0) {
      readSize = atoi(readSizeBuf);

    }

    if (readSize > rBufS) {
      char *rBufNew = realloc(rBuf, readSize + 1);
      if (rBufNew == NULL) {
        sprintf(errStr, "[S: %03d] Can't realloc memory getRespQueue, rBuf",
                        session->shortID);
        handleError(LOG_ERR, errStr, 1, 0, 1);

      } else {
        rBuf = rBufNew;
        rBufS = readSize;

      }
    }

    if (readSize > 0) {
      if ((pLen = read(fd, rBuf, readSize)) > 0) {
        rBuf[readSize] = '\0';
      }
    }
  }

  response = MHD_create_response_from_buffer(strlen(rBuf), (void *) rBuf,
             MHD_RESPMEM_MUST_COPY);

  if (!response) return(MHD_NO);

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] Web response list retrieved", session->shortID);
  writeLog(LOG_DEBUG, errStr, 0);

  MHD_destroy_response(response);

  // Free memory from buffers
  free(rBuf);
  return(ret);
}


// Stop the backend listening for packets from hl7 server
static enum MHD_Result stopListenWeb(struct Session *session,
                                     struct MHD_Connection *connection, const char *url) {

  enum MHD_Result ret;
  struct MHD_Response *response;
  char errStr[300] = "";

  cleanSession(session);
  session->isListening = 0;

  response = MHD_create_response_from_buffer(2, "OK", MHD_RESPMEM_PERSISTENT);

  if (!response) return(MHD_NO);

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");
  //addCookie(session, response);

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);

  MHD_destroy_response(response);
  return(ret);
}


// Start a HL7 Listener in a forked child process
static int startListenWeb(struct Session *session, struct MHD_Connection *connection,
                           const char *url, int argc, char *argv[]) {

  int rc = -1, rLen = 0, rTries = 0, maxTries = 10;
  char rBuf[3] = "", errStr[41] = "";

  // Make children ignore waiting for parent process before closing
  signal(SIGCHLD, SIG_IGN);

  // Ensure we've stopped the previous listener before starting another
  if (session->isListening == 1) {
    stopListenWeb(session, connection, url);
  }

  pid_t pidBefore = getpid();
  session->lpid = fork();

  if (session->lpid == 0) {
    // Make the child exit if the  parent exits
    int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (r == -1 || getppid() != pidBefore) {
      exit(0);
    }

    rc = startMsgListener("127.0.0.1", session->lPort, session->sIP, session->sPort,
                          argc, 0, argv, 0, NULL);
    if (rc < 0) {
      stopListenWeb(session, connection, url);
      sprintf(errStr, "Failed to start listener on port: %s", session->lPort);
      handleError(LOG_ERR, errStr, 1, 1, 1);
    }

    _exit(0);

  }

  // Create a named pipe to read from
  char hhl7fifo[21]; 
  sprintf(hhl7fifo, "%s%d", "/tmp/hhl7fifo.", session->lpid);
  mkfifo(hhl7fifo, 0666);
  session->readFD = open(hhl7fifo, O_RDONLY | O_NONBLOCK);

  // Read message from pipe, should return FS if child listener started OK.
  while (rLen < 1 && rTries < maxTries) {
    rLen = read(session->readFD, rBuf, 2);
    usleep(100000);
    rTries++;
  }

  if (strcmp(rBuf, "FS") == 0) {
    char hhl7rfifo[22];
    sprintf(hhl7rfifo, "%s%d", "/tmp/hhl7rfifo.", session->lpid);
    mkfifo(hhl7rfifo, 0666);
    // Do not combine these two lines to "O_WRONLY | O_NONBLOCK" or open fails
    session->respFD = open(hhl7rfifo, O_WRONLY);
    fcntl(session->respFD, F_SETFL, O_NONBLOCK);
    //fcntl(session->respFD, F_SETPIPE_SZ, 1048576); // Change pipe size, default seems OK

    session->isListening = 1;
    return(0);

  } else {
    //stopListenWeb(session, connection, url);
    session->isListening = 1;
    sprintf(errStr, "Failed to start listener on port: %s", session->lPort);
    handleError(LOG_ERR, errStr, 1, 0, 1);
    return(1);
  }
  return(1);
}


// Update the list of response templates to listen to
static void sendRespList(struct Session *session, struct json_object *rootObj) {
  const char *tempStr = json_object_to_json_string_ext(rootObj, JSON_C_TO_STRING_PLAIN);
  char writeSize[11] = "";

  sprintf(writeSize, "%d", (int) strlen(tempStr));

  if (write(session->respFD, writeSize, 11) == -1) {
    handleError(LOG_ERR, "Failed to write to named pipe", 1, 0, 0);
  }

  if (write(session->respFD, tempStr, strlen(tempStr)) == -1) {
    handleError(LOG_ERR, "Failed to write to named pipe", 1, 0, 0);
  }
}


// Start the message responder 
static int startResponder(struct Session *session, struct MHD_Connection *connection,
                           struct json_object *rootObj) {

  struct json_object *dataArray = NULL, *dataObj = NULL;
  int dataInt = 0, curInt = 0, i = 0, maxL = 0;

  json_object_object_get_ex(rootObj, "templates", &dataArray);
  if (dataArray == NULL) {
    handleError(LOG_ERR, "Responder list from web post missing templates object", 1, 0, 0);
    return(1);
  }

  dataInt = json_object_array_length(dataArray);

  // Find the longest template name
  for (i = 0; i < dataInt; i++) {
    dataObj = json_object_array_get_idx(dataArray, i);
    curInt = strlen(json_object_get_string(dataObj)) + 1; 
    if (curInt > maxL) maxL = curInt;
  }

  // Create an array containing the template names
  char temps[dataInt][maxL];
  char *tempPtrs[dataInt];
  for (i = 0; i < dataInt; i++) {
    dataObj = json_object_array_get_idx(dataArray, i);
    tempPtrs[i] = temps[i];
    sprintf(temps[i], "%s", json_object_get_string(dataObj));
  }

  return(startListenWeb(session, connection, "/respond", dataInt, tempPtrs));
}


// TODO - move legacy style iterations to newer json object POSTs
static enum MHD_Result iteratePost(void *coninfo_cls, enum MHD_ValueKind kind,
              const char *key, const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off, size_t size) {

  struct connection_info_struct *con_info = coninfo_cls;

  (void) kind;               /* Unused. Silent compiler warning. */
  (void) filename;           /* Unused. Silent compiler warning. */
  (void) content_type;       /* Unused. Silent compiler warning. */
  (void) transfer_encoding;  /* Unused. Silent compiler warning. */
  (void) off;                /* Unused. Silent compiler warning. */

  struct json_object *rootObj= NULL, *postObj = NULL;
  char errStr[90] = "";
  int aStatus = -1, nStatus = -1, rc = -1;

  // A new connection should have already found it's sesssion, but check to be safe
  if (con_info->session == NULL) {
    con_info->session = getSession(con_info->connection);
  }

  // Discard messages only containing new line characters
  if (size - numLines(data) == 0) size = 0;

  con_info->answerstring[0] = '\0';

  if (size > 0) {
    // Security check, only these post items are allowed without already being authed
    if (con_info->session->aStatus < 1) {
      if (strcmp(key, "pcaction") == 0 || strcmp(key, "uname") == 0 ||
          strcmp(key, "pword") == 0) {

      } else {
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "L0");  // Require login
        return(MHD_YES);
      }
    }

    // Handle a JSON string sent in a POST
    if (strcmp(key, "jsonPOST") == 0 ) {
      rootObj = json_tokener_parse(data);
      json_object_object_get_ex(rootObj, "postFunc", &postObj);

      if (json_object_get_type(postObj) != json_type_string) {
        writeLog(LOG_ERR, "POST received a non-string value for postFunc", 0);
        return(MHD_NO);

      } else {
        if (strcmp(json_object_get_string(postObj), "procRespond") == 0) {
          if (con_info->session->isListening == 0) {
            rc = startResponder(con_info->session, con_info->connection, rootObj);
            if (rc != 0) snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "RX");
          } else {
            sendRespList(con_info->session, rootObj);

          }
        }
      }
      json_object_put(rootObj);
    }

    if (strcmp (key, "hl7MessageText") == 0 ) {
      if (con_info->poststring == NULL) {
        con_info->poststring = malloc(strlen(data) + 5);
        con_info->poststring[0] = '\0';

      } else {
        int msgLen = strlen(con_info->poststring) + strlen(data) + 5;
        char *tmpPtr = NULL;
        tmpPtr = realloc(con_info->poststring, msgLen);

        if (tmpPtr == NULL) {
          handleError(LOG_ERR, "Failed to allocate memory - server OOM??", 1, 0, 1);
          free(con_info->poststring);
          con_info->poststring = NULL;
          return(MHD_NO);

        } else {
          con_info->poststring = tmpPtr;

        }
      }

      strcat(con_info->poststring, data);
      snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "DP"); // Partial data
      return(MHD_YES);


    } else if (strcmp(key, "pcaction") == 0 ) {
      con_info->session->pcaction = atoi(data);
      // Temporarily list the answer code as DP (partial data) until we receive passwd
      snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "DP");

    } else if (strcmp(key, "uname") == 0 ) {
      if (validStr((char *) data, 4, 26, 1) == 0)
        sprintf(con_info->session->userid, "%s", data);

      // Temporarily list the answer code as DP (partial data) until we receive passwd
      snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "DP");

    } else if (strcmp(key, "pword") == 0 ) {
      // Return partial data code if invalid request
      if (strlen(con_info->session->userid) == 0 || con_info->session->pcaction < 1 ||
          validStr((char *) data, 8, 200, 1) > 0) {
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "DP");

      } else {
        aStatus = checkAuth(con_info->session->userid, data);

        if (aStatus == 3 && con_info->session->pcaction == 2) {
          nStatus = regNewUser(con_info->session->userid, (char *) data);

          if (nStatus == 0) {
            snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "LC");  // Account created
            sprintf(errStr, "[S: %03d] New user account created, uid: %s",
                            con_info->session->shortID, con_info->session->userid);
            writeLog(LOG_INFO, errStr, 0);

          } else {
            snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "LF");  // New account creation failure
            sprintf(errStr, "[S: %03d] New user account creation failed, uid: %s",
                            con_info->session->shortID, con_info->session->userid);
            writeLog(LOG_INFO, errStr, 0);

          }

        } else if (aStatus == 3 && con_info->session->pcaction == 1) {
          snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "LR");  // Reject login, no account
          sprintf(errStr, "[S: %03d] Login rejected (no account), uid: %s",
                          con_info->session->shortID, con_info->session->userid);
          writeLog(LOG_INFO, errStr, 0);

        } else if (aStatus == 2) {
          snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "LR");  // Reject login, wrong password
          sprintf(errStr, "[S: %03d] Login rejected (wrong password), uid: %s",
                          con_info->session->shortID, con_info->session->userid);
          writeLog(LOG_INFO, errStr, 0);

        } else if (aStatus == 1) {
          snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "LD");  // Account disabled, but login OK
          sprintf(errStr, "[S: %03d] Login rejected (account disabled), uid: %s",
                          con_info->session->shortID, con_info->session->userid);
          writeLog(LOG_INFO, errStr, 0);

        } else if (aStatus == 0 && con_info->session->pcaction == 3) {
          con_info->session->aStatus = 2;
          snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "DP");

        } else if (aStatus == 0) {
          con_info->session->aStatus = 1;
          updatePasswdFile(con_info->session->userid, "attempts", NULL, 0);
          snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "LA");  // Accept login
          sprintf(errStr, "[S: %03d] Login accepted, uid: %s",
                          con_info->session->shortID, con_info->session->userid);
          writeLog(LOG_INFO, errStr, 0);

        } else {
          snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "DP");
 
        }
      }

    } else if (strcmp(key, "npword") == 0 ) {
      // NOTE: con_info->session->aStatus = 2 can only be true after a succesful
      // username/passwd check from a username/passwd/newPasswd post.
      snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "SX");
      if (con_info->session->aStatus == 2) {
        if (validStr((char *) data, 8, 200, 1) > 0) {
          snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "DP");

        } else {
          con_info->session->aStatus = 1;
          if (updatePasswd(con_info->session->userid, data) == 0) {
            snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "OK");  // Password updated succesfully
            sprintf(errStr, "[S: %03d] Password changed, uid: %s",
                            con_info->session->shortID, con_info->session->userid);
            writeLog(LOG_INFO, errStr, 0);

          }
        } 
      }

    } else if (strcmp(key, "sIP") == 0 ) {
      if (updatePasswdFile(con_info->session->userid, key, data, -1) == 0) {
        sprintf(con_info->session->sIP, "%s", data);
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "OK");  // Save OK
        sprintf(errStr, "[S: %03d] Send IP settings saved OK, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_INFO, errStr, 0);

      } else {
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "SX");  // Save failed
        sprintf(errStr, "[S: %03d] Failed to save send IP settings, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_WARNING, errStr, 0);

      }

    } else if (strcmp(key, "sPort") == 0 ) {
      if (updatePasswdFile(con_info->session->userid, key, data, -1) == 0) {
        sprintf(con_info->session->sPort, "%s", data);
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "OK");  // Save OK
        sprintf(errStr, "[S: %03d] Send port settings saved OK, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_INFO, errStr, 0);

      } else {
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "SX");  // Save failed
        sprintf(errStr, "[S: %03d] Failed to save send port settings, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_WARNING, errStr, 0);

      }

    } else if (strcmp(key, "lPort") == 0 ) {
      if (lPortUsed(con_info->session->userid, data) == 1) {
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "SR");  // Save Rejected (port in use)
        sprintf(errStr, "[S: %03d] Rejected listen port change, port in use, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_INFO, errStr, 0);

      } else if (updatePasswdFile(con_info->session->userid, key, data, -1) == 0) {
        sprintf(con_info->session->lPort, "%s", data);
        if (con_info->session->isListening == 1) {
          stopListenWeb(con_info->session, con_info->connection, "/postListSets");
          startListenWeb(con_info->session, con_info->connection, "/postListSets", -1, NULL);
        }
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "OK");  // Save OK
        sprintf(errStr, "[S: %03d] Listen port settings saved OK, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_INFO, errStr, 0);

      } else {
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "SX");  // Save failed
        sprintf(errStr, "[S: %03d] Failed to save send port settings, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_WARNING, errStr, 0);

      }

    } else if (strcmp(key, "ackTimeout") == 0 ) {
      if (updatePasswdFile(con_info->session->userid, key, NULL, atoi(data)) == 0) {    
        con_info->session->ackTimeout = atoi(data);
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "OK");  // Save OK
        sprintf(errStr, "[S: %03d] Listen port settings saved OK, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_INFO, errStr, 0);

      } else {
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "SX");  // Save failed
        sprintf(errStr, "[S: %03d] Failed to save send port settings, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_WARNING, errStr, 0);

      }      

    } else if (strcmp(key, "webTimeout") == 0 ) {
      if (updatePasswdFile(con_info->session->userid, key, NULL, atoi(data)) == 0) {    
        con_info->session->webTimeout = atoi(data);
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "OK");  // Save OK
        sprintf(errStr, "[S: %03d] Listen port settings saved OK, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_INFO, errStr, 0);

      } else {
        snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "SX");  // Save failed
        sprintf(errStr, "[S: %03d] Failed to save send port settings, uid: %s",
                        con_info->session->shortID, con_info->session->userid);
        writeLog(LOG_WARNING, errStr, 0);

      }
    }

  } else {
    // No post data was received, reply with code "D0" (no data)
    snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "D0");
    sprintf(errStr, "[S: %03d] Post received with no data, uid: %s",
                    con_info->session->shortID, con_info->session->userid);
    writeLog(LOG_WARNING, errStr, 0);

    return(MHD_NO);

  }
  return(MHD_YES);
}


static enum MHD_Result sendPage(struct Session *session, struct MHD_Connection *connection,
                                const char* connectiontype, const char *page,
                                char *ansStrP, const char *url) {

  enum MHD_Result ret;
  struct MHD_Response *response;
  char errStr[300] = "";

  if (ansStrP != NULL) page = ansStrP;
  response = MHD_create_response_from_buffer(strlen(page), (void *) page,
                                             MHD_RESPMEM_MUST_COPY);

  if (!response) {
    if (ansStrP != NULL) free(ansStrP);
    return(MHD_NO);
  }

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  if (strcmp(page, "L0") == 0 || strcmp(page, "LR") == 0 || strcmp(page, "LD") == 0) {
    ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, response);
    sprintf(errStr, "[S: %03d][401] %s: %s", session->shortID, connectiontype, url);
    writeLog(LOG_INFO, errStr, 0);


  } else if (strcmp(page, "DP") == 0) {
    ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
    sprintf(errStr, "[S: %03d][400] %s: %s", session->shortID, connectiontype, url);
    writeLog(LOG_INFO, errStr, 0);

  } else if (strcmp(page, "D0") == 0) {
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_ACCEPTABLE, response);
    sprintf(errStr, "[S: %03d][406] %s: %s", session->shortID, connectiontype, url);
    writeLog(LOG_INFO, errStr, 0);

  } else if (strcmp(page, "RX") == 0 || strcmp(page, "CX") == 0 ||
             strcmp(page, "PX") == 0 || strcmp(page, "PT") == 0 ||
             strcmp(page, "PU") == 0) {
    ret = MHD_queue_response(connection, MHD_HTTP_CONFLICT, response);
    sprintf(errStr, "[S: %03d][409] %s: %s", session->shortID, connectiontype, url);
    writeLog(LOG_INFO, errStr, 0);

  } else if (strcmp(page, "DM") == 0) {
    ret = MHD_queue_response(connection, MHD_HTTP_CONTENT_TOO_LARGE, response);
    sprintf(errStr, "[S: %03d][413] %s: %s", session->shortID, connectiontype, url);
    writeLog(LOG_INFO, errStr, 0);

  } else if (strcmp(page, "PZ") == 0) {
    ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
    sprintf(errStr, "[S: %03d][500] %s: %s", session->shortID, connectiontype, url);
    writeLog(LOG_INFO, errStr, 0);

  } else if (strcmp(page, "TP") == 0) {
    ret = MHD_queue_response(connection, 418, response);

  } else {
    //addCookie(session, response);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    sprintf(errStr, "[S: %03d][200] %s: %s", session->shortID, connectiontype, url);
    writeLog(LOG_INFO, errStr, 0);

  }

  MHD_destroy_response(response);
  if (ansStrP != NULL) {
    free(ansStrP);
    ansStrP = NULL;
  }

  return(ret);
}


// Logout of the web front end and close backend session
static enum MHD_Result logout(struct Session *session, struct MHD_Connection *connection,
                              const char *url) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  char errStr[300] = "";
  session->sessID[0] = '\0';

  response = MHD_create_response_from_buffer(2, "LO", MHD_RESPMEM_PERSISTENT);

  if (!response) return(MHD_NO);

  MHD_add_response_header(response, "Content-Type", "text/plain");
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  sprintf(errStr, "[S: %03d][200] GET: %s", session->shortID, url);
  writeLog(LOG_INFO, errStr, 0);
  sprintf(errStr, "[S: %03d] User logged out, uid: %s", session->shortID, session->userid);
  writeLog(LOG_INFO, errStr, 0);


  MHD_destroy_response(response);
  return(ret);
}


// Debug function used for showing connection values, e.g user-agent
//static int print_out_key (void *cls, enum MHD_ValueKind kind, const char *key,
//               const char *value) {
//  printf ("%s: %s\n", key, value);
//  return(MHD_YES);
//}


// Answer to a connection request
static enum MHD_Result answerToConnection(void *cls, struct MHD_Connection *connection,
                                            const char *url, const char *method,
                                            const char *version, const char *upload_data,
                                            size_t *upload_data_size, void **con_cls) {

  (void) cls;           /* Unused. Silence compiler warning. */
  (void) version;       /* Unused. Silence compiler warning. */

  struct connection_info_struct *con_info = *con_cls;
  struct Session *session;
  int rc = -1, sockfd = -1;
  char retCode[3] = "", resStr[3] = "", errStr[90] = "";

  // Debug - print connection values, e.g user-agent
  //MHD_get_connection_values(connection, MHD_HEADER_KIND, print_out_key, NULL);

  if (*con_cls == NULL) {
    con_info = malloc(sizeof(struct connection_info_struct));
    if (con_info == NULL) return(MHD_NO);
    con_info->connection = connection;
    con_info->session = getSession(connection);
    con_info->answerstring[0] = '\0';
    con_info->poststring = NULL;
    con_info->postprocessor = NULL;

    if (strcmp(method, "POST") == 0) {
      con_info->postprocessor = MHD_create_post_processor(connection, POSTBUFFERSIZE,
                                                     iteratePost, (void *) con_info);

      if (con_info->postprocessor == NULL) {
        free(con_info);
        return(MHD_NO);
      }

      con_info->connectiontype = POST;

    } else {
      con_info->connectiontype = GET;
    }

    *con_cls = (void *) con_info;
    return(MHD_YES);
  }

  if (con_info->session == NULL) {
    con_info->session = getSession(connection);

    if (con_info->session == NULL) {
      fprintf (stderr, "Failed to setup session for %s\n", url);
      return(MHD_NO);
    }
  }
  session = con_info->session;
  session->lastSeen = time(NULL);

  if (strcmp(method, "GET") == 0) {
    if (strstr(url, "/images/")) {
      return(getImage(session, connection, url));

    } else if (strstr(url, "/templates/")) {
      return(getTempForm(session, connection, url));

    } else if (strstr(url, "/getTemplateList")) {
      // Request login if not logged in
      if (session->aStatus != 1) return(requestLogin(session, connection, url));
      return(getTemplateList(session, connection, url, 0));

    } else if (strcmp(url, "/getRespondList") == 0) {
      // Request login if not logged in
      if (session->aStatus != 1) return(requestLogin(session, connection, url));
      return(getTemplateList(session, connection, url, 1));

    } else if (strcmp(url, "/getRespQueue") == 0) {
      // Request login if not logged in
      if (session->aStatus != 1) return(requestLogin(session, connection, url));
      return(getRespQueue(session, connection, url));

    } else if (strcmp(url, "/getServers") == 0) {
      // Request login if not logged in
      if (session->aStatus != 1) return(requestLogin(session, connection, url));
      return(getServers(session, connection, con_info, url));

    } else if (strncmp(url, "/getSettings", 12) == 0) {
      // Request login if not logged in
      if (session->aStatus != 1) return(requestLogin(session, connection, url));
      return(getSettings(session, connection, con_info, url));

    } else if (strcmp(url, "/stopListenHL7") == 0) {
      if (session->aStatus != 1) return(requestLogin(session, connection, url));
      return(stopListenWeb(session, connection, url));

    } else if (strcmp(url, "/logout") == 0) {
      if (session->aStatus != 1) return(requestLogin(session, connection, url));
      return(logout(session, connection, url));

    } else if (strcmp(url, "/listenHL7") == 0) {
      if (session->aStatus != 1) return(requestLogin(session, connection, url));

      if (session->isListening == 0) {
        rc = startListenWeb(con_info->session, con_info->connection, url, -1, NULL);
        if (rc != 0) {
          sprintf(retCode, "%s", "FX");
          return(sendHL72Web(session, connection, session->readFD, url, retCode));
        }
      }

      if (session->isListening == 1) {
        return(sendHL72Web(session, connection, session->readFD, url, retCode));

      } else {
        session->isListening = 1;
        return(MHD_YES);
      }

    } else {
      return(main_page(session, connection, url));
    }
  }

  if (strcmp(method, "POST") == 0) {
    if (*upload_data_size != 0) {
      MHD_post_process(con_info->postprocessor, upload_data, *upload_data_size);
      *upload_data_size = 0;
      return(MHD_YES);

    } else if (strlen(con_info->answerstring) > 0) {
      // If we have a hl7 message to send, send it and clear memory
      if (con_info->poststring != NULL) {
        char *resList = malloc(301);
        sprintf(resList, "%s", "{ \"results\":\"");

        if (strncmp(con_info->poststring, "Coffee?", 7) == 0 && 
            strlen(con_info->poststring) <= 10) {
          snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "TP"); // Teapot

        } else {
          sockfd = connectSvr(con_info->session->sIP, con_info->session->sPort);

          if (sockfd >= 0) {
            rc = sendPacket(sockfd, con_info->poststring, resStr, &resList, 0,
                            con_info->session->ackTimeout);

            if (rc >= 0) {
              sprintf(errStr, "[S: %03d][%s] Sent %ld byte packet to socket: %d",
                              con_info->session->shortID, resStr,
                              strlen(con_info->poststring), sockfd);

              writeLog(LOG_INFO, errStr, 0);
              snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", resStr);

            } else if (rc == -1) {
              sprintf(errStr, "[S: %03d][%s] Failed to send packet to socket: %d",
                              con_info->session->shortID, resStr, sockfd);

              writeLog(LOG_WARNING, errStr, 0);
              snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "PX");

            } else if (rc == -2) {
              sprintf(errStr, "[S: %03d][%s] Timeout listening for ACK response",
                              con_info->session->shortID, resStr);

              writeLog(LOG_WARNING, errStr, 0);
              snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "PT");

            } else if (rc == -3) {
              sprintf(errStr, "[S: %03d][%s] Failed to understand ACK response",
                              con_info->session->shortID, resStr);

              writeLog(LOG_WARNING, errStr, 0);
              snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "PU");

            } else {
              sprintf(errStr, "[S: %03d][%s] Unknown error while sending message",
                              con_info->session->shortID, resStr);

              writeLog(LOG_ERR, errStr, 0);
              snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "PZ");
            }

          } else {
            sprintf(errStr, "[S: %03d] Can't open socket to send packet",
                            con_info->session->shortID);
            handleError(LOG_ERR, errStr, 1, 0, 1);
            snprintf(con_info->answerstring, MAXANSWERSIZE, "%s", "CX"); // Connection failed
          }
        }

        free(con_info->poststring);
        con_info->poststring = NULL;
        if (rc >= 0) {
          return(sendPage(session, connection, method, con_info->answerstring, resList, url));
        } else {
          free(resList);
        }
      }

      return(sendPage(session, connection, method, con_info->answerstring, NULL, url));

    }
  }

  // We should never get here, but left as a catch all
  return(sendPage(session, connection, method, errorPage, NULL, url));
}


// Clear down old sessions
static int expireSessions(struct MHD_Daemon *daemon, int expireAfter) {
  struct Session *pos;
  struct Session *prev;
  struct Session *next;
  time_t now;
  int sessCount = 0, rmCount = 0, expireSecs = 0, maxExpire = 0;
  char errStr[89] = "";

  now = time(NULL);
  prev = NULL;
  pos = sessions;

  writeLog(LOG_INFO, "Checking for expired session...", 0);

  // Loop through all sessions
  while (pos != NULL) {
    sessCount++;
    next = pos->next;

    // Expire old sessions
    if ((now - pos->lastSeen) > expireAfter) {
      rmCount++;
      sprintf(errStr, "[S: %03d] Session expired, removing session", pos->shortID);
      writeLog(LOG_INFO, errStr, 0);

      cleanSession(pos);

      if (prev == NULL) {
        sessions = pos->next;
      } else {
        prev->next = next;
      }
      free(pos);

    } else {
      // Log the maximum expiry time for return value
      expireSecs = expireAfter - (now - pos->lastSeen);
      if (expireSecs > maxExpire) maxExpire = expireSecs;
      prev = pos;
    }

    pos = next;
  }

  // Log the current session status
  sprintf(errStr, "%d sessions total, %d sessions expired, %d sessions left active",
                   sessCount, rmCount, sessCount - rmCount);
  writeLog(LOG_INFO, errStr, 0);

  // If the last session has expired, close the daemon
  sessCount = sessCount - rmCount;
  if (sessCount < 1 && isDaemon == 1) {
    writeLog(LOG_INFO, "No active sessions remaining, exiting", 0);
    closeLog();
    MHD_stop_daemon(daemon);
    exit(0);
  }

  return(maxExpire);
}


// Start the web interface
int listenWeb(int daemonSock) {
  struct MHD_Daemon *daemon;
  int maxExpire = 0, expireSecs = 900, expireDelay = 10;
  time_t now, last = time(NULL) - (expireSecs + expireDelay + 1) ;
  int port = 5377;
  char SKEY[256];
  char SPEM[256];
  char errStr[54] = "";

  // Get the port and expire times from the config file if it exists
  if (globalConfig) {
    if (strlen(globalConfig->wPort) > 0) port = atoi(globalConfig->wPort);
    if (globalConfig->sessExpiry > 0) expireSecs = globalConfig->sessExpiry;
    if (globalConfig->exitDelay > 0) expireDelay = globalConfig->exitDelay;
  }

  // Configure the cert location dependant on if we're a daemon or dev mode
  if (strlen(globalConfig->wKey) > 0 && strlen(globalConfig->wCrt) > 0) {
    sprintf(SKEY, "%s", globalConfig->wKey);
    sprintf(SPEM, "%s", globalConfig->wCrt);

  } else if (isDaemon == 1) {
    sprintf(SKEY, "%s", "/usr/local/hhl7/certs/server.key");
    sprintf(SPEM, "%s", "/usr/local/hhl7/certs/server.pem");

  } else {
    sprintf(SKEY, "%s", "./certs/server.key");
    sprintf(SPEM, "%s", "./certs/server.pem");

  }

  // Check the cert files exist
  if (checkFile(SKEY, 4) == 1 || checkFile(SPEM, 4) == 1 ) {
    handleError(LOG_CRIT, "The HTTPS key/cert files could not be read", 1, 1, 1);
  }

  int keySize = getFileSize(SKEY);
  int pemSize = getFileSize(SPEM);
  char key_pem[keySize + 1];
  char cert_pem[pemSize + 1];
  FILE *fpSKEY = openFile(SKEY, "r");
  FILE *fpSPEM = openFile(SPEM, "r");

  if ((fpSKEY == NULL) || (fpSPEM == NULL)) {
    handleError(LOG_CRIT, "The HTTPS key/cert files could not be read", 1, 1, 1);
  }

  file2buf(key_pem, fpSKEY, keySize);
  file2buf(cert_pem, fpSPEM, pemSize);
  fclose(fpSKEY);
  fclose(fpSPEM);

  struct timeval tv, *tvp;
  //struct timeval *tvp;
  fd_set rs;
  fd_set ws;
  fd_set es;
  MHD_socket max;
  MHD_UNSIGNED_LONG_LONG mhd_timeout;

  // Daemon uses systemd socket with MHD_OPTION_LISTEN_SOCKET, -w uses port variable
  if (isDaemon == 1) {
    daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_TURBO |
                              MHD_USE_TLS | MHD_USE_TCP_FASTOPEN,
                              port,
                              NULL, NULL,
                              &answerToConnection, NULL,
                              MHD_OPTION_HTTPS_MEM_KEY, key_pem,
                              MHD_OPTION_HTTPS_MEM_CERT, cert_pem,
                              MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15,
                              MHD_OPTION_NOTIFY_COMPLETED, &reqComplete, NULL,
                              MHD_OPTION_LISTEN_SOCKET, daemonSock,
                              MHD_OPTION_END);

  } else {
    daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_TURBO |
                              MHD_USE_TLS | MHD_USE_TCP_FASTOPEN,
                              port,
                              NULL, NULL,
                              &answerToConnection, NULL,
                              MHD_OPTION_HTTPS_MEM_KEY, key_pem,
                              MHD_OPTION_HTTPS_MEM_CERT, cert_pem,
                              MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15,
                              MHD_OPTION_NOTIFY_COMPLETED, &reqComplete, NULL,
                              MHD_OPTION_END);
  }

  if (daemon == NULL) {
    handleError(LOG_CRIT, "Failed to start HTTPS daemon", 1, 1, 1);

  } else {
    if (isDaemon == 1) {
      // Start a single session to prevent daemon closing until a real session exists
      getSession(NULL);
    }
    webRunning = 1;
  }


  while (1) {
    writeLog(LOG_DEBUG, "Main loop running...", 0);
    // Expire any old sessions and get the time of the last session expiry
    now = time(NULL);
    if (now >= (last + expireSecs + expireDelay)) {
      maxExpire = expireSessions(daemon, expireSecs) + expireDelay;
      last = time(NULL);
    }

    max = 0;
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_ZERO(&es);

    // Set MHD file descriptiors to watch for activity...
    if (MHD_YES != MHD_get_fdset(daemon, &rs, &ws, &es, &max)) break; // internal error

    // See if MHD wants to set a time to next poll
    if (MHD_get_timeout(daemon, &mhd_timeout) == MHD_YES) {
      tv.tv_sec = mhd_timeout / 1000;
      tv.tv_usec = (mhd_timeout - (tv.tv_sec * 1000)) * 1000;
      tvp = &tv;

    // ... or we poll again after the last session expiry is expected
    } else {
      tv.tv_sec = maxExpire;
      tv.tv_usec = 0;
      tvp = &tv;
      sprintf(errStr, "Sessions inactive, last expiry in %d seconds", maxExpire);
      writeLog(LOG_INFO, errStr, 0);

    }

    // watch until FDs are active or timeout reached
    if (select(max + 1, &rs, &ws, &es, tvp) == -1) {
      if (errno != EINTR) {
        handleError(LOG_ERR, "listenWeb() Failed during select() routine", 1, 1, 1);
      }
      break;
    }

    MHD_run(daemon);
  }

  MHD_stop_daemon(daemon);
  return(0);
}
