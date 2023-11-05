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
// TODO change any references of this to use the library json from file
int readJSONFile(FILE *fp, long int fileSize, char *jsonMsg) {
  if (!fread(jsonMsg, fileSize, 1, fp)) {
    fprintf(stderr, "ERROR: Could not read contents of json template\n");
    return 1;
  }
  jsonMsg[fileSize] = '\0';
  return 0;
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
static void addVar2WebForm(char **webForm, int *webFormS, struct json_object *fieldObj) {
  struct json_object *nameObj = NULL, *optsObj = NULL, *optObj = NULL, *oObj = NULL;
  struct json_object *defObj = NULL;
  char *oStr = NULL, *nStr = NULL, *dStr = NULL;
  int o = 0, reqS = 0;

  // HTML strings to keep the code below tidy
  const char wStr1[]  = "<div class='tempFormField'><div class='tempFormKey'>";
  const char wStr2[]  = ":</div><div class='tempFormValue'>";
  const char wStr3[]  = "<select id='HHL7_FL_";
  const char wStr4[]  = "' class='tempFormSelct'";
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
  json_object_object_get_ex(fieldObj, "name", &nameObj);
  json_object_object_get_ex(fieldObj, "options", &optsObj);
  json_object_object_get_ex(fieldObj, "default", &defObj);
  nStr = (char *) json_object_get_string(nameObj);
  dStr = (char *) json_object_get_string(defObj);

  // Stop processing if this is not a named object or is already on the form
  if (!nStr) return;
  char valSpan[strlen(nStr) + 15];
  sprintf(valSpan, "%s%s%s", "id='HHL7_FL_", nStr, "'");
  if (strstr(*webForm, valSpan) != NULL) return;

  // Increase memory, if required, for all strings outside the below for loop
  reqS = strlen(*webForm) + (2 * strlen(nStr)) + 128;
  if (reqS > *webFormS) *webForm = dblBuf(*webForm, webFormS, reqS);

  // Add the key name to the web form
  sprintf(*webForm + strlen(*webForm), "%s%s%s", wStr1, nStr, wStr2);

  // If this is variable is a select item, build the option list
  if (optsObj) {
    sprintf(*webForm + strlen(*webForm), "%s%s%s%s", wStr3, nStr, wStr4, wStr5);

    for (o = 0; o < json_object_array_length(optsObj); o++) {
      optObj = json_object_array_get_idx(optsObj, o);
      json_object_object_get_ex(optObj, "option", &oObj);
      oStr = (char *) json_object_get_string(oObj);

      // Increase memory, if required, for all strings inside this for loop
      reqS = reqS + (2 * strlen(oStr)) + 50;
      if (reqS > *webFormS) *webForm = dblBuf(*webForm, webFormS, reqS);

      sprintf(*webForm + strlen(*webForm), "%s%s", wStr6, oStr);
      if (strcmp(oStr, dStr) == 0) {
        sprintf(*webForm + strlen(*webForm), "%s", wStr7);
      } 

      sprintf(*webForm + strlen(*webForm), "%s%s%s", wStr8, oStr, wStr9);
    }

    sprintf(*webForm + strlen(*webForm), "%s", wStr10);

  } else {
    sprintf(*webForm + strlen(*webForm), "%s%s%s%s", wStr11, nStr, wStr12, wStr5);

  }
  sprintf(*webForm + strlen(*webForm), "%s", wStr13);
}


// Function turn json variables in to a value
static void parseVals(char ***hl7Msg, int *hl7MsgS, char *vStr, char *nStr, char fieldTok,
                      char *argv[], int lastField, int isWeb, char **webForm,
                      struct json_object *fieldObj) {

  struct json_object *defObj = NULL, *lower = NULL, *upper = NULL;
  char *dStr = NULL;
  // TODO - 32? Random lenght, check it
  char rndStr[32];
  int varLen = strlen(vStr), varNum = 0, reqS = 0;
  char varNumBuf[varLen];
  char dtNow[26] = "";
  char dtVar[26] = "";
  int aMins = 0;

  // Replace json value with the current datetime.
  if (strncmp(vStr, "$NOW", 4) == 0) {
    if (strlen(vStr) == 4) {
      if (dtNow[0] == '\0') timeNow(dtNow, 0);
      reqS = strlen(**hl7Msg) + 28;
      if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
      strcat(**hl7Msg, dtNow);

    } else {
      if (vStr[4] == '+' || vStr[4] == '-') {
        aMins = atoi(vStr + 4);
        timeNow(dtVar, aMins);
        reqS = strlen(**hl7Msg) + 28;
        if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
        strcat(**hl7Msg, dtVar);

      } else {
        // TODO Error handle
        printf("Invalid time adjustment\n");
      }
    }

  } else if (strncmp(vStr, "$RND", 4) == 0) {
    // TODO- No error checking here
    json_object_object_get_ex(fieldObj, "lower", &lower);
    json_object_object_get_ex(fieldObj, "upper", &upper);
    const char *l = json_object_get_string(lower);
    const char *u = json_object_get_string(upper);

    getRand(atoi(l), atoi(u), 2, rndStr);
    reqS = strlen(**hl7Msg) + strlen(rndStr); 
    if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
    strcat(**hl7Msg, rndStr);

  // Replace json value with a command line argument
  } else if (nStr && strncmp(vStr, "$VAR", 4) == 0) {
    if (varLen == 4) {
      // TODO use web friendly function for errors
      fprintf(stderr, "ERROR: No numeric value found after $VAR in JSON template\n");
      exit(1);

    } else {
      strncpy(varNumBuf, vStr + 4, varLen - 4);
      varNumBuf[varLen - 4] = '\0';
      // TODO error check for non-integers
      varNum = atoi(varNumBuf) - 1;

      // Add the variable to the web form and add spans to the HL7 message for linking
      if (isWeb == 1) {
        // Check if ths value has a default and add it to the JSON template
        json_object_object_get_ex(fieldObj, "default", &defObj);
        dStr = (char *) json_object_get_string(defObj);
        
        // Increase the memory allocated to hl7Msg if needed
        reqS = strlen(**hl7Msg) + strlen(nStr) + 37;
        if (dStr) reqS = reqS + strlen(dStr);
        if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);

        // Write the span to the HL7 message to link from web form
        if (dStr) {
          sprintf(**hl7Msg + strlen(**hl7Msg), "%s%s%s%s%s", "<span class='HHL7_FL_",
                                                       nStr, "_HL7'>", dStr, "</span>");
        } else {
          sprintf(**hl7Msg + strlen(**hl7Msg), "%s%s%s", "<span class='HHL7_FL_",
                                                            nStr, "_HL7'></span>");
        }

      } else {
        reqS = strlen(**hl7Msg) + strlen(argv[varNum]) + 2;
        if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);

        strcat(**hl7Msg, argv[varNum]);
      }
    }

  } else {
    reqS = strlen(**hl7Msg) + strlen(vStr) + 2;
    if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);

    strcat(**hl7Msg, vStr); 

  }

  if (lastField > 1) {
    // No need to add mem with dblBuf, the above +N values handle it
    sprintf(**hl7Msg + strlen(**hl7Msg), "%c", fieldTok);
  }
}


// Parse the JSON field
static void parseJSONField(struct json_object *fieldObj, int *lastFid, int lastField, 
                           char **hl7Msg, int *hl7MsgS, char *argv[], char fieldTok,
                           int isWeb, char **webForm) {

  struct json_object *valObj = NULL, *idObj = NULL;
  char *vStr = NULL, *nStr = NULL;
  char nameStr[65] = "";
  int f = 0, fid = 0, reqS = 0;

  // Get the ID of the current field value
  json_object_object_get_ex(fieldObj, "id", &idObj);
  fid = (int) json_object_get_int(idObj);

  // Add memory to hl7Msg if needed
  reqS = strlen(*hl7Msg) + (fid - *lastFid);
  if (reqS > *hl7MsgS) *hl7Msg = dblBuf(*hl7Msg, hl7MsgS, reqS);

  while (f < fid - *lastFid - 1) {
    sprintf(*hl7Msg + strlen(*hl7Msg), "%c", fieldTok);
    f++;
  }

  // TODO - remove a value and/or name from a template to test error checking
  // Parse the value for this field
  json_object_object_get_ex(fieldObj, "value", &valObj);
  if (json_object_get_type(valObj) == json_type_string) {
    vStr = (char *) json_object_get_string(valObj);

    // Get the name of the field for use on the web form
    if (isWeb == 1) {
      json_object_object_get_ex(fieldObj, "name", &valObj);
      nStr = (char *) json_object_get_string(valObj);

      if (json_object_get_type(valObj) == json_type_string) {
        // TODO - consider allowing longer names and malloc - or pointer to json object!
        if (strlen(nStr) > 64) {
          // TODO - update this to web error handling
          fprintf(stderr, "ERROR: JSON template name exceeds 64 character limit.\n");
          exit(1);
        } else {
          strcpy(nameStr, nStr);
        }
      }
    }

    // Parse JSON values
    parseVals(&hl7Msg, hl7MsgS, vStr, nameStr, fieldTok, argv, lastField, 
              isWeb, webForm, fieldObj);

  }

  *lastFid = fid;
}


// Parse the JSON segment
static void parseJSONSegs(struct json_object *segObj, char **hl7Msg, int *hl7MsgS,
                          char fieldTok) {

  struct json_object *valObj = NULL;
  char *vStr = NULL;
  int reqS = 0;

  // Get the name of this segment
  json_object_object_get_ex(segObj, "name", &valObj);
  vStr = (char *) json_object_get_string(valObj);

  if (json_object_get_type(valObj) != json_type_string) {
    fprintf(stderr, "ERROR: Could not read string name for segment from json template.\n");
    exit(1);
  }

  // Increase buffer size if needed
  reqS = strlen(*hl7Msg) + strlen(vStr) + 1;
  if (reqS > *hl7MsgS) *hl7Msg = dblBuf(*hl7Msg, hl7MsgS, reqS);

  // Write the name of the segment and field separator to the message
  sprintf(*hl7Msg + strlen(*hl7Msg), "%s%c", vStr, fieldTok);
}


// Add the termination character to the hl7Msg
static void endJSONSeg(char **hl7Msg, int *hl7MsgS, int isWeb) {
  int reqS = 0;

  // Increase buffer size if needed and add segment terminator
  reqS = strlen(*hl7Msg) + 7;
  if (reqS > *hl7MsgS) *hl7Msg = dblBuf(*hl7Msg, hl7MsgS, reqS);
  if (isWeb == 1) {
    sprintf(*hl7Msg + strlen(*hl7Msg), "%s", "<br />");
  } else {
    sprintf(*hl7Msg + strlen(*hl7Msg), "%c", '\r');
  }
}


// Parse a JSON template file
// Warning: This function can't change the pointer to hl7Msg, othrewise sub funtions
// need updating to ***hl7Msg.
void parseJSONTemp(char *jsonMsg, char **hl7Msg, int *hl7MsgS, char **webForm,
                   int *webFormS, int argc, char *argv[], int isWeb) {

  struct json_object *rootObj= NULL, *valObj = NULL, *segsObj = NULL, *segObj = NULL;
  struct json_object *fieldsObj = NULL, *fieldObj = NULL, *subFObj = NULL;

  // TODO - make these variable based on MSH segment for CLI (maybe not web?)
  char fieldTok = '|', sfTok = '^';
  int argcount = 0, segCount = 0, s = 0, fieldCount = 0, f = 0, lastFid = 0;
  int subFCount = 0, sf = 0;

  rootObj = json_tokener_parse(jsonMsg);
  json_object_object_get_ex(rootObj, "argcount", &valObj);
  argcount = json_object_get_int(valObj);

  if (json_object_get_type(valObj) != json_type_int) {
    fprintf(stderr, "ERROR: Could not read integer value for argcount from JSON template.\n");
    exit(1);

  // TODO handle error when running as web
  } else if (isWeb == 0 && argc - argcount != 0) {
    fprintf(stderr, "ERROR: The number of arguments passed on the commmand line (%d) does not match the json template (%d)\n", argc, argcount);
    exit(1);
  }

  // Get this segment section from the json template and parse it
  json_object_object_get_ex(rootObj, "segments", &segsObj);
  segCount = json_object_array_length(segsObj);

  if (segCount > 0) {
    // For each individual segment
    for (s = 0; s < segCount; s++) {
      // Get individual segment as json
      segObj = json_object_array_get_idx(segsObj, s);
      parseJSONSegs(segObj, hl7Msg, hl7MsgS, fieldTok);

      // Loop through all fields
      json_object_object_get_ex(segObj, "fields", &fieldsObj);
      // TODO - error handle all the JSON template functions for temps with missing objs
      fieldCount = json_object_array_length(fieldsObj);
      lastFid = 0;

      for (f = 0; f < fieldCount; f++) {
        // Get field and add it to the HL7 message and web form if appropriate
        fieldObj = json_object_array_get_idx(fieldsObj, f);
        if (isWeb == 1) addVar2WebForm(webForm, webFormS, fieldObj);
        parseJSONField(fieldObj, &lastFid, fieldCount - f, hl7Msg, hl7MsgS, argv,
                       fieldTok, isWeb, webForm);

        json_object_object_get_ex(fieldObj, "subfields", &subFObj);
        if (subFObj) {
          subFCount = json_object_array_length(subFObj);

          for (sf = 0; sf < subFCount; sf++) {
            fieldObj = json_object_array_get_idx(subFObj, sf);
            if (isWeb == 1) addVar2WebForm(webForm, webFormS, fieldObj);
            parseJSONField(fieldObj, &lastFid, subFCount - sf, hl7Msg, hl7MsgS,
                           argv, sfTok, isWeb, webForm);
          }
        }
      }

      // Add terminator to segment
      endJSONSeg(hl7Msg, hl7MsgS, isWeb);
    }
  }

  // Free memory
  json_object_put(rootObj);
}
