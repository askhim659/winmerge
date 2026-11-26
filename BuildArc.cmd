pushd "%~dp0"

setlocal
call SetVersion.cmd
if "%2" == "-ci" (
  if exist .hg (
    for /F "delims=" %%i in ('hg id') do set SAFEAPPVER=%SAFEAPPVER%-%%i
  ) else if exist .git (
    for /F "delims=" %%i in ('git rev-parse --short head') do set SAFEAPPVER=%SAFEAPPVER%-%%i
  ) else (
    set SAFEAPPVER=%SAFEAPPVER%-%APPVEYOR_BUILD_VERSION%
  )
)
set DISTDIR=.\Build\Releases
set path="%ProgramFiles%\7-zip";"%ProgramFiles(x86)%\7-zip";%path%

if "%1" == "" (
  call :BuildZip x86
  call :BuildZip x64
  call :BuildZip ARM
  call :BuildZip ARM64
) else (
  call :BuildZip %1 
)

del "%DISTDIR%\winmerge-%SAFEAPPVER%-full-src.7z" 2> NUL

7z.exe a -t7z -xr!*.gcno -xr!*.gcda -xr!*.gcov -xr!*.orig -xr!*.o -xr!*.a -xr!.dep -xr!*.asm -xr!*.out -xr!debug_static* -xr!debug_shared* -xr!release_static* -xr!release_shared* -xr!*.bak -xr!*.lang -xr!*.db -xr!*.ncb  -xr!*.sdf -xr!*.bsc -xr!*.opt -xr!*.plg -xr!*.suo -xr!*.obj -xr!*.ilk -xr!*.pdb -xr!*.pch -xr!*.res -xr!*.exe -xr!*.sbr -xr!.vs -xr!*.VC.db-* -xr!*.zip -xr!WinMergeSplash.psd -xr!*.lib -xr!*.exp -xr!*.idb -xr!*.%COMPUTERNAME%.%USERNAME%.user -xr!BuildLog.htm -xr!ipch -xr!*.tlb -xr!*.tlog -xr!*.*~ -xr!CVS -xr!.svn -xr!.hg -xr!*.log -xr!*.lastbuildstate -xr!FreeImage*.dll -xr!WinIMergeLib.dll -xr!WinIMerge.exe -xr!WinWebDiff.dll -xr!WinWebDiff.exe -xr!darkmode.dll -xr!BuildTmp -xr!Docs\Manual\Tools "%DISTDIR%\winmerge-%SAFEAPPVER%-full-src.7z" ArchiveSupport ColorSchemes Docs Externals Filters Installer Plugins ShellExtension Src Testing Tools Translations Web readme.txt Version.h Version.in *.cmd *.bat *.inf *.sln *.js

(
echo "%DISTDIR%\WizMerge-%SAFEAPPVER%-Setup.exe"
echo "%DISTDIR%\WizMerge-%SAFEAPPVER%-x64-Setup.exe"
echo "%DISTDIR%\WizMerge-%SAFEAPPVER%-x64-PerUser-Setup.exe"
echo "%DISTDIR%\WizMerge-%SAFEAPPVER%-ARM64-Setup.exe"
echo "%DISTDIR%\wizmerge-%SAFEAPPVER%-exe.zip"
echo "%DISTDIR%\wizmerge-%SAFEAPPVER%-x64-exe.zip"
echo "%DISTDIR%\wizmerge-%SAFEAPPVER%-ARM-exe.zip"
echo "%DISTDIR%\wizmerge-%SAFEAPPVER%-ARM64-exe.zip"
echo "%DISTDIR%\winmerge-%SAFEAPPVER%-pdb.7z"
echo "%DISTDIR%\winmerge-%SAFEAPPVER%-x64-pdb.7z"
echo "%DISTDIR%\winmerge-%SAFEAPPVER%-ARM-pdb.7z"
echo "%DISTDIR%\winmerge-%SAFEAPPVER%-ARM64-pdb.7z"
echo "%DISTDIR%\winmerge-%SAFEAPPVER%-full-src.7z"
) > "%DISTDIR%\files.txt"

popd
goto :eof


:BuildZip

set PLATFORM=%1
if "%1" == "x86" (
  set PLATFORMH=
) else (
  set PLATFORMH=%1-
)

echo.
echo ============================================================
echo BUILD wizmerge-%SAFEAPPVER%-%PLATFORMH%exe.zip...
echo ============================================================

rmdir /q /s "%DISTDIR%\%PLATFORMH%zip-version" > NUL 2> NUL
mkdir "%DISTDIR%" 2> NUL

rem Copy platform setups
if not "%1" == "ARM" (
  copy "Build\WizMerge-%RCVER%-%PLATFORMH%Setup.exe" "%DISTDIR%\WizMerge-%SAFEAPPVER%-%PLATFORMH%Setup.exe" > NUL
)
if "%1" == "x64" (
  copy "Build\WizMerge-%RCVER%-%PLATFORMH%PerUser-Setup.exe" "%DISTDIR%\WizMerge-%SAFEAPPVER%-%PLATFORMH%PerUser-Setup.exe" > NUL
)

rem Create folder structure
for %%i in (ColorSchemes Languages\ShellExtension Filters MergePlugins Docs Frhed\Docs Frhed\Languages WinIMerge WinWebDiff darkmodelib TreeSitterGrammars Merge7z\Lang Commands\Apache-Tika Commands\cliphcat Commands\tidy-html5 Commands\jq Commands\q Commands\msys2 Commands\md4c Resources) do (
  mkdir "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\%%i" 2> NUL
)

rem Docs
echo Copy Docs...
for %%i in (Translations\Docs\Readme\ReadMe-*.txt Build\Manual\htmlhelp\WinMerge*.chm Docs\Users\ReleaseNotes*.html Docs\Users\ChangeLog*.html) do (
  copy "%%i" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Docs" > NUL
)
for %%i in (Src\COPYING Docs\Users\Contributors.txt Docs\Users\ReadMe.txt) do (
  copy "%%i" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge" > NUL
)

rem Excecutables
echo Copy Excecutables...
copy Build\%PLATFORM%\Release\WinMergeU.exe "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
if "%1" == "x64" (
  copy Plugins\WinMerge32BitPluginProxy\Release\WinMerge32BitPluginProxy.exe "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
)

rem ShellExtension
echo Copy ShellExtension...
if "%1" == "x86" (
  copy "Build\ShellExtension\ShellExtensionU.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
  copy "Build\ShellExtension\ShellExtensionX64.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
  copy "Build\ShellExtension\x64\WinMergeContextMenu.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
)
if "%1" == "x64" (
  copy "Build\ShellExtension\ShellExtensionU.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
  copy "Build\ShellExtension\ShellExtensionX64.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
  copy "Build\ShellExtension\x64\WinMergeContextMenu.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
)
if "%1" == "ARM" (
  copy "Build\ShellExtension\ShellExtensionARM.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
  copy "Build\ShellExtension\ShellExtensionARM64.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
  copy "Build\ShellExtension\ARM64\WinMergeContextMenu.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
)
if "%1" == "ARM64" (
  copy "Build\ShellExtension\ShellExtensionARM64.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
  copy "Build\ShellExtension\ARM64\WinMergeContextMenu.dll" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
)
copy ShellExtension\Register.bat "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
copy ShellExtension\RegisterPerUser.bat "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
copy ShellExtension\RegisterWinMergeContextMenuPackage.bat "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
copy ShellExtension\UnRegister.bat "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
copy ShellExtension\UnRegisterPerUser.bat "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
copy ShellExtension\UnregisterWinMergeContextMenuPackage.bat "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL
copy "Build\ShellExtension\WinMergeContextMenuPackage.msix" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL

rem Translations
echo Copy Translations...
copy Build\Translations\WinMerge\*.po "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Languages\" > NUL
copy Build\Translations\ShellExtension\*.po "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Languages\ShellExtension\" > NUL

rem ColorSchemes
echo Copy ColorSchemes...
for %%i in (ColorSchemes\*.ini) do (
  copy "%%i" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\ColorSchemes" > NUL
)

rem Filters
echo Copy Filters...
for %%i in (Filters\*.flt Filters\*.tmpl Filters\*.txt) do (
  copy "%%i" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Filters" > NUL
)

rem Plugins
echo Copy Plugins...
copy Plugins\dlls\%PLATFORM%\*.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\MergePlugins\" > NUL
copy Plugins\dlls\*.sct "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\MergePlugins\" > NUL
del "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\MergePlugins\CompareMS*.dll" 2> NUL
del "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\MergePlugins\Watch*OfLog.dll" 2> NUL
del "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\MergePlugins\DisplayXMLFiles.dll" 2> NUL

rem 7zPlugins
echo Copy 7zPlugins...
copy Build\%PLATFORM%\Release\Merge7z\Merge7z.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Merge7z\" > NUL
copy Build\%PLATFORM%\Release\Merge7z\7z.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Merge7z\" > NUL
copy Build\%PLATFORM%\Release\Merge7z\*.txt "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Merge7z\" > NUL
copy Build\%PLATFORM%\Release\Merge7z\Lang\*.txt "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Merge7z\Lang\" > NUL

rem Frhed
echo Copy Frhed...
copy Build\%PLATFORM%\Release\Frhed\GPL.txt "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\frhed\" > NUL
copy Build\%PLATFORM%\Release\Frhed\hekseditU.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\frhed\" > NUL
copy Build\%PLATFORM%\Release\Frhed\Docs\*.txt "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\frhed\Docs" > NUL
copy Build\%PLATFORM%\Release\Frhed\Docs\Sample.tpl "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\frhed\Docs" > NUL
copy Build\%PLATFORM%\Release\Frhed\Languages\*.po "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\frhed\Languages" > NUL
copy Build\%PLATFORM%\Release\Frhed\Languages\heksedit.lng "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\frhed\Languages" > NUL

rem WinIMerge
echo Copy WinIMerge...
copy Build\%PLATFORM%\Release\WinIMerge\GPL.txt "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\WinIMerge\" > NUL
copy Build\%PLATFORM%\Release\WinIMerge\freeimage-license-gplv2.txt "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\WinIMerge\" > NUL
copy Build\%PLATFORM%\Release\WinIMerge\WinIMergeLib.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\WinIMerge\" > NUL
copy Build\%PLATFORM%\Release\WinIMerge\vcomp*.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\" > NUL

rem WinWebDiff
echo Copy WinWebDiff...
copy Build\%PLATFORM%\Release\WinWebDiff\LICENSE*.txt "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\WinWebDiff\" > NUL
copy Build\%PLATFORM%\Release\WinWebDiff\WinWebDiffLib.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\WinWebDiff\" > NUL

rem darkmodelib
echo Copy darkmodelib...
copy Build\%PLATFORM%\Release\darkmodelib\LICENSE.md "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\darkmodelib\" > NUL
copy Build\%PLATFORM%\Release\darkmodelib\darkmode.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\darkmodelib\" > NUL

rem TreeSitterGrammars
echo Copy TreeSitterGrammars...
copy Build\%PLATFORM%\Release\TreeSitterGrammars\*.dll "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\TreeSitterGrammars\" > NUL
copy Build\%PLATFORM%\Release\TreeSitterGrammars\*.scm "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\TreeSitterGrammars\" > NUL
for /D %%i in (Build\%PLATFORM%\Release\TreeSitterGrammars\tree-sitter-*) do (
    xcopy /Y /I "%%i\LICENSE" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\TreeSitterGrammars\%%~nxi\" > NUL
)

rem Commands
echo Copy Commands...
xcopy /s/y Plugins\Commands "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands"
copy Plugins\Commands\DownloadFiles.bat "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands"

rem Patch
echo Copy Patch...
robocopy Build\msys2 "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\msys2" /E /XF lemon*.* re2c*.* /XD lemon*.* re2c*.* > NUL

rem Copy cliphcat...
echo Copy cliphcat...
copy Build\cliphcat\cliphcat.exe "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\cliphcat\cliphcat.exe" > NUL
copy Build\cliphcat\LICENSE "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\cliphcat\" > NUL

rem Copy jq...
echo Copy jq...
copy Build\jq\jq-windows-i386.exe "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\jq\jq.exe" > NUL
copy Build\jq\jq-jq-1.8.1\COPYING "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\jq\" > NUL

rem Copy tidy-html5...
echo Copy tidy-html5...
copy Build\tidy-html5\bin\tidy.* "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\tidy-html5\" > NUL
copy Build\tidy-html5\tidy-html5-5.4.0\README\LICENSE.md "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\tidy-html5\" > NUL

rem Copy md4c...
echo Copy md4c...
copy Build\md4c\mingw32\bin\*.* "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\md4c\" > NUL
copy Build\md4c\mingw32\share\licenses\md4c\LICENSE.md "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Commands\md4c\" > NUL

rem Plugin.xml
copy Plugins\Plugins.xml "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\MergePlugins\" > NUL

rem Copy Resources...
echo Copy Resources...
copy Src\res\splash.png "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\Resources\" > NUL

echo.
echo ------------------------------------------------------------
echo Pack archive...
echo ------------------------------------------------------------
7z.exe a -tzip "%DISTDIR%\wizmerge-%SAFEAPPVER%-%PLATFORMH%exe.zip" "%DISTDIR%\%PLATFORMH%zip-version\WizMerge\"
set PDBFILE="Build\%PLATFORM%\Release\WinMergeU.pdb"
7z.exe a -t7z  "%DISTDIR%\winmerge-%SAFEAPPVER%-%PLATFORMH%pdb.7z" %PDBFILE:\\=\%

goto :eof
