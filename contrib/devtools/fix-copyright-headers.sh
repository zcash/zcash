#!/usr/bin/env bash

current_year=$(date +%Y)

read -r -d '' VERIFY_SCRIPT << EOS
for party in "The Zcash developers" "The Bitcoin Core developers" "Bitcoin Developers"; do
  sed -i"" -e "s#Copyright (c) \([0-9]\{4\}\)\(-[0-9]\{4\}\)\? \$party#Copyright (c) \1-$current_year \$party#" COPYING
  sed -i"" -e "s#\(.*\)\([0-9]\{4\}\)\(-[0-9]\{4\}\)\, \$party#\1\2-$current_year, \$party#" contrib/debian/copyright
done

sed -i"" -e "s/define(_COPYRIGHT_YEAR, [0-9]\{4\})/define(_COPYRIGHT_YEAR, $current_year)/" configure.ac
sed -i"" -e "s/#define COPYRIGHT_YEAR [0-9]\{4\}/#define COPYRIGHT_YEAR $current_year/" src/clientversion.h

git grep "^// Copyright (c) .* The Zcash developers" \\
  | awk -F ':' '{print \$1}' \\
  | xargs -I {} sed -i"" -e "s#// Copyright (c) \([0-9]\{4\}\)\(-[0-9]\{4\}\)\? The Zcash developers#// Copyright (c) \1-$current_year The Zcash developers#" {}
EOS

bash << EOB
$VERIFY_SCRIPT
EOB

git commit -a -F - << EOS
scripted-diff: Update Zcash copyrights to $current_year

-BEGIN VERIFY SCRIPT-
$VERIFY_SCRIPT
-END VERIFY SCRIPT-
EOS
