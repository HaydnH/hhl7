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
#include <systemd/sd-daemon.h>
#include "hhl7extern.h"
#include "hhl7utils.h"
#include "hhl7json.h"
#include "hhl7net.h"
#include "hhl7web.h"

int isDaemon = 0;


// Show help message
// TODO: Write help message, no point doing it until we've coded it...
void showHelp() {
  printf("TODO: Help Message!\n");
}


// TODO - if no arguments are provided then there's no error message, showHelp
// Main
int main(int argc, char *argv[]) {
  int daemonSock = 0, sockfd, opt, option_index = 0;
  int fSend = 0, fListen = 0, fRespond = 0, fSendTemplate = 0, fShowTemplate = 0;
  int noSend = 0, fWeb = 0;
  FILE *fp;

  // TODO - error check for file names, hostnames > 256 etc
  // TODO move bind port (sIP) to config file
  char sIP[256] = "127.0.0.1";
  char lIP[256] = "127.0.0.1";
  char sPort[6] = "11011";
  char lPort[6] = "22022";
  char tName[256] = "";
  char fileName[256] = "file.txt";

  // Seed RNG
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == -1) {
    // TODO - use error handle function
    handleError(2, "Failed to obtain system timestamp", 1, 1, 1);
  }
  srand((uint64_t) ts.tv_nsec);

  // TODO: Check error message for unknown long options works properly
  // Parse command line options
  static struct option long_options[] = {
    {"help", no_argument, 0, 'H'},
    {0, 0, 0, 0}
  };

  while((opt = getopt_long(argc, argv, ":0HD:slr:t:T:owWh:L:p:P:f:", long_options, &option_index)) != -1) {
    switch(opt) {
      case 0:
        exit(1);

      case 'H':
        showHelp();
        exit(0);

      case 'D':
        if (*argv[optind-1] == '-') handleError(3, "Option -D requires a value", 1, 1, 1);
        isDaemon = 1;
        if (optarg) daemonSock = atoi(optarg);
        break;

      case 's':
        fSend = 1;
        break;

      case 'l':
        fListen = 1;
        break;

      case 'r':
        if (*argv[optind-1] == '-') handleError(3, "Option -r requires a value", 1, 1, 1);
        fRespond = 1;
        if (optarg) strcpy(tName, optarg);
        break;

      case 't':
        if (*argv[optind-1] == '-') handleError(3, "Option -t requires a value", 1, 1, 1);
        fSendTemplate = 1;
        if (optarg) strcpy(tName, optarg);
        break;

      case 'T':
        if (*argv[optind-1] == '-') handleError(3, "Option -T requires a value", 1, 1, 1);
        fSendTemplate = 1;
        fShowTemplate = 1;
        if (optarg) strcpy(tName, optarg);
        break;

      case 'o':
        noSend = 1;
        break;

      case 'w':
        fWeb = 1;
        break;

      case 'h':
        if (*argv[optind-1] == '-') handleError(3, "Option -h requires a value", 1, 1, 1);
        // TODO error check all command line arguments (length etc)
        if (optarg) strcpy(sIP, optarg);
        break;

      case 'L':
        if (*argv[optind-1] == '-') handleError(3, "Option -L requires a value", 1, 1, 1);
        if (optarg) strcpy(lIP, optarg);
        break;

      case 'p':
        if (*argv[optind-1] == '-') handleError(3, "Option -p requires a value", 1, 1, 1);
        if (validPort(optarg) > 0) handleError(3, "Port provided by -p is invalid (valid: 1024-65535)", 1, 1, 1);
        if (optarg) strcpy(sPort, optarg);
        break;

      case 'P':
        if (*argv[optind-1] == '-') handleError(3, "Option -P requires a value", 1, 1, 1);
        if (validPort(optarg) > 0) handleError(3, "Port provided by -P is invalid (valid: 1024-65535)", 1, 1, 1);
        if (optarg) strcpy(lPort, optarg);
        break;

      case 'f':
        fSend = 1;
        if (*argv[optind-1] == '-') handleError(3, "Option -f requires a value", 1, 1, 1);
        if (optarg) strcpy(fileName, optarg);
        break;

      case ':':
        sprintf(infoStr, "Option -%c requires a value", optopt);
        handleError(3, infoStr, 1, 1, 1);

      case '?':
        sprintf(infoStr, "Unknown option: -%c", optopt);
        handleError(3, infoStr, 1, 1, 1);
    } 
  } 


  if (isDaemon == 1) {
    // Check for valid options when running as Daemon
    if (fSend + fListen + fSendTemplate + fWeb > 0)
      handleError(3, "-D can only be used on it's own, no other functional flags", 1, 1, 1);

    // Open the syslog file
    openLog();

    if (sd_listen_fds(0) != 1) 
      handleError(3, "Systemd has sent an unexpected number of socket file descriptors", 1, 1, 0);   

    daemonSock = SD_LISTEN_FDS_START + 0;
  }


  // Check we're only using 1 of listen, send, template or web option
  if (fSend + fListen + fSendTemplate + fWeb > 1)
    handleError(3, "Only one of -f, -l, -s, -t or -w may be used at a time", 1, 1, 1);

  if (fSend == 1) {
    // Connect to the server
    sockfd = connectSvr(sIP, sPort);

    // Send a file test
    fp = openFile(fileName, "r");
    sendFile(fp, getFileSize(fileName), sockfd);
  } 


  if (fListen == 1) {
    // Listen for incomming messages
    startMsgListener(lIP, lPort, NULL, NULL, NULL);
  }


  if (fRespond == 1) {
    // Listen for incomming messages & respond using template
    startMsgListener(lIP, lPort, sIP, sPort, tName);
  }


  if (fSendTemplate == 1) {
    // Send a message based on the given jon template & arguments
    sendTemp(sIP, sPort, tName, noSend, fShowTemplate, optind, argc, argv, NULL);
  }


  if (fWeb == 1) {
    writeLog(6, "Local web process starting...", 1);
    listenWeb(daemonSock);
  }

  if (isDaemon == 1) {
    writeLog(6, "Daemon starting...", 0);
    listenWeb(daemonSock);
  }

  if (fWeb == 1) {
    writeLog(6, "Local web process starting...", 1);
    listenWeb(daemonSock);
  }

// TODO - Make sure we're closing fp etc etc - not much here!
}
