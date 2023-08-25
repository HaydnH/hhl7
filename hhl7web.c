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
#include <netdb.h>
#include <netinet/in.h>
#include <microhttpd.h>
#include <json.h>
#include "hhl7extern.h"
#include "hhl7net.h"
#include "hhl7utils.h"
#include "hhl7webpages.h"
#include "hhl7json.h"

// TODO - make these variable
#define PORT 8888
#define REALM     "\"Maintenance\""
#define USER      "haydn"
#define PASSWORD  "password"
#define GET             0
#define POST            1
// TODO - allow more space if needed?
#define MAXNAMESIZE     1024
#define MAXANSWERSIZE  50 
#define POSTBUFFERSIZE  1024

struct connection_info_struct {
  int connectiontype;
  char *answerstring;
  struct MHD_PostProcessor *postprocessor;
};

// TODO - do we have 2 isWeb variables?
char webErrStr[256] = "";
int webRunning = 0;
int isListening = 0;
int readFD = 0;
pid_t wpid;


// Send error message web browser
static enum MHD_Result webErrXHR(struct MHD_Connection *connection) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  // TODO - check memory of res
  //char *res = malloc(1124);

  //sprintf(res, "event: rcvHL7\ndata: %s\nretry:100\n\n", webErrStr);

  response = MHD_create_response_from_buffer(strlen(webErrStr), (void *) webErrStr,
             MHD_RESPMEM_PERSISTENT);

  if (!response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  //webErrStr[0] = '\0';
  return ret;
}


static enum MHD_Result ask_for_authentication(struct MHD_Connection *connection,
                                              const char *realm) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  char *headervalue;
  size_t slen;
  const char *strbase = "Basic realm=";

  response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
  if (! response)
    return MHD_NO;

  slen = strlen(strbase) + strlen(realm) + 1;
  if (NULL == (headervalue = malloc(slen)))
    return MHD_NO;
  snprintf(headervalue, slen, "%s%s", strbase, realm);
  ret = MHD_add_response_header(response, "WWW-Authenticate", headervalue);
  free(headervalue);

  if (! ret) {
    MHD_destroy_response(response);
    return MHD_NO;
  }

  ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, response);
  MHD_destroy_response(response);
  return ret;
}

static int is_authenticated(struct MHD_Connection *connection,
                     const char *username,
                     const char *password) {

  const char *headervalue;
  char *expected_b64;
  char *expected;
  const char *strbase = "Basic ";
  int authenticated;
  size_t slen;

  headervalue = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
  if (NULL == headervalue)
    return 0;
  if (0 != strncmp(headervalue, strbase, strlen(strbase)))
    return 0;

  slen = strlen(username) + 1 + strlen(password) + 1;
  if (NULL == (expected = malloc(slen)))
    return 0;
  snprintf(expected, slen, "%s:%s", username, password);
  expected_b64 = str2base64(expected);
  free(expected);
  if (NULL == expected_b64)
    return 0;

  authenticated = (strcmp(headervalue + strlen(strbase), expected_b64) == 0);
  free(expected_b64);
  return authenticated;
}


static enum MHD_Result main_page(struct MHD_Connection *connection) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  const char *page = mainPage;

  response = MHD_create_response_from_buffer(strlen(page), (void *) page,
             MHD_RESPMEM_PERSISTENT);

  if (! response) return MHD_NO;
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}


static enum MHD_Result getImage(struct MHD_Connection *connection, const char *url) {
  struct MHD_Response *response;
  unsigned char *buffer = NULL;
  FILE *fp;
  int ret;
  const char *errorstr ="<html><body>An internal server error has occurred!</body></html>";

  // TODO change this to work with /usr/share or ./?
  char *serverURL = malloc(strlen(url) + 2);
  strcpy(serverURL, ".");
  strcat(serverURL, url);

  fp = openFile(serverURL);
  int fSize = getFileSize(serverURL); 

  // Error accessing file
  if (fp == NULL) {
    fclose(fp);
    free(serverURL);

    response = MHD_create_response_from_buffer(strlen (errorstr), 
                                               (void *) errorstr,
                                               MHD_RESPMEM_PERSISTENT);
    if (response) {
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
                                                 // MHD_RESPMEM_MUST_COPY);

      if (response) {
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
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  // TODO: Closing the file breaks the next request for the image, needs to be fixed to avoi a mem leak 
  //fclose(fp);
  free(serverURL);
  return ret;
  //fclose(fp);
}

// Get a list of template files and return them as a set of <select> <option>s
static enum MHD_Result getTemplateList(struct MHD_Connection *connection) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  DIR *dp;
  FILE *fp = 0;
  struct dirent *file;
  // TODO: Get templates from all possible directories...
  char *tPath = "/usr/local/share/hhl7/templates/";
  char *nOpt = "<option value=\"None\">None</option>\n";
  char fName[50], tName[50], fullName[50+strlen(tPath)], *ext, *tempOpts;

  // TODO - check pointers updated to new memory created with realloc
  // TODO is sizeof needed for char, should just be 36?
  tempOpts = (char *) malloc(36 * sizeof(char));
  strcpy(tempOpts, nOpt);

  if (tempOpts == NULL) {
    fprintf(stderr, "ERROR: Cannot cannot allocate memory for template list.\n");
    exit(1);
  }

  dp = opendir(tPath); 
  if (dp == NULL) {
    fprintf(stderr, "ERROR: Cannot open path: %s\n", tPath);
    exit(1);
  }

  while ((file = readdir(dp)) != NULL) {
    ext = 0; 
    if (file->d_type == DT_REG) {
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
        fp = openFile(fullName);
        readJSONFile(fp, fSize, jsonMsg);

        // If not hidden, add it to the option list
        if (getJSONValue(jsonMsg, 1, "hidden", NULL) != 1) {
          // Get the name of the template, otherwise use filename
          if (getJSONValue(jsonMsg, 2, "name", tName) == -1) {
            strcpy(tName, fName);
          }

          // TODO - check realloc
          tempOpts = (char *) realloc(tempOpts, (2*strlen(fName)) + strlen(tempOpts) + 34);
          strcat(tempOpts, "<option value=\"");
          strcat(tempOpts, fName);
          strcat(tempOpts, ".json\">");
          strcat(tempOpts, tName);
          strcat(tempOpts, "</option>\n");
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
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

// TODO - function getting long, consider splitting off to functions
static enum MHD_Result getTempForm(struct MHD_Connection *connection, const char *url) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  FILE *fp;
  // TODO: Get templates from all possible directories...
  char *tPath = "/usr/local/share/hhl7";
  char *fileName = malloc(strlen(tPath) + strlen(url) + 1);
  char *formStr = malloc(1024);
  // TODO - reduce this to 1 and add reallocs below
  char *formHL7 = malloc(1024);
  char *jsonReply = malloc(1);
  formStr[0] = '\0';
  formHL7[0] = '\0';
  jsonReply[0] = '\0';
  char dt[26] = "";
  char *vStr = NULL, *oStr = NULL, *dStr = NULL;
  int s, segCount = 0, f, fldCount = 0, fIdx = 0, fid = 0, o = 0;
  struct json_object *rootObj= NULL, *segsObj = NULL, *segObj = NULL;
  struct json_object *fieldsObj = NULL, *fieldObj = NULL, *valObj = NULL, *idObj = NULL;
  struct json_object *optsObj = NULL, *optObj = NULL, *oObj = NULL, *defObj = NULL;

  strcpy(fileName, tPath);
  strcat(fileName, url);

  fp = openFile(fileName);
  int fSize = getFileSize(fileName); 
  char *jsonMsg = malloc(fSize + 1);

  readJSONFile(fp, fSize, jsonMsg);
  rootObj = json_tokener_parse(jsonMsg);

  json_object_object_get_ex(rootObj, "argcount", &valObj);
  if (json_object_get_type(valObj) != json_type_int) {
    fprintf(stderr, "ERROR: Could not read integer value for argcount from json template\n"
);
    exit(1);
  }

  // Get full segment section from the json template
  json_object_object_get_ex(rootObj, "segments", &segsObj);
  segCount = json_object_array_length(segsObj);

  // Alloc enough space for the segment headers, first separator and end of line
  //formHL7 = realloc(formHL7, 5 * segCount);

  for (s = 0; s < segCount; s++) {
    // Get individual segment as json
    segObj = json_object_array_get_idx(segsObj, s);

    // Add name and separator to HL7 message
    json_object_object_get_ex(segObj, "name", &valObj);
    vStr = (char *) json_object_get_string(valObj);
    strcat(formHL7, vStr);
    strcat(formHL7, "|");

    // Get Fields object
    json_object_object_get_ex(segObj, "fields", &fieldsObj);
    json_object_object_get_ex(segObj, "fieldcount", &valObj);
    fldCount = (int) json_object_get_int(valObj);

    if (json_object_get_type(valObj) != json_type_int) {
      fprintf(stderr, "ERROR: Could not read fieldcount (int) for segment %d (of %d) from json template\n", s + 1, segCount);
      exit(1);
    }

    fIdx = 0;
    for (f = 0; f < fldCount; f++) {
      // Get individual field object from fields
      fieldObj = json_object_array_get_idx(fieldsObj, fIdx);

      // Get the ID of the current field value
      json_object_object_get_ex(fieldObj, "id", &idObj);
      fid = (int) json_object_get_int(idObj);

      // If the json id matches the expected field index then process the value
      if (fid == f + 1) {
        json_object_object_get_ex(fieldObj, "value", &valObj);
        vStr = (char *) json_object_get_string(valObj);

        // Check if this is variable, date or just a value
        // TODO: Add a HTML error message for template issues
        if (strcmp(vStr, "$VAR") == 0) {
          // Get the name, options list and default option for the variable
          json_object_object_get_ex(fieldObj, "name", &valObj);
          vStr = (char *) json_object_get_string(valObj);

          // TODO - change to html error and not exit
          if (!vStr) {
            printf("ERROR: Template variable does not include a \"name\" field.\n");
            exit(1);
          }

          json_object_object_get_ex(fieldObj, "options", &optsObj);
          json_object_object_get_ex(fieldObj, "default", &defObj);
          dStr = (char *) json_object_get_string(defObj);

          //if (json_object_get_type(optsObj) != json_type_string) {
            //TODO - error here
          //}
          // TODO - realloc error checks here and below
          //formStr = realloc(formStr, strlen(formStr) + 143 + strlen(vStr));

          strcat(formStr, "<div class='tempFormField'><div class='tempFormKey'>");
          strcat(formStr, vStr);
          strcat(formStr, ":</div><div class='tempFormValue'>");

          strcat(formHL7, "<span id='HHL7_FL_");
          strcat(formHL7, vStr);
          strcat(formHL7, "_HL7'>");

          // Create options list
          if (optsObj) {
            strcat(formStr, "<select id='HHL7_FL_");
            strcat(formStr, vStr);
            strcat(formStr, "' class='tempFormInput' onInput='updateHL7(this.id, this.value);' />");

            for (o = 0; o < json_object_array_length(optsObj); o++) {
              optObj = json_object_array_get_idx(optsObj, o);
              json_object_object_get_ex(optObj, "option", &oObj);
              oStr = (char *) json_object_get_string(oObj);
              //formStr = realloc(formStr, strlen(formStr) + 98 + (2*strlen(oStr)));

              strcat(formStr, "<option value='");
              strcat(formStr, oStr);
              if (strcmp(oStr, dStr) == 0) {
                strcat(formStr, "' selected='selected'>");
                strcat(formHL7, oStr);
              } else {
                strcat(formStr, "'>");
              }
              strcat(formStr, oStr);
              strcat(formStr, "</option>");
            }

              strcat(formStr, "</select>");

          } else {
            strcat(formStr, "<input id='HHL7_FL_");
            strcat(formStr, vStr);
            strcat(formStr, "' class='tempFormInput' onInput='updateHL7(this.id, this.value);' />");

          }

          strcat(formHL7, "</span>");
          strcat(formStr, "</div></div>");

        } else if (strcmp(vStr, "$NOW") == 0) {
          // Get the current time
          timeNow(dt);
          strcat(formHL7, dt);

        } else {
          // Escape slashes in any JSON strings from the template
          char evStr[2 * strlen(vStr) + 1];
          escapeSlash(evStr, vStr);
          strcat(formHL7, evStr);

        }
        fIdx++;
        strcat(formHL7, "|");

      // Needed to avoid the last field hitting the else, but non-functional
      } else if (fid > f + 1) {
        strcat(formHL7, "|");

        
      } else {
        // TODO - change this to a HTML error message
        fprintf(stderr, "ERROR: Failed to process fields for segment %d (of %d) from json template\n", s + 1, segCount);

      }
    }
    strcat(formHL7, "<br />");

  }

  // Construct the JSON reply
  jsonReply = realloc(jsonReply, strlen(formStr) + strlen(formHL7) + 53);

  strcpy(jsonReply, "{ \"form\":\"");
  strcat(jsonReply, formStr);
  strcat(jsonReply, "\",\n\"hl7\":\"");
  strcat(jsonReply, formHL7);
  strcat(jsonReply, "\" }");

  printf("HS:\n%s\n", jsonReply);

  // Free memory
  fclose(fp);
  free(formStr);
  free(formHL7);
  free(jsonMsg);
  json_object_put(rootObj);

  // Succesful template list response
  response = MHD_create_response_from_buffer(strlen(jsonReply), (void *) jsonReply,
             MHD_RESPMEM_MUST_COPY);
             //MHD_RESPMEM_PERSISTENT);

  // TODO - check mallocs above and add frees.
  free(jsonReply);

  if (! response) return MHD_NO;
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

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

  if (strcmp (key, "hl7MessageText") == 0 ) {
    if ((size > 0) && (size <= MAXNAMESIZE)) {
      char newData[strlen(data) + 1];
      strcpy(newData, data);
      //unix2hl7(newData);
      wrapMLLP(newData);

      char *answerstring;
      answerstring = malloc(MAXANSWERSIZE);

      if (!answerstring) return MHD_NO;

      // TODO - change this to config based ip:port
      sockfd = connectSvr("127.0.0.1", "11011");
      if (sockfd >= 0) {
        sendPacket(sockfd, newData);
        listenACK(sockfd, resStr);

        snprintf(answerstring, MAXANSWERSIZE, resStr, newData);
        con_info->answerstring = answerstring;
      }

    } else {
      con_info->answerstring = NULL;
    }

    return MHD_NO;
  }
  return MHD_YES;
}

// Send the HL7 message to the web browser
static enum MHD_Result stopListenWeb(struct MHD_Connection *connection) {
  enum MHD_Result ret;
  struct MHD_Response *response;

  // Stop the child process
  if (wpid > 0) {
    kill(wpid, SIGTERM);
    wpid = 0;
  }
  close(readFD);
  isListening = 0;

  response = MHD_create_response_from_buffer(2, "OK", MHD_RESPMEM_PERSISTENT);

  if (!response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}


// TODO - libmicrohttpd leaving processes open for some reason (ps -eLf |grep MHD)
// Send the HL7 message to the web browser
static enum MHD_Result sendHL72Web(struct MHD_Connection *connection, int fd) {
  enum MHD_Result ret;
  struct MHD_Response *response;
  // TODO - check memory of res and rBuf
  int fBufS = 2048, rBufS = 512;
  char *fBuf = malloc(fBufS);
  char *rBuf = malloc(rBufS);
  char readSizeBuf[11];
  int p = 0, pLen = 1, maxPkts = 250, readSize = 0;

  if (strlen(webErrStr) > 0) {
    free(fBuf);
    free(rBuf);
    return webErrXHR(connection);
  }

  strcpy(fBuf, "event: rcvHL7\ndata: ");

  while (pLen > 0 && p < maxPkts) {
    if ((pLen = read(fd, readSizeBuf, 11)) > 0) {
      readSize = atoi(readSizeBuf);

      if (readSize > rBufS) {
        char *rBufNew = realloc(rBuf, readSize + 1);
        if (rBufNew == NULL) {
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

// TODO - WORKING - closing fd breaks listener, but leaves fd open if not present. :(
//  close(fd);

  strcat(fBuf, "\nretry:500\n\n");

  // TODO Rewrite this to be a normal page send to figure out why processes holding open
  if (p == 0) {
    strcpy(fBuf, "event: rcvHL7\ndata: HB\nretry:500\n\n");
  }

  response = MHD_create_response_from_buffer(strlen(fBuf), (void *) fBuf,
             MHD_RESPMEM_MUST_COPY);

  if (!response) return MHD_NO;

  MHD_add_response_header(response, "Content-Type", "text/event-stream");
  MHD_add_response_header(response, "Cache-Control", "no-cache");

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  // Free memory from buffers
  free(fBuf);
  free(rBuf);

  return ret;
}


// TODO - Use this instead of all the individual functions for pages
static enum MHD_Result send_page(struct MHD_Connection *connection, const char* connectiontype, const char *page) {
  enum MHD_Result ret;
  struct MHD_Response *response;

  if (strlen(webErrStr) > 0) {
    return webErrXHR(connection);
  }

  response = MHD_create_response_from_buffer(strlen(page), (void *) page, MHD_RESPMEM_PERSISTENT);

  if (! response) return MHD_NO;
  if (strcmp(connectiontype, "POST") == 0) {
    MHD_add_response_header(response, "Content-Type", "text/plain");
  }

  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  return ret;
}


// TODO - Using the web's listen interface breaks the post interface...
static enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                            const char *url, const char *method,
                                            const char *version, const char *upload_data,
                                            size_t *upload_data_size, void **con_cls) {

  (void) cls;               /* Unused. Silence compiler warning. */
  (void) url;               /* Unused. Silence compiler warning. */
  (void) version;           /* Unused. Silence compiler warning. */
  (void) upload_data;       /* Unused. Silence compiler warning. */
  (void) upload_data_size;  /* Unused. Silence compiler warning. */

  // TODO change this to /use/local/share
  char *imagePath = "/images/";
 
  if (! is_authenticated(connection, USER, PASSWORD))
    return ask_for_authentication(connection, REALM);

  if (*con_cls == NULL) {
    struct connection_info_struct *con_info;
    con_info = malloc(sizeof(struct connection_info_struct));
    if (con_info == NULL) return MHD_NO;
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
    // TODO - WORKING - free seems to crash web but seems to cause memory leak without it
    //free(con_info);
    return MHD_YES;
  }

  if (strcmp(method, "GET") == 0) {
    if (strstr(url, imagePath)) {
      return getImage(connection, url);

    } else if (strstr(url, "/templates/")) {
      return getTempForm(connection, url);

    } else if (strcmp(url, "/getTemplateList") == 0) {
      return getTemplateList(connection);

    } else if (strcmp(url, "/stopListenHL7") == 0) {
      return stopListenWeb(connection);

    } else if (strcmp(url, "/listenHL7") == 0) {
      // TODO - change static port
      if (isListening == 0) {
        // Make children ignore waiting for parent process before closing
        signal(SIGCHLD, SIG_IGN);

        pid_t pidBefore = getpid();
        wpid = fork();
        if (wpid == 0) {
          // Check for parent exiting and repeat
          int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
          if (r == -1 || getppid() != pidBefore) {
            exit(0);
          }

          startMsgListener("127.0.0.1", "22022");
          _exit(0);

        } else {

        }
        char *hhl7fifo = "hhl7fifo";
        readFD = open(hhl7fifo, O_RDONLY | O_NONBLOCK);
        //fcntl(readFD, F_SETPIPE_SZ, 1048576);
      } 

      if (isListening == 1) {
        return sendHL72Web(connection, readFD);

      } else {
        isListening = 1;
        return MHD_YES;
      }
    } else {
      return main_page(connection);
    }
  }

  if (strcmp(method, "POST") == 0) {
    struct connection_info_struct *con_info = *con_cls;

    if (*upload_data_size != 0) {
      MHD_post_process(con_info->postprocessor, upload_data, *upload_data_size);
      *upload_data_size = 0;
      return MHD_YES;

    } else if (NULL != con_info->answerstring) {
      return send_page(connection, method, con_info->answerstring);
    }
  }

  // TODO - error page
  return send_page(connection, method, errorPage);
}

int listenWeb() {
  struct MHD_Daemon *daemon;

  #define SKEY "server.key"
  #define SPEM "server.pem"

  int keySize = getFileSize(SKEY);
  int pemSize = getFileSize(SPEM);
  char key_pem[keySize + 1];
  char cert_pem[pemSize + 1];
  FILE *fpSKEY = openFile(SKEY);
  FILE *fpSPEM = openFile(SPEM);

  file2buf(key_pem, fpSKEY, keySize);
  file2buf(cert_pem, fpSPEM, pemSize);
  fclose(fpSKEY);
  fclose(fpSPEM);

  // TODO - delete below, left in sa reminder for above line changes
  //file2buf(key_pem, openFile(SKEY), keySize);
  //file2buf(cert_pem, openFile(SPEM), pemSize);

  if ((key_pem == NULL) || (cert_pem == NULL)) {
    fprintf(stderr, "ERROR: The HTTPS key/certificate files could not be read.\n");
    exit(1);
  }

//  daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_TLS, PORT, NULL,
//                            NULL, &answer_to_connection, NULL,
//                            MHD_OPTION_HTTPS_MEM_KEY, key_pem,
//                            MHD_OPTION_HTTPS_MEM_CERT, cert_pem, MHD_OPTION_END);

//  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_EPOLL | 
//                            MHD_USE_TURBO| MHD_USE_TLS,
//                            PORT, NULL, NULL, &answer_to_connection, NULL,
//                            MHD_OPTION_THREAD_POOL_SIZE, (unsigned int) 4,
//                            MHD_OPTION_HTTPS_MEM_KEY, key_pem,
//                            MHD_OPTION_HTTPS_MEM_CERT, cert_pem, MHD_OPTION_END);

//  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG | MHD_USE_POLL | MHD_USE_TCP_FASTOPEN |
//                            MHD_USE_TLS,
//                            PORT, NULL, NULL, &answer_to_connection, NULL,
//                            MHD_OPTION_THREAD_POOL_SIZE, (unsigned int) 8,
//                            MHD_OPTION_HTTPS_MEM_KEY, key_pem,
//                            MHD_OPTION_HTTPS_MEM_CERT, cert_pem, MHD_OPTION_END);


  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_EPOLL |
                            MHD_USE_TCP_FASTOPEN |
                            MHD_USE_TLS,
                            PORT, NULL, NULL, &answer_to_connection, NULL,
                            MHD_OPTION_THREAD_POOL_SIZE, (unsigned int) 4,
                            MHD_OPTION_HTTPS_MEM_KEY, key_pem,
                            MHD_OPTION_HTTPS_MEM_CERT, cert_pem, MHD_OPTION_END);



  if (NULL == daemon) {
    fprintf(stderr, "ERROR: Failed to start HTTPS daemon.\n");
    exit(1);

  } else {
    webRunning = 1;
  }

  // TODO - make this while(1) instead of get char
  (void) getchar();

  MHD_stop_daemon(daemon);
  return 0;
}
