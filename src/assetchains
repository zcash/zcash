#!/bin/bash
set -eo pipefail

# You can now add delay line to pubkey.txt file
source pubkey.txt
overide_args="$@"
seed_ip=`getent hosts zero.kolo.supernet.org | awk '{ print $1 }'`
komodo_binary='./komodod'

if [ -z "$delay" ]; then delay=20; fi

./listassetchainparams | while read args; do
  gen=""
  if [ $[RANDOM % 10] == 1 ]; then
      gen=" -gen -genproclimit=1"
  fi

  $komodo_binary $gen $args $overide_args -pubkey=$pubkey -addnode=$seed_ip &
  sleep $delay
done
