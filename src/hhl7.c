/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include "hhl7extern.h"
#include "hhl7utils.h"
#include "hhl7json.h"
#include "hhl7net.h"
#include "hhl7web.h"

char hl7Buf[1024] = "";

// Show help message
// TODO: Write help message, no point doing it until we've coded it...
void showHelp() {
  printf("TODO: Help Message!\n");
}


// TODO - if no arguments are provided then there's no error message, showHelp
// Main
int main(int argc, char *argv[]) {
  int sockfd, opt, option_index=0;
  int fSend = 0, fListen = 0, fSendTemplate = 0, fShowTemplate = 0, noSend = 0, fWeb = 0;
  FILE *fp;

  // TODO - error check for file names, hostnames > 256 etc
  // TODO move bind port (sIP) to config file
  char sIP[256] = "127.0.0.1";
  char lIP[256] = "127.0.0.1";
  char sPort[10] = "11011";
  char lPort[10] = "22022";
  char tName[256] = "";
  char fileName[256] = "file.txt";

  // Seed RNG
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == -1) {
    // TODO - use error handle function
    printf("ERROR: Failed to obtain system timestamp.\n");
  }
  srand((uint64_t) ts.tv_nsec);
  //srand ((unsigned int) time (NULL));

  // TODO: Check error message for unknown long options works properly
  // Parse command line options
  static struct option long_options[] = {
    {"help", no_argument, 0, 'H'},
    {0, 0, 0, 0}
  };

  while((opt = getopt_long(argc, argv, ":0Hslt:T:owh:L:p:P:f:", long_options, &option_index)) != -1) {
    switch(opt) {
      case 0:
        exit(1);

      case 'H':
        showHelp();
        exit(0);

      case 's':
        fSend = 1;
        break;

      case 'l':
        fListen = 1;
        break;

      case 't':
        fSendTemplate = 1;
        if (optarg) strcpy(tName, optarg);
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -t requires a value\n");
          exit(1);
        }
        break;

      case 'T':
        fSendTemplate = 1;
        fShowTemplate = 1;
        if (optarg) strcpy(tName, optarg);
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -T requires a value\n");
          exit(1);
        }
        break;

      case 'o':
        noSend = 1;
        break;

      case 'w':
        fWeb = 1;
        break;

      case 'h':
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -h requires a value\n");
          exit(1);
        } 
        if (optarg) strcpy(sIP, optarg);
        break;

      case 'L':
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -L requires a value\n");
          exit(1);
        }
        if (optarg) strcpy(lIP, optarg);
        break;

      case 'p':
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -p requires a value\n");
          exit(1);
        } 
        if (optarg) strcpy(sPort, optarg);
        break;

      case 'P':
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -P requires a value\n");
          exit(1);
        }
        if (optarg) strcpy(lPort, optarg);
        break;

// TODO - -f without an argument seems to try to connect to server 1st before failing
      case 'f':
        fSend = 1;
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -f requires a value\n");
          exit(1);
        } 
        if (optarg) strcpy(fileName, optarg);
        break;

      case ':':
        fprintf(stderr, "ERROR: Option -%c requires a value\n", optopt);
        exit(1);

      case '?':
        printf("ERROR: Unknown option: %c\n", optopt);
        exit(1);
    } 
  } 
      
  // Check we're only using 1 of listen, send, template or web option
  if (fSend + fListen + fSendTemplate + fWeb > 1) {
    fprintf(stderr, "ERROR: Only one of -f, -l, -s, -t or -w may be used at a time.\n");
    exit(1);
  }

  // TODO handle port/ip config etc instead of repeating in other flags.
  if (fSend == 1) {
    // Connect to the server
    sockfd = connectSvr(sIP, sPort);

    // Send a file test
    fp = openFile(fileName, "r");
    sendFile(fp, getFileSize(fileName), sockfd);
    //fclose(fp);
  } 

  if (fListen == 1) {
    // Listen for incomming messages
    startMsgListener(lIP, lPort);
  }

  if (fSendTemplate == 1) {
    // Find the template file
    fp = findTemplate(fileName, tName);
    int fSize = getFileSize(fileName);
    fprintf(stderr, "INFO:  Using template file: %s\n", fileName);

    char *jsonMsg = malloc(fSize + 1);
    // TODO - max hl7 size is 1024? Need to malloc here!
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
      // Wrap the packet as MLLP
      //wrapMLLP(hl7Msg);

      // Connect to server, send & listen for ack
      sockfd = connectSvr(sIP, sPort);
      sendPacket(sockfd, hl7Msg, NULL);
    }

    // Free memory
    free(jsonMsg);
    fclose(fp);
  }

  if (fWeb == 1) {
    listenWeb();

  }

// TODO - Make sure we're closing fp etc etc - not much here!
}
