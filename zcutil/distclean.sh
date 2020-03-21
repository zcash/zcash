#!/bin/sh
# Copyright (c) 2020 The Zcash developers

zcutil/clean.sh

rm -rf depends/*-*-*
rm -rf depends/built
rm -rf depends/sources
rm -rf target

rm -f .cargo/config
rm -f .cargo/.configured-for-offline
