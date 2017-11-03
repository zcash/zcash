@echo off

if "%1" == "addr" (
   votecoin-cli getnewaddress
   goto exit
)


if "%1" == "zaddr" (
   votecoin-cli z_getnewaddress
   goto exit
)


if "%1" == "send" (
   votecoin-cli z_sendmany %2 "[{\"address\": \"%3\", \"amount\": %4}]" 1 0
   goto exit
)



if "%1" == "sendto" (
   votecoin-cli sendtoaddress %2 %3
   goto exit
)


if "%1" == "status" (
   votecoin-cli z_getoperationresult
   goto exit
)


if "%1" == "totals" (
   votecoin-cli z_gettotalbalance
   goto exit
)


:help

   cls
   echo.
   echo ------------------------------------------------------------------------
   echo   This is helper script to make VoteCoin operations easier.
   echo   Make sure votecoind.exe is running so we can communicate with it.
   echo   Supported commands:
   echo.
   echo    vot addr ... generate new T address
   echo    vot zaddr  ... generate new Z address
   echo    vot send FROM TO AMOUNT ... send coins FROM address to TO address
   echo    vot sendto TO AMOUNT ... send coins from any address to TO address
   echo    vot status ... show status of last transaction.
   echo    vot totals ... show total balances in your entire wallet
   echo ------------------------------------------------------------------------
   echo.
   echo.
   cmd.exe
   goto exit

:exit
