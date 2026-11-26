

if "%1" == "" (
  call :BuildInstaller x86 || exit /b 1
  call :BuildInstaller x64 || exit /b 1
  call :BuildInstaller x64NonAdmin || exit /b 1
  call :BuildInstaller ARM64 || exit /b 1
) else if "%1" == "x64" (
  call :BuildInstaller x64 || exit /b 1
  call :BuildInstaller x64NonAdmin || exit /b 1
) else if "%1" == "x86" (
  call :BuildInstaller x86 || exit /b 1
) else if "%1" == "ARM64" (
  call :BuildInstaller ARM64 || exit /b 1
) else if "%1" == "ARM" (
  echo platform %1 is not supported
  exit /b 1
) else (
  call :BuildInstaller %1 || exit /b 1
)
goto :eof

:BuildInstaller
set PLATFORM=%1

rem Inno Setup is not preinstalled on GitHub-hosted runners, so make sure
rem iscc.exe is available first (installs Inno Setup 6 via Chocolatey only
rem when no iscc.exe is found). See :EnsureInnoSetup.
call :EnsureInnoSetup || exit /b 1

rem Find iscc.exe: Inno Setup 6 first, then Inno Setup 5.
set "ISCC="
set "IS6="
for %%i in ("%LOCALAPPDATA%\Programs" "%ProgramFiles(x86)%" "%ProgramFiles%") do (
  if not defined ISCC if exist "%%~i\Inno Setup 6\iscc.exe" set "ISCC=%%~i\Inno Setup 6\iscc.exe"
)
if defined ISCC set "IS6=1"
if not defined ISCC (
  for %%i in ("%LOCALAPPDATA%\Programs" "%ProgramFiles(x86)%" "%ProgramFiles%") do (
    if not defined ISCC if exist "%%~i\Inno Setup 5\iscc.exe" set "ISCC=%%~i\Inno Setup 5\iscc.exe"
  )
)
if not defined ISCC (
  echo ERROR: iscc.exe not found even though EnsureInnoSetup succeeded.
  exit /b 1
)

rem Inno Setup 6 uses the .is6.iss script when one exists for this platform;
rem Inno Setup 5 (and platforms without a .is6.iss, e.g. x64NonAdmin) use .iss.
set "ISSFILE=WinMerge%PLATFORM%.iss"
if defined IS6 if exist "Installer\innosetup\WinMerge%PLATFORM%.is6.iss" set "ISSFILE=WinMerge%PLATFORM%.is6.iss"

echo.
echo ============================================================
echo Build %ISSFILE% with %ISCC%
echo ============================================================
"%ISCC%" /DTranslationsDir=..\..\Build\Translations "Installer\innosetup\%ISSFILE%" || exit /b 1

rem iscc.exe can exit 0 while producing nothing, so verify the installer
rem file was actually produced (BuildArc.cmd copies it to Build\Releases).
rem NOTE: do not use "|| pause" after iscc.exe: in CI there is no keyboard
rem and the job would hang until it times out.
if "%PLATFORM%" == "x64" set "OUTPAT=Build\WizMerge-*-x64-Setup.exe"
if "%PLATFORM%" == "x64NonAdmin" set "OUTPAT=Build\WizMerge-*-x64-PerUser-Setup.exe"
if "%PLATFORM%" == "ARM64" set "OUTPAT=Build\WizMerge-*-ARM64-Setup.exe"
if "%PLATFORM%" == "x86" set "OUTPAT=Build\WizMerge-*-Setup.exe"
if not defined OUTPAT set "OUTPAT=Build\WizMerge-*-%PLATFORM%-Setup.exe"
if not exist "%OUTPAT%" (
  echo ERROR: iscc.exe finished but %OUTPAT% was not produced.
  exit /b 1
)

goto :eof

:EnsureInnoSetup
rem If iscc.exe is already installed, reuse it and skip the download.
for %%i in ("%LOCALAPPDATA%\Programs" "%ProgramFiles(x86)%" "%ProgramFiles%") do (
  if exist "%%~i\Inno Setup 6\iscc.exe" goto :eof
  if exist "%%~i\Inno Setup 5\iscc.exe" goto :eof
)

rem Not found: install Inno Setup 6 via Chocolatey, which is preinstalled
rem on GitHub-hosted windows-latest runners. The Chocolatey package
rem "innosetup" installs to "%ProgramFiles(x86)%\Inno Setup 6".
where choco.exe >NUL 2>&1
if errorlevel 1 (
  echo ERROR: iscc.exe not found and Chocolatey is not available. Install Inno Setup 6 manually.
  exit /b 1
)
echo Installing Inno Setup 6 via Chocolatey...
choco install innosetup -y --no-progress
if errorlevel 1 (
  echo ERROR: choco install innosetup failed.
  exit /b 1
)
for %%i in ("%LOCALAPPDATA%\Programs" "%ProgramFiles(x86)%" "%ProgramFiles%") do (
  if exist "%%~i\Inno Setup 6\iscc.exe" goto :eof
)
echo ERROR: Inno Setup installed but iscc.exe was not found.
exit /b 1
