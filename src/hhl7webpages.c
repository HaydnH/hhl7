/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

const char *mainPage = "<!DOCTYPE HTML>\n\
<html>\n\
  <head>\n\
    <title>HHL7</title>\n\
    <link rel=\"icon\" type=\"image/x-icon\" href=\"/images/favicon.ico\">\n\
    <style>\n\
      @media (min-resolution: 101dpi) {\n\
        html {\n\
          zoom: 0.8;\n\
        }\n\
      }\n\
      html {\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 15px;\n\
        color: #000;\n\
      }\n\
      body {\n\
        margin: 0px;\n\
      }\n\
      a {\n\
        text-decoration: none;\n\
        color: #fff;\n\
      }\n\
      a.black {\n\
        color: #000;\n\
      }\n\
      a:hover {\n\
        color: #eeb11e;\n\
      }\n\
      .boldRed {\n\
        color: #be1313;\n\
        font-weight: bold;\n\
      }\n\
      .boldYel {\n\
        color: #ffb81e;\n\
        font-weight: bold;\n\
      }\n\
      #postForm {\n\
        height: 100%;\n\
      }\n\
      #headerBar {\n\
        position: fixed;\n\
        top: 0;\n\
        left: 0;\n\
        right: 0;\n\
        background-color: #142248;\n\
        height: 70px;\n\
        margin: 0px;\n\
      }\n\
      #logo {\n\
        position: absolute;\n\
        left: 10px;\n\
        top: 5px;\n\
      }\n\
      #person {\n\
        position: absolute;\n\
        right: 24px;\n\
        top: 10px;\n\
        width: 50px;\n\
        height: 50px;\n\
        background-image: url(/images/person.png);\n\
      }\n\
      #person:hover {\n\
        background-image: url(/images/person_hl.png);\n\
      }\n\
      #menuBar {\n\
        position: fixed;\n\
        top: 70px;\n\
        left: 0;\n\
        right: 0;\n\
        height: 36px;\n\
        line-height: 36px;\n\
        background-color: #8a93ae;\n\
        text-align: left;\n\
      }\n\
      .menuOption {\n\
        font-weight: bold;\n\
        color: #fff;\n\
        padding: 0px 15px;\n\
      }\n\
      .titleBar {\n\
        display: block;\n\
        min-height: 36px;\n\
        width: 100%;\n\
        line-height: 38px;\n\
        border-width: 2px 0px 2px 0px;\n\
        margin-top: -2px;\n\
        border-style: solid;\n\
        border-color: #8a93ae;\n\
        background-color: #d4dcf1;\n\
        text-align: left;\n\
        padding: 0px 15px;\n\
        font-weight: bold;\n\
      }\n\
      #tempTitleBar {\n\
        display: inline-block;\n\
      }\n\
      #tempSelLabel {\n\
        float: left;\n\
        width: 5%;\n\
        min-width: 80px;\n\
      }\n\
      #tempSelDiv {\n\
        float: left;\n\
        width: 90%;\n\
      }\n\
      #tempSelHelp {\n\
        position: absolute;\n\
        right: 0px;\n\
        width: 1%;\n\
        font-size: 20px;\n\
        font-weight: bold;\n\
        color: #8a93ae;\n\
        padding: 0px 14px;\n\
      }\n\
      #tHostText {\n\
        float: right;\n\
        height: 36px;\n\
        line-height: 38px;\n\
        margin: 0px 80px 2px 0px;\n\
        padding: 0px 15px;\n\
        background-color: #eaeffe;\n\
        border-width: 0px 1px;\n\
        border-style: solid;\n\
        border-color: #8a93ae;\n\
      }\n\
      #sendButton.sendActive:hover {\n\
        content: url(\"/images/send_hl.png\");\n\
        cursor: pointer;\n\
      }\n\
      #sendButton {\n\
        position: absolute;\n\
        right: 15px;\n\
        margin-top: 4px;\n\
      }\n\
      #sendPane {\n\
        position: fixed;\n\
        top: 106px;\n\
        left: 0;\n\
        right: 0;\n\
        bottom: 35px;\n\
        display: flex;\n\
        flex-direction: column;\n\
      }\n\
      #listPane {\n\
        position: fixed;\n\
        top: 106px;\n\
        left: 0;\n\
        right: 0;\n\
        bottom: 35px;\n\
        display: none;\n\
        flex-direction: column;\n\
      }\n\
      #respPane {\n\
        position: fixed;\n\
        top: 106px;\n\
        left: 0;\n\
        right: 0;\n\
        bottom: 35px;\n\
        display: none;\n\
        flex-direction: column;\n\
      }\n\
      #tempForm {\n\
        display: flex;\n\
        flex-wrap: wrap;\n\
        display: none;\n\
        background-color: #eeeff3;\n\
      }\n\
      .tempFormField {\n\
        flex: 0 33%;\n\
        height: 36px;\n\
        line-height: 34px;\n\
      }\n\
      .tempFormKey {\n\
        width: 36%;\n\
        float: left;\n\
        text-align: right;\n\
        font-weight: 600;\n\
        padding: 0px 20px 0px 0px;\n\
        background-color: #eeeff3;\n\
      }\n\
      .tempFormValue {\n\
        float: left;\n\
        height: 36px;\n\
        width: 60%;\n\
        background-color: #eeeff3;\n\
      }\n\
      .tempFormInput {\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        background-color: #fff;\n\
        border: 0px;\n\
        height: 26px;\n\
        width: 94%;\n\
        padding: 0px 10px 0px 10px;\n\
        outline: none;\n\
      }\n\
      .tempFormSelect {\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        background-color: #fff;\n\
        border: 0px;\n\
        height: 26px;\n\
        width: 100%;\n\
        padding: 0px 10px 0px 10px;\n\
        outline: none;\n\
      }\n\
      #respPlus {\n\
        display: inline-block;\n\
        font-size: 28px;\n\
        font-weight: bold;\n\
        height: 36px;\n\
        line-height: 32px;\n\
        vertical-align: bottom;\n\
        margin: 0px 5px;\n\
      }\n\
      #respForm {\n\
        display: flex;\n\
        flex-wrap: wrap;\n\
      }\n\
      .respFormItem {\n\
        margin: 3px 3px 4px 3px;\n\
        height: 30px;\n\
        line-height: 30px;\n\
        background-color: #eeeff3;\n\
        border-radius: 6px;\n\
        border: 2px solid #8a93ae;\n\
        padding: 0px 15px 0px 20px;\n\
      }\n\
      .respFormClose {\n\
        font-size: 18px;\n\
        padding-left: 10px;\n\
      }\n\
      select {\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        background-color: #eaeffe;\n\
        border: 1px solid #c4ccd1;\n\
      }\n\
      .tempSelect {\n\
        margin: 0px 0px 0px 10px;\n\
        padding: 0px 5px;\n\
        outline: none;\n\
        width: 15%;\n\
        height: 26px;\n\
      }\n\
      #contContainer {\n\
        display: flex;\n\
        flex-direction: row;\n\
        height: 100%;\n\
        width: 100%;\n\
        overflow-y: scroll;\n\
      }\n\
      #respContainer {\n\
        position: relative;\n\
        width: 54px;\n\
        background-color: #eaeffe;\n\
      }\n\
      #hl7Container {\n\
        height: 100%;\n\
        width: 100%;\n\
      }\n\
      .hl7Message {\n\
        height: 95%;\n\
        font-family: \"Courier New\", Courier, monospace;\n\
        font-size: 17px;\n\
        font-weight: 500;\n\
        padding: 14px 14px;\n\
        outline: none;\n\
      }\n\
      .hl7MsgResp {\n\
        position: absolute;\n\
        width: 50px;\n\
        font-family: \"Courier New\", Courier, monospace;\n\
        font-size: 17px;\n\
        font-weight: 500;\n\
        text-align: center;\n\
        background-color: #d4dcf1;\n\
        border-width: 1px 0px;\n\
        border-style: solid;\n\
        border-color: #8a93ae;\n\
      }\n\
      .hl7MsgRespG {\n\
        background-color: #a9dfad;\n\
      }\n\
      .hl7MsgRespR {\n\
        background-color: #ff9191;\n\
      }\n\
      .hl7MsgRespT {\n\
        background-color: #d1a95a;\n\
      }\n\
      #hl7Log {\n\
        overflow-y: scroll;\n\
        white-space: pre;\n\
      }\n\
      #tempDesc {\n\
        display: none;\n\
        box-sizing: border-box;\n\
        min-height: 36px;\n\
        line-height: 20px;\n\
        width: 100%;\n\
        padding: 10px 15px;\n\
        background-color: #eaeffe;\n\
        border-width: 2px 0px 2px 0px;\n\
        margin-top: -2px;\n\
        border-style: solid;\n\
        border-color: #8a93ae;\n\
      }\n\
      #footer {\n\
        position: fixed;\n\
        left: 0;\n\
        bottom: 0;\n\
        right: 0;\n\
        height: 36px;\n\
        line-height: 38px;\n\
        background-color: #142248;\n\
        color: #fff;\n\
        text-align: right;\n\
        padding: 0px 20px;\n\
        font-size: 14px;\n\
      }\n\
      .popDim {\n\
        display: none;\n\
        position: fixed;\n\
        left: 0;\n\
        top: 0;\n\
        height: 100%;\n\
        width: 100%;\n\
        background-color: rgba(0, 0, 0, 0.5);\n\
      }\n\
      #loginDim {\n\
        display: block;\n\
      }\n\
      #menuDim {\n\
        display: none;\n\
        background-color: rgba(0, 0, 0, 0);\n\
      }\n\
      .popWindow {\n\
        position: absolute;\n\
        left: 50%;\n\
        top: 50%;\n\
        transform: translate(-50%, -50%);\n\
        width: 30%;\n\
        height: 25%;\n\
        background-color: #fff;\n\
        border-width: 2px;\n\
        border-style: solid;\n\
        border-color: #142248;\n\
      }\n\
      #menuWindow {\n\
        position: fixed;\n\
        right: 49px;\n\
        top: 35px;\n\
        width: 25%;\n\
        background-color: #fff;\n\
        border-width: 2px;\n\
        border-style: solid;\n\
        border-color: #142248;\n\
        outline: 1px solid #fff;\n\
      }\n\
      .popTitle {\n\
        font-weight: bold;\n\
        background-color: #142248;\n\
        color: #fff;\n\
        height: 36px;\n\
        line-height: 36px;\n\
        padding: 0px 15px;\n\
      }\n\
      .popMsg {\n\
        display: flex;\n\
        flex-direction: row;\n\
        align-items: center;\n\
        background-color: #fff;\n\
        position: absolute;\n\
        top: 36px;\n\
        right: 0px;\n\
        bottom: 41px;\n\
        left: 0px;\n\
      }\n\
      .popImg {\n\
        height: 70px;\n\
        width: 70px;\n\
        background-image: url(/images/error.png);\n\
        background-repeat: no-repeat;\n\
        background-position: center center;\n\
      }\n\
      #loginImg {\n\
        background-image: url(/images/person_hl.png);\n\
        padding: 0px 0px 0px 20px;\n\
      }\n\
      .popTxt {\n\
        flex: 1;\n\
        padding: 0px 25px 0px 15px;\n\
        text-align: justify;\n\
        text-justify: inter-word;\n\
      }\n\
      .popFooter {\n\
        position: absolute;\n\
        bottom: 0px;\n\
        left: 0px;\n\
        right: 0px;\n\
        height: 41px;\n\
        background-color: #fff;\n\
      }\n\
      .menuTitleText {\n\
        float: left;\n\
        width: 90%;\n\
      }\n\
      .menuTitleTogl {\n\
        float: right;\n\
        width: 10%;\n\
        font-size: 20px;\n\
        margin-right: -8px;\n\
        margin-bottom: -2px;\n\
        text-align: right;\n\
      }\n\
      .menuHeader {\n\
        box-sizing: border-box;\n\
        display: inline-block;\n\
        width: 100%;\n\
        font-weight: bold;\n\
        background-color: #8a93ae;\n\
        color: #fff;\n\
        height: 36px;\n\
        line-height: 36px;\n\
        padding: 0px 15px;\n\
        border-top: 1px solid #142248;\n\
        border-bottom: 1px solid #142248;\n\
      }\n\
      .menuItem {\n\
        display: flex;\n\
        flex-direction: row;\n\
        flex-wrap: nowrap;\n\
        width: 100%;\n\
      }\n\
      .menuItemButton, .menuItemButtonInactive {\n\
        box-sizing: border-box;\n\
        display: inline-block;\n\
        width: 100%;\n\
        font-weight: 600;\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        color: #fff;\n\
        height: 36px;\n\
        line-height: 28px;\n\
        text-align: center;\n\
        border-width: 3px;\n\
        border-style: double;\n\
        border-color: #fff;\n\
        background-color: #142248;\n\
      }\n\
      .menuItemButton {\n\
        background-color: #142248;\n\
      }\n\
      .menuItemButton:hover {\n\
        color: #eeb11e;\n\
        cursor: pointer;\n\
      }\n\
      .menuItemButtonInactive {\n\
        background-color: #BBBBBB;\n\
      }\n\
      @keyframes fadeGreen {\n\
        0% { background-color: #2aa33a; }\n\
        40% { background-color: #2aa33a; }\n\
        100% { background-color: #BBBBBB; }\n\
      }\n\
      @keyframes fadeRed {\n\
        0% { background-color: #cf2d33; }\n\
        40% { background-color: #cf2d33; }\n\
        100% { background-color: #BBBBBB; }\n\
      }\n\
      .menuItemButtonGreen {\n\
        animation-duration: 500ms;\n\
        animation-name: fadeGreen;\n\
      }\n\
      .menuItemButtonRed {\n\
        animation-duration: 500ms;\n\
        animation-name: fadeRed;\n\
      }\n\
      .menuSpacer {\n\
        box-sizing: border-box;\n\
        display: inline-block;\n\
        width: 100%;\n\
        height: 100%;\n\
        background-color: #fff;\n\
      }\n\
      .menuSubHeader {\n\
        display: inline-block;\n\
        width: 26%;\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        font-weight: bold;\n\
        background-color: #d4dcf1;\n\
        height: 36px;\n\
        line-height: 36px;\n\
        padding: 0px 15px;\n\
      }\n\
      .menuDataItem {\n\
        display: inline-block;\n\
        flex-grow: 4;\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        font-weight: bolder;\n\
        background-color: #fff;\n\
        height: 36px;\n\
        line-height: 36px;\n\
      }\n\
      .menuInput {\n\
        box-sizing: border-box;\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        height: 100%;\n\
        width: 100%;\n\
        padding: 0px 15px;\n\
        border: none;\n\
        outline: none;\n\
      }\n\
      .menuSelect {\n\
        box-sizing: border-box;\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        height: 100%;\n\
        width: 100%;\n\
        padding: 0px 12px;\n\
        border: none;\n\
        outline: none;\n\
        background-color: #fff;\n\
      }\n\
      .menuInput:focus {\n\
        background-color: #fff8d6;\n\
      }\n\
      .menuInputErr {\n\
        background-color: #f2c7c7 !important;\n\
      }\n\
      .loginInfo {\n\
        padding: 3px 0px 3px 0px;\n\
      }\n\
      .loginInput {\n\
        font-family: inherit;\n\
        font-size: inherit;\n\
        width: 65%;\n\
      }\n\
      #confPwordD {\n\
        display: none;\n\
      }\n\
      .loginQ {\n\
        float: left;\n\
        height: 20px;\n\
        line-height: 26px;\n\
        width: 30%;\n\
        font-weight: bold;\n\
      }\n\
      .logFrm {\n\
        float: left;\n\
        width: 70%;\n\
      }\n\
      .button {\n\
        position: absolute;\n\
        text-align: center;\n\
        font-weight: bold;\n\
        color: #fff;\n\
        border-width: 3px;\n\
        border-style: double;\n\
        border-color: #fff;\n\
        background-color: #142248;\n\
      }\n\
      .button:hover {\n\
        color: #eeb11e;\n\
        cursor: pointer;\n\
      }\n\
      .buttonInactive {\n\
        text-align: center;\n\
        color: #fff;\n\
        padding: 0px 0px 0px 2px;\n\
        background-color: #BBBBBB;\n\
        border-width: 3px;\n\
        border-style: double;\n\
        border-color: #fff;\n\
        cursor: default;\n\
      }\n\
      #ok {\n\
        right: 15px;\n\
        bottom: 15px;\n\
        height: 26px;\n\
        width: 45px;\n\
        line-height: 28px;\n\
      }\n\
      #register {\n\
        left: 15px;\n\
        bottom: 15px;\n\
        height: 32px;\n\
        width: 92px;\n\
      }\n\
      #submit {\n\
        position: absolute;\n\
        right: 15px;\n\
        bottom: 15px;\n\
        height: 32px;\n\
        width: 52px;\n\
        padding: 0px 0px 0px 3px;\n\
      }\n\
      #hl7Queue {\n\
        overflow-y: scroll;\n\
        padding: 0;\n\
        margin: 0;\n\
      }\n\
      #respQTable, #respQBody {\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 15px;\n\
        font-weight: 500;\n\
        width: 100%;\n\
        border-spacing: 1px;\n\
        font-size: 14px;\n\
      }\n\
      #rCount {\n\
        position: absolute;\n\
        right: 0px;\n\
        width: 100px;\n\
        display: inline-block;\n\
        text-align: right;\n\
        padding: 0px 15px;\n\
        font-weight: bold;\n\
      }\n\
      thead, tr, tfoot {\n\
        height: 36px;\n\
        line-height: 38px;\n\
      }\n\
      thead, tfoot {\n\
        background-color: #eeeff3;\n\
      }\n\
      .trEven {\n\
        background-color: #f9f9ff;\n\
      }\n\
      .trOdd {\n\
        background-color: #f4f4ff;\n\
      }\n\
      th, td {\n\
        padding: 0px 15px;\n\
      }\n\
      td.foot {\n\
        text-align: center;\n\
        font-weight: bold;\n\
      }\n\
      .thSendT, .thDest, .thSTFmt {\n\
        width: 25%;\n\
        text-align: left;\n\
      }\n\
      .rSendTime, .thSendTime {\n\
        display: none;\n\
      }\n\
      .thResp, .tdCtr, .tdCtrG, .tdCtrR {\n\
        width: 80px;\n\
        text-align: center;\n\
      }\n\
      .tdCtrG {\n\
        background-color: #a9dfad;\n\
      }\n\
      .tdCtrR {\n\
        background-color: #ff9191;\n\
      }\n\
    </style>\n\
\n\
    <script>\n\
      var webLocked = true;\n\
      var isConnectionOpen = false;\n\
      var ackTimeout = 3;\n\
      var webTimeout = 750;\n\
\n\
      function showLogin() {\n\
        webLocked = true;\n\
        hideMenu();\n\
        var login = document.getElementById(\"loginDim\");\n\
        var uname = document.getElementById(\"uname\");\n\
        login.style.display = \"block\";\n\
        uname.focus();\n\
        uname.selectionStart = uname.selectionEnd = uname.value.length;\n\
      }\n\
\n\
      function switchLogin() {\n\
        var reg = document.getElementById(\"register\");\n\
        var tit = document.getElementById(\"loginTitle\");\n\
        var conf = document.getElementById(\"confPwordD\");\n\
\n\
        if (reg.innerText == \"Login\") {\n\
          tit.innerText = \"Login:\";\n\
          reg.innerText = \"Register\";\n\
          conf.style.display = \"none\";\n\
\n\
        } else {\n\
          tit.innerText = \"Register new account:\";\n\
          reg.innerText = \"Login\";\n\
          conf.style.display = \"block\";\n\
        }\n\
        validLogin();\n\
      }\n\
\n\
      var logPostFunc = function(event) { postCreds(event); };\n\
      var logSubmitFunc = function(event) { if (event.key === \"Enter\") postCreds(event); };\n\
      function validLogin(evt) {\n\
        var reg = document.getElementById(\"register\").innerText;\n\
        var usr = document.getElementById(\"uname\");\n\
        var pwd = document.getElementById(\"pword\");\n\
        var cpw = document.getElementById(\"confPword\");\n\
        var sub = document.getElementById(\"submit\");\n\
\n\
        sub.removeEventListener(\"click\", logPostFunc);\n\
        usr.removeEventListener(\"keyup\", logSubmitFunc);\n\
        pwd.removeEventListener(\"keyup\", logSubmitFunc);\n\
        cpw.removeEventListener(\"keyup\", logSubmitFunc);\n\
\n\
        if (usr.value.length < 5 || usr.value.length > 25 || \n\
            pwd.value.length < 8 || pwd.value.length > 200) {\n\
\n\
          sub.classList.replace(\"button\", \"buttonInactive\");\n\
          return false;\n\
        }\n\
\n\
        if (reg == \"Login\") {\n\
          if (cpw.value.length < 8) {\n\
            sub.classList.replace(\"button\", \"buttonInactive\");\n\
            return false;\n\
          }\n\
          if (pwd.value != cpw.value) {\n\
            sub.classList.replace(\"button\", \"buttonInactive\");\n\
            return false;\n\
          }\n\
        }\n\
\n\
        usr.addEventListener(\"keyup\", logSubmitFunc);\n\
        pwd.addEventListener(\"keyup\", logSubmitFunc);\n\
        cpw.addEventListener(\"keyup\", logSubmitFunc);\n\
        sub.addEventListener(\"click\", logPostFunc);\n\
        sub.classList.replace(\"buttonInactive\", \"button\");\n\
      }\n\
\n\
      function showMenu() {\n\
        popSettings();\n\
        var pane = document.getElementById(\"menuDim\");\n\
        pane.style.display = \"block\";\n\
      }\n\
\n\
      var sendPostFunc = function(event) { postSendSets(event); };\n\
      function validSendSets() {\n\
        sPortObj = document.getElementById(\"tPort\");\n\
        sPort = Number(sPortObj.value);\n\
        sButton = document.getElementById(\"saveSendSets\");\n\
\n\
        sButton.removeEventListener(\"click\", sendPostFunc);\n\
        sButton.classList.remove(\"menuItemButtonGreen\");\n\
        sButton.classList.remove(\"menuItemButtonRed\");\n\
\n\
        if (typeof(sPort) == 'number' && sPort > 1023 && sPort <= 65535) {\n\
          sButton.addEventListener(\"click\", sendPostFunc);\n\
          sButton.classList.replace(\"menuItemButtonInactive\", \"menuItemButton\");\n\
          sPortObj.classList.remove(\"menuInputErr\");\n\
\n\
        } else {\n\
          sButton.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
          sPortObj.classList.add(\"menuInputErr\");\n\
\n\
        }\n\
      }\n\
\n\
      var listPostFunc = function(event) { postListSets(event); };\n\
      function validListSets() {\n\
        lPortObj = document.getElementById(\"lPort\");\n\
        lPort = Number(lPortObj.value);\n\
        lButton = document.getElementById(\"saveListSets\");\n\
\n\
        lButton.removeEventListener(\"click\", listPostFunc);\n\
        lButton.classList.remove(\"menuItemButtonGreen\");\n\
        lButton.classList.remove(\"menuItemButtonRed\");\n\
\n\
        if (typeof(lPort) == 'number' && lPort > 1023 && lPort <= 65535) {\n\
          lButton.addEventListener(\"click\", listPostFunc);\n\
          lButton.classList.replace(\"menuItemButtonInactive\", \"menuItemButton\");\n\
          lPortObj.classList.remove(\"menuInputErr\");\n\
\n\
        } else {\n\
          lButton.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
          lPortObj.classList.add(\"menuInputErr\");\n\
\n\
        }\n\
      }\n\
\n\
      var ackPostFunc = function(event) { postACKSets(event); };\n\
      function validACKSets() {\n\
        ackTObj = document.getElementById(\"ackTimeout\");\n\
        ackT = Number(ackTObj.value);\n\
        webTObj = document.getElementById(\"webTimeout\");\n\
        webT = Number(webTObj.value);\n\
        aButton = document.getElementById(\"saveACKSets\");\n\
\n\
        aButton.removeEventListener(\"click\", listPostFunc);\n\
        aButton.classList.remove(\"menuItemButtonGreen\");\n\
        aButton.classList.remove(\"menuItemButtonRed\");\n\
\n\
        if (typeof(ackT) == 'number' && ackT > 0 && ackT < 100 && \n\
            typeof(webT) == 'number' && webT >= 50 && webT <= 5000) {\n\
          aButton.addEventListener(\"click\", ackPostFunc);\n\
          aButton.classList.replace(\"menuItemButtonInactive\", \"menuItemButton\");\n\
          ackTObj.classList.remove(\"menuInputErr\");\n\
          webTObj.classList.remove(\"menuInputErr\");\n\
\n\
        } else {\n\
          aButton.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
          ackTObj.classList.add(\"menuInputErr\");\n\
          webTObj.classList.add(\"menuInputErr\");\n\
\n\
        }\n\
      }\n\
\n\
      var pwdPostFunc = function(event) { postPwdSets(event); };\n\
      function validPwdSets() {\n\
        oldPwdObj = document.getElementById(\"oldPwd\");\n\
        newPwdObj = document.getElementById(\"newPwd\");\n\
        conPwdObj = document.getElementById(\"conPwd\");\n\
        oldPwd = oldPwdObj.value;\n\
        newPwd = newPwdObj.value;\n\
        conPwd = conPwdObj.value;\n\
        pButton = document.getElementById(\"resetPW\");\n\
\n\
        pButton.removeEventListener(\"click\", pwdPostFunc);\n\
        pButton.classList.remove(\"menuItemButtonGreen\");\n\
        pButton.classList.remove(\"menuItemButtonRed\");\n\
\n\
\n\
        if (oldPwd.length > 7 && newPwd.length > 7 && conPwd.length > 7 && newPwd == conPwd) {\n\
          pButton.addEventListener(\"click\", pwdPostFunc);\n\
          pButton.classList.replace(\"menuItemButtonInactive\", \"menuItemButton\");\n\
          oldPwdObj.classList.remove(\"menuInputErr\");\n\
          newPwdObj.classList.remove(\"menuInputErr\");\n\
          conPwdObj.classList.remove(\"menuInputErr\");\n\
\n\
        } else {\n\
          pButton.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
          oldPwdObj.classList.add(\"menuInputErr\");\n\
          newPwdObj.classList.add(\"menuInputErr\");\n\
          conPwdObj.classList.add(\"menuInputErr\");\n\
\n\
        }\n\
      }\n\
\n\
      function clearMenuButtons() {\n\
        const btns = [ \"saveSendSets\", \"saveListSets\", \"resetPW\" ];\n\
        for (let i = 0; i < btns.length; i++) {\n\
          btn = document.getElementById(btns[i]);\n\
          btn.classList.remove(\"menuItemButtonGreen\");\n\
          btn.classList.remove(\"menuItemButtonRed\");\n\
        }\n\
      }\n\
\n\
      function hideMenu() {\n\
        clearMenuButtons();\n\
        var pane = document.getElementById(\"menuDim\");\n\
        pane.style.display = \"none\";\n\
      }\n\
\n\
      function clearResponders() {\n\
        var respForm = document.getElementById(\"respForm\");\n\
        respForm.style.display = \"none\";\n\
        respForm.textContent = \"\";\n\
      }\n\
\n\
      function showList() {\n\
        clearResponders();\n\
        stopHL7Listener();\n\
        var pane = document.getElementById(\"sendPane\");\n\
        pane.style.display = \"none\";\n\
        var pane = document.getElementById(\"respPane\");\n\
        pane.style.display = \"none\";\n\
        var pane = document.getElementById(\"listPane\");\n\
        pane.style.display = \"flex\";\n\
        startHL7Listener();\n\
      }\n\
\n\
      function showResp() {\n\
        clearResponders();\n\
        stopHL7Listener();\n\
        var pane = document.getElementById(\"sendPane\");\n\
        pane.style.display = \"none\";\n\
        var pane = document.getElementById(\"listPane\");\n\
        pane.style.display = \"none\";\n\
        var pane = document.getElementById(\"respPane\");\n\
        pane.style.display = \"flex\";\n\
      }\n\
\n\
      function showSend() {\n\
        clearResponders();\n\
        stopHL7Listener();\n\
        var pane = document.getElementById(\"listPane\");\n\
        pane.style.display = \"none\";\n\
        var pane = document.getElementById(\"respPane\");\n\
        pane.style.display = \"none\";\n\
        var pane = document.getElementById(\"sendPane\");\n\
        pane.style.display = \"flex\";\n\
      }\n\
\n\
      function placeCursor(target) {\n\
        const rng = document.createRange();\n\
        const sel = window.getSelection();\n\
\n\
        if (target.nextElementSibling) target = target.nextElementSibling;\n\
        if (target.innerText != \"\\n\") {\n\
          var nDiv = document.createElement(\"div\");\n\
          nDiv.innerText = \"\\n\";\n\
          target.parentNode.insertBefore(nDiv, target);\n\
          target = nDiv;\n\
        }\n\
\n\
        rng.setStart(target, 0);\n\
        rng.collapse(true);\n\
        sel.removeAllRanges();\n\
        sel.addRange(rng);\n\
      }\n\
\n\
      function unwrapResps(curDiv) {\n\
        var hl7Msg = document.getElementById(\"hl7Message\");\n\
        var hl7Msgs = hl7Msg.getElementsByTagName(\"DIV\");\n\
        var delCount = 0;\n\
\n\
        for (var i = 0; i < hl7Msgs.length; i++) {\n\
          if (hl7Msgs[i].getElementsByTagName(\"DIV\").length > 0) {\n\
            hl7Msgs[i].replaceWith(...hl7Msgs[i].childNodes);\n\
            placeCursor(curDiv);\n\
          }\n\
        }\n\
        if (hl7Msg.lastChild.innerText != \"\\n\") {\n\
          var nDiv = document.createElement(\"div\");\n\
          nDiv.innerText = \"\\n\";\n\
          hl7Msg.appendChild(nDiv);\n\
        }\n\
      }\n\
\n\
      function wrapResps() {\n\
        var dDiv = document.getElementById(\"hl7Message\");\n\
        var childs = dDiv.childNodes;\n\
        var msgFound = 0;\n\
        var nDiv;\n\
\n\
        for (var i = childs.length - 1; i >= 0; i--) {\n\
          if (childs[i].tagName != \"DIV\") {\n\
            if (childs[i].nodeType == Node.TEXT_NODE && i == 0) {\n\
              nDiv.insertBefore(childs[i], nDiv.firstChild);\n\
              dDiv.insertBefore(nDiv, childs[i]);\n\
              msgFound = 0;\n\
\n\
            } else if (childs[i].nodeType == Node.TEXT_NODE &&\n\
               (childs[i].nodeValue.slice(0, 4) == \"MSH|\" ||\n\
                childs[i].nodeValue.slice(1, 5) == \"MSH|\")) {\n\
\n\
              nDiv.insertBefore(childs[i], nDiv.firstChild);\n\
              dDiv.insertBefore(nDiv, childs[i]);\n\
              msgFound = 0;\n\
\n\
              if (childs[i].nextSibling.innerText.replace(/[\\n\\r\\t ]/g, \"\") != \"\") {\n\
                nDiv = document.createElement(\"DIV\");\n\
                nDiv.innerHTML = \"<br />\";\n\
                dDiv.insertBefore(nDiv, childs[i].nextSibling);\n\
              }\n\
\n\
            } else {\n\
              if (msgFound == 0) {\n\
                nDiv = document.createElement(\"DIV\");\n\
                msgFound = 1;\n\
              }\n\
              nDiv.insertBefore(childs[i], nDiv.firstChild);\n\
\n\
            }\n\
          }\n\
        }\n\
      }\n\
\n\
      function resizeResps(moveCursor) {\n\
        var sel = document.getSelection();\n\
        var curDiv = sel.anchorNode;\n\
        unwrapResps(curDiv);\n\
        wrapResps();\n\
\n\
        var mResps = document.getElementById(\"respContainer\");\n\
        mResps.innerHTML = \"\";\n\
        var hl7Msg = document.getElementById(\"hl7Message\");\n\
        var hl7Msgs = hl7Msg.getElementsByTagName(\"DIV\");\n\
        var dTop = 0;\n\
        var dHeight = 0;\n\
        var msgCount = 0;\n\
\n\
        for (var i = 0; i < hl7Msgs.length; i++) {\n\
          var addDiv = 0;\n\
          if (hl7Msgs[i].getElementsByTagName(\"DIV\").length > 0) {\n\
            addDiv = 0;\n\
\n\
          //} else if (hl7Msgs[i].innerText.slice(0,4) == \"MSH|\" && msgCount == 0) {\n\
          } else if (msgCount == 0) {\n\
            dTop = hl7Msgs[i].offsetTop;\n\
            dHeight = hl7Msgs[i].offsetHeight;\n\
            msgCount++;\n\
\n\
          } else if (hl7Msgs[i].innerText.slice(0,4) == \"MSH|\" && msgCount > 0) {\n\
            msgCount++;\n\
            addDiv = 1;\n\
\n\
          } else if (! hl7Msgs[i].nextElementSibling) {\n\
            addDiv = 1;\n\
\n\
          } else if (hl7Msgs[i].innerText.replace(/[\\n\\r\\t ]/g, \"\") != \"\") {\n\
            dHeight = hl7Msgs[i].offsetTop + hl7Msgs[i].offsetHeight - dTop;\n\
          }\n\
\n\
          if (addDiv == 1) {\n\
            var rDiv = document.createElement(\"div\");\n\
            rDiv.innerText = \"--\";\n\
            rDiv.className = \"hl7MsgResp\";\n\
            rDiv.style.top = dTop - hl7Msg.offsetTop + \"px\";\n\
            rDiv.style.height = dHeight + \"px\";\n\
            rDiv.style.lineHeight = dHeight + \"px\";\n\
            mResps.appendChild(rDiv);\n\
            if (hl7Msgs[i]) {\n\
              dTop = hl7Msgs[i].offsetTop;\n\
              dHeight = hl7Msgs[i].offsetTop + hl7Msgs[i].offsetHeight - dTop;\n\
            }\n\
          }\n\
        }\n\
        mResps.style.height = hl7Msg.scrollHeight + 20 + \"px\";\n\
      }\n\
\n\
      function updateResps(event) {\n\
        if (!event) {\n\
          setTimeout(function() { resizeResps(); }, 0);\n\
\n\
        } else {\n\
          if (event.key == \"Enter\" && event.shiftKey) {\n\
            event.preventDefault();\n\
          } else if (event.key == \"Enter\") {\n\
            resizeResps();\n\
          } else if (event.key == \"Backspace\") {\n\
            resizeResps();\n\
          } else if (event.key == \"Control\") {\n\
            resizeResps();\n\
          }\n\
        }\n\
      }\n\
\n\
      function popResps(resJObj) {\n\
        var respCont = document.getElementById(\"respContainer\");\n\
        var mResps = respCont.getElementsByTagName(\"DIV\");\n\
        jObj = JSON.parse(resJObj);\n\
\n\
        if (jObj.count != mResps.length) {\n\
          errHandler(\"ERROR: The backend server replied with a different number of message responses than expected. \" + jObj.count + \" reponses for \" + mResps.length + \" messages\");\n\
        } else {\n\
          const rArr = jObj.results.split(\",\");\n\
          for (var i = 0; i < jObj.count; i++) {\n\
            mResps[i].innerText = rArr[i];\n\
            if (rArr[i] == \"AA\" || rArr[i] == \"CA\") {\n\
              mResps[i].classList.add(\"hl7MsgRespG\");\n\
            } else if (rArr[i] == \"AE\" || rArr[i] == \"AR\" ||\n\
                       rArr[i] == \"CE\" || rArr[i] == \"CR\" ||\n\
                       rArr[i] == \"EE\") {\n\
              mResps[i].classList.add(\"hl7MsgRespR\");\n\
            } else {\n\
              errHandler(\"ERROR: The backed sent an invalid response code (\" + rArr[i] + \", expected one of AA, AE, AR, CA, CE, CR or EE)\");\n\
            }\n\
          }\n\
        }\n\
      }\n\
\n\
      function showDesc() {\n\
        var pane = document.getElementById(\"tempDesc\");\n\
        if (pane.style.display == \"none\" || pane.style.display == \"\") {\n\
          pane.style.display = \"block\";\n\
        } else {\n\
          pane.style.display = \"none\";\n\
        }\n\
      }\n\
\n\
      function clrHl7Help() {\n\
        var txt = document.getElementById(\"hl7Message\");\n\
        if (txt.innerText == \"Paste/type HL7 message here or use a template above.\") {\n\
          txt.innerText = \"\";\n\
        }\n\
      }\n\
\n\
      function showTempForm(thisSel, hide) {\n\
        clrRes();\n\
        var tForm = document.getElementById(\"tempForm\");\n\
        if (thisSel.value == \"None\" || hide == true) {\n\
          tForm.style.display = \"none\";\n\
        } else {\n\
          popTempForm(thisSel);\n\
          tForm.style.display = \"flex\";\n\
        }\n\
      }\n\
\n\
      function updateTemps(thisSel) {\n\
        var tempDesc = document.getElementById(\"tempDesc\");\n\
        tempDesc.innerHTML = \"\";\n\
        var parent = thisSel.parentNode;\n\
        var children = parent.childNodes;\n\
        var myID = Number(thisSel.id.slice(10));\n\
\n\
        for (var c = children.length - 1; c > 0; c--) {\n\
          if (children[c].id) {\n\
            if (children[c].id == thisSel.id) break;\n\
            parent.removeChild(children[c]);\n\
          }\n\
        }\n\
\n\
        var tBar = document.getElementById(\"tempSelDiv\");\n\
        var thisOpt = thisSel.options[thisSel.selectedIndex];\n\
        if (thisOpt.value != \"None\" && thisOpt.dataset.dir) {\n\
          var newSel = document.createElement(\"select\");\n\
          var nextID = Number(thisSel.id.slice(10)) + 1;\v\
          newSel.id = \"tempSelect\" + nextID;\n\
          newSel.classList.add(\"tempSelect\");\n\
          newSel.addEventListener(\"change\", function() { updateTemps(this) });\n\
          tBar.appendChild(newSel);\n\
          popTemplates(0);\n\
          showTempForm(newSel, true);\n\
\n\
        } else {\n\
          showTempForm(thisSel);\n\
\n\
        }\n\
      }\n\
\n\
      function getTempURL() {\n\
        var tBar = document.getElementById(\"tempSelDiv\");\n\
        var children = tBar.children;\n\
        var url = \"\";\n\
\n\
        for (var c = 0; c <= children.length - 2; c++) {\n\
          if (children[c].id) {\n\
            url = url.concat(\"/\", children[c].value);\n\
          }\n\
        }\n\
        return url + \"/\";\n\
      }\n\
\n\
      function showRespForm(respCount) {\n\
        var tForm = document.getElementById(\"respForm\");\n\
        if (respCount <= 0) {\n\
          tForm.style.display = \"none\";\n\
        } else {\n\
          tForm.style.display = \"flex\";\n\
        }\n\
      }\n\
\n\
      function clrRes() {\n\
        var mResp = document.getElementById(\"respContainer\");\n\
        var mResps = mResp.getElementsByTagName(\"DIV\");\n\
        for (var i = 0; i < mResps.length; i++) {\n\
          mResps[i].innerText = \"--\";\n\
          mResps[i].className = \"hl7MsgResp\";\n\
        }\n\
\n\
        var btn = document.getElementById(\"sendButton\");\n\
        btn.addEventListener(\"click\", postHL7);\n\
        btn.classList.add(\"sendActive\");\n\
      }\n\
\n\
      function updateHL7(id, val) {\n\
        clrRes();\n\
        var hl7Spans = document.getElementsByClassName(id + \"_HL7\");\n\
        for (var i = 0; i < hl7Spans.length; ++i) {\n\
          hl7Spans[i].innerHTML = val;\n\
        }\n\
      }\n\
\n\
      function closeErrWin() {\n\
        var errWin = document.getElementById(\"errDim\");\n\
        errWin.style.display = \"none\";\n\
      }\n\
\n\
      function initPage() {\n\
        /* Pre load images */\n\
        var eImg = new Image();\n\
        eImg.src = \"/images/send_hl.png\";\n\
        var eImg = new Image();\n\
        eImg.src = \"/images/person_hl.png\";\n\
        var eImg = new Image();\n\
        eImg.src = \"/images/error.png\";\n\
        var wImg = new Image();\n\
        wImg.src = \"/images/warning.png\";\n\
\n\
        checkAuth();\n\
        if (webLocked = false) unlockWeb();\n\
      }\n\
\n\
      function unlockWeb() {\n\
        webLocked = false;\n\
        var errWin = document.getElementById(\"loginDim\");\n\
        errWin.style.display = \"none\";\n\
        var pwdInp = document.getElementById(\"newPwd\");\n\
        var conInp = document.getElementById(\"conPwd\");\n\
        pwdInp.value = \"\";\n\
        conInp.value = \"\";\n\
\n\
        popSettings();\n\
        popTemplates(0);\n\
        popTemplates(1);\n\
      }\n\
\n\
      function checkLPort() {\n\
        lPort = document.getElementById(\"lPort\").value;\n\
        if (lPort == \"\") {\n\
          errHandler(\"WARN: Please select a port to listen on in the settings menu. Listerner has not been started.\");\n\
          return false;\n\
        }\n\
        return true;\n\
      }\n\
\n\
      var isRespond = false;\n\
      function procResponders(stopListen) {\n\
        var respJSON = { \"postFunc\":\"procRespond\", \"templates\":[ ] };\n\
        var respTable = document.getElementById(\"respQBody\");\n\
        var respFrm = document.getElementById(\"respForm\");\n\
        var respList = respFrm.children;\n\
\n\
        if (respList.length <= 0) {\n\
          if (stopListen == true) {\n\
            stopHL7Listener();\n\
            isRespond = false;\n\
          }\n\
          showRespForm(respList.length);\n\
\n\
        } else {\n\
          for (var c = 0; c < respList.length; c++) {\n\
            respJSON.templates[c] = respList[c].id.slice(8);\n\
          }\n\
\n\
          showRespForm(respList.length);\n\
          postJSON(JSON.stringify(respJSON));\n\
          isRespond = true;\n\
          getRespQueue();\n\
\n\
        }\n\
      }\n\
\n\
      function delResponder(divID) {\n\
        var respFrm = document.getElementById(\"respForm\");\n\
        var delDiv = document.getElementById(divID);\n\
        respFrm.removeChild(delDiv);\n\
        procResponders(true);\n\
      }\n\
\n\
      function addResponder() {\n\
        if (!checkLPort()) return false;\n\
\n\
        var respFrm = document.getElementById(\"respForm\");\n\
        var respSel = document.getElementById(\"respSelect\");\n\
\n\
        if (respSel.value != \"None\") {\n\
          var divID = \"RESPFRM_\" + respSel.value;\n\
\n\
          if (!document.getElementById(divID)) {\n\
            var respDiv = document.createElement(\"div\");\n\
            respDiv.id = divID;\n\
\n\
            respDiv.innerHTML = respSel.options[respSel.selectedIndex].text + '<span class=\"respFormClose\"><a class=\"black\" href=\"\" onClick=\"delResponder(\\'' + divID + '\\'); return false;\">&#10006;</a></span>';\n\
\n\
            respDiv.className = \"respFormItem\";\n\
            respFrm.appendChild(respDiv);\n\
            procResponders(true);\n\
          }\n\
        }\n\
      }\n\
\n\
      function procQTimes() {\n\
        var tNow = Date.now();\n\
        var qTimes = document.querySelectorAll(\"#respQBody .rSendTime\");\n\
        var qTimesFmt = document.querySelectorAll(\"#respQBody .rSTFmt\");\n\
        var nextRes = 1000;\n\
\n\
        for (var t = 0; t < qTimes.length; t++) {\n\
          if (qTimes[t].innerText == \"\") {\n\
            qTimesFmt[t].innerText = \"ERROR: NULL send time\";\n\
            continue;\n\
          }\n\
\n\
          var sTime = qTimes[t].innerText * 1000;\n\
\n\
          if (tNow >= sTime) {\n\
            sDate = new Date(sTime);\n\
            qTimesFmt[t].innerText = sDate.toISOString().slice(0, -5);\n\
\n\
          } else if ((tNow) < sTime) {\n\
            var sDiff = sTime - tNow;\n\
            var nextRefresh = sDiff;\n\
            if (sDiff < 200) nextRefresh = 200;\n\
            if (sDiff > 1000) nextRefresh = 1000;\n\
            if (sDiff < nextRes) nextRes = nextRefresh;\n\
\n\
            var sTimer  = new Date(sDiff);\n\
            qTimesFmt[t].innerText = sTimer.toISOString().slice(11, 19);\n\
\n\
          } else if (tNow < sTime) {\n\
            qTimesFmt[t].innerText = \"00:00:00\";\n\
\n\
          } else {\n\
            qTimesFmt[t].innerText = \"ERROR: Invalid send time\";\n\
\n\
          }\n\
        }\n\
      }\n\
\n\
\n\
      /* Server communication functions */\n\
      function errHandler(resStr) {\n\
        var errHTML;\n\
        var errDivT = document.getElementById(\"errTxt\");\n\
        var errDivD = document.getElementById(\"errDim\");\n\
        var errDivI = document.getElementById(\"errImg\");\n\
\n\
        if (resStr.slice(0, 7) == \"ERROR: \") {\n\
          errDivI.style.backgroundImage = \"url(/images/error.png)\";\n\
          errHTML = '<span class=\"boldRed\">ERROR: </span>' + resStr.slice(7);\n\
          errDivT.innerHTML = errHTML;\n\
          errDivD.style.display = \"block\";\n\
          return 1;\n\
\n\
        } else if (resStr.slice(0, 6) == \"WARN: \") {\n\
          errDivI.style.backgroundImage = \"url(/images/warning.png)\";\n\
          errHTML = '<span class=\"boldYel\">WARN: </span>' + resStr.slice(6);\n\
          errDivT.innerHTML = errHTML;\n\
          errDivD.style.display = \"block\";\n\
          return 2;\n\
\n\
        } else {\n\
          return 0;\n\
        }\n\
      }\n\
\n\
      function postJSON(jsonObj) {\n\
        event.preventDefault();\n\
        var xhr = new XMLHttpRequest();\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                return xhr.responseText;\n\
              }\n\
\n\
            } else if (xhr.status == 409) {\n\
              if (xhr.responseText == \"RX\") {\n\
                stopHL7Listener();\n\
                errHandler(\"ERROR: Failed to start responder, port already in use?\");\n\
              }\n\
\n\
            } else if (xhr.status == 500) {\n\
              stopHL7Listener();\n\
              errHandler(\"ERROR: An unexpected back end error occurred.\");\n\
\n\
            } else {\n\
              stopHL7Listener();\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postJSON\");\n\
        xhr.timeout = ackTimeout + webTimeout;\n\
        var formData = new FormData();\n\
        formData.append(\"jsonPOST\", jsonObj);\n\
        xhr.send(formData);\n\
      }\n\
\n\
      function postCreds() {\n\
        event.preventDefault();\n\
        var xhr = new XMLHttpRequest();\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"LA\") {\n\
                  unlockWeb();\n\
\n\
                } else if (xhr.responseText == \"LC\") {\n\
                  errHandler(\"WARN: Account created but requires activation. Please wait for your system admin to enable your account\");\n\
                  return false;\n\
\n\
               } else if (xhr.responseText == \"LF\") {\n\
                  errHandler(\"WARN: Account creation failed, please try again\");\n\
                  return false;\n\
\n\
               } else {\n\
                  errHandler(\"ERROR: Login failed, please try again\");\n\
                  return false;\n\
                }\n\
              }\n\
\n\
            } else if (xhr.status === 400) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"DP\") {\n\
                  errHandler(\"ERROR: The backend received bad data from the client, please try again\");\n\
                  return false;\n\
\n\
                } else {\n\
                  errHandler(\"ERROR: Login failed, please try again\");\n\
                  return false;\n\
                }\n\
              }\n\
\n\
            } else if (xhr.status === 401) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"LD\") {\n\
                  errHandler(\"WARN: Account already exists but is locked or requires activation. Please contact your system admin to enable your account\");\n\
                  return false;\n\
\n\
                } else {\n\
                  errHandler(\"ERROR: Login failed, please try again\");\n\
                  return false;\n\
                }\n\
              }\n\
\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postCreds\");\n\
        xhr.timeout = webTimeout;\n\
\n\
        var formData = new FormData();\n\
\n\
        var pcaction = document.getElementById(\"register\").innerText;\n\
        if (pcaction == \"Register\") {\n\
          formData.append(\"pcaction\", 1);\n\
        } else if (pcaction == \"Login\") {\n\
          formData.append(\"pcaction\", 2);\n\
        } else {\n\
          formData.append(\"pcaction\", 0);\n\
        }\n\
\n\
        var uname = document.getElementById(\"uname\").value;\n\
        var pword = document.getElementById(\"pword\").value;\n\
        formData.append(\"uname\", uname);\n\
        formData.append(\"pword\", pword);\n\
\n\
        xhr.send(formData);\n\
      }\n\
\n\
      function postSendSets() {\n\
        event.preventDefault();\n\
        var xhr = new XMLHttpRequest();\n\
        var btn = document.getElementById(\"saveSendSets\");\n\
        var tHost = document.getElementById(\"tHost\");\n\
        var tHostText = document.getElementById(\"tHostText\");\n\
        var tHostSave = document.getElementById(\"tHostSave\");\n\
        var tPort = document.getElementById(\"tPort\");\n\
        var tPortSave = document.getElementById(\"tPortSave\");\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"OK\") {\n\
                  btn.classList.add(\"menuItemButtonGreen\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  var tHostSel = tHost.options[tHost.selectedIndex];\n\
                  tHostText.innerHTML = \"[\" + tHostSel.innerHTML + \"]\";\n\
                  tHostSave.value = tHost.value;\n\
                  tPortSave.value = tPort.value;\n\
                } else {\n\
                  btn.classList.add(\"menuItemButtonRed\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  tHost.value = tHostSave.value;\n\
                  tPort.value = tPortSave.value;\n\
                  errHandler(\"ERROR: The backend failed to save your settings, please try again.\");\n\
                }\n\
              }\n\
\n\
            } else if (xhr.status == 401) {\n\
              showLogin();\n\
\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postSendSets\");\n\
        xhr.timeout = webTimeout;\n\
\n\
        var formData = new FormData();\n\
\n\
        formData.append(\"sIP\", tHost.value);\n\
        formData.append(\"sPort\", tPort.value);\n\
\n\
        xhr.send(formData);\n\
      }\n\
\n\
      function postListSets() {\n\
        event.preventDefault();\n\
        var xhr = new XMLHttpRequest();\n\
        var btn = document.getElementById(\"saveListSets\");\n\
        var lPort = document.getElementById(\"lPort\");\n\
        var lPortSave = document.getElementById(\"lPortSave\");\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"OK\") {\n\
                  btn.classList.add(\"menuItemButtonGreen\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  lPortSave.value = lPort.value;\n\
\n\
                } else if (xhr.responseText == \"SR\") {\n\
                  btn.classList.add(\"menuItemButtonRed\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  lPort.value = lPortSave.value;\n\
                  errHandler(\"ERROR: Your chosen port number is being used by another user, please try again.\");\n\
\n\
                } else {\n\
                  btn.classList.add(\"menuItemButtonRed\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  lPort.value = lPortSave.value;\n\
                  errHandler(\"ERROR: The backend failed to save your settings, please try again.\");\n\
                }\n\
              }\n\
\n\
            } else if (xhr.status == 401) {\n\
              showLogin();\n\
\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postListSets\");\n\
        xhr.timeout = webTimeout;\n\
\n\
        var formData = new FormData();\n\
        formData.append(\"lPort\", lPort.value);\n\
\n\
        xhr.send(formData);\n\
      }\n\
\n\
      function postACKSets() {\n\
        event.preventDefault();\n\
        var xhr = new XMLHttpRequest();\n\
        var btn = document.getElementById(\"saveACKSets\");\n\
        var ackT = document.getElementById(\"ackTimeout\");\n\
        var ackTSave = document.getElementById(\"ackTimeoutSave\");\n\
        var webT = document.getElementById(\"webTimeout\");\n\
        var webTSave = document.getElementById(\"webTimeoutSave\");\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"OK\") {\n\
                  btn.classList.add(\"menuItemButtonGreen\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  ackTSave.value = ackT.value;\n\
                  webTSave.value = webT.value;\n\
                  ackTimeout = ackT.value;\n\
                  webTimeout = webT.value;\n\
\n\
                } else if (xhr.responseText == \"SR\") {\n\
                  btn.classList.add(\"menuItemButtonRed\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  ackT.value = ackTSave.value;\n\
                  webT.value = webTSave.value;\n\
                  errHandler(\"ERROR: Invalid timeout values submitted, valid ranges are 1-99 and 50-5000.\");\n\
\n\
                } else {\n\
                  btn.classList.add(\"menuItemButtonRed\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  ackT.value = ackTSave.value;\n\
                  webT.value = webTSave.value;\n\
                  errHandler(\"ERROR: The backend failed to save your settings, please try again.\");\n\
                }\n\
              }\n\
\n\
            } else if (xhr.status == 401) {\n\
              showLogin();\n\
\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postACKSets\");\n\
        xhr.timeout = webTimeout;\n\
\n\
        var formData = new FormData();\n\
        formData.append(\"ackTimeout\", ackT.value);\n\
        formData.append(\"webTimeout\", webT.value);\n\
\n\
        xhr.send(formData);\n\
      }\n\
\n\
      function postPwdSets() {\n\
        event.preventDefault();\n\
        var xhr = new XMLHttpRequest();\n\
        var btn = document.getElementById(\"resetPW\");\n\
        var oldPwd = document.getElementById(\"oldPwd\");\n\
        var newPwd = document.getElementById(\"newPwd\");\n\
        var conPwd = document.getElementById(\"conPwd\");\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"OK\") {\n\
                  btn.classList.add(\"menuItemButtonGreen\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  oldPwd.value = \"\";\n\
                  newPwd.value = \"\";\n\
                  conPwd.value = \"\";\n\
                } else {\n\
                  btn.classList.add(\"menuItemButtonRed\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  errHandler(\"ERROR: The backend failed to save your password, please try again.\");\n\
                }\n\
              }\n\
\n\
            } else if (xhr.status == 401) {\n\
              showLogin();\n\
\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postPwdSets\");\n\
        xhr.timeout = webTimeout;\n\
\n\
        var formData = new FormData();\n\
\n\
        formData.append(\"pcaction\", 3);\n\
        formData.append(\"pword\", oldPwd.value);\n\
        formData.append(\"npword\", newPwd.value);\n\
\n\
        xhr.send(formData);\n\
      }\n\
\n\
      async function checkAuth() {\n\
        try {\n\
          const response = await fetch(\"/getRespondList\");\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            webLocked = true;\n\
            showLogin();\n\
\n\
          } else if (response.ok) {\n\
            webLocked = false;\n\
            unlockWeb();\n\
\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      async function logout() {\n\
        try {\n\
          const response = await fetch(\"/logout\");\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.ok) {\n\
            showLogin();\n\
\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      async function getRespQueue() {\n\
        try {\n\
          const response = await fetch(\"/getRespQueue\");\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.ok) {\n\
            if (htmlData.length > 0 || htmlData === \"QE\") {\n\
              var respTable = document.getElementById(\"respQBody\");\n\
              var rCount = document.getElementById(\"rCount\");\n\
\n\
              if (htmlData === \"QE\") {\n\
                respTable.innerHTML = \"\";\n\
              } else {\n\
                jObj = JSON.parse(htmlData);\n\
                respTable.innerHTML = jObj.data;\n\
                if (jObj.count < (0.9 * jObj.max)) rCount.style.color = \"#000\";\n\
                if (jObj.count >= (0.9 * jObj.max)) rCount.style.color = \"#fa7d00\";\n\
                if (jObj.count >= jObj.max) rCount.style.color = \"#ff5151\";\n\
                rCount.innerText = jObj.count + \" / \" + jObj.max;\n\
              }\n\
            }\n\
\n\
            procQTimes();\n\
            if(isRespond == true) setTimeout(getRespQueue, 1000);\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      function postHL7() {\n\
        event.preventDefault();\n\
        var btn = document.getElementById(\"sendButton\");\n\
        btn.classList.remove(\"sendActive\");\n\
        btn.removeEventListener(\"click\", postHL7);\n\
\n\
        var xhr = new XMLHttpRequest();\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                popResps(xhr.responseText);\n\
              }\n\
\n\
            } else if (xhr.status == 401) {\n\
              showLogin();\n\
\n\
            } else if (xhr.status === 406) {\n\
              errHandler(\"ERROR: Message contains no data to send.\");\n\
\n\
            } else if (xhr.status === 409) {\n\
                if (xhr.responseText == \"CX\") {\n\
                  errHandler(\"ERROR: Backend server failed to connect to target server.\");\n\
\n\
                } else if (xhr.responseText == \"PX\") {\n\
                  errHandler(\"ERROR: Failed to send message from backend server to target server.\");\n\
\n\
                } else if (xhr.responseText == \"PT\") {\n\
                  errHandler(\"ERROR: Timeout awaiting ACK response from target server.\");\n\
\n\
                } else if (xhr.responseText == \"PU\") {\n\
                  errHandler(\"ERROR: Could not understand ACK response code from target server.\");\n\
\n\
                }\n\
\n\
            } else if (xhr.status === 413) {\n\
              errHandler(\"ERROR: Message is too large (>15kb).\");\n\
\n\
            } else if (xhr.status === 418) {\n\
              hl7Msg = document.getElementById(\"hl7Message\");\n\
              hl7Msg.innerHTML = \"<div>[418] Coffee is impossible, or at least, infinitely improbable, for I am a teapot.</div>\";\n\
              resizeResps();\n\
              respCont = document.getElementById(\"respContainer\");\n\
              var mResps = respCont.getElementsByTagName(\"DIV\");\n\
              mResps[0].classList.add(\"hl7MsgRespT\");\n\
              mResps[0].innerText = \"TP\";\n\
            } else if (xhr.status === 500) {\n\
              errHandler(\"ERROR: An unhandled backend server error occured.\");\n\
\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postHL7\");\n\
        xhr.timeout = (ackTimeout * 1000) + webTimeout;\n\
\n\
        var HL7Text = document.getElementById(\"hl7Message\").innerText;\n\
        var formData = new FormData();\n\
        formData.append(\"hl7MessageText\", HL7Text.replace(/\\n{2,}/g, \"\\n\"));\n\
        xhr.send(formData);\n\
      }\n\
\n\
      async function popTemplates(temptype) {\n\
        try {\n\
          if (temptype == 0) {\n\
            var turl = getTempURL();\n\
            tempPath = \"/getTemplateList\" + turl;\n\
          }\n\
          if (temptype == 1) tempPath = \"/getRespondList\";\n\
          const response = await fetch(tempPath);\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.ok) {\n\
            if (temptype == 0) {\n\
              var sel = document.getElementById(\"tempSelect1\");\n\
              var parent = document.getElementById(\"tempSelDiv\")\n\
              if (parent.childNodes.length > 3) var sel = parent.lastChild;\n\
            }\n\
            if (temptype == 1) var sel = document.getElementById(\"respSelect\");\n\
            sel.innerHTML = htmlData;\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      async function popServers(setIP) {\n\
        try {\n\
          const response = await fetch(\"/getServers\");\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.ok) {\n\
            var sIP = document.getElementById(\"tHost\");\n\
            var sHost = document.getElementById(\"tHost\");\n\
            var sHostText = document.getElementById(\"tHostText\");\n\
            sIP.innerHTML = \"\";\n\
\n\
            var sObj = JSON.parse(htmlData);\n\
            for (var i = 0; i < sObj.length; i++) {\n\
              if (sObj[i].address == setIP || sObj[i].hidden != true) {\n\
                var opt = document.createElement(\"option\");\n\
                opt.innerHTML = sObj[i].displayName;\n\
                opt.value = sObj[i].address;\n\
                if (sObj[i].address == setIP) {\n\
                  opt.selected = true;\n\
                  sHostText.innerHTML = \"[\" + sObj[i].displayName + \"]\";\n\
                }\n\
                sIP.appendChild(opt);\n\
              }\n\
            }\n\
            var sHostSel = sHost.options[sHost.selectedIndex];\n\
            sHostText.innerHTML = \"[\" + sHostSel.innerHTML + \"]\";\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      async function popSettings() {\n\
        try {\n\
          var sHostText = document.getElementById(\"tHostText\");\n\
          if (sHostText.innerHTML == \"\") var uPath = window.location.pathname;\n\
          const response = await fetch(\"/getSettings\" + uPath);\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.ok) {\n\
            jObj = JSON.parse(htmlData, function(key, value) {\n\
              return value;\n\
            });\n\
\n\
            var sHost = document.getElementById(\"tHost\");\n\
            var sHostText = document.getElementById(\"tHostText\");\n\
            var sHostSave = document.getElementById(\"tHostSave\");\n\
            var sPort = document.getElementById(\"tPort\");\n\
            var sPortSave = document.getElementById(\"tPortSave\");\n\
            var lPort = document.getElementById(\"lPort\");\n\
            var lPortSave = document.getElementById(\"lPortSave\");\n\
            var ackT = document.getElementById(\"ackTimeout\");\n\
            var ackTSave = document.getElementById(\"ackTimeoutSave\");\n\
            var webT = document.getElementById(\"webTimeout\");\n\
            var webTSave = document.getElementById(\"webTimeoutSave\");\n\
\n\
            popServers(jObj.sIP);\n\
            sHostSave.value = sHost.value;\n\
\n\
            if (jObj.sPort > 0) {\n\
              sPort.value = jObj.sPort;\n\
              sPortSave.value = jObj.sPort;\n\
              sPort.classList.remove(\"menuInputErr\");\n\
            } else {\n\
              sPort.value = \"\";\n\
              sPort.classList.add(\"menuInputErr\");\n\
            }\n\
\n\
            if (jObj.lPort > 0) {\n\
              lPort.value = jObj.lPort;\n\
              lPortSave.value = jObj.lPort;\n\
              lPort.classList.remove(\"menuInputErr\");\n\
            } else {\n\
              lPort.value = \"\";\n\
              lPort.classList.add(\"menuInputErr\");\n\
            }\n\
\n\
            ackT.value = jObj.ackT;\n\
            ackTSave.value = jObj.ackT;\n\
            webT.value = jObj.webT;\n\
            webTSave.value = jObj.webT;\n\
            ackTimeout = jObj.ackT;\n\
            webTimeout = jObj.webT;\n\
\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      async function popTempForm(thisSel) {\n\
        var turl = getTempURL();\n\
        var sel = document.getElementById(\"tempSelect1\");\n\
        var tName = turl + thisSel.value;\n\
\n\
        if (thisSel.value != \"None\") {\n\
          try {\n\
            const response = await fetch(\"/templates/\" + tName);\n\
            const htmlData = await response.text();\n\
\n\
            if (response.ok) {\n\
              if (htmlData == \"TX\") {\n\
                errHandler(\"ERROR: Failed to parse JSON template. Please contact your system administrator\");\n\
\n\
              } else {\n\
                jsonData = JSON.parse(htmlData);\n\
                formHTML = jsonData[\"form\"];\n\
                var sel = document.getElementById(\"tempForm\");\n\
                sel.innerHTML = formHTML;\n\
                formHTML = jsonData[\"hl7\"];\n\
                var sel = document.getElementById(\"hl7Message\");\n\
                sel.innerHTML = formHTML;\n\
                formHTML = jsonData[\"desc\"];\n\
                var sel = document.getElementById(\"tempDesc\");\n\
                sel.innerHTML = formHTML;\n\
                resizeResps();\n\
              }\n\
            }\n\
\n\
          } catch(error) {\n\
            console.log(error);\n\
            errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
          } \n\
\n\
        } else {\n\
          var sel = document.getElementById(\"hl7Message\");\n\
          sel.innerHTML = \"\";\n\
          resizeResps();\n\
        }\n\
      }\n\
\n\
      /* Functions to stop/start the HL7 listener */\n\
      async function stopBackendListen() {\n\
        try {\n\
          const response = await fetch(\"/stopListenHL7\");\n\
          const htmlData = await response.text();\n\
          if (response.ok) {\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      var evtSource = null;\n\
\n\
      function updateHL7Log(event) {\n\
        hl7Log = document.getElementById(\"hl7Log\");\n\
        if (event.data == \"FX\") {\n\
          stopHL7Listener();\n\
          errHandler(\"ERROR: Failed to start listener, port already in use?\");\n\
\n\
        } else if (event.data != \"HB\") {\n\
          hl7Log.innerHTML += event.data;\n\
          hl7Log.scrollTo(0, hl7Log.scrollHeight);\n\
        }\n\
      }\n\
\n\
      function startHL7Listener() {\n\
        if (!checkLPort()) return false;\n\
\n\
        try {\n\
          if (typeof(EventSource) !== \"undefined\") {\n\
            if (!isConnectionOpen) {\n\
              evtSource = new EventSource(\"/listenHL7\");\n\
              evtSource.addEventListener(\"rcvHL7\", updateHL7Log);\n\
              isConnectionOpen = true;\n\
            }\n\
          } else {\n\
            document.getElementById(\"hl7Log\").innerHTML = \"Sorry, your browser does not support server-sent events.\";\n\
          }\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      function stopHL7Listener() {\n\
        if (isConnectionOpen) {\n\
          evtSource.close();\n\
        }\n\
        stopBackendListen();\n\
        var respFrm = document.getElementById(\"respForm\");\n\
        respFrm.textContent = \"\";\n\
        isRespond = false;\n\
        procResponders(false);\n\
        isConnectionOpen = false;\n\
      }\n\
    </script>\n\
  </head>\n\
\n\
  <body onLoad=\"initPage();\">\n\
    <div id=\"headerBar\">\n\
      <img id=\"logo\" src=\"/images/logo.png\" />\n\
      <a href=\"\" onClick=\"showMenu(); return false;\">\n\
        <div id=\"person\"></div>\n\
      </a>\n\
    </div>\n\
\n\
    <div id=\"menuBar\">\n\
      <span class=\"menuOption\"><a href=\"\" onClick=\"showSend(); return false;\">SEND</a></span>\n\
      <span class=\"menuOption\"><a href=\"\" onClick=\"showList(); return false;\">LISTEN</a></span>\n\
      <span class=\"menuOption\"><a href=\"\" onClick=\"showResp(); return false;\">RESPOND</a></span>\n\
    </div>\n\
\n\
    <form id=\"postForm\" action=\"/postHL7\" method=\"post\" enctype=\"text/plain\" onSubmit=\"return false;\">\n\
      <div id=\"sendPane\">\n\
        <div class=\"titleBar\">\n\
          <div id=\"tempSelLabel\">Template:</div>\n\
          <div id=\"tempSelDiv\">\n\
            <select id=\"tempSelect1\" class=\"tempSelect\" onChange=\"updateTemps(this);\"></select>\n\
          </div>\n\
          <div id=\"tempSelHelp\"><a href=\"\" class=\"black\" onClick=\"showDesc(); return false;\">&quest;</a></div>\n\
        </div>\n\
\n\
        <div id=\"tempDesc\"></div>\n\
        <div id=\"tempForm\"></div>\n\
\n\
        <div class=\"titleBar\">HL7 Message(s):\n\
          <div id=\"tHostText\"></div>\n\
          <img id=\"sendButton\" src=\"/images/send.png\" title=\"Send HL7\"/>\n\
        </div>\n\
        <div id=\"contContainer\">\n\
          <div id=\"respContainer\"></div>\n\
          <div id=\"hl7Container\">\n\
            <div id=\"hl7Message\" class=\"hl7Message\" contenteditable=\"true\" onFocus=\"clrHl7Help();\" onInput=\"clrRes();\" onkeyup=\"updateResps(event);\" onpaste=\"updateResps();\">Paste/type HL7 message here or use a template above.</div>\n\
          </div>\n\
        </div>\n\
        <div id=\"tempDesc\"></div>\n\
      </div>\n\
    </form>\n\
\n\
    <div id=\"listPane\">\n\
      <div class=\"titleBar\">Listening...</div>\n\
      <div id=\"hl7Log\" class=\"hl7Message\"></div>\n\
    </div>\n\
\n\
    <div id=\"respPane\">\n\
      <div class=\"titleBar\">Active responders:\n\
        <select name=\"respSelect\" id=\"respSelect\" class=\"tempSelect\"></select>\n\
        <div id=\"respPlus\">\n\
          <a class=\"black\" href=\"\" onClick=\"addResponder(); return false;\">&#43;</a>\n\
        </div>\n\
      </div>\n\
      <div id=\"respForm\"></div>\n\
      <div class=\"titleBar\">Response queue:<div id=\"rCount\"></div></div>\n\
      <div id=\"hl7Queue\" class=\"hl7Message\">\n\
        <table id=\"respQTable\">\n\
          <thead>\n\
            <th class==\"thResp\">#</th>\n\
            <th class=\"thSendT\">Responder</th>\n\
            <th class=\"thSendT\">Send Template</th>\n\
            <th class=\"thDest\">Destination</th>\n\
            <th class=\"thSendTime\"></th>\n\
            <th class=\"thSTFmt\">Send Time</th>\n\
            <th class=\"thResp\">Response</th>\n\
          </thead>\n\
          <tbody id=\"respQBody\"></tbody>\n\
          <tfoot><tr><td class=\"foot\" colspan=\"6\">End of response queue</td></tr></tfoot>\n\
        </table>\n\
      </div>\n\
    </div>\n\
\n\
    <div id=\"footer\">&copy; Haydn Haines</div>\n\
\n\
    <!-- login window -->\n\
    <div id=\"loginDim\" class=\"popDim\">\n\
      <div id=\"loginWindow\" class=\"popWindow\">\n\
        <div id=\"loginTitle\" class=\"popTitle\">Login:</div>\n\
        <div id=\"loginMsg\" class=\"popMsg\">\n\
          <div id=\"loginImg\" class=\"popImg\"></div>\n\
          <div id=\"loginTxt\" class=\"popTxt\">\n\
            <form>\n\
            <div class=\"loginInfo\">\n\
              <div class=\"loginQ\">Username:</div>\n\
              <div class=\"loginFrm\">\n\
                <input name=\"uname\" id=\"uname\" class=\"loginInput\" onInput=\"validLogin(event);\" onChange=\"validLogin(event);\" />\n\
              </div>\n\
            </div>\n\
            <div class=\"loginInfo\">\n\
              <div class=\"loginQ\">Password:</div>\n\
              <div class=\"loginFrm\">\n\
                <input name=\"pword\" id=\"pword\" type=\"password\" autocomplete=\"current-password\" class=\"loginInput\" onInput=\"validLogin(event);\" onChange=\"validLogin(event);\" />\n\
              </div>\n\
            </div>\n\
            <div id=\"confPwordD\" class=\"loginInfo\">\n\
              <div class=\"loginQ\">Confirm PW:</div>\n\
              <div class=\"loginFrm\">\n\
                <input name=\"cpword\" id=\"confPword\" type=\"password\" autocomplete=\"new-password\" class=\"loginInput\" onInput=\"validLogin(event);\" onChange=\"validLogin(event);\" />\n\
              </div>\n\
            </div>\n\
            </form>\n\
          </div>\n\
        </div>\n\
        <div id=\"loginFooter\" class=\"popFooter\">\n\
          <button id=\"register\" class=\"button\" onclick=\"switchLogin(); return false;\">Register</button>\n\
          <button id=\"submit\" class=\"buttonInactive\">&#9654;&#9654;</button>\n\
        </div>\n\
      </div>\n\
    </div>\n\
\n\
    <!-- Menu window -->\n\
    <div id=\"menuDim\" class=\"popDim\">\n\
      <div id=\"menuWindow\">\n\
        <div id=\"menuTitle\" class=\"popTitle\">\n\
          <div class=\"menuTitleText\">Settings</div>\n\
          <div class=\"menuTitleTogl\">\n\
            <a href=\"\" onClick=\"hideMenu(); return false;\">&#10006;</a>\n\
          </div>\n\
        </div>\n\
        <div class=\"menuHeader\">Send Settings</div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Target Host:</div>\n\
          <div class=\"menuDataItem\">\n\
            <select id=\"tHost\" class=\"menuSelect\" onInput=\"validSendSets();\" onChange=\"validSendSets();\"></select>\n\
            <input type=\"hidden\" id=\"tHostSave\" name=\"hhl7tHostSave\" autocomplete=\"new-password\" />\n\
          </div>\n\
        </div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Target Port:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"tPort\" class=\"menuInput\" onInput=\"validSendSets();\" onChange=\"validSendSets();\" name=\"hhl7tPort\" autocomplete=\"new-password\" />\n\
            <input type=\"hidden\" id=\"tPortSave\" name=\"hhl7tPortSave\" autocomplete=\"new-password\" />\n\
          </div>\n\
        </div>\n\
        <button id=\"saveSendSets\" class=\"menuItemButtonInactive\">Save Send Settings</button>\n\
        <div class=\"menuSpacer\"></div>\n\
        <div class=\"menuHeader\">Listen Settings</div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Listen Port:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"lPort\" class=\"menuInput\" onInput=\"validListSets();\" onChange=\"validListSets();\" name=\"hhl7lPort\" autocomplete=\"new-password\" />\n\
            <input type=\"hidden\" id=\"lPortSave\" name=\"hhl7lPortSave\" autocomplete=\"new-password\" />\n\
          </div>\n\
        </div>\n\
        <button id=\"saveListSets\" class=\"menuItemButtonInactive\">Save Listen Settings</button>\n\
        <div class=\"menuSpacer\"></div>\n\
        <div class=\"menuHeader\">ACK Timeout Settings</div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">ACK (s):</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"ackTimeout\" class=\"menuInput\" onInput=\"validACKSets();\" onChange=\"validACKSets();\" name=\"hhl7ackTimeout\" autocomplete=\"new-password\" />\n\
            <input type=\"hidden\" id=\"ackTimeoutSave\" name=\"hhl7ackTimeoutSave\" autocomplete=\"new-password\" />\n\
          </div>\n\
        </div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Web (ms):</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"webTimeout\" class=\"menuInput\" onInput=\"validACKSets();\" onChange=\"validACKSets();\" name=\"hhl7webTimeout\" autocomplete=\"new-password\" />\n\
            <input type=\"hidden\" id=\"webTimeoutSave\" name=\"hhl7webTimeoutSave\" autocomplete=\"new-password\" />\n\
          </div>\n\
        </div>\n\
        <button id=\"saveACKSets\" class=\"menuItemButtonInactive\">Save Timeout Settings</button>\n\
        <div class=\"menuSpacer\"></div>\n\
        <div class=\"menuHeader\">Reset Password</div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Old Pwd:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"oldPwd\" class=\"menuInput\" type=\"password\" onInput=\"validPwdSets();\" onChange=\"validPwdSets();\" name=\"hhl7oldPwd\" autocomplete=\"new-password\" />\n\
          </div>\n\
        </div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">New Pwd:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"newPwd\" class=\"menuInput\" type=\"password\" onInput=\"validPwdSets();\" onChange=\"validPwdSets();\" name=\"hhl7newPwd\" autocomplete=\"new-password\" />\n\
          </div>\n\
        </div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Confirm Pwd:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"conPwd\" class=\"menuInput\" type=\"password\" onInput=\"validPwdSets();\" onChange=\"validPwdSets();\" name=\"hhl7conPwd\" autocomplete=\"new-password\" />\n\
          </div>\n\
        </div>\n\
        <button id=\"resetPW\" class=\"menuItemButtonInactive\">Reset Password</button>\n\
        <div class=\"menuSpacer\"></div>\n\
        <button class=\"menuItemButton\" onclick=\"logout();\">Logout</button>\n\
      </div>\n\
    </div>\n\
\n\
    <!-- Error message window -->\n\
    <div id=\"errDim\" class=\"popDim\">\n\
      <div id=\"errWindow\" class=\"popWindow\">\n\
        <div id=\"errTitle\" class=\"popTitle\">Error:</div>\n\
        <div id=\"errMsg\" class=\"popMsg\">\n\
          <div id=\"errImg\" class=\"popImg\"></div>\n\
          <div id=\"errTxt\" class=\"popTxt\"></div>\n\
        </div>\n\
        <div id=\"errFooter\" class=\"popFooter\">\n\
          <a href=\"\" onclick=\"closeErrWin(); return false;\">\n\
            <div id=\"ok\" class=\"button\">OK</div>\n\
          </a>\n\
        </div>\n\
      </div>\n\
    </div>\n\
  </body>\n\
</html>";


// This should never be shown...
const char *errorPage = "<html><body>ERROR</body></html>";
