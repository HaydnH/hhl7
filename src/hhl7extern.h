/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

// Global config struct
struct globalConfigInfo {
  // General settings
  int logLevel;

  // Web daemon settings
  char wPort[6];
  int maxAttempts;
  int sessExpiry;
  int exitDelay;
  int rQueueSize;
  int rExpiryTime;

  // Send/Listen address info
  char sIP[256];
  char sPort[6];
  char lIP[256];
  char lPort[6];
  int ackTimeout;
  int webTimeout;
};

extern struct globalConfigInfo *globalConfig;
extern int isDaemon;
extern int webRunning;
