#!/bin/bash

if [ -d "$1" ]; then
  cd $1
  for i in *.bin; do
    echo "converting $i!"
    ../scripts/load_and_convert.py -b $i
  done
else echo "Please provide desired directory"
fi
