/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <openssl/evp.h> 
#include <openssl/rand.h>
#include <json.h>
#include "hhl7utils.h"
#include "hhl7json.h"
#include "hhl7extern.h"

static int PWLOCK = 0;

// Generate a random salt
static void genPwdSalt(char *salt, int saltL) {
  int b64Size = ((saltL - 1) / 3) * 4 + 6;
  unsigned char *tSalt = malloc(b64Size);

  RAND_bytes(tSalt, saltL);
  EVP_EncodeBlock((unsigned char *) salt, tSalt, saltL);
  free(tSalt);
}


// Convert a password to a hash, based on ref:
// https://wiki.openssl.org/index.php/EVP_Message_Digests
static void genPwdHash(const char* pwd, size_t pwdS, char *pwdHash) {
  char algo[7] = "SHA256";
  unsigned char binHash[129] = "";
  unsigned int md_len = -1;
  int isErr = 0;

  const EVP_MD *md = EVP_get_digestbyname(algo);
  if (md != NULL) {
    EVP_MD_CTX *mdctx;
    mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL) isErr = 1;
    if (isErr == 0) EVP_MD_CTX_init(mdctx);
    if (isErr == 0) { if (EVP_DigestInit_ex(mdctx, md, NULL) != 1) isErr = 1; }
    if (isErr == 0) { if (EVP_DigestUpdate(mdctx, pwd, pwdS) != 1) isErr = 1; }
    if (isErr == 0) { if (EVP_DigestFinal_ex(mdctx, binHash, &md_len) != 1) isErr = 1; }
    EVP_MD_CTX_free(mdctx);
  }

  if (isErr == 0 && md_len >= ((int) pwdS / 2)) { 
    EVP_EncodeBlock((unsigned char *) pwdHash, binHash, pwdS);

  } else {
    handleError(LOG_ERR, "ERROR: Could not create password hash", 1, 0, 1);
  }
}


// Create a new passwd file if needed
static int createPWFile(char *fileName) {
  FILE *fp;
  char fileData[21] = "{\n  \"users\": [\n  ]\n}\n";

  if (PWLOCK == 0) {
    fp = openFile(fileName, "w");
    if (fp == NULL) {
      fclose(fp);
      handleError(LOG_ERR, "ERROR: Could not create new password file", 1, 1, 1);
      return 1;

    } else {
      fprintf(fp, "%s", fileData);
      fflush(fp);
      fsync(fileno(fp));
      fclose(fp);
      writeLog(LOG_INFO, "New password file created", 0);
      return 0;
    }
  }
  handleError(LOG_ERR, "ERROR: Could not create new password file", 1, 1, 1);
  return 1;
}
 

// Check if a user exists
static int userExists(struct json_object *userArray, char *uid) {
  struct json_object *userObj = NULL, *uidStr = NULL, *enabledStr = NULL;
  int u = 0, uCount = 0, enabled = 0;

  uCount = json_object_array_length(userArray);

  for (u = 0; u < uCount; u++) {
    userObj = json_object_array_get_idx(userArray, u);
    uidStr = json_object_object_get(userObj, "uid");
    enabledStr = json_object_object_get(userObj, "enabled");
    enabled = json_object_get_boolean(enabledStr);    

    if (strcmp(json_object_get_string(uidStr), uid) == 0) {
      if (enabled == 0) return 2;
      return 0;
    }
  }

  return 1;
}


// Add new user to passwd file
int regNewUser(char *uid, char *passwd) {
  struct json_object *newUserObj = json_object_new_object();
  struct json_object *pwObj = NULL, *userArray = NULL;

  int sleepInt = 250000, sleepC = 0, sleepMax = 8;
  int uExists = 0;
  int maxPassL = 32;
  size_t saltedL = 3 * maxPassL;
  char salt[maxPassL + 1];
  char saltPasswd[saltedL];
  char pwdHash[4* maxPassL];
  saltPasswd[0] = '\0';
  pwdHash[0] = '\0';
  char errStr[68] = "";

  // Define passwd file location
  char pwFile[34];
  if (isDaemon == 1) {
    sprintf(pwFile, "%s", "/usr/local/hhl7/conf/passwd.hhl7");
  } else {
    sprintf(pwFile, "%s", "./conf/passwd.hhl7");
  }

  // Create a new passwd file with a user JSON array if needed 
  if (checkFile(pwFile, 2) != 0) {
    createPWFile(pwFile);
  }

  // If lock is true, sleep until lock is cleared
  while (PWLOCK == 1) {
    usleep(sleepInt);
    sleepC++;
    if (sleepC > sleepMax) handleError(LOG_CRIT, "Could not unlock password file", 1, 1, 0);
  }
  PWLOCK = 1;

  // Re-read the passwd file to ensure we have the latest info
  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    handleError(LOG_ERR, "Failed to read password file", 1, 0, 0);
    PWLOCK = 0;
    json_object_put(pwObj);
    return 1;
  }

  json_object_object_get_ex(pwObj, "users", &userArray);
  uExists = userExists(userArray, uid);

  if (uExists == 1) {
    // Generate salt and password hash
    genPwdSalt(salt, maxPassL);
    sprintf(saltPasswd, "%s%s", salt, passwd);

    genPwdHash(saltPasswd, strlen(saltPasswd), pwdHash);

    json_object_object_add(newUserObj, "uid", json_object_new_string(uid));
    json_object_object_add(newUserObj, "salt", json_object_new_string(salt));
    json_object_object_add(newUserObj, "passwd", json_object_new_string(pwdHash));
    json_object_object_add(newUserObj, "enabled", json_object_new_boolean(0));
    json_object_object_add(newUserObj, "attempts", json_object_new_int(0));
    json_object_object_add(newUserObj, "admin", json_object_new_boolean(0));
    json_object_object_add(newUserObj, "sIP", json_object_new_string("127.0.0.1"));
    json_object_object_add(newUserObj, "sPort", json_object_new_string("11011"));
    json_object_object_add(newUserObj, "lPort", json_object_new_string(""));

    json_object_array_add(userArray, newUserObj);

    if (json_object_to_file_ext(pwFile, pwObj,
        JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY) == -1) {

      sprintf(errStr, "Couldn't write new user (%s) to passwd file", uid);
      handleError(LOG_ERR, errStr, 1, 0, 0);
      PWLOCK = 0;
      json_object_put(pwObj);
      return 1;
    }

  } else {
    sprintf(errStr, "Tried to create already existing user (%s)", uid);
    writeLog(LOG_WARNING, errStr, 0);

  }
  PWLOCK = 0;
  json_object_put(pwObj);
  sprintf(errStr, "New user registered succesfully (%s)", uid);
  writeLog(LOG_INFO, errStr, 0);
  return 0;
}


// Update an entry in the password file
// TODO - allow multiple updates at once
int updatePasswdFile(char *uid, const char *key, const char *val, int iVal) {
  int uExists = 0, u = 0, uCount = 0;
  struct json_object *pwObj = NULL, *userArray = NULL, *userObj = NULL, *uidStr = NULL;
  char errStr[125] = "";
  int sleepInt = 250000, sleepC = 0, sleepMax = 8;

  // Define passwd file location
  char pwFile[34];
  if (isDaemon == 1) {
    sprintf(pwFile, "%s", "/usr/local/hhl7/conf/passwd.hhl7");
  } else {
    sprintf(pwFile, "%s", "./conf/passwd.hhl7");
  }

  // This should not be needed under normal use, however it's here for security purposes
  if (strcmp(key, "uid") == 0 || strcmp(key, "salt") == 0 || strcmp(key, "passwd") == 0 ||
      strcmp(key, "enabled") == 0 || strcmp(key, "admin") == 0) {

    sprintf(errStr, "Unexpected attempt to update user (%s) credentials, exiting", uid);
    handleError(LOG_CRIT, errStr, 1, 1, 0);
    return 1;
  }

  // If lock is true, sleep until lock is cleared
  while (PWLOCK == 1) {
    usleep(sleepInt);
    sleepC++;
    if (sleepC > sleepMax) handleError(LOG_CRIT, "Could not unlock password file", 1, 1, 0);
  }
  PWLOCK = 1;

  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    handleError(LOG_ERR, "Failed to read password file", 1, 0, 0);
    PWLOCK = 0;
    json_object_put(pwObj);
    return 1;
  }

  json_object_object_get_ex(pwObj, "users", &userArray);
  uExists = userExists(userArray, uid);

  if (uExists == 1) {
    PWLOCK = 0;
    json_object_put(pwObj);
    return 1;

  } else {
    uCount = json_object_array_length(userArray);
    for (u = 0; u < uCount; u++) {
      userObj = json_object_array_get_idx(userArray, u);
      uidStr = json_object_object_get(userObj, "uid");

      if (strcmp(json_object_get_string(uidStr), uid) == 0) {
        json_object_object_del(userObj, key);

        if (val != NULL) {
          json_object_object_add(userObj, key, json_object_new_string(val));

        } else if (iVal >= 0) {
          json_object_object_add(userObj, key, json_object_new_int(iVal));

        } else {
          handleError(LOG_ERR, "Attempt to update password file with no values supplied", 1, 0, 0);
          PWLOCK = 0;
          json_object_put(pwObj);
          return 1;
        }
        break;
      }
    }

    if (json_object_to_file_ext(pwFile, pwObj,
        JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY) == -1) {

      sprintf(errStr, "Couldn't write password update (%s - %s) to passwd file", uid, key);
      handleError(LOG_ERR, errStr, 1, 0, 0);
      PWLOCK = 0;
      json_object_put(pwObj);
      return 1;
    }
  }

  PWLOCK = 0;
  json_object_put(pwObj);
  return 0;
}


// Check if a username/password combination is valid, return codes:
//    0 - User exists and password matches
//    1 - User exists and password matches, but account is disabled (or locked)
//    2 - User exists but password is incorrect
//    3 - User doesn't exist
int checkAuth(char *uid, const char *passwd) {
  struct json_object *pwObj = NULL, *userArray = NULL, *userObj = NULL;
  struct json_object *uidStr = NULL, *atInt = NULL, *saltStr = NULL, *pwdStr = NULL;
  int uCount = 0, u = 0, uExists = 0, attempts = 0;
  // TODO - move maxAttempts to config file
  int maxAttempts = 3;
  int maxPassL = 32;
  size_t saltedL = 3 * maxPassL;
  char saltPasswd[saltedL];
  char pwdHash[4* maxPassL];
  saltPasswd[0] = '\0';
  pwdHash[0] = '\0';
  char errStr[63] = "";

  // Define passwd file location
  char pwFile[34];
  if (isDaemon == 1) {
    sprintf(pwFile, "%s", "/usr/local/hhl7/conf/passwd.hhl7");
  } else {
    sprintf(pwFile, "%s", "./conf/passwd.hhl7");
  }

  // Create a new passwd file with a user JSON array if needed 
  if (checkFile(pwFile, 2) != 0) {
    createPWFile(pwFile);
  }

  // Read the password file to a json_object
  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    handleError(LOG_ERR, "Failed to read password file", 1, 0, 0);
    json_object_put(pwObj);
    return 1;
  }

  json_object_object_get_ex(pwObj, "users", &userArray);
  if (userArray == NULL) {
    handleError(LOG_ERR, "Failed to get user object, passwd file corrupt?", 1, 0, 0);
    json_object_put(pwObj);
    return 1;
  }

  uExists = userExists(userArray, uid);
  if (uExists == 1) {
    json_object_put(pwObj);
    return 3;

  } else {
    uCount = json_object_array_length(userArray);

    for (u = 0; u < uCount; u++) {
      userObj = json_object_array_get_idx(userArray, u);
      uidStr = json_object_object_get(userObj, "uid");

      if (strcmp(json_object_get_string(uidStr), uid) == 0) {
        atInt = json_object_object_get(userObj, "attempts");
        attempts = json_object_get_int(atInt);
        // Return if the max failed login attempts breached 
        if (attempts >= maxAttempts && maxAttempts > 0) {
          json_object_put(pwObj);
          return 1;
        }

        saltStr = json_object_object_get(userObj, "salt");
        pwdStr = json_object_object_get(userObj, "passwd");

        if (strlen(json_object_get_string(saltStr)) < maxPassL ||
            strlen(json_object_get_string(pwdStr)) < maxPassL) {

          sprintf(errStr, "Invalid password in password file (%s)", uid);
          handleError(LOG_WARNING, errStr, 1, 0, 0);
          json_object_put(pwObj);
          return 1;

        } else {
          sprintf(saltPasswd, "%s%s", json_object_get_string(saltStr), passwd);
          genPwdHash(saltPasswd, strlen(saltPasswd), pwdHash);

          if (strncmp(json_object_get_string(pwdStr), pwdHash, maxPassL) == 0) {
            // Return 1 if the user exists, the password is correct, but account disabled
            json_object_put(pwObj);
            if (uExists == 2) return 1;
            return 0;

          } else {
            if (maxAttempts > 0) updatePasswdFile(uid, "attempts", NULL, attempts + 1); 
            json_object_put(pwObj);
            return 2;
          }
        }
      }
    }
  }
  json_object_put(pwObj);
  // We shouldn't get here, but return a denial as a safety net
  return 2;
}


// Update a users password, return values:
//    0 - Password updated
//    1 - An error occured, user doesn't exist, couldn't write file etc
int updatePasswd(char *uid, const char *password) {
  struct json_object *pwObj = NULL, *userArray = NULL, *userObj = NULL, *uidStr = NULL;

  int sleepInt = 250000, sleepC = 0, sleepMax = 8;
  int uExists = 0, u = 0, uCount = 0;
  int maxPassL = 32;
  size_t saltedL = 3 * maxPassL;
  char salt[maxPassL + 1];
  char saltPasswd[saltedL];
  char pwdHash[4* maxPassL];
  saltPasswd[0] = '\0';
  pwdHash[0] = '\0';
  char errStr[78] = "";

  // Define passwd file location
  char pwFile[34];
  if (isDaemon == 1) {
    sprintf(pwFile, "%s", "/usr/local/hhl7/conf/passwd.hhl7");
  } else {
    sprintf(pwFile, "%s", "./conf/passwd.hhl7");
  }

  // If lock is true, sleep until lock is cleared
  while (PWLOCK == 1) {
    usleep(sleepInt);
    sleepC++;
    if (sleepC > sleepMax) handleError(LOG_CRIT, "Could not unlock password file", 1, 1, 0);
  }
  PWLOCK = 1;

  // Re-read the passwd file to ensure we have the latest info
  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    handleError(LOG_ERR, "Failed to read password file", 1, 0, 0);
    PWLOCK = 0;
    json_object_put(pwObj);
    return 1;
  }

  json_object_object_get_ex(pwObj, "users", &userArray);
  uExists = userExists(userArray, uid);

  if (uExists == 1) {
    PWLOCK = 0;
    return 1;

  } else {
    // Generate salt and password hash
    genPwdSalt(salt, maxPassL);
    sprintf(saltPasswd, "%s%s", salt, password);
    genPwdHash(saltPasswd, strlen(saltPasswd), pwdHash);

    uCount = json_object_array_length(userArray);
    for (u = 0; u < uCount; u++) {
      userObj = json_object_array_get_idx(userArray, u);
      uidStr = json_object_object_get(userObj, "uid");

      if (strcmp(json_object_get_string(uidStr), uid) == 0) {
        json_object_object_del(userObj, "salt");
        json_object_object_del(userObj, "passwd");
        json_object_object_add(userObj, "salt", json_object_new_string(salt));
        json_object_object_add(userObj, "passwd", json_object_new_string(pwdHash));
        break;
      }
    }

    if (json_object_to_file_ext(pwFile, pwObj,
        JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY) == -1) {

      sprintf(errStr, "Failed to update password file with new password (%s)", uid);
      handleError(LOG_ERR, errStr, 1, 0, 0);
      PWLOCK = 0;
      json_object_put(pwObj);
      return 1;
    }
  }

  PWLOCK = 0;
  json_object_put(pwObj);
  return 0;
}


// Check if someone else has the same listening port configured
int lPortUsed(char *uid, const char *lPort) {
  int u = 0, uCount = 0;

  struct json_object *pwObj = NULL, *lPortStr = NULL;
  struct json_object *userArray = NULL, *userObj = NULL, *uidStr = NULL;

  // Define passwd file location
  char pwFile[34];
  if (isDaemon == 1) {
    sprintf(pwFile, "%s", "/usr/local/hhl7/conf/passwd.hhl7");
  } else {
    sprintf(pwFile, "%s", "./conf/passwd.hhl7");
  }

  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    handleError(LOG_ERR, "Failed to read password file", 1, 0, 0);
    PWLOCK = 0;
    json_object_put(pwObj);
    return 1;
  }

  json_object_object_get_ex(pwObj, "users", &userArray);

  uCount = json_object_array_length(userArray);
  for (u = 0; u < uCount; u++) {
    userObj = json_object_array_get_idx(userArray, u);
    uidStr = json_object_object_get(userObj, "uid");
    lPortStr = json_object_object_get(userObj, "lPort");

    if (strcmp(json_object_get_string(lPortStr), lPort) == 0 &&
        strcmp(json_object_get_string(uidStr), uid) != 0) {

      json_object_put(pwObj);
      return 1;
    }
  }
  json_object_put(pwObj);
  return 0;
}
