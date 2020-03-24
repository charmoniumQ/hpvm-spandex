#!/bin/bash

CURRENT_DIR=`pwd`
BUILD_DIR=$CURRENT_DIR/build
HPVM_RT=hpvm-rt/hpvm-rt.bc

if [ -f $BUILD_DIR/tools/hpvm/projects/$HPVM_RT ]; then
    true
else
    echo $BUILD_DIR/tools/hpvm/projects/$HPVM_RT
    echo HPVM not installed! Exiting without running tests!.
    exit 0
fi

LIT_DIR=$BUILD_DIR/bin/

LIT_TOOL=$LIT_DIR/llvm-lit

TEST_DIR=$CURRENT_DIR/test
REG_TEST_DIR=$TEST_DIR/regressionTests
UNIT_TEST_DIR=$TEST_DIR/unitTests

echo
echo Running tests ...
echo

# Run regression tests
$LIT_TOOL -v $REG_TEST_DIR

# Run unit tests
#$LIT_TOOL -v $UNIT_TEST_DIR
