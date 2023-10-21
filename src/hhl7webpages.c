/*
Copyright 2023 Haydn Haines.

This file is part of hhl7.

hhl7 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

hhl7 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with hhl7. If not, see <https://www.gnu.org/licenses/>. 
*/

// TODO - check all files for printf errors instead of frpintf

const char *mainPage = "<!DOCTYPE HTML>\n\
<html>\n\
  <head>\n\
    <title>HHL7</title>\n\
    <link rel=\"icon\" type=\"image/x-icon\" href=\"/images/favicon.ico\">\n\
    <style>\n\
      html {\n\
      }\n\
      body {\n\
        margin: 0px;\n\
      }\n\
      a {\n\
        text-decoration: none;\n\
        color: #fff;\n\
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
      /* PostForm Needed for height pass through */\n\
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
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        font-weight: bold;\n\
        color: #fff;\n\
        padding: 0px 15px;\n\
      }\n\
      .titleBar {\n\
        display: block;\n\
        height: 36px;\n\
        width: 100%;\n\
        line-height: 38px;\n\
        border-width: 2px 0px 2px 0px;\n\
        border-style: solid;\n\
        border-color: #8a93ae;\n\
        background-color: #d4dcf1;\n\
        text-align: left;\n\
        padding: 0px 15px;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        font-weight: bold;\n\
        color: #000;\n\
      }\n\
      #sendButton:hover {\n\
        content: url(\"/images/send_hl.png\");\n\
        cursor: pointer;\n\
      }\n\
      #sendButton {\n\
        position: absolute;\n\
        right: 60px;\n\
        margin-top: 4px;\n\
      }\n\
      #sendRes {\n\
        border-width: 1px;\n\
        border-style: solid;\n\
        border-color: #8a93ae;\n\
        box-shadow: inset 1px 1px 3px #999;\n\
        position: absolute;\n\
        right: 0px;\n\
        padding: 0px 0px 0px 2px;\n\
        font-size: 28px;\n\
        height: 36px;\n\
        line-height: 40px;\n\
        width: 50px;\n\
        display: inline-block;\n\
        text-align: center;\n\
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
      #tempForm {\n\
        display: flex;\n\
        flex-wrap: wrap;\n\
        justify-content: space-between;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        display: none;\n\
        background-color: #eeeff3;\n\
      }\n\
      .tempFormField {\n\
        flex: 0 33%;\n\
        height: 36px;\n\
        line-height: 40px;\n\
      }\n\
      .tempFormKey {\n\
        width: 36%;\n\
        float: left;\n\
        text-align: right;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        line-height: 38px;\n\
        font-weight: bold;\n\
        padding: 0px 10px 0px 0px;\n\
        background-color: #eeeff3;\n\
      }\n\
      .tempFormValue {\n\
        float: left;\n\
        height: 38px;\n\
        width: 60%;\n\
        background-color: #eeeff3;\n\
      }\n\
      .tempFormInput {\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        background-color: #fff;\n\
        border: 0px;\n\
        height: 26px;\n\
        width: 94%;\n\
        padding: 0px 10px 0px 10px;\n\
        outline: none;\n\
      }\n\
      select {\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        background-color: #d4dcf1;\n\
        border-width: 0px;\n\
        padding: 0px 5px 0px 50px;\n\
        outline: none;\n\
      }\n\
      #hl7Container {\n\
        flex: 1;\n\
      }\n\
      .hl7Message {\n\
        height: 100%;\n\
        font-family: \"Courier New\", Courier, monospace;\n\
        font-size: 18px;\n\
        padding: 10px 14px 10px 14px;\n\
        /* TODO - scroll bar not showing scrolling - height? */\n\
        overflow-y: scroll;\n\
        outline: none;\n\
      }\n\
      #hl7Log {\n\
        white-space: pre;\n\
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
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
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
        display: none;\n\
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
        width: 25%;\n\
        height: 20%;\n\
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
        border-width: 2px;\n\
        border-style: solid;\n\
        border-color: #142248;\n\
        outline: 1px solid #fff;\n\
      }\n\
      .popTitle {\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
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
      }\n\
      .popTxt {\n\
        flex: 1;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
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
        font-size: 26px;\n\
        padding-top: 2px;\n\
        margin-right: -8px;\n\
        margin-bottom: -2px;\n\
        text-align: right;\n\
      }\n\
      .menuHeader {\n\
        box-sizing: border-box;\n\
        display: inline-block;\n\
        width: 100%;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
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
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        font-weight: bold;\n\
        color: #fff;\n\
        height: 36px;\n\
        line-height: 36px;\n\
        padding: 0px 15px;\n\
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
        height: 0px;\n\
        background-color: #fff;\n\
      }\n\
      .menuSubHeader {\n\
        display: inline-block;\n\
        width: 23%;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        font-weight: bold;\n\
        background-color: #d4dcf1;\n\
        color: #000;\n\
        height: 36px;\n\
        line-height: 36px;\n\
        padding: 0px 15px;\n\
      }\n\
      .menuDataItem {\n\
        display: inline-block;\n\
        //width: 60%;\n\
        flex-grow: 4;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        font-weight: bold;\n\
        background-color: #fff;\n\
        color: #000;\n\
        height: 36px;\n\
        line-height: 36px;\n\
      }\n\
      .menuInput {\n\
        box-sizing: border-box;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        height: 100%;\n\
        width: 100%;\n\
        padding: 0px 15px;\n\
        border: none;\n\
        outline: none;\n\
      }\n\
      .menuSelect {\n\
        box-sizing: border-box;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        height: 100%;\n\
        width: 100%;\n\
        padding: 0px 15px;\n\
        border: none;\n\
        outline: none;\n\
        background-color: #fff;\n\
      }\n\
      .menuInputErr {\n\
        background-color: #f2c7c7;\n\
      }\n\
      .menuInput:focus {\n\
        background-color: #fff8d6;\n\
      }\n\
      .loginInfo {\n\
        padding: 0px 0px 2px 0px;\n\
      }\n\
      #confPwordD {\n\
        display: none;\n\
      }\n\
      .loginQ {\n\
        float: left;\n\
        height: 20px;\n\
        line-height: 28px;\n\
        width: 30%;\n\
      }\n\
      .logFrm {\n\
        float: left;\n\
        width: 70%;\n\
      }\n\
      .button {\n\
        position: absolute;\n\
        text-align: center;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
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
        line-height: 28px;\n\
      }\n\
      #submit {\n\
        position: absolute;\n\
        right: 15px;\n\
        bottom: 15px;\n\
        height: 32px;\n\
        width: 52px;\n\
        //line-height: 26px;\n\
        padding: 0px 0px 0px 3px;\n\
      }\n\
    </style>\n\
\n\
    <script>\n\
      function showLogin() {\n\
        hideMenu();\n\
        var login = document.getElementById(\"loginDim\");\n\
        login.style.display = \"block\";\n\
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
      function validLogin() {\n\
        var reg = document.getElementById(\"register\").innerText;\n\
        var usr = document.getElementById(\"uname\").value;\n\
        var pwd = document.getElementById(\"pword\").value;\n\
        var cpw = document.getElementById(\"confPword\").value;\n\
        var sub = document.getElementById(\"submit\");\n\
\n\
        sub.removeEventListener(\"click\", logPostFunc);\n\
\n\
        /* TODO change min username & pw length to config file? */\n\
        if (usr.length < 5 || pwd.length < 8) {\n\
          sub.classList.replace(\"button\", \"buttonInactive\");\n\
          return false;\n\
        }\n\
        if (reg == \"Login\") {\n\
          if (cpw.length < 8) {\n\
            sub.classList.replace(\"button\", \"buttonInactive\");\n\
            return false;\n\
          }\n\
          if (pwd != cpw) {\n\
            sub.classList.replace(\"button\", \"buttonInactive\");\n\
            return false;\n\
          }\n\
        }\n\
\n\
        sub.addEventListener(\"click\", logPostFunc);\n\
        sub.classList.replace(\"buttonInactive\", \"button\");\n\
      }\n\
\n\
      function showMenu() {\n\
        var pane = document.getElementById(\"menuDim\");\n\
        pane.style.display = \"block\";\n\
      }\n\
\n\
      var sendPostFunc = function(event) { postSendSets(event); };\n\
      function validSendSets() {\n\
        //sIPObj = document.getElementById(\"tHost\");\n\
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
      function showList() {\n\
        var pane = document.getElementById(\"sendPane\");\n\
        pane.style.display = \"none\";\n\
        var pane = document.getElementById(\"listPane\");\n\
        pane.style.display = \"flex\";\n\
        startHL7Listener();\n\
      }\n\
\n\
      function showSend() {\n\
        stopHL7Listener();\n\
        var pane = document.getElementById(\"listPane\");\n\
        pane.style.display = \"none\";\n\
        var pane = document.getElementById(\"sendPane\");\n\
        pane.style.display = \"flex\";\n\
      }\n\
\n\
      function clrHl7Help() {\n\
        var txt = document.getElementById(\"hl7Message\");\n\
        if (txt.innerText == \"Paste/type HL7 message here or use a template above.\") {\n\
          txt.innerText = \"\";\n\
        }\n\
      }\n\
\n\
      function showTemp() {\n\
        var sel = document.getElementById(\"tempSelect\");\n\
        var tForm = document.getElementById(\"tempForm\");\n\
        if (sel.value == \"None\") {\n\
          tForm.style.display = \"none\";\n\
        } else {\n\
          popTempForm();\n\
          tForm.style.display = \"flex\";\n\
        }\n\
      }\n\
\n\
      function clrRes() {\n\
        var res = document.getElementById(\"sendRes\");\n\
        if (res.innerHTML != \"--\") {\n\
          res.innerHTML = \"--\";\n\
          res.style.backgroundColor = \"#d4dcf1\";\n\
        }\n\
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
        popTemplates();\n\
      }\n\
\n\
      function unlockWeb() {\n\
        var errWin = document.getElementById(\"loginDim\");\n\
        errWin.style.display = \"none\";\n\
\n\
        /* Try to load the template list again */\n\
        popSettings();\n\
        popTemplates();\n\
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
                } else if (xhr.responseText == \"LD\") {\n\
                  errHandler(\"WARN: Account already exists but requires activation. Please wait for your system admin to enable your account\");\n\
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
            } else if (xhr.status === 401) {\n\
              errHandler(\"ERROR: Login failed, please try again\");\n\
              return false;\n\
\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postCreds\");\n\
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
        //return false;\n\
      }\n\
\n\
      function postSendSets() {\n\
        event.preventDefault();\n\
        var xhr = new XMLHttpRequest();\n\
        var btn = document.getElementById(\"saveSendSets\");\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"OK\") {\n\
                  btn.classList.add(\"menuItemButtonGreen\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                } else {\n\
                  btn.classList.add(\"menuItemButtonRed\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  errHandler(\"ERROR: The backend failed to save your settings, please try again.\");\n\
                }\n\
              }\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postSendSets\");\n\
\n\
        var formData = new FormData();\n\
\n\
        var tHost = document.getElementById(\"tHost\").value;\n\
        formData.append(\"sIP\", tHost);\n\
        var tPort = document.getElementById(\"tPort\").value;\n\
        formData.append(\"sPort\", tPort);\n\
\n\
        xhr.send(formData);\n\
      }\n\
\n\
      function postListSets() {\n\
        event.preventDefault();\n\
        var xhr = new XMLHttpRequest();\n\
        var btn = document.getElementById(\"saveListSets\");\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                if (xhr.responseText == \"OK\") {\n\
                  btn.classList.add(\"menuItemButtonGreen\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                } else {\n\
                  btn.classList.add(\"menuItemButtonRed\");\n\
                  btn.classList.replace(\"menuItemButton\", \"menuItemButtonInactive\");\n\
                  errHandler(\"ERROR: The backend failed to save your settings, please try again.\");\n\
                }\n\
              }\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postListSets\");\n\
\n\
        var formData = new FormData();\n\
\n\
        var lPort = document.getElementById(\"lPort\").value;\n\
        formData.append(\"lPort\", lPort);\n\
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
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postPwdSets\");\n\
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
      async function logout() {\n\
        try {\n\
          const response = await fetch(\"/logout\");\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.status == 500) {\n\
            // TODO internal server error\n\
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
      function postHL7() {\n\
        event.preventDefault();\n\
        var res = document.getElementById(\"sendRes\");\n\
        res.innerHTML = \"--\";\n\
        res.style.backgroundColor = \"#d4dcf1\";\n\
        var xhr = new XMLHttpRequest();\n\
\n\
        xhr.onreadystatechange = function() {\n\
          if (xhr.readyState === 4) {\n\
            if (xhr.status === 200) {\n\
              if (errHandler(xhr.responseText) == 0) {\n\
                res.innerHTML = xhr.responseText;\n\
                if (xhr.responseText == \"AA\" || xhr.responseText == \"CA\") {\n\
                  res.style.backgroundColor = \"#a9dfad\";\n\
                } else {\n\
                  res.style.backgroundColor = \"#ff9191\";\n\
                }\n\
              }\n\
            } else {\n\
              errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
            }\n\
          }\n\
        };\n\
\n\
        xhr.open(\"POST\", \"/postHL7\");\n\
\n\
        var HL7Text = document.getElementById(\"hl7Message\").innerText;\n\
        var formData = new FormData();\n\
        formData.append(\"hl7MessageText\", HL7Text);\n\
        xhr.send(formData);\n\
        // TODO: Add error checking for missing values here (or disable the button until form complete\n\
      }\n\
\n\
      async function popTemplates() {\n\
        try {\n\
          const response = await fetch(\"/getTemplateList\");\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.status == 500) {\n\
            // TODO internal server error\n\
\n\
          } else if (response.ok) {\n\
            var sel = document.getElementById(\"tempSelect\");\n\
            sel.innerHTML = htmlData;\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      async function popServers() {\n\
        try {\n\
          const response = await fetch(\"/getServers\");\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.status == 500) {\n\
            // TODO internal server error\n\
\n\
          } else if (response.ok) {\n\
            var sIP = document.getElementById(\"tHost\");\n\
            sIP.innerHTML = \"\";\n\
            var sObj = JSON.parse(htmlData);\n\
            for (var i = 0; i < sObj.length; i++) {\n\
              var opt = document.createElement(\"option\");\n\
              opt.innerHTML = sObj[i].displayName;\n\
              opt.value = sObj[i].address;\n\
              sIP.appendChild(opt);\n\
\n\
            }\n\
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
          const response = await fetch(\"/getSettings\");\n\
          const htmlData = await response.text();\n\
\n\
          if (response.status == 401) {\n\
            showLogin();\n\
\n\
          } else if (response.status == 500) {\n\
            // TODO internal server error\n\
\n\
          } else if (response.ok) {\n\
            jObj = JSON.parse(htmlData, function(key, value) {\n\
              return value;\n\
            });\n\
\n\
            popServers();\n\
\n\
            var sPort = document.getElementById(\"tPort\");\n\
            var lPort = document.getElementById(\"lPort\");\n\
\n\
            /* if (jObj.sIP) {\n\
              sIP.value = jObj.sIP;\n\
              sIP.classList.remove(\"menuInputErr\");\n\
            } else {\n\
              sIP.value = \"\";\n\
              sIP.classList.add(\"menuInputErr\");\n\
            } */\n\
\n\
            if (jObj.sPort > 0) {\n\
              sPort.value = jObj.sPort;\n\
              sPort.classList.remove(\"menuInputErr\");\n\
            } else {\n\
              sPort.value = \"\";\n\
              sPort.classList.add(\"menuInputErr\");\n\
            }\n\
\n\
            if (jObj.lPort > 0) {\n\
              lPort.value = jObj.lPort;\n\
              lPort.classList.remove(\"menuInputErr\");\n\
            } else {\n\
              lPort.value = \"\";\n\
              lPort.classList.add(\"menuInputErr\");\n\
            }\n\
          }\n\
\n\
        } catch(error) {\n\
          console.log(error);\n\
          errHandler(\"ERROR: The hhl7 backend is not running.\");\n\
        }\n\
      }\n\
\n\
      async function popTempForm() {\n\
        var sel = document.getElementById(\"tempSelect\");\n\
        var tName = sel.value;\n\
\n\
        if (tName != \"None\") {\n\
          try {\n\
            const response = await fetch(\"/templates/\" + tName);\n\
            const htmlData = await response.text();\n\
\n\
            if (response.ok) {\n\
              jsonData = JSON.parse(htmlData);\n\
              formHTML = jsonData[\"form\"];\n\
              var sel = document.getElementById(\"tempForm\");\n\
              sel.innerHTML = formHTML;\n\
              formHTML = jsonData[\"hl7\"];\n\
              var sel = document.getElementById(\"hl7Message\");\n\
              sel.innerHTML = formHTML;\n\
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
      var isConnectionOpen = false;\n\
\n\
      function updateHL7Log(event) {\n\
        hl7Log = document.getElementById(\"hl7Log\");\n\
        if (event.data != \"HB\") {\n\
          hl7Log.innerHTML += event.data;\n\
          hl7Log.scrollTo(0, hl7Log.scrollHeight);\n\
        }\n\
      }\n\
\n\
      function startHL7Listener() {\n\
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
      /* TODO - listen starts again after closing if no msgs sent */\n\
      function stopHL7Listener() {\n\
        if (isConnectionOpen) {\n\
          evtSource.close();\n\
          stopBackendListen();\n\
          isConnectionOpen = false;\n\
        }\n\
      }\n\
    </script>\n\
  </head>\n\
\n\
  <body onLoad=\"initPage();\">\n\
    <div id=\"headerBar\">\n\
      <img id=\"logo\" src=\"./images/logo.png\" />\n\
      <a href=\"\" onClick=\"showMenu(); return false;\">\n\
        <!-- <img id=\"person\" src=\"./images/person.png\" /> -->\n\
        <div id=\"person\"></div>\n\
      </a>\n\
    </div>\n\
\n\
    <div id=\"menuBar\">\n\
      <span class=\"menuOption\"><a href=\"\" onClick=\"showSend(); return false;\">SEND</a></span>\n\
      <span class=\"menuOption\"><a href=\"\" onClick=\"showList(); return false;\">LISTEN</a></span>\n\
    </div>\n\
\n\
    // TODO - how much for info is needed? I think just <form> to allow conten editable?\n\
    <form id=\"postForm\" action=\"/postHL7\" method=\"post\" enctype=\"text/plain\" onSubmit=\"return false;\">\n\
      <div id=\"sendPane\">\n\
        <div class=\"titleBar\">Template:\n\
          <select name=\"tempSelect\" id=\"tempSelect\" onChange=\"showTemp();\">\n\
          </select>\n\
        </div>\n\
\n\
        <div id=\"tempForm\">\n\
        </div>\n\
\n\
        <div class=\"titleBar\">HL7 Message:\n\
          <img id=\"sendButton\" src=\"./images/send.png\" title=\"Send HL7\" onClick=\"postHL7();\" />\n\
          <div id=\"sendRes\">--</div>\n\
        </div>\n\
        <div id=\"hl7Container\">\n\
          <div id=\"hl7Message\" class=\"hl7Message\" contenteditable=\"true\" onFocus=\"clrHl7Help();\" onInput=\"clrRes();\">Paste/type HL7 message here or use a template above.</div>\n\
        </div>\n\
      </div>\n\
    </form>\n\
\n\
    <div id=\"listPane\">\n\
      <div class=\"titleBar\">Listening...</div>\n\
      <div id=\"hl7Log\" class=\"hl7Message\"></div>\n\
    </div>\n\
    <div id=\"footer\">Haydn Haines</div>\n\
\n\
    <!-- login window -->\n\
    <div id=\"loginDim\" class=\"popDim\">\n\
      <div id=\"loginWindow\" class=\"popWindow\">\n\
        <div id=\"loginTitle\" class=\"popTitle\">Login:</div>\n\
        <div id=\"loginMsg\" class=\"popMsg\">\n\
          <div id=\"loginImg\" class=\"popImg\"></div>\n\
          <div id=\"loginTxt\" class=\"popTxt\">\n\
            <div class=\"loginInfo\">\n\
              <div class=\"loginQ\">Username:</div>\n\
              <div class=\"loginFrm\">\n\
                <input id=\"uname\" class=\"loginInput\" onInput=\"validLogin();\" onChange=\"validLogin();\" />\n\
              </div>\n\
            </div>\n\
            <div class=\"loginInfo\">\n\
              <div class=\"loginQ\">Password:</div>\n\
              <div class=\"loginFrm\">\n\
                <input id=\"pword\" type=\"password\" class=\"loginInput\" onInput=\"validLogin();\" onChange=\"validLogin();\" />\n\
              </div>\n\
            </div>\n\
            <div id=\"confPwordD\" class=\"loginInfo\">\n\
              <div class=\"loginQ\">Confirm PW:</div>\n\
              <div class=\"loginFrm\">\n\
                <input id=\"confPword\" type=\"password\" autocomplete=\"off\" class=\"loginInput\" onInput=\"validLogin();\" onChange=\"validLogin();\" />\n\
              </div>\n\
            </div>\n\
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
          </div>\n\
        </div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Target Port:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"tPort\" class=\"menuInput\" onInput=\"validSendSets();\" onChange=\"validSendSets();\" />\n\
          </div>\n\
        </div>\n\
        <button id=\"saveSendSets\" class=\"menuItemButtonInactive\">Save Send Settings</button>\n\
        <div class=\"menuSpacer\"></div>\n\
        <div class=\"menuHeader\">Listen Settings</div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Listen Port:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"lPort\" class=\"menuInput\" onInput=\"validListSets();\" onChange=\"validListSets();\" />\n\
          </div>\n\
        </div>\n\
        <button id=\"saveListSets\" class=\"menuItemButtonInactive\">Save Listen Settings</button>\n\
        <div class=\"menuSpacer\"></div>\n\
        <div class=\"menuHeader\">Reset Password</div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Old Pwd:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"oldPwd\" class=\"menuInput\" type=\"password\" onInput=\"validPwdSets();\" onChange=\"validPwdSets();\" />\n\
          </div>\n\
        </div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">New Pwd:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"newPwd\" class=\"menuInput\" type=\"password\" autocomplete=\"off\" onInput=\"validPwdSets();\" onChange=\"validPwdSets();\" />\n\
          </div>\n\
        </div>\n\
        <div class=\"menuItem\">\n\
          <div class=\"menuSubHeader\">Confirm Pwd:</div>\n\
          <div class=\"menuDataItem\">\n\
            <input id=\"conPwd\" class=\"menuInput\" type=\"password\" autocomplete=\"off\" onInput=\"validPwdSets();\" onChange=\"validPwdSets();\" />\n\
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
