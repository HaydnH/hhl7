/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

int regNewUser(char *uid, char *passwd);
int checkAuth(char *uid, const char *passwd);
int updatePasswdFile(char *uid, const char *key, const char *val, int iVal);
int updatePasswd(char *uid, const char *password);
int lPortUsed(char *uid, const char *lPort);
