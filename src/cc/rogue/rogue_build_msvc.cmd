@echo off
echo Rogue Build Script by Decker (c) 2019

@REM Check for Visual Studio
call set "VSPATH="
if defined VS140COMNTOOLS ( if not defined VSPATH (
 call set "VSPATH=%%VS140COMNTOOLS%%"
) )

@REM check if we already have the tools in the environment
if exist "%VCINSTALLDIR%" (
 goto compile
)

if not defined VSPATH (
 echo You need Microsoft Visual Studio 15 installed
 pause
 exit
)

@REM set up the environment
if exist "%VSPATH%..\..\vc\vcvarsall.bat" (
 call "%%VSPATH%%..\..\vc\vcvarsall.bat" amd64
 goto compile
)

echo Unable to set up the environment
pause
exit

:compile

mkdir x86_64-w64-msvc\deps
mkdir x86_64-w64-msvc\deps\install

pushd x86_64-w64-msvc\deps

:compile_pdcurses
rem git clone https://github.com/wmcbrine/PDCurses PDCurses.org
git clone https://github.com/Bill-Gray/PDCurses

set PREFIX_DIR=%CD%\install

pushd PDCurses
mkdir build64 & pushd build64
rem cmake -G"Visual Studio 14 2015 Win64" -DPDC_WIDE=ON -DCMAKE_INSTALL_PREFIX=%PREFIX_DIR% -DCMAKE_BUILD_TYPE=Debug -DPDCDEBUG=ON ..
cmake -G"Visual Studio 14 2015 Win64" -DPDC_WIDE=ON -DCMAKE_INSTALL_PREFIX=%PREFIX_DIR% -DCMAKE_BUILD_TYPE=Release ..
popd
rem cmake --build build64 --config Debug --target install
cmake --build build64 --config Release --target install
popd

:compile_curl

git clone https://github.com/curl/curl
pushd curl

mkdir build64 & pushd build64
cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_INSTALL_PREFIX=%PREFIX_DIR% -DCMAKE_USE_WINSSL:BOOL=ON ..
cmake --build . --config Release --target libcurl
cmake --build . --config Release --target install
popd
popd

