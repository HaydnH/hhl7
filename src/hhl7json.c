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
#include <math.h>
#include <json.h>
#include <syslog.h>
#include "hhl7utils.h"
#include "hhl7extern.h"


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
  const char wStr4[]  = "' class='tempFormSelect'";
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
static int parseVals(char ***hl7Msg, int *hl7MsgS, char *vStr, char *nStr, char fieldTok,
                     char *argv[], int lastField, int isWeb, char **webForm,
                     struct json_object *fieldObj, int *incArray,
                     int msgCount, char *post) {

  struct json_object *defObj = NULL, *min = NULL, *max = NULL, *dp = NULL;
  struct json_object *start = NULL, *iMax = NULL, *iType = NULL;
  char *dStr = NULL;
  // TODO - 32? Random length, check it
  char rndStr[32];
  int varLen = strlen(vStr), varNum = 0, reqS = 0;
  char varNumBuf[varLen];
  char dtNow[26] = "";
  char dtVar[26] = "";
  int aMins = 0, incNum = -1;

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

  } else if (strncmp(vStr, "$INC", 4) == 0) {
    incNum = vStr[4] - 48; 
    if (incNum >= 0 && incNum <= 9) {
      // TODO- No error checking here, bad template will seg fault
      json_object_object_get_ex(fieldObj, "start", &start);
      json_object_object_get_ex(fieldObj, "max", &iMax);
      json_object_object_get_ex(fieldObj, "type", &iType);

      const int s = json_object_get_int(start);
      const int m = json_object_get_int(iMax);
      const char *t = json_object_get_string(iType);

      if (incArray[incNum] == -1 || incArray[incNum] == m) {
        incArray[incNum] = s;

      } else if (strncmp(t, "msg", 3) == 0) {
        incArray[incNum] = msgCount + s - 1;

      } else if (strncmp(t, "use", 3) == 0){
        incArray[incNum]++;

      }

      int count = (incArray[incNum] == 0) ? 1 : log10(incArray[incNum]) + 1;
      char numStr[count + 1];
      sprintf(numStr, "%d", incArray[incNum]);
      reqS = strlen(**hl7Msg) + count;
      if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
      strcat(**hl7Msg, numStr);
    }

  } else if (strncmp(vStr, "$RND", 4) == 0) {
    // TODO- No error checking here, bad template will seg fault
    json_object_object_get_ex(fieldObj, "min", &min);
    json_object_object_get_ex(fieldObj, "max", &max);
    json_object_object_get_ex(fieldObj, "dp", &dp);

    const char *l = json_object_get_string(min);
    const char *u = json_object_get_string(max);
    const char *d = json_object_get_string(dp);

    // TODO why am I using atoi? Can't we get float above?
    int negOne = -1;
    getRand(atoi(l), atoi(u), atoi(d), rndStr, &negOne);
    reqS = strlen(**hl7Msg) + strlen(rndStr); 
    if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
    strcat(**hl7Msg, rndStr);

  // Replace json value with a command line argument
  } else if (nStr && strncmp(vStr, "$VAR", 4) == 0) {
    if (varLen == 4) {
      handleError(LOG_ERR, "No numeric value found after $VAR in JSON template", 1, 0, 1);
      return(1);

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

  if (post != NULL) {
    reqS = strlen(**hl7Msg) + strlen(post) + 1;
    if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
    strcat(**hl7Msg, post);
  }

  if (lastField > 1) {
    // No need to add mem with dblBuf, the above +N values handle it
    sprintf(**hl7Msg + strlen(**hl7Msg), "%c", fieldTok);
  }
  return(0);
}


// Parse the JSON field
static int parseJSONField(struct json_object *fieldObj, int *lastFid, int lastField, 
                          char **hl7Msg, int *hl7MsgS, char *argv[], char fieldTok,
                          int isWeb, char **webForm, int incArray[], int msgCount) {

  struct json_object *valObj = NULL, *idObj = NULL;
  char *vStr = NULL, *nStr = NULL, *pre = NULL, *post = NULL;
  char nameStr[65] = "";
  int f = 0, fid = 0, reqS = 0, preL = 0, retVal = 0;

  // Get the ID of the current field value
  json_object_object_get_ex(fieldObj, "id", &idObj);
  fid = (int) json_object_get_int(idObj);

  // Get pre and post values for the field if they exist
  json_object_object_get_ex(fieldObj, "pre", &valObj);
  if (json_object_get_type(valObj) == json_type_string) {
    pre = (char *) json_object_get_string(valObj);
    preL = strlen(pre);
  }
  json_object_object_get_ex(fieldObj, "post", &valObj);
  if (json_object_get_type(valObj) == json_type_string) {
    post = (char *) json_object_get_string(valObj);
  }

  // Add memory to hl7Msg if needed
  reqS = strlen(*hl7Msg) + (fid - *lastFid) + preL;
  if (reqS > *hl7MsgS) *hl7Msg = dblBuf(*hl7Msg, hl7MsgS, reqS);

  while (f < fid - *lastFid - 1) {
    sprintf(*hl7Msg + strlen(*hl7Msg), "%c", fieldTok);
    f++;
  }

  // Add the prefix to to the field
  if (preL > 0) sprintf(*hl7Msg + strlen(*hl7Msg), "%s", pre);

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
          handleError(LOG_ERR, "JSON template name exceeds 64 character limit", 1, 0, 1);
          return(1);

        } else {
          strcpy(nameStr, nStr);
        }
      }
    }

    // Parse JSON values
    retVal = parseVals(&hl7Msg, hl7MsgS, vStr, nameStr, fieldTok, argv, lastField, 
                       isWeb, webForm, fieldObj, incArray, msgCount, post);

  }

  *lastFid = fid;
  return(retVal);
}


// Parse the JSON segment
static int parseJSONSegs(struct json_object *segObj, char **hl7Msg, int *hl7MsgS,
                         char fieldTok) {

  struct json_object *valObj = NULL;
  char *vStr = NULL;
  int reqS = 0;

  // Get the name of this segment
  json_object_object_get_ex(segObj, "name", &valObj);
  vStr = (char *) json_object_get_string(valObj);

  if (json_object_get_type(valObj) != json_type_string) {
    handleError(LOG_ERR, "Could not read string name for segment from json template", 1, 0, 1);
    return(1);
  }

  // Increase buffer size if needed
  reqS = strlen(*hl7Msg) + strlen(vStr) + 1;
  if (reqS > *hl7MsgS) *hl7Msg = dblBuf(*hl7Msg, hl7MsgS, reqS);

  // Write the name of the segment and field separator to the message
  sprintf(*hl7Msg + strlen(*hl7Msg), "%s%c", vStr, fieldTok);

  return(0);
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
int parseJSONTemp(char *jsonMsg, char **hl7Msg, int *hl7MsgS, char **webForm,
                  int *webFormS, int argc, char *argv[], int isWeb) {

  struct json_object *rootObj= NULL, *valObj = NULL, *segsObj = NULL, *segObj = NULL;
  struct json_object *fieldsObj = NULL, *fieldObj = NULL, *subFObj = NULL;

  // TODO - make these variable based on MSH segment for CLI (maybe not web?)
  char fieldTok = '|', sfTok = '^', *vStr = NULL;
  int argcount = 0, segCount = 0, s = 0, fieldCount = 0, f = 0, lastFid = 0;
  int msgCount = 0, subFCount = 0, sf = 0, retVal = 0;
  int incArray[10] = {-1};

  rootObj = json_tokener_parse(jsonMsg);
  json_object_object_get_ex(rootObj, "argcount", &valObj);
  argcount = json_object_get_int(valObj);

  if (json_object_get_type(valObj) != json_type_int) {
    handleError(LOG_ERR, "Could not read integer value for argcount from JSON template", 1, 0, 1);
    json_object_put(rootObj);
    return(1);

  } else if (isWeb == 0 && argc - argcount != 0) {
    sprintf(infoStr, "The number of arguments passed on the commmand line (%d) does not match the json template (%d)", argc, argcount);
    handleError(LOG_ERR, infoStr, 1, 0, 1);
    json_object_put(rootObj);
    return(1);

  }

  // Get this segment section from the json template and parse it
  json_object_object_get_ex(rootObj, "segments", &segsObj);
  segCount = json_object_array_length(segsObj);

  if (segCount > 0) {
    // For each individual segment
    for (s = 0; s < segCount; s++) {
      // Get individual segment as json
      segObj = json_object_array_get_idx(segsObj, s);

      // Get the name of this segment
      json_object_object_get_ex(segObj, "name", &valObj);
      vStr = (char *) json_object_get_string(valObj);

      if (json_object_get_type(valObj) != json_type_string) {
        handleError(LOG_ERR, "Could not read string name for segment from json template", 1, 0, 1);
        json_object_put(rootObj);
        return(1);

      }

      // If this is a 2nd or more MSH segment, add a blank line
      if (strcmp(vStr, "MSH") == 0) {
        if (msgCount > 0) {
          endJSONSeg(hl7Msg, hl7MsgS, isWeb);
        }
        msgCount++;
      }

      retVal = parseJSONSegs(segObj, hl7Msg, hl7MsgS, fieldTok);
      if (retVal > 0) {
        json_object_put(rootObj);
        return(1);
      }


      // Loop through all fields
      json_object_object_get_ex(segObj, "fields", &fieldsObj);
      // TODO - error handle all the JSON template functions for temps with missing objs
      fieldCount = json_object_array_length(fieldsObj);
      lastFid = 0;

      for (f = 0; f < fieldCount; f++) {
        // Get field and add it to the HL7 message and web form if appropriate
        fieldObj = json_object_array_get_idx(fieldsObj, f);
        if (isWeb == 1) addVar2WebForm(webForm, webFormS, fieldObj);

        retVal = parseJSONField(fieldObj, &lastFid, fieldCount - f, hl7Msg, hl7MsgS, argv,
                                fieldTok, isWeb, webForm, incArray, msgCount);
        if (retVal > 0) {
          json_object_put(rootObj);
          return(1);
        }

        json_object_object_get_ex(fieldObj, "subfields", &subFObj);
        if (subFObj) {
          subFCount = json_object_array_length(subFObj);

          for (sf = 0; sf < subFCount; sf++) {
            fieldObj = json_object_array_get_idx(subFObj, sf);
            if (isWeb == 1) addVar2WebForm(webForm, webFormS, fieldObj);
            retVal = parseJSONField(fieldObj, &lastFid, subFCount - sf, hl7Msg, hl7MsgS,
                                    argv, sfTok, isWeb, webForm, incArray, msgCount);
            if (retVal > 0) {
              json_object_put(rootObj);
              return(1);
            }
          }
        }
      }

      // Add terminator to segment
      endJSONSeg(hl7Msg, hl7MsgS, isWeb);
    }
  }

  // Free memory
  json_object_put(rootObj);
  return(retVal);
}
