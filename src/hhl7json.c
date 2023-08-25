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
#include <json.h>
#include "hhl7utils.h"

void readJSONFile(FILE *fp, long int fileSize, char *jsonMsg) {
  if (!fread(jsonMsg, fileSize, 1, fp)) {
    fprintf(stderr, "ERROR: Could not read contents of json template\n");
    exit(0);
  }
  jsonMsg[fileSize] = '\0';
}

// TODO enumerate the variables so they can be used more time, e.g: VAR1 is hospnum1 & 2
// Function turn json variables in to a value
void parseopts(char *vStr, char *dt, int *varc, char *varv) {
  // Replace json value with the current datetime.
  if (strcmp(vStr, "$NOW") == 0) {
    if (dt[0] == '\0') timeNow(dt);
    strcpy(vStr, dt);

  // Replace json value with a command line argument
  } else if (strcmp(vStr, "$VAR") == 0) {
    strcpy(vStr, varv);
    (*varc)++;

  }
}

// Function to get the value of a json key - currently only for root object keys
// int type determines data type - only strings and bools for now:
//    1 = boolean
//    2 = string
int getJSONValue(char *jsonMsg, int type, char *key, char *resVal) {
  struct json_object *rootObj= NULL, *valObj = NULL;
  int retVal = -1;

  rootObj = json_tokener_parse(jsonMsg);
  json_object_object_get_ex(rootObj, key, &valObj);

  if (type == 1) {
    if (json_object_get_type(valObj) == json_type_boolean) {
      retVal = json_object_get_boolean(valObj);
    } else {
      retVal = -1;
    }

  } else if (type == 2) {
    strcpy(resVal, (char *) json_object_get_string(valObj));
    if (json_object_get_type(valObj) == json_type_string) {
      retVal = 0;
    } else {
      retVal = -1;
    }
  }
  json_object_put(rootObj);
  return retVal;
}


// TODO - make sure we're sending argc as optind args, maybe change variable name?
void json2hl7(char *jsonMsg, char* hl7Msg, int argc, char *argv[]) {
  char *vStr = NULL;
  int s = 0, f = 0, fid = 0, argcount = 0, segCount = 0, fIdx = 0, fieldCount = 0;
  char dt[26] = "";
  int varc = 0;
  struct json_object *rootObj= NULL, *idObj = NULL, *valObj = NULL;
  struct json_object *segsObj = NULL, *segObj = NULL, *fieldsObj = NULL, *fieldObj = NULL;

  rootObj = json_tokener_parse(jsonMsg);
  json_object_object_get_ex(rootObj, "argcount", &valObj);
  argcount = json_object_get_int(valObj);

  if (json_object_get_type(valObj) != json_type_int) {
    fprintf(stderr, "ERROR: Could not read integer value for argcount from json template\n");
    exit(1);

  } else if (argc - argcount != 0) {
    fprintf(stderr, "ERROR: The number of arguments passed on the commmand line (%d) does not match the json template (%d)\n", argc, argcount);
    exit(1);
  }

  // Get full segment section from the json template
  json_object_object_get_ex(rootObj, "segments", &segsObj);
  segCount = json_object_array_length(segsObj);
  
  // For each individual segment
  for (s = 0; s < segCount; s++) {

    // Get individual segment as json
    segObj = json_object_array_get_idx(segsObj, s);

    // Get the name of this segment
    json_object_object_get_ex(segObj, "name", &valObj);
    vStr = (char *) json_object_get_string(valObj);
    if (json_object_get_type(valObj) != json_type_string) {
      fprintf(stderr, "ERROR: Could not read string name for segment %d (of %d) from json template\n", s + 1, segCount);
      exit(1);
    }
    sprintf(hl7Msg + strlen(hl7Msg), "%s|", vStr);

    // Get the fieldcount for this segment
    json_object_object_get_ex(segObj, "fieldcount", &valObj);
    fieldCount = (int) json_object_get_int(valObj);
    if (json_object_get_type(valObj) != json_type_int) {
      fprintf(stderr, "ERROR: Could not read fieldcount (int) for segment %d (of %d) from json template\n", s + 1, segCount);
      exit(1);
    }

    // Get full fields object for this segment from the json template:
    fIdx = 0;
    json_object_object_get_ex(segObj, "fields", &fieldsObj);

    // For each individual id:value field in this segment
    for (f = 0; f < fieldCount; f++) {
      // Get individual field object from fields
      fieldObj = json_object_array_get_idx(fieldsObj, fIdx);

      // Get the ID of the current field value
      json_object_object_get_ex(fieldObj, "id", &idObj);
      fid = (int) json_object_get_int(idObj);

      // If the json id matches the expected field index then process the value
      if (fid == f + 1) {
        json_object_object_get_ex(fieldObj, "value", &valObj);
        vStr = (char *) json_object_get_string(valObj);

        // Check if any of the values are variables, e.g: time now etc
        parseopts(vStr, dt, &varc, argv[varc]);

        sprintf(hl7Msg + strlen(hl7Msg), "%s|", vStr);
        fIdx++;

      } else if (fid > f + 1) {
        hl7Msg[strlen(hl7Msg)] = '|';
        
      } else {
        fprintf(stderr, "ERROR: Failed to process fields for segment %d (of %d) from json template\n", s + 1, segCount);

      }
    }
    hl7Msg[strlen(hl7Msg)] = '\r';
  }

  // Null terminate C string
  hl7Msg[strlen(hl7Msg)] = '\0';

  // Free memory
  json_object_put(rootObj);
}
