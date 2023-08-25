/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

#include <stdio.h>
#include <stdlib.h>
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


// Main
int main(int argc, char *argv[]) {
  int sockfd, opt, option_index=0;
  int fSend = 0, fListen = 0, fSendTemplate = 0, fWeb = 0;
  FILE *fp;
  // TODO IP length only allows IP addresses - allow hostnames?
  char ip[16] = "127.0.0.1";
  // TODO - malloc these?
  char port[10] = "";
  char tName[256] = "";
  char fileName[4096] = "file.txt";
  //char templateFile[255] = "\0";

  // TODO: Check error message for unknown long options works properly
  // Parse command line options
  static struct option long_options[] = {
    {"help", no_argument, 0, 'H'},
    {0, 0, 0, 0}
  };

  while((opt = getopt_long(argc, argv, ":0Hslt:wh:p:f:", long_options, &option_index)) != -1) {
    switch(opt) {
      case 0:
        // TODO - Why did I put a yay?!? Catch all for unknown longopts?
        printf("Yay!");
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

      case 'w':
        fWeb = 1;
        break;

      case 'h':
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -h requires a value\n");
          exit(1);
        } 
        if (optarg) strcpy(ip, optarg);
        break;

      case 'p':
        if (*argv[optind-1] == '-') {
          fprintf(stderr, "ERROR: Option -p requires a value\n");
          exit(1);
        } 
        if (optarg) strcpy(port, optarg);
        break;

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
    // Set default port if not set with -p
    if (!strcmp(port, "")) strcpy(port, "11011");
    
    // Connect to the server
    sockfd = connectSvr(ip, port);

    // Send a file test
    fp = openFile(fileName);
    sendFile(fp, getFileSize(fileName), sockfd);
    //fclose(fp);
  } 

  if (fListen == 1) {
    // Set default port if not set with -p
    if (!strcmp(port, "")) strcpy(port, "22022");

    // Listen for incomming messages
    startMsgListener(ip, port);
  }

  if (fSendTemplate == 1) {
    // Set default port if not set with -p
    if (!strcmp(port, "")) strcpy(port, "11011");

    // Find the template file
    fp = findTemplate(fileName, tName);
    int fSize = getFileSize(fileName);
    fprintf(stderr, "INFO:  Using template file: %s\n", fileName);

    char *jsonMsg = malloc(fSize + 1);
    // TODO - make this a variable buffer size
    char hl7Msg[1024];

    // Read the json template to jsonMsg
    readJSONFile(fp, fSize, jsonMsg);
    // Generate HL7 based on the json template
    // TODO check these argc/argv values before using them
    json2hl7(jsonMsg, hl7Msg, argc - optind, argv + optind);

    // TODO - add a wrap & send here.
    // Wrap the packet as MLLP
    wrapMLLP(hl7Msg);

    // Connect to server, send & listen for ack
    sockfd = connectSvr(ip, port);
    sendPacket(sockfd, hl7Msg);
    listenACK(sockfd, NULL);


    // Free memory
    free(jsonMsg);
    fclose(fp);
  }

  if (fWeb == 1) {
    listenWeb();

  }

// TODO - Make sure we're closing fp etc etc - not much here!
}
