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
#include <time.h>
#include <syslog.h>
#include <openssl/evp.h>
#include "hhl7utils.h"
#include "hhl7extern.h"


// Read a json file from file pointer
int readJSONFile(FILE *fp, long int fileSize, char *jsonMsg) {
  if (!fread(jsonMsg, fileSize, 1, fp)) {
    return(1);
  }
  jsonMsg[fileSize] = '\0';
  return(0);
}


// Get the guide info (arguments or description) for a template
// gType: 0 = cmdhelp (description), 1 = description
int hhgttg(char *tName, int gType) {
  struct json_object *tmpObj = NULL, *valObj = NULL;
  char fileName[256] = "", funcStr[15] = "", errStr[289] = "";

  findTemplate(fileName, tName, 0);
  sprintf(errStr, "Using template file: %s", fileName);
  writeLog(LOG_INFO, errStr, 1);

  tmpObj = json_object_from_file(fileName);
  if (tmpObj == NULL) {
    handleError(LOG_ERR, "Failed to read template file", 1, 0, 1);
    json_object_put(tmpObj);
    return(1);
  }

  if (gType == 1) {
    json_object_object_get_ex(tmpObj, "description", &valObj);
    sprintf(funcStr, "%s", "Description:\n");
  } else {
    json_object_object_get_ex(tmpObj, "cmdhelp", &valObj);
    sprintf(funcStr, "%s", "Usage: ");
  }

  if (valObj == NULL) {
    handleError(LOG_INFO, "Failed to read guide info from template", 1, 0, 1);
    json_object_put(tmpObj);
    return(1);
  } 

  printf("%s%s\n\n", funcStr, json_object_get_string(valObj));
  json_object_put(tmpObj);
  return(0);
}


// Take a json object and check validity of repeat/start/stop/inc values
static int timeRange(struct json_object *segObj, time_t *start, time_t *stop, int *inc) {
  struct json_object *repObj, *startObj, *endObj, *incObj;
  char *startStr = NULL, *endStr = NULL;
  int tmpInc = 2, aMins = 0;
  time_t now = time(NULL), tmpStart = now, tmpEnd = now + 1;

  repObj = json_object_object_get(segObj, "repeat");
  if (repObj != NULL) {
    startObj = json_object_object_get(segObj, "start");
    endObj = json_object_object_get(segObj, "end");
    incObj = json_object_object_get(segObj, "inc");

    if (startObj == NULL || endObj == NULL || incObj == NULL) {
      handleError(LOG_WARNING, "Template with MSH repeat missing start, end or inc options", 1, 0, 1);
      return(1);
    }

    tmpInc = json_object_get_int(incObj) * 60;
    startStr = (char *) json_object_get_string(startObj);
    endStr = (char *) json_object_get_string(endObj);

    if (strcmp(json_object_get_string(repObj), "time") == 0) {
      now = time(NULL);
      if (startStr[4] == '+' || startStr[4] == '-') aMins = atoi(startStr + 4);
      tmpStart = now + (aMins * 60);
      aMins = 0;
      if (endStr[4] == '+' || endStr[4] == '-') aMins = atoi(endStr + 4);
      tmpEnd = now + (aMins * 60);

    }
  }

  if (tmpStart > tmpEnd || tmpInc <= 0) {
    handleError(LOG_WARNING, "Template with MSH repeat has invalid start, end & inc options", 1, 0, 1);
    return(1);

  }

  *start = tmpStart;
  *stop = tmpEnd;
  *inc = tmpInc; 

  return(0);
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
  return(retVal);
}


// Add a JSON template object to the template web form
static void addVar2WebForm(char **webForm, int *webFormS, struct json_object *fieldObj) {
  struct json_object *nameObj = NULL, *optsObj = NULL, *optObj = NULL, *oObj = NULL;
  struct json_object *defObj = NULL, *txtBoxObj = NULL, *newLineObj = NULL, *escObj = NULL;
  char *oStr = NULL, *nStr = NULL, *dStr = NULL, *nlStr = NULL, *escStr = NULL;
  long unsigned int o = 0;
  int reqS = 0, textBox = -1;

  // HTML strings to keep the code below tidy
  const char wStr1[]  = "<div class='tempFormField'><div class='tempFormKey'>";
  const char wStr2[]  = ":</div><div class='tempFormValue'>";
  const char wStr3[]  = "<select id='HHL7_FL_";
  const char wStr4[]  = "' class='tempFormSelect'";
  const char wStr5[]  = " onInput='updateHL7(this.id, this.value, true);' />";
  const char wStr6[]  = "<option value='";
  const char wStr7[]  = "' selected='selected";
  const char wStr8[]  = "'>";
  const char wStr9[]  = "</option>";
  const char wStr10[] = "</select>";
  const char wStr11[] = "<input id='HHL7_FL_";
  const char wStr12[] = "' class='tempFormInput'";
  const char wStr13[] = "</div></div>";
  const char wStr14[] = "' class='tempFormText' data-br='";
  const char wStr15[] = "' data-esc='";
  const char wStr16[] = "' onFocus='refreshBox(this);'";
  const char wStr17[] = "<a href='' class='tempTextButton' ";
  const char wStr18[] = "onclick=\\\"showTextBox('HHL7_FL_";
  const char wStr19[] = "'); return false;\\\">&#7790;</a>";

  // Try to obtain the options and default values from the JSON template
  json_object_object_get_ex(fieldObj, "name", &nameObj);
  json_object_object_get_ex(fieldObj, "options", &optsObj);
  json_object_object_get_ex(fieldObj, "default", &defObj);
  json_object_object_get_ex(fieldObj, "textbox", &txtBoxObj);
  json_object_object_get_ex(fieldObj, "newline", &newLineObj);
  json_object_object_get_ex(fieldObj, "escape", &escObj);
  nStr = (char *) json_object_get_string(nameObj);
  dStr = (char *) json_object_get_string(defObj);
  nlStr = (char *) json_object_get_string(newLineObj);
  escStr = (char *) json_object_get_string(escObj);
  textBox = json_object_get_boolean(txtBoxObj);

  // Stop processing if this is not a named object or is already on the form
  if (!nStr) return;
  char valSpan[strlen(nStr) + 15];
  sprintf(valSpan, "%s%s%s", "id='HHL7_FL_", nStr, "'");
  if (strstr(*webForm, valSpan) != NULL) return;

  // TODO - change the 350 below to multiple if newline/escape obj, reqS = reqS + N
  // Increase memory, if required, for all strings outside the below for loop
  reqS = strlen(*webForm) + (3 * strlen(nStr)) + 350;
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
    if (txtBoxObj && newLineObj) {
      if (textBox == 1 && strlen(nlStr) > 0) {
        if (escObj) {
          sprintf(*webForm + strlen(*webForm), "%s%s%s%s%s%s%s%s%s%s%s%s",
                                              wStr11, nStr, wStr14, nlStr, wStr15, escStr,
                                              wStr16, wStr5, wStr17, wStr18, nStr, wStr19);
        } else {
          sprintf(*webForm + strlen(*webForm), "%s%s%s%s%s%s%s%s%s%s%s",
                                              wStr11, nStr, wStr14, nlStr, wStr15, wStr16,
                                              wStr5, wStr17, wStr18, nStr, wStr19);
        }
      } else {
        sprintf(*webForm + strlen(*webForm), "%s%s%s%s", wStr11, nStr, wStr12, wStr5);
      }
    } else {
      sprintf(*webForm + strlen(*webForm), "%s%s%s%s", wStr11, nStr, wStr12, wStr5);
    }
  }
  sprintf(*webForm + strlen(*webForm), "%s", wStr13);
}


// Function turn json variables in to a value
static int parseVals(char ***hl7Msg, int *hl7MsgS, char *vStr, char *nStr, char fieldTok,
                     int argc, char *argv[], int lastField, int isWeb,
                     struct json_object *fieldObj, int *incArr, float *strArr,
                     int msgCount, char *post, time_t rangeVal, int msgRand) {

  struct json_object *defObj = NULL, *min = NULL, *max = NULL, *dp = NULL, *str = NULL;
  struct json_object *start = NULL, *iMax = NULL, *iType = NULL, *rHide = NULL;
  struct json_object *dataFile = NULL, *dataType = NULL;
  char *dStr = NULL;
  char rndStr[32];
  int varLen = strlen(vStr), varNum = 0, reqS = 0, rHideInt = 0;
  char varNumBuf[varLen + 1];
  char dtNow[26] = "";
  char dtVar[26] = "";
  int aMins = 0, incNum = -1;
  float resF = 0.0;

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
        handleError(LOG_ERR, "Invalid time adjustment for JSON template", 1, 0, 1);
        return(1);

      }
    }

  } else if (strncmp(vStr, "$TRV", 4) == 0) {
    struct tm *tm = localtime(&rangeVal);
    strftime(dtVar, 26, "%Y%m%d%H%M%S", tm);
    reqS = strlen(**hl7Msg) + 28;
    if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
    strcat(**hl7Msg, dtVar);

  } else if (strncmp(vStr, "$INC", 4) == 0) {
    incNum = vStr[4] - 48; 
    if (incNum >= 0 && incNum <= 9) {
      json_object_object_get_ex(fieldObj, "start", &start);
      json_object_object_get_ex(fieldObj, "max", &iMax);
      json_object_object_get_ex(fieldObj, "type", &iType);

      if (start == NULL || iMax == NULL || iType == NULL) {
        handleError(LOG_ERR, "Template $INC missing start, max or type", 1, 0, 1);
        return(1);
      }

      const int s = json_object_get_int(start);
      const int m = json_object_get_int(iMax);
      const char *t = json_object_get_string(iType);

      if (incArr[incNum] == -1 || incArr[incNum] == m) {
        incArr[incNum] = s;

      } else if (strncmp(t, "msg", 3) == 0) {
        incArr[incNum] = msgCount + s - 1;

      } else if (strncmp(t, "use", 3) == 0){
        incArr[incNum]++;

      }

      int count = (incArr[incNum] == 0) ? 1 : log10(incArr[incNum]) + 1;
      char numStr[count + 1];
      sprintf(numStr, "%d", incArr[incNum]);
      reqS = strlen(**hl7Msg) + count;
      if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
      strcat(**hl7Msg, numStr);
    }

  } else if (strncmp(vStr, "$RND", 4) == 0) {
    json_object_object_get_ex(fieldObj, "min", &min);
    json_object_object_get_ex(fieldObj, "max", &max);
    json_object_object_get_ex(fieldObj, "dp", &dp);

    if (min == NULL || max == NULL || dp == NULL) {
      handleError(LOG_ERR, "Template $RND missing min, max or dp", 1, 0, 1);
      return(1);
    }

    const char *l = json_object_get_string(min);
    const char *u = json_object_get_string(max);
    const char *d = json_object_get_string(dp);

    int negOne = -1;
    getRand(atoi(l), atoi(u), atoi(d), rndStr, &negOne, &resF);

    rHide = json_object_object_get(fieldObj, "hidden");
    rHideInt = json_object_get_boolean(rHide);    

    // Store the random number if requested
    json_object_object_get_ex(fieldObj, "store", &str);
    if (str != NULL) {
      const int st = atoi(json_object_get_string(str)) - 1;
      if (st < 0 || st > 9) {
        handleError(LOG_ERR, "Template store number < 0 or > 9", 1, 0, 1);
        return(1);

      } else {
        strArr[st] = resF;

      }
    }

    if (rHideInt == 1) return(-1);

    reqS = strlen(**hl7Msg) + strlen(rndStr);
    if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
    strcat(**hl7Msg, rndStr);


  // Read a data file and file the value with a line of it
  } else if (strncmp(vStr, "$DAT", 4) == 0) {
    json_object_object_get_ex(fieldObj, "datafile", &dataFile);
    json_object_object_get_ex(fieldObj, "type", &dataType);

    if (dataFile == NULL || dataType == NULL) {
      handleError(LOG_WARNING, "$DAT in JSON template missing datafile or type argument", 1, 0, 1);
      return(1);

    } else {
      FILE* fp = NULL;
      char dataFName[strlen(json_object_get_string(dataFile)) + 28];
      sprintf(dataFName, "/usr/local/hhl7/datafiles/%s", json_object_get_string(dataFile));
      long int dataSize = getFileSize(dataFName);
      char fileData[dataSize + 1];
      long unsigned int dataLines = 0;
      int getLine = 0, lineStart = 0, lineLen = 0;

      sprintf(dataFName, "/usr/local/hhl7/datafiles/%s", json_object_get_string(dataFile));
      fp = openFile(dataFName, "r"); 

      if (fp == NULL) { 
        handleError(LOG_WARNING, "Could not open datafile from JSON template", 1, 0, 1);
        return(1);

      } else {
        file2buf(fileData, fp, dataSize);
        dataLines = numLines(fileData);

        if (strcmp(json_object_get_string(dataType), "rand") == 0) {
          getRand(1, dataLines, 0, NULL, &getLine, NULL);
        } else if (strcmp(json_object_get_string(dataType), "msgrand") == 0) {
          getLine = (dataLines * msgRand) / 100;
        } else {
          getLine = msgCount - 1;
        }
        findLine(fileData, dataSize, getLine, &lineStart, &lineLen);

        reqS = strlen(**hl7Msg) + lineLen + 1;
        if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);

        char dLine[lineLen + 1];
        strncpy(dLine, fileData + lineStart, lineLen);
        dLine[lineLen] = '\0';
        strcat(**hl7Msg, dLine);

      }
      fclose(fp);
    }


  // Read a data file and file the value with a line of it
  } else if (strncmp(vStr, "$B64", 4) == 0) {
    json_object_object_get_ex(fieldObj, "datafile", &dataFile);

    if (dataFile == NULL) {
      handleError(LOG_WARNING, "$B64 in JSON template missing datafile argument", 1, 0, 1);
      return(1);

    } else {
      FILE* fp = NULL;
      char dataFName[strlen(json_object_get_string(dataFile)) + 28];
      sprintf(dataFName, "/usr/local/hhl7/datafiles/%s", json_object_get_string(dataFile));
      long int dataSize = getFileSize(dataFName);
      unsigned char fileData[dataSize];

      sprintf(dataFName, "/usr/local/hhl7/datafiles/%s", json_object_get_string(dataFile));
      fp = openFile(dataFName, "r"); 

      if (fp == NULL) { 
        handleError(LOG_WARNING, "Could not open datafile from JSON template", 1, 0, 1);
        return(1);

      } else {
        if (fread(fileData, dataSize, 1, fp) <= 0) {
          handleError(LOG_WARNING, "Failed to read datafile contents", 1, 0, 1);
          fclose(fp);
          return(1);
        }

        int b64Size = 4 * ((dataSize / 3) + 1) + 1;
        unsigned char b64Data[b64Size];

        if (EVP_EncodeBlock(b64Data, fileData, dataSize) <= 0) {
          handleError(LOG_WARNING, "Failed to base64 encode datafile contents", 1, 0, 1);
          fclose(fp);
          return(1);
        }

        reqS = strlen(**hl7Msg) + b64Size + 1;
        if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
        strcat(**hl7Msg, (char *) b64Data);

      }
      fclose(fp);
    }


  // Retrieve a value from the random number store
  } else if (strncmp(vStr, "$STR", 4) == 0) {
    if (varLen != 5) {
      handleError(LOG_ERR, "No numeric value found after $STR in JSON template", 1, 0, 1);
      return(1);
    }

    strncpy(varNumBuf, vStr + 4, varLen - 4);
    varNumBuf[varLen - 4] = '\0';
    varNum = atoi(varNumBuf) - 1;

    if (varNum < 0 || varNum > 9) {
      handleError(LOG_ERR, "Template store number < 0 or > 10", 1, 0, 1);
      return(1);
    }

    json_object_object_get_ex(fieldObj, "ranges", &defObj);
    if (defObj == NULL) {
      handleError(LOG_ERR, "No ranges array found after $STR in JSON template", 1, 0, 1);
      return(1);
    }

    json_object_object_foreach(defObj, rKey, rVal) {
      if (strArr[varNum] <= json_object_get_double(rVal)) {
        reqS = strlen(**hl7Msg) + strlen(rKey) + 1;
        if (reqS > *hl7MsgS) **hl7Msg = dblBuf(**hl7Msg, hl7MsgS, reqS);
        strcat(**hl7Msg, rKey);
        break;
      }
    }

  // Replace json value with a command line argument
  } else if (nStr && strncmp(vStr, "$VAR", 4) == 0) {
    if (varLen == 4) {
      handleError(LOG_ERR, "No numeric value found after $VAR in JSON template", 1, 0, 1);
      return(1);
    }

    strncpy(varNumBuf, vStr + 4, varLen - 4);
    varNumBuf[varLen - 4] = '\0';
    varNum = atoi(varNumBuf) - 1;

    // Check the VAR number is within a valid range, 0 to argcount
    if (isWeb == 0) {
      if (varNum < 0 || varNum >= argc) {
        handleError(LOG_ERR, "Template contains $VAR with number out of range", 1, 0, 1);
        return(1);
      }
    }

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
                          char **hl7Msg, int *hl7MsgS, int argc, char *argv[],
                          char fieldTok, int isWeb, int incArr[], float strArr[],
                          int msgCount, time_t rangeVal, int msgRand, char *segment) {

  struct json_object *valObj = NULL, *idObj = NULL;
  char *vStr = NULL, *nStr = NULL, *pre = NULL, *post = NULL;
  char nameStr[65] = "";
  int f = 0, fid = 0, reqS = 0, preL = 0, retVal = 0;

  // Get the ID of the current field value
  json_object_object_get_ex(fieldObj, "id", &idObj);
  fid = (int) json_object_get_int(idObj);
  if (strcmp(segment, "MSH") == 0) {
    if (fid == 1) writeLog(LOG_WARNING, "MSH segment contains a field id=1, MSH segment may be corrupt", 1);
    fid = fid - 1;
  }

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

  // Parse the value for this field
  json_object_object_get_ex(fieldObj, "value", &valObj);
  if (valObj != NULL) {
    if (json_object_get_type(valObj) == json_type_string) {
      vStr = (char *) json_object_get_string(valObj);

      // Get the name of the field for use on the web form
      if (isWeb == 1) {
        json_object_object_get_ex(fieldObj, "name", &valObj);
        if (valObj != NULL) {
          nStr = (char *) json_object_get_string(valObj);

          if (json_object_get_type(valObj) == json_type_string) {
            if (strlen(nStr) > 64) {
              handleError(LOG_ERR, "JSON template name exceeds 64 character limit", 1, 0, 1);
              return(1);

            } else {
              strcpy(nameStr, nStr);
            }
          }
        }
      }
    }

    // Parse JSON values
    retVal = parseVals(&hl7Msg, hl7MsgS, vStr, nameStr, fieldTok, argc, argv, lastField,
                       isWeb, fieldObj, incArr, strArr, msgCount, post, rangeVal, msgRand);

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
static void endJSONSeg(char **hl7Msg, int *hl7MsgS, int isWeb, int eType) {
  int reqS = 0;

  // Increase buffer size if needed and add segment terminator
  reqS = strlen(*hl7Msg) + 30;
  if (reqS > *hl7MsgS) *hl7Msg = dblBuf(*hl7Msg, hl7MsgS, reqS);

  if (isWeb == 1) {
    if (eType == 2) {
      sprintf(*hl7Msg + strlen(*hl7Msg), "%s", "</div><div><br /></div>");
    } else if (eType == 1) {
      sprintf(*hl7Msg + strlen(*hl7Msg), "%s", "</div><div><br /></div><div>");
    } else if (eType == 0) {
      sprintf(*hl7Msg + strlen(*hl7Msg), "%s", "<br />");
    }

  } else {
    if (eType == 3) {
      sprintf(*hl7Msg + strlen(*hl7Msg), "%c", '|');
    } else {
      sprintf(*hl7Msg + strlen(*hl7Msg), "%c", '\r');
    }
  }
}


// Parse a JSON template file
// Warning: This function can't change the pointer to hl7Msg, otherwise sub funtions
// need updating to ***hl7Msg.
int parseJSONTemp(char *jsonMsg, char **hl7Msg, int *hl7MsgS, char **webForm,
                  int *webFormS, int argc, char *argv[], int isWeb) {

  struct json_object *rootObj= NULL, *msgsObj = NULL, *msgObj = NULL;
  struct json_object *segsObj = NULL, *segObj = NULL, *valObj = NULL;
  struct json_object *fieldsObj = NULL, *fieldObj = NULL, *subFObj = NULL;

  // TODO - make these variable based on MSH segment for CLI (maybe not web?)
  char fieldTok = '|', sfTok = '^', *vStr = NULL;
  int argcount = 0, msgsCount = 0, segCount = 0, s = 0, fieldCount = 0, f = 0, lastFid = 0;
  int lastFidPreSF = 0, mCount = 0, msgCount = 0, subFCount = 0, sf = 0, retVal = 0;
  // TODO - increment & store has a max of 10? Consider malloc/realloc...
  int incArr[10] = {-1};
  float strArr[10] = {-1.0};
  char errStr[112] = "";
  int inc = 2, rCount = 0, msgRand = 0;

  rootObj = json_tokener_parse(jsonMsg);
  json_object_object_get_ex(rootObj, "argcount", &valObj);
  argcount = json_object_get_int(valObj);

  if (json_object_get_type(valObj) != json_type_int) {
    handleError(LOG_ERR, "Could not read integer value for argcount from JSON template", 1, 0, 1);
    json_object_put(rootObj);
    return(1);

  } else if (isWeb == 0 && argc - argcount != 0) {
    sprintf(errStr, "The number of arguments passed on the commmand line (%d) does not match the json template (%d)", argc, argcount);
    handleError(LOG_ERR, errStr, 1, 0, 1);
    json_object_put(rootObj);
    return(1);

  }

  // Get 1st segment section from the json template, could be in messages array for >1 msg
  json_object_object_get_ex(rootObj, "messages", &msgsObj);
  if (msgsObj == NULL) {
    json_object_object_get_ex(rootObj, "segments", &segsObj);
    msgsCount = 1;

  } else {
    msgObj = json_object_array_get_idx(msgsObj, 0);
    json_object_object_get_ex(msgObj, "segments", &segsObj);
  }

  if (segsObj == NULL) {
    handleError(LOG_ERR, "Could not find any segment sections in the template", 1, 0, 1);
    json_object_put(rootObj);
    return(1);

  } else if (msgsCount == 0) {
    msgsCount = json_object_array_length(msgsObj);

  }


  // Loop through each message in the messages section
  for (mCount = 0; mCount < msgsCount; mCount++) {
    time_t rStart = time(NULL), rEnd = rStart + 1, step = rStart;

    if (mCount > 0) {
      msgObj = json_object_array_get_idx(msgsObj, mCount);
      json_object_object_get_ex(msgObj, "segments", &segsObj);
    }

    segCount = json_object_array_length(segsObj);

    // Iterate over the time scale in steps, or once due to init values if no MSH repeat
    while (step <= rEnd) {
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

          // Check if we repeat msgs using $TRV
          if (strcmp(vStr, "MSH") == 0) {
            if (timeRange(segObj, &rStart, &rEnd, &inc) != 0) {
              handleError(LOG_ERR, "Failed to parse MSH repeat in JSON template", 1, 0, 1);
              json_object_put(rootObj);
              return(1);

            } else {
              step = rStart + (rCount * inc);

            }

            msgCount++;

            // Create a random number for this message
            getRand(0, 100, 0, NULL, &msgRand, NULL);
          }

          retVal = parseJSONSegs(segObj, hl7Msg, hl7MsgS, fieldTok);
          if (retVal > 0) {
            json_object_put(rootObj);
            return(1);
          }


          // Loop through all fields
          json_object_object_get_ex(segObj, "fields", &fieldsObj);
          if (fieldsObj == NULL) {
            handleError(LOG_ERR, "Segment in JSON template missing fields array", 1, 0, 1);
            return(1);
          }

          fieldCount = json_object_array_length(fieldsObj);
          lastFid = 0;

          for (f = 0; f < fieldCount; f++) {
            // Get field and add it to the HL7 message and web form if appropriate
            fieldObj = json_object_array_get_idx(fieldsObj, f);
            if (fieldObj == NULL) {
              handleError(LOG_ERR, "Field array in JSON template missing field object", 1, 0, 1);
              return(1);
            }

            if (isWeb == 1) addVar2WebForm(webForm, webFormS, fieldObj);

            retVal = parseJSONField(fieldObj, &lastFid, fieldCount - f, hl7Msg, hl7MsgS,
                                    argc, argv, fieldTok, isWeb, incArr, strArr, msgCount,
                                    step, msgRand, vStr);

            if (retVal > 0) {
              json_object_put(rootObj);
              return(1);
            }

            json_object_object_get_ex(fieldObj, "subfields", &subFObj);
            if (subFObj) {
              lastFidPreSF = lastFid;
              lastFid = 0;
              subFCount = json_object_array_length(subFObj);

              for (sf = 0; sf < subFCount; sf++) {
                fieldObj = json_object_array_get_idx(subFObj, sf);
                if (isWeb == 1) addVar2WebForm(webForm, webFormS, fieldObj);
                retVal = parseJSONField(fieldObj, &lastFid, subFCount - sf, hl7Msg,
                                        hl7MsgS, argc, argv, sfTok, isWeb, incArr, strArr,
                                        msgCount, step, msgRand, vStr);

                if (retVal > 0) {
                  json_object_put(rootObj);
                  return(1);
                }
              }
              lastFid = lastFidPreSF;
              endJSONSeg(hl7Msg, hl7MsgS, isWeb, 3); // EOSF
            }
          }

          // Add terminator to segment
          if (retVal == 0) {
            segObj = NULL;
            vStr = NULL;

            if (s < segCount - 1) {
              // Get the name of the next segment if it exists
              segObj = NULL;
              vStr = NULL;
              segObj = json_object_array_get_idx(segsObj, s + 1);
              if (segObj) {
                json_object_object_get_ex(segObj, "name", &valObj);
                vStr = (char *) json_object_get_string(valObj);
              }
            }    

            if (s == segCount - 1) { 
              endJSONSeg(hl7Msg, hl7MsgS, isWeb, 2); // EOF

            } else if (strcmp(vStr, "MSH") == 0) {
              endJSONSeg(hl7Msg, hl7MsgS, isWeb, 1); // EOM

            } else {
              endJSONSeg(hl7Msg, hl7MsgS, isWeb, 0); // EOS

            }
          }
        }
        rCount++;
        step = rStart + (rCount * inc);

      }
    }
  }

  // Free memory
  json_object_put(rootObj);
  return(retVal);
}
