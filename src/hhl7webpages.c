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
        top: 15px;\n\
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
      #errDim {\n\
        display: none;\n\
        position: fixed;\n\
        left: 0;\n\
        top: 0;\n\
        height: 100%;\n\
        width: 100%;\n\
        background-color: rgba(0, 0, 0, 0.5);\n\
      }\n\
      #errWindow {\n\
        position: absolute;\n\
        left: 50%;\n\
        top: 50%;\n\
        transform: translate(-50%, -50%);\n\
        width: 25%;\n\
        height: 20%;\n\
        border-width: 2px;\n\
        border-style: solid;\n\
        border-color: #142248;\n\
      }\n\
      #errTitle {\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        font-weight: bold;\n\
        background-color: #142248;\n\
        color: #fff;\n\
        height: 36px;\n\
        line-height: 36px;\n\
        padding: 0px 15px;\n\
      }\n\
      #errMsg {\n\
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
      #errImg {\n\
        height: 70px;\n\
        width: 70px;\n\
        background-image: url(/images/error.png);\n\
        background-repeat: no-repeat;\n\
        background-position: center center;\n\
      }\n\
      #errTxt {\n\
        flex: 1;\n\
        font-family: Verdana, Helvetica, sans-serif;\n\
        font-size: 16px;\n\
        padding: 0px 25px 0px 15px;\n\
        text-align: justify;\n\
        text-justify: inter-word;\n\
      }\n\
      #errFooter {\n\
        position: absolute;\n\
        bottom: 0px;\n\
        left: 0px;\n\
        right: 0px;\n\
        height: 41px;\n\
        background-color: #fff;\n\
      }\n\
      .okButton {\n\
        position: absolute;\n\
        right: 15px;\n\
        bottom: 15px;\n\
        height: 26px;\n\
        width: 45px;\n\
        line-height: 28px;\n\
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
    </style>\n\
\n\
    <script>\n\
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
      function postHL7() {\n\
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
          if (response.ok) {\n\
            var sel = document.getElementById(\"tempSelect\");\n\
            sel.innerHTML = htmlData;\n\
\n\
            /* We also need to preload error handling images in case we need them */\n\
            var eImg = new Image();\n\
            eImg.src = \"/images/error.png\";\n\
            var wImg = new Image();\n\
            wImg.src = \"/images/warning.png\";\n\
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
            // alert(\"Stopped Listening\");\n\
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
  <body onLoad=\"popTemplates();\">\n\
    <div id=\"headerBar\">\n\
      <img id=\"logo\" src=\"./images/logo.png\" />\n\
      <img id=\"person\" src=\"./images/person.png\" />\n\
    </div>\n\
\n\
    <div id=\"menuBar\">\n\
      <span class=\"menuOption\"><a href=\"\" onClick=\"showSend(); return false;\">SEND</a></span>\n\
      <span class=\"menuOption\"><a href=\"\" onClick=\"showList(); return false;\">LISTEN</a></span>\n\
    </div>\n\
\n\
    <form id=\"postForm\" action=\"/postHL7\" method=\"post\" enctype=\"text/plain\">\n\
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
    <div id=\"errDim\">\n\
      <div id=\"errWindow\">\n\
        <div id=\"errTitle\">Error:</div>\n\
        <div id=\"errMsg\">\n\
          <div id=\"errImg\"></div>\n\
          <div id=\"errTxt\"></div>\n\
        </div>\n\
        <div id=\"errFooter\">\n\
          <div class=\"okButton\">\n\
            <a href=\"\" onclick=\"closeErrWin(); return false;\">OK</a>\n\
          </div>\n\
        </div>\n\
      </div>\n\
    </div>\n\
  </body>\n\
</html>";


const char *errorPage = "<html><body>ERROR</body></html>";
