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

   echo.
   echo Helper script to make VoteCoin operations easier
   echo Supported syntax:
   echo.
   echo    vot addr ... generate new T address
   echo    vot zaddr  ... generate new Z address
   echo    vot send FROM TO AMOUNT ... send coins FROM address to TO address of given AMOUNT, with zero fee
   echo    vot sendto TO AMOUNT ... send coins from any address to TO address of given AMOUNT
   echo    vot status ... show status of last transaction. Empty status means transaction still in progress
   echo    vot totals ... show total balances in your entire wallet
   goto exit

:exit
