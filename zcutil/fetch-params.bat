@echo off
call :GET_CURRENT_DIR
cd %THIS_DIR%
IF NOT EXIST "%APPDATA%"\ZcashParams (
MKDIR "%APPDATA%"\ZcashParams
)
IF NOT EXIST "%APPDATA%"\ZcashParams\sprout-proving.key (
    ECHO Downloading Zcash trusted setup sprout-proving.key, this may take a while ...
    .\wget64.exe --progress=dot:giga --continue --retry-connrefused --waitretry=3 --timeout=30 https://z.cash/downloads/sprout-proving.key.deprecated-sworn-elves -O "%APPDATA%"\ZcashParams\sprout-proving.key
)
IF NOT EXIST "%APPDATA%"\ZcashParams\sprout-verifying.key (
    ECHO Downloading Zcash trusted setup sprout-verifying.key, this may take a while ...
    .\wget64.exe --progress=dot:giga --continue --retry-connrefused --waitretry=3 --timeout=30 https://z.cash/downloads/sprout-verifying.key -O "%APPDATA%"\ZcashParams\sprout-verifying.key
)
IF NOT EXIST "%APPDATA%"\ZcashParams\sapling-spend.params (
    ECHO Downloading Zcash trusted setup sprout-proving.key, this may take a while ...
    .\wget64.exe --progress=dot:giga --continue --retry-connrefused --waitretry=3 --timeout=30 https://z.cash/downloads/sapling-spend.params -O "%APPDATA%"\ZcashParams\sapling-spend.params
)
IF NOT EXIST "%APPDATA%"\ZcashParams\sapling-output.params (
    ECHO Downloading Zcash trusted setup sprout-verifying.key, this may take a while ...
    .\wget64.exe --progress=dot:giga --continue --retry-connrefused --waitretry=3 --timeout=30 https://z.cash/downloads/sapling-output.params -O "%APPDATA%"\ZcashParams\sapling-output.params
)
IF NOT EXIST "%APPDATA%"\ZcashParams\sprout-groth16.params (
    ECHO Downloading Zcash trusted setup sprout-verifying.key, this may take a while ...
    .\wget64.exe --progress=dot:giga --continue --retry-connrefused --waitretry=3 --timeout=30 https://z.cash/downloads/sprout-groth16.params -O "%APPDATA%"\ZcashParams\sprout-groth16.params
)
goto :EOF
:GET_CURRENT_DIR
pushd %~dp0
set THIS_DIR=%CD%
popd
goto :EOF
