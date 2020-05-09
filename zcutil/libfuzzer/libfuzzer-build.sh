#!/usr/bin/env bash

set -eu -o pipefail

usage() {
  echo ""
  echo "$0 <build stage> <options> <passthrough build arguments>"
  echo ""
  echo "Build a fuzzer in the local repo using the following options:"
  echo ""
  echo "<build stage>:" 
  echo " either \"depends\" or \"fuzzer\""
  echo ""
  echo "<options>:"
  echo " -f,--fuzzer <fuzzername> (ignored if building \"depends\")"
  echo " [-s,--sanitizers <sanitizers>]"
  echo " [-i,--instrument <instrument>]"
  echo " [-l,--logfile <logfile>] # default is ./zcash-build-wrapper.log"
  echo " [-h,--help]"
  echo ""
  echo "Where fuzzer is an entry in ./src/fuzzing/*, the default sanitizer"
  echo "is \"address\" and default instrument is ( \"^.*/src/$\" )."
  echo ""
  exit -1
}

die() {
  echo $1
  exit -1
}

# parse command line options

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
  -f|--fuzzer)
  FUZZER_NAME="$2"
  if [ ! -f "./src/fuzzing/$FUZZER_NAME/fuzz.cpp" ]
  then
    die "Cannot find source code for fuzzer."
  fi
  shift
  shift
  ;;
  -s|--sanitizers)
  LLVM_SANITIZERS="$2"
  shift
  shift
  ;;
  -i|--instrument)
  INSTRUMENT_CODE="$2"
  shift
  shift
  ;;
  -l|--logfile)
  LOGFILE="$2"
  shift
  shift
  ;;
  -h|--help)
  usage
  shift
  ;;
  *)
  POSITIONAL+=("$1")
  shift
  ;;
esac
done

# positional arguments

if [ ${#POSITIONAL[@]} -lt 1 ]
then
  usage
else
  BUILD_STAGE=${POSITIONAL[0]}
fi

case "${BUILD_STAGE:-undefined}" in
  depends)
  FUZZER_NAME=notused
  # fine
  ;;
  fuzzer)
  # fine
  ;;
  *)
  # not fine
  usage
  ;;
esac

# required arguments

if [ "${FUZZER_NAME:-undefined}" = "undefined" ]
then
  usage
fi

# default values

if [ "${LLVM_SANITIZERS:-undefined}" = "undefined" ]
then
  export LLVM_SANITIZERS="address"
fi
if [ "${INSTRUMENT_CODE:-undefined}" = "undefined" ]
then
  export INSTRUMENT_CODE=("^.*\/src")
fi
if [ "${LOGFILE:-undefined}" = "undefined" ]
then
  export LOGFILE=./zcash-build-wrapper.log
fi

set -x

export ZCUTIL=$(realpath "./zcutil")

# overwrite the Linux make profile to use clang instead of GCC:

cat $ZCUTIL/../depends/hosts/linux.mk | sed -e 's/=gcc/=clang/g' | sed -e 's/=g++/=clang++/g' > x
mv x $ZCUTIL/../depends/hosts/linux.mk

# the build_stage distinction helps to layer an intermediate docker 
# container for the built dependencies, so we can resume building 
# from there assuming no other build arguments have changed

if [ "$BUILD_STAGE" = "depends" ]
then
  # make an empty fuzz file just so we can build dependencies 
  > src/fuzz.cpp
else
  cp "./src/fuzzing/$FUZZER_NAME/fuzz.cpp" src/fuzz.cpp || die "Can't copy fuzz.cpp for that fuzzer"
fi

# sneak the variable into zcashd's build.sh

export BUILD_STAGE

# run build.sh with our compiler wrapper, and BUILD_STAGE environment set:

CONFIGURE_FLAGS="--enable-tests=no --disable-bench" \
  "$ZCUTIL/build.sh" \
  -j$(nproc) \
  "CC=$ZCUTIL/libfuzzer/zcash-wrapper-clang" \
  "CXX=$ZCUTIL/libfuzzer/zcash-wrapper-clang++" "${POSITIONAL[@]:1}" || die "Build failed at stage $BUILD_STAGE."
