/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

// Function Prototypes
int validPort(char *port);
int connectSvr(char *ip, char *port);
int listenACK(int sockfd, char *res);
void sendFile(FILE *fp, long int fileSize, int sockfd);
int sendPacket(int sockfd, char *hl7msg, char *resStr, int fShowTemplate);
void sendTemp(char *sIP, char *sPort, char *tName, int noSend, int fShowTemplate,
              int optind, int argc, char *argv[], char *resStr);
int sendAck(int sessfd, char *hl7msg);
int listenServer(char *port, int isWeb);
int startMsgListener(char *lIP, const char *lPort, char *sIP, char *sPort,
                     int argc, int optind, char *argv[]);
