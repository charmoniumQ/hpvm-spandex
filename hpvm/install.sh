#!/bin/bash

SCRIPTS_DIR=scripts

BASH=/bin/bash

# Run installer script
$BASH $SCRIPTS_DIR/llvm_installer.sh

# Run the tests
$BASH $SCRIPTS_DIR/automated_tests.sh
