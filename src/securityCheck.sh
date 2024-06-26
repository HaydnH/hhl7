#!/bin/bash

BASEURL='https://localhost:5376'
CURLCMD='curl -k --silent --output /dev/null --write-out %{http_code}'

# Check we can connect
echo -n "Checking connection  [200 status to /]: "
RES=$($CURLCMD $BASEURL);
if [ "$RES" == "200" ]; then
  echo -e "OK";
else
  echo -en "FAIL\n\n";
  exit 1;
fi


# Secure GET pages, should return 401
SGETPS="templates/admit
        templates/admit.json
        templates/doesntExist
        getTemplateList
        getRespondList
        getRespQueue
        getServers
        getSettings
        stopListenHL7
        logout
        listenHL7"

ERRS=0
echo -n "Testing secure GET pages  [401 status]: "
for P in $SGETPS; do
  RES=$($CURLCMD $BASEURL/$P);
  
  if [ "$RES" != "401" ]; then
     echo -en "\nFAIL: $P";
     ((ERRS++));
  fi
done

if [ "$ERRS" -eq 0 ]; then
  echo -e "OK";
else
  echo -e "\n";
fi

# curl -k --silent --output /dev/null --write-out %{http_code} -d 'jsonPOST={"postFunc":"login","uname":"haydn","pword":"g0feve3E$2"}' -X POST https://localhost:5377/jsonPOST

SPOSTPS="jsonPOST={\"postFunc\":\"login\",\"uname\":\"badName\",\"pword\":\"badPass\"}
         jsonPOST={\"postFunc\":\"procRespond\",\"templates\":[]}
         jsonPOST={\"postFunc\":\"changeSends\",\"sIP\":\"127.0.0.1\",\"sPort\":\"8888\"}
         jsonPOST={\"postFunc\":\"changeLists\",\"lPort\":\"8888\"}
         jsonPOST={\"postFunc\":\"changeAcks\",\"aTimeout\":\"3\",\"wTimeout\":\"750\"}
         jsonPOST={\"postFunc\":\"changePwd\",\"oldPass\":\"badPw\",\"newPass\":\"badPw\"}
         hl7MessageText=\"MSH|bad|msg\"
         badKey=\"badMsg\""

ERRS=0
echo -n "Testing secure POST pages [401 status]: "
for P in $SPOSTPS; do
  RES=$($CURLCMD -d "$P" -X POST $BASEURL/jsonPOST);

  if [ "$RES" != "401" ]; then
     echo -en "\nFAIL: $P";
     ((ERRS++));
  fi
done

if [ "$ERRS" -eq 0 ]; then
  echo -e "OK";
else
  echo -e "\n";
fi

