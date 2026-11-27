@echo off
pushd "%~dp0"
echo Prepare Translations...
rmdir /S /Q "Build\Translations" 2> NUL
mkdir "Build\Translations\WinMerge" 2> NUL
mkdir "Build\Translations\ShellExtension" 2> NUL
copy Translations\WinMerge\*.po Build\Translations\WinMerge\ > NUL
copy Translations\ShellExtension\*.po Build\Translations\ShellExtension\ > NUL
powershell.exe -ExecutionPolicy RemoteSigned -File "Translations\StripPoComments.ps1" "Build\Translations"
rem Merge20 (W1): sentinel - the bundled Japanese.po must contain the
rem Merge20 must-have msgids. The MergeLang build step rewrites the .po
rem files from English.pot, and a regression there would silently ship an
rem English UI again (111 Merge20 msgids were lost that way in the
rem 2026-11-25 build). Failing here proves in the CI log that the
rem shipped .po is current. (ASCII msgids only, so findstr works on the
rem UTF-8 file. The Wiz labels themselves are verified by inspecting the
rem CI artifact ZIP, not here.)
echo Checking bundled Japanese.po for Merge20 strings...
rem Merge20 (S0-F1): the excluded-lines column description and the two
rem per-side breakdown tooltip wordings are also must-have msgids.
rem Merge20 (H-DOC): the user manual menu item is also a must-have msgid.
rem Merge20 (Wave 5 F4): the FORM spotlight banner, callout and preview
rem strings are also must-have msgids. "Old (before)" / "New (after)" are
rem NOT listed here: their parentheses would break the FOR set (same
rem reason as the missing-manual message above).
rem NOTE: the msgid is "&User Manual" but the sentinel uses the substring
rem "User Manual" because & is a batch metacharacter. The missing-manual
rem message is not listed here (its parentheses would break the FOR set);
rem it is covered by the .po count audit instead.
rem NOTE: literal percent signs in msgids are written %% here, so
rem that findstr searches for %1 / %2 / %3.
for %%M in ("Review State" "Include in Output" "Action Category" "Risk" "Summary" "Memo" "Number of lines blanked by the prediffer (excluded lines) in the file. Shows the largest side count; hover over the cell for the per-side breakdown." "Excluded lines: old %%1, new %%2" "Excluded lines: left %%1, middle %%2, right %%3" "User Manual" "Previous change" "Next change" "Clear spotlight" "Go to changed area" "Spotlight" "Changed attributes" "Change details" "No shape") do (
  findstr /C:%%M "Build\Translations\WinMerge\Japanese.po" > NUL || (
    echo ERROR: Merge20 msgid %%M is missing from the bundled Japanese.po.
    exit /b 1
  )
)
echo Merge20 strings present in the bundled Japanese.po.
popd
