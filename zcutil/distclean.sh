#!/bin/sh
# Copyright (c) 2020 The Zcash developers

zcutil/clean.sh

rm -rf depends/*-*-*
rm -rf depends/built
rm -rf depends/sources

rm -rf afl-temp
rm -rf src/fuzzing/*/output

# These are not in clean.sh because they are only generated when building dependencies.
rm -f zcutil/bin/db_*
rmdir zcutil/bin 2>/dev/null
