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
  const char *tPath = "/usr/local/share/hhl7/templates/";
  const char *nOpt = "<option value=\"None\">None</option>\n";
  char fName[50], tName[50], fullName[50+strlen(tPath)], *ext; //, *tempOpts;
  char *tempOpts = malloc(36);
  char *newPtr;

  // TODO - check pointers updated to new memory created with realloc
  // TODO is sizeof needed for char, should just be 36?
  //tempOpts = (char *) malloc(36 * sizeof(char));
  //tempOpts = (char *) malloc(36);
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

          // Increase memory for tempOpts to allow file name etc
          newPtr = realloc(tempOpts, (2*strlen(fName)) + strlen(tempOpts) + 37);
          if (newPtr == NULL) {
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
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

static enum MHD_Result getTempForm(struct MHD_Connection *connection, const char *url) {
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

  fp = openFile(fileName);
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
  //strcat(jsonReply, webHL7);
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

  if ((key_pem == NULL) || (cert_pem == NULL)) {
    fprintf(stderr, "ERROR: The HTTPS key/certificate files could not be read.\n");
    exit(1);
  }

  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_EPOLL |
                            MHD_USE_TCP_FASTOPEN | MHD_USE_TLS,
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
