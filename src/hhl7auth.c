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
#include <openssl/evp.h> 
#include <openssl/rand.h>
#include <json.h>
#include "hhl7utils.h"
#include "hhl7json.h"

static int PWLOCK = 0;

// Generate a random salt
static void genPwdSalt(char *salt, int saltL) {
  int b64Size = ((saltL - 1) / 3) * 4 + 6;
  unsigned char *tSalt = malloc(b64Size);

  RAND_bytes(tSalt, saltL);
  EVP_EncodeBlock((unsigned char *) salt, tSalt, saltL);
}


// Convert a password to a hash, based on ref:
// https://wiki.openssl.org/index.php/EVP_Message_Digests
static void genPwdHash(const char* pwd, size_t pwdS, char *pwdHash) {
  char algo[7] = "SHA256";
  unsigned char binHash[4 * pwdS];
  unsigned int md_len = -1;

  const EVP_MD *md = EVP_get_digestbyname(algo);
  if(NULL != md) {
    // TODO - check if error handling needed or checking md_len is enough?
    EVP_MD_CTX *mdctx;
    mdctx = EVP_MD_CTX_new();
    EVP_MD_CTX_init(mdctx);
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, pwd, pwdS);
    EVP_DigestFinal_ex(mdctx, binHash, &md_len);
    EVP_MD_CTX_free(mdctx);

  }

  if (md_len >= ((int) pwdS / 2)) { 
    EVP_EncodeBlock((unsigned char *) pwdHash, binHash, pwdS);

  } else {
    // TODO Send error to web
    printf("ERROR: Could not create password hash\n");
    exit(1);
  }
}


// Create a new passwd file if needed
// TODO - error handling
static int createPWFile(char *fileName) {
  FILE *fp;
  char fileData[21] = "{\n  \"users\": [\n  ]\n}\n";

  if (PWLOCK == 0) {
    fp = openFile(fileName, "w");
    if (fp == NULL) {
      fclose(fp);
      return 1;

    } else {
      fprintf(fp, "%s", fileData);
      fflush(fp);
      fsync(fileno(fp));
      fclose(fp);
      return 0;
    }
  }
  return 1;
}
 

// Check if a user exists
static int userExists(struct json_object *userArray, char *uid) {
  //struct json_object *userArray = NULL;
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
  char pwFile[] = "./conf/passwd.hhl7";
  struct json_object *newUserObj = json_object_new_object();
  struct json_object *pwObj = NULL, *userArray = NULL;

  int uExists = 0;
  int maxPassL = 32;
  size_t saltedL = 3 * maxPassL;
  char salt[maxPassL + 1];
  char saltPasswd[saltedL];
  char pwdHash[4* maxPassL];

  // Create a new passwd file with a user JSON array if needed 
  if (checkFile(pwFile, 2) != 0) {
    createPWFile(pwFile);
  }

  // If lock is true, sleep until lock is cleared
  while (PWLOCK == 1) {
    usleep(250000);
    printf("sleep\n");
  }
  PWLOCK = 1;

  // Re-read the passwd file to ensure we have the latest info
  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    printf("Failed to read password file\n");
    exit(1);
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
    json_object_object_add(newUserObj, "admin", json_object_new_boolean(0));
    json_object_object_add(newUserObj, "sIP", json_object_new_string("127.0.0.1"));
    json_object_object_add(newUserObj, "sPort", json_object_new_string("11011"));
    json_object_object_add(newUserObj, "lPort", json_object_new_string(""));

    json_object_array_add(userArray, newUserObj);

    if (json_object_to_file_ext(pwFile, pwObj,
        JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY) == -1) {

      printf("ERROR: Couldn't write passwd file\n");
      PWLOCK = 0;
      json_object_put(pwObj);
      return 1;
    }

  } else {
    printf("User already exists\n");

  }
  PWLOCK = 0;
  json_object_put(pwObj);
  return 0;
}


// Check if a username/password combination is valid, return codes:
//    0 - User exists and password matches
//    1 - User exists and password matches, but account is disabled
//    2 - User exists but password is incorrect
//    3 - User doesn't exist
int checkAuth(char *uid, const char *passwd) {
  char pwFile[] = "./conf/passwd.hhl7";
  struct json_object *pwObj = NULL, *userArray = NULL, *userObj = NULL;
  struct json_object  *uidStr = NULL, *saltStr = NULL, *pwdStr = NULL;
  int uCount = 0, u = 0, uExists = 0;
  int maxPassL = 32;
  size_t saltedL = 3 * maxPassL;
  char saltPasswd[saltedL];
  char pwdHash[4* maxPassL];

  // Create a new passwd file with a user JSON array if needed 
  if (checkFile(pwFile, 2) != 0) {
    createPWFile(pwFile);
  }

  // Read the password file to a json_object
  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    printf("Failed to read password file\n");
    exit(1);
  }

  json_object_object_get_ex(pwObj, "users", &userArray);
  if (userArray == NULL) {
    printf("Failed to get user object, passwd file corrupt?\n");
    exit(1);
  }

  // If... blah == 0
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
        saltStr = json_object_object_get(userObj, "salt");
        pwdStr = json_object_object_get(userObj, "passwd");

        if (strlen(json_object_get_string(saltStr)) < maxPassL ||
            strlen(json_object_get_string(pwdStr)) < maxPassL) {

          // TODO - error handle
          printf("Invalid password in password file\n");
          exit(1);

        } else {
          sprintf(saltPasswd, "%s%s", json_object_get_string(saltStr), passwd);
          genPwdHash(saltPasswd, strlen(saltPasswd), pwdHash);

          if (strncmp(json_object_get_string(pwdStr), pwdHash, maxPassL) == 0) {
            // Return 1 if the user exists, the password is correct, but account disabled
            json_object_put(pwObj);
            if (uExists == 2) return 1;
            return 0;

          } else {
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


// Update an entry in the password file
// TODO - allow multiple updates at once
int updatePasswdFile(char *uid, const char *key, const char *val) {
  // TODO make config item - check for other references
  char pwFile[] = "./conf/passwd.hhl7";
  int uExists = 0, u = 0, uCount = 0;
  struct json_object *pwObj = NULL, *userArray = NULL, *userObj = NULL, *uidStr = NULL;

  // This should not be needed under normal use, however it's here for security purposes
  if (strcmp(key, "uid") == 0 || strcmp(key, "salt") == 0 || strcmp(key, "passwd") == 0 ||
      strcmp(key, "enabled") == 0 || strcmp(key, "admin") == 0) {
    printf("ERROR: Unexpected attempt to update user credentials, exiting\n");
    exit(1);
  }

  // TODO - add max sleep count as timeout? Look for other sleeps
  // If lock is true, sleep until lock is cleared
  while (PWLOCK == 1) {
    usleep(250000);
    printf("sleep\n");
  }
  PWLOCK = 1;

  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    printf("Failed to read password file\n");
    exit(1);
  }

  json_object_object_get_ex(pwObj, "users", &userArray);
  uExists = userExists(userArray, uid);

  if (uExists == 1) {
    PWLOCK = 0;
    json_object_put(pwObj);
    return 1;

  } else {
    // TODO - WORKING - update object here
    uCount = json_object_array_length(userArray);

    for (u = 0; u < uCount; u++) {
      userObj = json_object_array_get_idx(userArray, u);
      uidStr = json_object_object_get(userObj, "uid");

      if (strcmp(json_object_get_string(uidStr), uid) == 0) {
        json_object_object_del(userObj, key);
        json_object_object_add(userObj, key, json_object_new_string(val));
        break;
      }
    }

    if (json_object_to_file_ext(pwFile, pwObj,
        JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY) == -1) {

      printf("ERROR: Couldn't write passwd file\n");
      PWLOCK = 0;
      json_object_put(pwObj);
      return 1;
    }
  }

  PWLOCK = 0;
  json_object_put(pwObj);
  return 0;
}


// Update a users password, return values:
//    0 - Password updated
//    1 - An error occured, user doesn't exist, couldn't write file etc
int updatePasswd(char *uid, const char *password) {
  // TODO - change this file location plus any others links to it
  char pwFile[] = "./conf/passwd.hhl7";
  struct json_object *pwObj = NULL, *userArray = NULL, *userObj = NULL, *uidStr = NULL;

  int uExists = 0, u = 0, uCount = 0;
  int maxPassL = 32;
  size_t saltedL = 3 * maxPassL;
  char salt[maxPassL + 1];
  char saltPasswd[saltedL];
  char pwdHash[4* maxPassL];

  // If lock is true, sleep until lock is cleared
  while (PWLOCK == 1) {
    usleep(250000);
    printf("sleep\n");
  }
  PWLOCK = 1;

  // Re-read the passwd file to ensure we have the latest info
  pwObj = json_object_from_file(pwFile);
  if (pwObj == NULL) {
    printf("Failed to read password file\n");
    exit(1);
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

      printf("ERROR: Couldn't write passwd file\n");
      PWLOCK = 0;
      json_object_put(pwObj);
      return 1;
    }
  }

  PWLOCK = 0;
  json_object_put(pwObj);
  return 0;
}
