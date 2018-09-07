#!/bin/bash

# SET AC
read -p "Enter AC name you use : " acname
sed -i "/#define ACNAME */c\#define ACNAME \"$acname\"" oraclefeed.c
# Set ORACLETXID
read -p "Enter your oracle TXID (Oracle should have L data type) : " oracletxid
sed -i "/#define ORACLETXID */c\#define ORACLETXID \"$oracletxid\"" oraclefeed.c
# SET PUBKEY
read -p "Enter your pubkey : " pubkey
sed -i "/#define MYPUBKEY */c\#define MYPUBKEY \"$pubkey\"" oraclefeed.c
# COMPILATION
echo "Great, compiling !"
gcc oraclefeed.c -lm -o oracle_dapp
mv oracle_dapp ../../oracle_dapp
echo "Oracle is ready to use !"
while true; do
    read -p "Would you like to run BTCUSD oracle app? [Y/N]" yn
    case $yn in
        [Yy]* ) cd ../..; ./oracle_dapp; break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done
