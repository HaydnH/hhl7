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


// Read a json file from file pointer
void readJSONFile(FILE *fp, long int fileSize, char *jsonMsg) {
  if (!fread(jsonMsg, fileSize, 1, fp)) {
    fprintf(stderr, "ERROR: Could not read contents of json template\n");
    exit(0);
  }
  jsonMsg[fileSize] = '\0';
}


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


// Add a JSON template object to the template web form
static void addVar2WebForm(char* hl7Msg, char *webForm, char *nStr,
                           struct json_object *fieldObj) {

  struct json_object *optsObj = NULL, *optObj = NULL, *oObj = NULL, *defObj = NULL;
  char *oStr = NULL, *dStr = NULL;
  int o = 0;

  // HTML strings to keep the code below tidy
  const char wStr1[]  = "<div class='tempFormField'><div class='tempFormKey'>";
  const char wStr2[]  = ":</div><div class='tempFormValue'>";
  const char wStr3[]  = "<select id='HHL7_FL_";
  const char wStr4[]  = "' class='tempFormInput'";
  const char wStr5[]  = " onInput='updateHL7(this.id, this.value);' />";
  const char wStr6[]  = "<option value='";
  const char wStr7[]  = "' selected='selected";
  const char wStr8[]  = "'>";
  const char wStr9[]  = "</option>";
  const char wStr10[] = "</select>";
  const char wStr11[] = "<input id='HHL7_FL_";
  const char wStr12[] = "' class='tempFormInput'";
  const char wStr13[] = "</div></div>";

  
  // Try to obtain the options and default values from the JSON template
  json_object_object_get_ex(fieldObj, "options", &optsObj);
  json_object_object_get_ex(fieldObj, "default", &defObj);
  dStr = (char *) json_object_get_string(defObj);

  // Add the key name to the web form
  sprintf(webForm + strlen(webForm), "%s%s%s", wStr1, nStr, wStr2);

  // If this is variable is a select item, build the option list
  if (optsObj) {
    sprintf(webForm + strlen(webForm), "%s%s%s%s", wStr3, nStr, wStr4, wStr5);

    for (o = 0; o < json_object_array_length(optsObj); o++) {
      optObj = json_object_array_get_idx(optsObj, o);
      json_object_object_get_ex(optObj, "option", &oObj);
      oStr = (char *) json_object_get_string(oObj);

      sprintf(webForm + strlen(webForm), "%s%s", wStr6, oStr);
      if (strcmp(oStr, dStr) == 0) {
        sprintf(webForm + strlen(webForm), "%s", wStr7);
//        sprintf(hl7Msg + strlen(hl7Msg), "%s%s%s", "<span class='HHL7_FL_",
//                                                    nStr, "_HL7'></span>");

//        sprintf(hl7Msg + strlen(hl7Msg), "%s", dStr);
      } 

      sprintf(webForm + strlen(webForm), "%s%s%s", wStr8, oStr, wStr9);
    }

    sprintf(webForm + strlen(webForm), "%s", wStr10);

  } else {
    sprintf(webForm + strlen(webForm), "%s%s%s%s", wStr11, nStr, wStr12, wStr5);

  }
  sprintf(webForm + strlen(webForm), "%s", wStr13);

  if (dStr) {
    sprintf(hl7Msg + strlen(hl7Msg), "%s%s%s%s%s", "<span class='HHL7_FL_", nStr,
                                                    "_HL7'>", dStr, "</span>");
  } else {
    sprintf(hl7Msg + strlen(hl7Msg), "%s%s%s", "<span class='HHL7_FL_", nStr,
                                                    "_HL7'></span>");
  }
}


// Function turn json variables in to a value
static void parseVals(char* hl7Msg, char *vStr, char *nStr, char fieldTok,
                      char *argv[], int lastField, int isWeb, char *webForm,
                      struct json_object *fieldObj) {

  int varLen = strlen(vStr), varNum = 0;
  char varNumBuf[varLen];
  char valSpan[strlen(nStr) + 124];
  char dt[26] = "";

  // Replace json value with the current datetime.
  if (strcmp(vStr, "$NOW") == 0) {
    if (dt[0] == '\0') timeNow(dt);
    strcat(hl7Msg, dt);

  // Replace json value with a command line argument
  } else if (nStr && strncmp(vStr, "$VAR", 4) == 0) {
    if (varLen == 4) {
      fprintf(stderr, "ERROR: No numeric value found after $VAR in JSON template\n");
      exit(1);

    } else {
      strncpy(varNumBuf, vStr + 4, varLen - 4);
      varNumBuf[varLen - 4] = '\0';
      // TODO error check for non-integers
      varNum = atoi(varNumBuf) - 1;

      // Add the variable to the web form and add spans to the HL7 message for linking
      if (isWeb == 1) {
        // Add the variable to the webform if not already present
        sprintf(valSpan, "%s%s%s", "id='HHL7_FL_", nStr, "'");
        if (!strstr(webForm, valSpan)) {
          addVar2WebForm(hl7Msg, webForm, nStr, fieldObj);

        } else {
          sprintf(hl7Msg + strlen(hl7Msg), "%s%s%s", "<span class='HHL7_FL_",
                                                      nStr, "_HL7'></span>");
        }

      } else {
        strcat(hl7Msg, argv[varNum]);

      }
    }

  } else {
    strcat(hl7Msg, vStr); 

  }

  if (lastField > 1) {
    sprintf(hl7Msg + strlen(hl7Msg), "%c", fieldTok);
  }
}


// Parse a JSON fields or subfields object and convert to HL7
static void parseJSONFields(struct json_object *fieldsObj, char* hl7Msg, int argc,
                            char *argv[], char fieldTok, char subFieldTok,
                            int isWeb, char *webForm) {

  struct json_object *fieldObj = NULL, *valObj = NULL, *idObj = NULL;
  char *vStr = NULL, *nStr = NULL;
  char nameStr[65] = "";
  int f, c = 0, fid = 0, fieldCount = 0;

  fieldCount = json_object_array_length(fieldsObj);

  // For each field
  for (f = 0; f < fieldCount; f++) {
    // Get individual segment as json
    fieldObj = json_object_array_get_idx(fieldsObj, f);
    // Get the ID of the current field value
    json_object_object_get_ex(fieldObj, "id", &idObj);
    fid = (int) json_object_get_int(idObj);

    // TODO - could seg fault here if the buffer isn't long enough...
    // Add a field separator until we reach the ID of this field
    c++;
    while (c < fid) {
      sprintf(hl7Msg + strlen(hl7Msg), "%c", fieldTok);
      c++;
    }

    // TODO - remove a value and/or name from a template and add error checking
    // Parse the value for this field
    json_object_object_get_ex(fieldObj, "value", &valObj);
    if (json_object_get_type(valObj) == json_type_string) {
      vStr = (char *) json_object_get_string(valObj);

      // Get the name of the field for use on the web form
      if (isWeb == 1) {
        json_object_object_get_ex(fieldObj, "name", &valObj);
        nStr = (char *) json_object_get_string(valObj);
      }

      // Parse the JSON value
      if (json_object_get_type(valObj) == json_type_string) {
        if (strlen(nStr) > 64) {
          fprintf(stderr, "ERROR: JSON template name exceeds 64 character limit.\n");
          exit(1);
        }
        strcpy(nameStr, nStr);
      }
      parseVals(hl7Msg, vStr, nameStr, fieldTok, argv, fieldCount - f, 
                isWeb, webForm, fieldObj);

    // If there's no value, check for and handle subfields
    } else {
      json_object_object_get_ex(fieldObj, "subfields", &valObj);
      int subFieldCount = json_object_array_length(valObj);
      if (subFieldCount > 0) {
        parseJSONFields(valObj, hl7Msg, argc, argv, subFieldTok, subFieldTok,
                        isWeb, webForm);

      } else {
        fprintf(stderr, "ERROR: Could not find a value or subfield flag for a field in json templatei.\n");
        exit(1);

      }
    }
  }
}


// Parse a JSON segments object and convert to HL7
static void parseJSONSegs(struct json_object *segsObj, char* hl7Msg, int argc,
                          char *argv[], char fieldTok, char subFieldTok,
                          int isWeb, char *webForm) {

  struct json_object *segObj = NULL, *fieldsObj = NULL, *valObj = NULL;
  char *vStr = NULL;
  int s, segCount = 0;

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
    sprintf(hl7Msg + strlen(hl7Msg), "%s%c", vStr, fieldTok);

    // Get full segment section from the json template and parse it
    json_object_object_get_ex(segObj, "fields", &fieldsObj);
    parseJSONFields(fieldsObj, hl7Msg, argc, argv, fieldTok, subFieldTok,
                    isWeb, webForm);

    if (isWeb == 1) {
      sprintf(hl7Msg + strlen(hl7Msg), "%s", "<br />");
    } else {
      sprintf(hl7Msg + strlen(hl7Msg), "%c", '\r');
    }
  }

  // TODO - check if the put from parseJSONTemp frees memory here
}


// Parse a JSON template file
void parseJSONTemp(char *jsonMsg, char *hl7Msg, char *webForm,
                   int argc, char *argv[], int isWeb) {

  struct json_object *rootObj= NULL, *valObj = NULL, *segsObj = NULL;
  // TODO - make these variable based on MSH segment for CLI (maybe not web?)
  char fieldTok = '|', subFieldTok = '^';
  int argcount = 0;

  rootObj = json_tokener_parse(jsonMsg);
  json_object_object_get_ex(rootObj, "argcount", &valObj);
  argcount = json_object_get_int(valObj);

  if (json_object_get_type(valObj) != json_type_int) {
    fprintf(stderr, "ERROR: Could not read integer value for argcount from JSON template.\n");
    exit(1);

  } else if (isWeb == 0 && argc - argcount != 0) {
    fprintf(stderr, "ERROR: The number of arguments passed on the commmand line (%d) does not match the json template (%d)\n", argc, argcount);
    exit(1);
  }

  // Get this segment section from the json template and parse it
  json_object_object_get_ex(rootObj, "segments", &segsObj);
  int segCount = json_object_array_length(segsObj);

  if (segCount > 0) {
    // json_object_to_json_string_ext
    parseJSONSegs(segsObj, hl7Msg, argc, argv, fieldTok, subFieldTok,
                  isWeb, webForm);
  }

  // Free memory
  json_object_put(rootObj);
}
