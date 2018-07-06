@call :GET_CURRENT_DIR
@cd %THIS_DIR%
komodo-cli.exe -ac_name=VRSC %1 %2 %3 %4 %5 %6 %7 %8 %9
@goto :EOF

:GET_CURRENT_DIR
@pushd %~dp0
@set THIS_DIR=%CD%
@popd
@goto :EOF




