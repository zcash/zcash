#!/bin/sh

TEST=$@

mkdir -p "$TEST/a/"
mkdir -p "$TEST/b/"

touch "$TEST/a/test.cpp"
touch "$TEST/b/test.cpp"
touch "$TEST/expected"
