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
#include <unistd.h> 
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <systemd/sd-daemon.h>
#include <syslog.h>
#include <signal.h>
#include "hhl7extern.h"
#include "hhl7utils.h"
#include "hhl7json.h"
#include "hhl7net.h"
#include "hhl7web.h"

int isDaemon = 0;


// Show version number
static void showVersion() {
  printf("HHL7 - version 0.01\n");
  exit(0);
}


// Show help message
static void showHelp(int exCode) {
  printf("Usage:\n");
  printf("  hhl7 [-s <IP>] [-L <IP] [-p <port>] [-P <port>] [-o]\n");
  printf("       {-D|-f|-F|-t|-T|-l|-r|-n|-N} [options] [argument ...]\n\n");
  printf("Help Options:\n");
  printf("  -h, --help               Show help page and exit\n");
  printf("  -v, --version            Show version information and exit\n\n");
  printf("Network Options:\n");
  printf("  -s <ip>                  Target IP/hostname to send messages to\n");
  printf("  -L <ip>                  IP address to bind when listening/responding\n");
  printf("  -p <port>                Target port number to send messages to\n");
  printf("  -P <port>                Target port number to use for listening/responding\n\n");
  printf("Functional Options:\n");
  printf("  -f <fileName>            Send contents of a file\n");
  printf("  -F                       Send ./file.txt (shorthand for \"-f ./file.txt\")\n");
  printf("  -t <temp> [args ...]     Generate a message from a JSON template and send it\n");
  printf("  -T <temp> [<arg ...]     Same as -t, but also print message to STDOUT\n");
  printf("  -o                       Suppress sending message, use with -T for STDOUT only\n");
  printf("  -l                       Listen for incoming messages\n");
  printf("  -r <temps ...>           Respond to incoming messages if they match template\n");
  printf("  -n <integer>             Send template multiple times, meant for stress testing\n");
  printf("  -r <temps ...>           Delay between sending multiple messages with -n\n\n");

  printf("Other Options:\n");
  printf("  -D <socket>              Run as a daemon, for systemd.socket use ONLY\n");
  printf("  -w                       Run web interface, for development use ONLY\n\n");

  exit(exCode);
}


// Clean shutdown
static void cleanShutdown() {
  if (getppid() > 1) {
    writeLog(LOG_INFO, "Listener child process shutting down", 0);
  } else {
    writeLog(LOG_INFO, "Received signal, politely shutting down", 1);
  }
  if (webRunning == 1) cleanAllSessions();
  exit(0);
}


// Main
int main(int argc, char *argv[]) {
  // Arguments are required
  if (argc == 1) showHelp(1);

  int daemonSock = 0, sockfd, opt, option_index = 0;
  int fSend = 0, fListen = 0, fRespond = 0, fSendTemplate = 0, fShowTemplate = 0;
  int noSend = 0, fWeb = 0, sc = 0, sCount = 1, sSleep = 500;
  FILE *fp;

  // TODO move bind port (sIP) to config file
  int maxNameL = 255;
  char sIP[256] = "127.0.0.1";
  char lIP[256] = "127.0.0.1";
  char sPort[6] = "11011";
  char lPort[6] = "22022";
  char tName[51] = "";
  char fileName[256] = "file.txt";
  char errStr[28] = "";

  // Catch signals for clean shutdown
  struct sigaction sa;
  sa.sa_handler = cleanShutdown;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGKILL, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // Seed RNG
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == -1) {
    handleError(LOG_CRIT, "Failed to obtain system timestamp", 1, 1, 1);
  }
  srand((uint64_t) ts.tv_nsec);

  // Parse command line options
  static struct option long_options[] = {
    {"version", no_argument, 0, 'v'},
    {"help",    no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  while((opt = getopt_long(argc, argv, ":0vhD:f:Flrt:T:n:N:owWs:L:p:P:", long_options, &option_index)) != -1) {
    switch(opt) {
      case 0:
        exit(1);

      case 'v':
        showVersion();
        exit(0);

      case 'h':
        showHelp(0);
        exit(0);

      case 'D':
        if (*argv[optind - 1] == '-')
          handleError(LOG_ERR, "Option -D requires a value", 1, 1, 1);
        isDaemon = 1;
        if (optarg) daemonSock = atoi(optarg);
        break;

      case 'f':
        fSend = 1;
        if (*argv[optind - 1] == '-')
          handleError(LOG_ERR, "Option -f requires a value", 1, 1, 1);
        if (validStr(optarg, 1, maxNameL, 1) > 0)
          handleError(LOG_ERR, "Invalid value for -f flag (1-255 chars, ASCII only)", 1, 1, 1);

        if (optarg) strcpy(fileName, optarg);
        break;

      case 'F':
        fSend = 1;
        break;

      case 'l':
        fListen = 1;
        break;

      case 'r':
        fRespond = 1;
        break;

      case 't':
        if (*argv[optind - 1] == '-')
          handleError(LOG_ERR, "Option -t requires a value", 1, 1, 1);
        if (validStr(optarg, 1, 50, 1) > 0)
          handleError(LOG_ERR, "Invalid value for -t flag (1-50 chars, ASCII only)", 1, 1, 1);

        fSendTemplate = 1;
        if (optarg) strcpy(tName, optarg);
        break;

      case 'T':
        if (*argv[optind - 1] == '-')
          handleError(LOG_ERR, "Option -T requires a value", 1, 1, 1);
        if (validStr(optarg, 1, 50, 1) > 0)
          handleError(LOG_ERR, "Invalid value for -T flag (1-50 chars, ASCII only)", 1, 1, 1);

        fSendTemplate = 1;
        fShowTemplate = 1;
        if (optarg) strcpy(tName, optarg);
        break;

      case 'n':
        if (*argv[optind - 1] == '-')
          handleError(LOG_ERR, "Option -n requires a value", 1, 1, 1);

        if (optarg) sCount = atoi(optarg);
        break;

      case 'N':
        if (*argv[optind - 1] == '-')
          handleError(LOG_ERR, "Option -n requires a value", 1, 1, 1);

        if (optarg) sSleep = atoi(optarg);
        break;

      case 'o':
        noSend = 1;
        break;

      case 'w':
        fWeb = 1;
        break;

      case 's':
        if (*argv[optind - 1] == '-')
          handleError(LOG_ERR, "Option -s requires a value", 1, 1, 1);
        if (validStr(optarg, 1, 255, 1) > 0)
          handleError(LOG_ERR, "Invalid value for -s flag (1-255 chars, ASCII only)", 1, 1, 1);

        if (optarg) strcpy(sIP, optarg);
        break;

      case 'L':
        if (*argv[optind - 1] == '-')
          handleError(LOG_ERR, "Option -L requires a value", 1, 1, 1);
        if (validStr(optarg, 1, 255, 1) > 0)
          handleError(LOG_ERR, "Invalid value for -L flag (1-255 chars, ASCII only)", 1, 1, 1);

        if (optarg) strcpy(lIP, optarg);
        break;

      case 'p':
        if (*argv[optind - 1] == '-') handleError(LOG_ERR, "Option -p requires a value", 1, 1, 1);
        if (validPort(optarg) > 0) handleError(LOG_ERR, "Port provided by -p is invalid (valid: 1024-65535)", 1, 1, 1);
        if (optarg) strcpy(sPort, optarg);
        break;

      case 'P':
        if (*argv[optind - 1] == '-') handleError(LOG_ERR, "Option -P requires a value", 1, 1, 1);
        if (validPort(optarg) > 0) handleError(LOG_ERR, "Port provided by -P is invalid (valid: 1024-65535)", 1, 1, 1);
        if (optarg) strcpy(lPort, optarg);
        break;

      case ':':
        sprintf(errStr, "Option -%c requires a value", optopt);
        handleError(LOG_ERR, errStr, 1, 1, 1);

      case '?':
        sprintf(errStr, "Unknown option: -%c", optopt);
        handleError(LOG_ERR, errStr, 1, 1, 1);
    } 
  } 


  if (isDaemon == 1) {
    // Check for valid options when running as Daemon
    if (fSend + fListen + fSendTemplate + fWeb > 0)
      handleError(LOG_ERR, "-D can only be used on it's own, no other functional flags", 1, 1, 1);

    // Open the syslog file
    openLog();

    if (sd_listen_fds(0) != 1) 
      handleError(LOG_ERR, "Systemd has sent an unexpected number of socket file descriptors", 1, 1, 0);   

    daemonSock = SD_LISTEN_FDS_START + 0;
  }


  // Check we're only using 1 of listen, send, template or web option
  if (fSend + fListen + fSendTemplate + fWeb > 1)
    handleError(LOG_ERR, "Only one of -f, -l, -s, -t or -w may be used at a time", 1, 1, 1);

  if (fSend == 1) {
    // Connect to the server
    sockfd = connectSvr(sIP, sPort);

    // Send a file test
    fp = openFile(fileName, "r");
    sendFile(fp, getFileSize(fileName), sockfd);

    // Close connection
    close(sockfd);
  } 


  if (fListen == 1) {
    // Listen for incoming messages
    startMsgListener(lIP, lPort, NULL, NULL, -1, 0, NULL);
  }


  if (fRespond == 1) {
    // Listen for incoming messages & respond using template
    startMsgListener(lIP, lPort, sIP, sPort, argc, optind, argv);
  }


  if (fSendTemplate == 1) {
    
    // Send a message based on the given JSON template & arguments, repeat N times
    for (sc = 0; sc < sCount; sc++) {
      sendTemp(sIP, sPort, tName, noSend, fShowTemplate, optind, argc, argv, NULL);
      usleep(sSleep);
    }
  }


  if (fWeb == 1) {
    writeLog(LOG_INFO, "Local web process starting...", 1);
    listenWeb(daemonSock);
  }

  if (isDaemon == 1) {
    writeLog(LOG_INFO, "Daemon starting...", 0);
    listenWeb(daemonSock);
  }

  if (fWeb == 1) {
    writeLog(LOG_INFO, "Local web process starting...", 1);
    listenWeb(daemonSock);
  }
}
