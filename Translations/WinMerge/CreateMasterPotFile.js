////
// This script creates the master POT file (English.pot).
//
// Copyright (C) 2007-2009 by Tim Gerundt
// Released under the "GNU General Public License"

var ForReading = 1;

var NO_BLOCK = 0;
var MENU_BLOCK = 1;
var DIALOGEX_BLOCK = 2;
var STRINGTABLE_BLOCK = 3;
var VERSIONINFO_BLOCK = 4;
var ACCELERATORS_BLOCK = 5;

var PATH_ENGLISH_POT = "English.pot";
var PATH_MERGE_RC = "../../Src/Merge.rc";
var PATH_PLUGIN_STRINGS_RC = "../../Plugins/Strings.rc";
var PATH_SRC_FOLDER = "../../Src";

var oFSO = new ActiveXObject("Scripting.FileSystemObject");

var bRunFromCmd = false;
if (oFSO.GetFileName(WScript.FullName).toLowerCase() === "cscript.exe") {
  bRunFromCmd = true;
}
var bInsertLineNumbers = true;
if (WScript.Arguments.Named.Exists("InsertLineNumbers")) {
  bInsertLineNumbers = (WScript.Arguments.Named("InsertLineNumbers").toLowerCase() === "true");
}

Main();

////
// ...
function Main() {
  var StartTime = new Date().getTime();
  
  InfoBox("Creating POT file from Merge.rc...", 3);
  
  var bNecessary = true;
  if (oFSO.FileExists(PATH_ENGLISH_POT)) { //If the POT file exists...
    bNecessary = GetArchiveBit(PATH_MERGE_RC) || GetArchiveBit(PATH_ENGLISH_POT); //RCs or POT file changed?
  }
  
  if (bNecessary) { //If update necessary...
    // Merge20 (W1): also scan the C++ sources for _()/N_()/NC_() strings.
    // The RC files alone do not cover strings that only exist in code, so
    // those translations were deleted from every shipped .po by the PO
    // updater (111 Merge20 msgids in the 2026-11-25 build). RC entries win
    // on a key conflict (they carry the resource ID as the #. comment).
    var oStrings = MergeDictionaries(
      MergeDictionaries(
        GetStringsFromRcFile(PATH_MERGE_RC),
        GetStringsFromRcFile(PATH_PLUGIN_STRINGS_RC)),
      GetStringsFromCppFiles(PATH_SRC_FOLDER));
    CreateMasterPotFile(PATH_ENGLISH_POT, oStrings);
    SetArchiveBit(PATH_MERGE_RC, false);
    SetArchiveBit(PATH_PLUGIN_STRINGS_RC, false);
    SetArchiveBit(PATH_ENGLISH_POT, false);
    for (var it = new Enumerator(oFSO.GetFolder(".").Files); !it.atEnd(); it.moveNext()) { //For all files in the current folder...
      var oFile = it.item();
      if (oFSO.GetExtensionName(oFile.Name).toLowerCase() === "po") { //If a PO file...
        SetArchiveBit(oFile.Path, true);
      }
    }
    
    var EndTime = new Date().getTime();
    var Seconds = (EndTime - StartTime) / 1000.0;
    
    InfoBox("POT file created, after " + Seconds + " second(s).", 10);
  } else { //If update NOT necessary...
    InfoBox("POT file already up-to-date.", 10);
  }
}

////
// ...
function MergeDictionaries(dict1, dict2) {
  var mergedDict = {};
  for (var key in dict1) {
    mergedDict[key] = dict1[key];
  }

  for (var key in dict2) {
    if (!(key in mergedDict)) {
      mergedDict[key] = dict2[key];
    }
  }
  return mergedDict;
}

////
// ...
function GetStringsFromRcFile(sRcFilePath) {
  var fContinuation = false;
  var oBlacklist = GetStringBlacklist("StringBlacklist.txt");
  var oStrings = {};
  if (oFSO.FileExists(sRcFilePath)) { //If the RC file exists...
    var sRcFileName = oFSO.GetFileName(sRcFilePath);
    var iLine = 0;
    var iBlockType = NO_BLOCK;
    var sParentContextID = "";
    var oRcFile = oFSO.OpenTextFile(sRcFilePath, ForReading);
    while (!oRcFile.AtEndOfStream) { //For all lines...
      var sLine = oRcFile.ReadLine().replace(/^\s+|\s+$/g, "");
      iLine++;
      
      var sReference = sRcFileName + ":" + iLine;
      var sString = "";
      var sComment = "";
      var sContext = "";
      
      if (fContinuation) {
        // Nothing to do
      } else if (sLine.indexOf(" MENU") >= 0 && sLine.indexOf("IDR_") >= 0) { //MENU...
        iBlockType = MENU_BLOCK;
        var m = /^\s*(IDR_[A-Za-z0-9_]*)/.exec(sLine);
        if (m) sParentContextID = m[1];
      } else if (sLine.indexOf(" DIALOGEX") >= 0) { //DIALOGEX...
        iBlockType = DIALOGEX_BLOCK;
        var m = /^\s*(IDD_[A-Za-z0-9_]*)/.exec(sLine);
        if (m) sParentContextID = m[1];
      } else if (sLine === "STRINGTABLE") { //STRINGTABLE...
        iBlockType = STRINGTABLE_BLOCK;
        sParentContextID = "";
      } else if (sLine.indexOf(" VERSIONINFO") >= 0) { //VERSIONINFO...
        iBlockType = VERSIONINFO_BLOCK;
      } else if (sLine.indexOf(" ACCELERATORS") >= 0) { //ACCELERATORS...
        iBlockType = ACCELERATORS_BLOCK;
      } else if (sLine === "BEGIN") { //BEGIN...
        //IGNORE FOR SPEEDUP!
      } else if (sLine === "END") { //END...
        if (iBlockType === STRINGTABLE_BLOCK) { //If inside stringtable...
          iBlockType = NO_BLOCK;
        }
      } else if (sLine.substring(0, 2) === "//") { //If comment line...
        sLine = "";
        //IGNORE FOR SPEEDUP!
      } else if (sLine !== "") { //If NOT empty line...
        switch (iBlockType) {
          case NO_BLOCK:
            var oMatch0 = /LANGUAGE (LANG_\w*, SUBLANG_\w*)/.exec(sLine);
            if (oMatch0) { //LANGUAGE...
              sString = oMatch0[1];
              sComment = "LANGUAGE, SUBLANGUAGE";
            }
            break;

          case MENU_BLOCK:
          case DIALOGEX_BLOCK:
          case STRINGTABLE_BLOCK:
            if (sLine.indexOf("\"") >= 0) { //If quote found (for speedup)...
              //--------------------------------------------------------------------------------
              // Replace 1st string literal only - 2nd string literal specifies control class!
              //--------------------------------------------------------------------------------
              var oMatch1 = /NC_\s*\("([^"]*)"\s*,\s*"([^"]*)"\s*\)/.exec(sLine);
              var oMatch2 = /"((?:""|[^"])*)"/.exec(sLine);
              var sTemp;
              if (oMatch1) { //String...
                sContext = oMatch1[1].replace(/^\s+|\s+$/g, "");
                sTemp = oMatch1[2];
              } else if (oMatch2) { //String...
                sTemp = oMatch2[1];
              } else {
                sTemp = "";
              }
              if (sTemp !== "" && !(sTemp in oBlacklist)) { //If NOT blacklisted...
                sString = sTemp.replace(/\"\"/g, "\\\"");
                var oMatch3 = /\/\/#\. (.*?)$/.exec(sLine);
                var oMatch4 = /\/\/msgctxt (.*?)$/.exec(sLine);
                if (oMatch3) { //If found a comment for the translators...
                  sComment = oMatch3[1].replace(/^\s+|\s+$/g, "");
                } else if (oMatch4) { //If found a context for the translation...
                  sContext = oMatch4[1].replace(/^\s+|\s+$/g, "");
                  sComment = sContext;
                }

                var sLineNoString = sLine.replace(/"(?:""|[^"])*"/g, ""); // Remove string literals
                var oMatchID = /\b(ID[A-Z_][A-Z0-9_]*)\b/.exec(sLineNoString);
                var sExtractedStr = "";
                if (oMatchID) {
                    sExtractedStr = oMatchID[1];
                }
                if (sParentContextID !== "" && (iBlockType === MENU_BLOCK || iBlockType === DIALOGEX_BLOCK)) {
                    sExtractedStr = sExtractedStr ? (sParentContextID + ", " + sExtractedStr) : sParentContextID;
                }

                if (sExtractedStr !== "") {
                  if (sComment !== "") {
                    sComment = sExtractedStr + ", " + sComment;
                  } else {
                    sComment = sExtractedStr;
                  }
                }
              }
            }
            break;

          case VERSIONINFO_BLOCK:
            var oMatch5 = /BLOCK "([0-9A-F]+)"/.exec(sLine);
            var oMatch6 = /VALUE "Comments", "(.*?)\\?0?"/.exec(sLine);
            var oMatch7 = /VALUE "Translation", (.*?)$/.exec(sLine);
            if (oMatch5) { //StringFileInfo.Block...
              sString = oMatch5[1];
              sComment = "StringFileInfo.Block";
            } else if (oMatch6) { //StringFileInfo.Comments...
              sString = oMatch6[1];
              sComment = "You should use a string like \"Translated by \" followed by the translator names for this string. It is ONLY VISIBLE in the StringFileInfo.Comments property from the final resource file!";
            } else if (oMatch7) { //VarFileInfo.Translation...
              sString = oMatch7[1];
              sComment = "VarFileInfo.Translation";
            }
            break;
        }
      }
      
      if (sString !== "") {
        var sKey = sContext + sString;
        var oString = { "Comment": "", "References": "", "Context": "", "Id": "", "Str": "" };
        if (sKey in oStrings) { //If the key is already used...
          oString = oStrings[sKey];
        }
        if (sComment !== "") {
          oString.Comment = sComment;
        }
        if (bInsertLineNumbers) {
          if (oString.References !== "") {
            oString.References = oString.References + "\t" + sReference;
          } else {
            oString.References = sReference;
          }
        }
        oString.Context = sContext;
        oString.Id = sString;
        oString.Str = "";
        
        if (sKey in oStrings) { //If the key is already used...
          oStrings[sKey] = oString;
        } else { //If the key is NOT already used...
          oStrings[sContext + sString] = oString;
        }
      }
      fContinuation = (sLine !== "") && ",|".indexOf(sLine.substring(sLine.length - 1)) >= 0;
    }
    oRcFile.Close();
  }
  return oStrings;
}

////
// ...
function GetStringBlacklist(sTxtFilePath) {
  var oBlacklist = {};
  if (oFSO.FileExists(sTxtFilePath)) { //If the blacklist file exists...
    var oTxtFile = oFSO.OpenTextFile(sTxtFilePath, ForReading);
    while (!oTxtFile.AtEndOfStream) { //For all lines...
      var sLine = oTxtFile.ReadLine().replace(/^\s+|\s+$/g, "");
      if (sLine !== "") {
        if (!(sLine in oBlacklist)) { //If the key is NOT already used...
          oBlacklist[sLine] = true;
        }
      }
    }
    oTxtFile.Close();
  }
  return oBlacklist;
}

////
/// Merge20 (W1): collect _()/N_()/NC_() string literals from the C++
/// sources below a folder (recursive, *.cpp/*.h). The POT file used to be
/// built from the RC files only, so strings that exist only in code never
/// reached the translators and were deleted from the shipped .po files.
/// The scan is deliberately narrow: only a literal string (with adjacent
/// literals joined, e.g. _("a" "b") or a literal split across lines) right
/// after _(/N_(/NC_( counts. A call with a variable argument, _T(), or a
/// _() inside a longer identifier does not match. The #. comment is left
/// empty (there is no resource ID); the #: reference (Src-relative path
/// and line) is the provenance. Files are scanned in sorted relative-path
/// order so the POT file is deterministic.
function GetStringsFromCppFiles(sFolderPath) {
  var oBlacklist = GetStringBlacklist("StringBlacklist.txt");
  var oStrings = {};
  var aFiles = [];
  CollectCppFiles(oFSO.GetFolder(sFolderPath), "", aFiles);
  aFiles.sort(function (a, b) { return a.rel < b.rel ? -1 : (a.rel > b.rel ? 1 : 0); });
  for (var i = 0; i < aFiles.length; i++) {
    var oFileStrings = GetStringsFromCppFile(aFiles[i].path, aFiles[i].rel, oBlacklist);
    for (var sKey in oFileStrings) {
      if (!(sKey in oStrings)) {
        oStrings[sKey] = oFileStrings[sKey];
      }
    }
  }
  return oStrings;
}

////
/// ...
function CollectCppFiles(oFolder, sRelDir, aFiles) {
  var oFiles = new Enumerator(oFolder.Files);
  for (; !oFiles.atEnd(); oFiles.moveNext()) {
    var oFile = oFiles.item();
    var sExt = oFSO.GetExtensionName(oFile.Name).toLowerCase();
    if (sExt === "cpp" || sExt === "h") {
      aFiles.push({ path: oFile.Path, rel: sRelDir + oFile.Name });
    }
  }
  var oSubFolders = new Enumerator(oFolder.SubFolders);
  for (; !oSubFolders.atEnd(); oSubFolders.moveNext()) {
    var oSub = oSubFolders.item();
    CollectCppFiles(oSub, sRelDir + oSub.Name + "/", aFiles);
  }
}

////
/// ...
function GetStringsFromCppFile(sFilePath, sRelPath, oBlacklist) {
  var oStrings = {};
  var oFile = oFSO.OpenTextFile(sFilePath, ForReading);
  var sText = oFile.ReadAll();
  oFile.Close();
  // _(...) / N_(...) / NC_(ctx, ...); the string literal follows the
  // opening parenthesis (whitespace/newlines allowed), adjacent literals
  // are joined. The leading (^|[^A-Za-z0-9_]) keeps _T() and identifiers
  // such as foo_() from matching.
  var reCall = /(^|[^A-Za-z0-9_])(NC_|N_|_)\s*\(\s*(?:"((?:\\.|[^"\\])*)"\s*,\s*)?"((?:\\.|[^"\\])*)"/g;
  var reNext = /^\s*"((?:\\.|[^"\\])*)"/;
  var oMatch;
  while ((oMatch = reCall.exec(sText)) != null) {
    var sContext = "";
    var sString = "";
    if (oMatch[2] === "NC_") {
      sContext = oMatch[3] || "";
      sString = oMatch[4] || "";
    } else {
      sString = oMatch[4] || "";
    }
    var iPos = reCall.lastIndex;
    var oNext;
    while ((oNext = reNext.exec(sText.substr(iPos))) != null) {
      sString += oNext[1];
      iPos += oNext[0].length;
    }
    reCall.lastIndex = iPos;
    if (sString === "" || (sString in oBlacklist)) {
      continue;
    }
    var sKey = sContext + sString;
    var oString = { "Comment": "", "References": "", "Context": "", "Id": "", "Str": "" };
    if (sKey in oStrings) {
      oString = oStrings[sKey];
    }
    if (bInsertLineNumbers) {
      var iLine = sText.substring(0, oMatch.index).split("\n").length;
      var sReference = sRelPath + ":" + iLine;
      if (oString.References !== "") {
        oString.References = oString.References + "\t" + sReference;
      } else {
        oString.References = sReference;
      }
    }
    oString.Context = sContext;
    oString.Id = sString;
    oString.Str = "";
    oStrings[sKey] = oString;
  }
  return oStrings;
}

////
// ...
function CreateMasterPotFile(sPotPath, oStrings) {
  var oPotFile = oFSO.CreateTextFile(sPotPath, true);
  
  oPotFile.Write("# This file is from WinMerge <https://winmerge.org/>\n");
  oPotFile.Write("# Released under the \"GNU General Public License\"\n");
  oPotFile.Write("#\n");
  oPotFile.Write("msgid \"\"\n");
  oPotFile.Write("msgstr \"\"\n");
  oPotFile.Write("\"Project-Id-Version: WinMerge\\n\"\n");
  oPotFile.Write("\"Report-Msgid-Bugs-To: https://github.com/WinMerge/winmerge/issues/\\n\"\n");
  oPotFile.Write("\"POT-Creation-Date: " + GetPotCreationDate() + "\\n\"\n");
  oPotFile.Write("\"PO-Revision-Date: \\n\"\n");
  oPotFile.Write("\"Last-Translator: \\n\"\n");
  oPotFile.Write("\"Language-Team: English <winmerge-translate@lists.sourceforge.net>\\n\"\n");
  oPotFile.Write("\"MIME-Version: 1.0\\n\"\n");
  oPotFile.Write("\"Content-Type: text/plain; charset=UTF-8\\n\"\n");
  oPotFile.Write("\"Content-Transfer-Encoding: 8bit\\n\"\n");
  oPotFile.Write("\"X-Poedit-Language: English\\n\"\n");
  oPotFile.Write("\"X-Poedit-SourceCharset: UTF-8\\n\"\n");
  oPotFile.Write("\"X-Poedit-Basepath: ../../Src/\\n\"\n");
  //oPotFile.Write("\"X-Generator: CreateMasterPotFile.js\\n\"\n");
  oPotFile.Write("\n");
  for (var sKey in oStrings) { //For all strings...
    var oString = oStrings[sKey];
    if (oString.Comment !== "") { //If comment exists...
      oPotFile.Write("#. " + oString.Comment + "\n");
    }
    if (bInsertLineNumbers) {
      var aReferences = oString.References.split("\t");
      var sCurrentLine = "";
      var nMaxLineLength = 78;

      for (var i = 0; i < aReferences.length; i++) {
        var sRef = aReferences[i];
        if (sCurrentLine === "") {
          sCurrentLine = "#: " + sRef;
        } else if ((sCurrentLine + " " + sRef).length <= nMaxLineLength) {
          sCurrentLine += " " + sRef;
        } else {
          oPotFile.Write(sCurrentLine + "\n");
          sCurrentLine = "#: " + sRef;
        }
      }
      if (sCurrentLine !== "") {
        oPotFile.Write(sCurrentLine + "\n");
      }
    }
    if (oString.Id.indexOf("%") >= 0) { //If c-format...
      oPotFile.Write("#, c-format\n");
    }
    if (oString.Context !== "") { //If context exists...
      oPotFile.Write("msgctxt \"" + oString.Context + "\"\n");
    }
    oPotFile.Write("msgid \"" + oString.Id + "\"\n");
    oPotFile.Write("msgstr \"\"\n");
    oPotFile.Write("\n");
  }
  oPotFile.Close();
}

////
// ...
function GetPotCreationDate() {
  var oNow = new Date();
  var sYear = oNow.getFullYear();
  var sMonth = oNow.getMonth() + 1;
  if (sMonth < 10) { sMonth = "0" + sMonth; }
  var sDay = oNow.getDate();
  if (sDay < 10) { sDay = "0" + sDay; }
  var sHour = oNow.getHours();
  if (sHour < 10) { sHour = "0" + sHour; }
  var sMinute = oNow.getMinutes();
  if (sMinute < 10) { sMinute = "0" + sMinute; }
  
  return sYear + "-" + sMonth + "-" + sDay + " " + sHour + ":" + sMinute + "+0000";
}

////
// ...
function InfoBox(sText, iSecondsToWait) {
  if (!bRunFromCmd) { //If run from command line...
    var oShell = WScript.CreateObject("WScript.Shell");
    return oShell.Popup(sText, iSecondsToWait, WScript.ScriptName, 64);
  } else { //If NOT run from command line...
    WScript.Echo(sText);
  }
}

////
// ...
function GetArchiveBit(sFilePath) {
  if (oFSO.FileExists(sFilePath)) { //If the file exists...
    var oFile = oFSO.GetFile(sFilePath);
    if (oFile.Attributes & 32) { //If archive bit set...
      return true;
    }
  }
  return false;
}

////
// ...
function SetArchiveBit(sFilePath, bValue) {
  if (oFSO.FileExists(sFilePath)) { //If the file exists...
    var oFile = oFSO.GetFile(sFilePath);
    if (oFile.Attributes & 32) { //If archive bit set...
      if (!bValue) {
        oFile.Attributes -= 32;
      }
    } else { //If archive bit NOT set...
      if (bValue) {
        oFile.Attributes += 32;
      }
    }
  }
}
