/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

// Function Prototypes
int readJSONFile(FILE *fp, long int fileSize, char *jsonMsg);
int hhgttg(char *tName, int gType);
int getJSONValue(char *jsonMsg, int type, char *key, char *resVal);
int parseJSONTemp(char *jsonMsg, char **hl7Msg, int *hl7MsgS, char **webForm,
                  int *webFormS, int argc, char *argv[], int isWeb);
