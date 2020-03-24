for i in $(find . -name '*.ll'); do
  ../../../build/bin/llvm-cbe $i
  done
