pushd "%~dp0"
rem NOTE (Merge20): every step must fail the whole build on error.
rem Do not use "|| goto :eof" here: a failed step used to be swallowed
rem and the CI run stayed green with no artifacts (run 45573109576).
call BuildBin.cmd %1 %2 || exit /b 1
pushd Testing\GoogleTest\UnitTests
UnitTests.exe || exit /b 1
popd
call BuildTranslations.cmd || exit /b 1
call BuildInstaller.cmd %1 %2 || exit /b 1
call BuildArc.cmd %1 %2 || exit /b 1
popd
