@echo off
set AGAMA_DIR=%1
if defined AGAMA_DIR (
	goto :CHECK_DIR
) else (
	call :GET_AGAMA_DIR
	goto :CHECK_DIR
)

:GET_AGAMA_DIR
set /p AGAMA_DIR="Please enter the path to the Agama directory i.e ~\Users\{USER}\Downloads\Agama-win32-x64 :"
goto :EOF

:CHECK_DIR
IF NOT EXIST %AGAMA_DIR% (
	ECHO "Directory does not exist. No changes applied. Exiting.
	goto :EOF
) else (
	goto :UPGRADE
)
goto :EOF

:UPGRADE
pushd %~dp0
set SCRIPT_DIR=%CD%
popd
cd %SCRIPT_DIR%
set TARGET=%AGAMA_DIR%\resources\app\assets\bin\win64\
xcopy komodo*.exe %TARGET% /b/v/y || echo  UPGRADE FAILED && goto :EOF
echo upgrade successful
goto :EOF

