#!/bin/bash

VERSION="9.0.0"

URL="http://releases.llvm.org"
 
WGET=wget

CURRENT_DIR=`pwd`
INSTALL_DIR=`pwd`/install
BUILD_DIR=$CURRENT_DIR/build

# Using 2 threads by default
NUM_THREADS=2

SUFFIX=".tar.xz"
CLANG_SRC="cfe-$VERSION.src"
LLVM_SRC="llvm-$VERSION.src"

HPVM_RT=hpvm-rt/hpvm-rt.bc

AUTOMATE="y"

read -p "Build and install HPVM automatically? (y or n): " AUTOMATE   

if [ ! $AUTOMATE == "y" ] && [ ! $AUTOMATE == "n" ]; then 
  echo invalid input!
  exit -1
fi

echo
read -p "Number of threads: " NUM_THREADS

if [ ! $NUM_THREADS -gt 0 ]; then
  NUM_THREADS = 2
  echo
  echo Using $NUM_THREADS threads by default.   
  echo
fi


if [ -d $LLVM_SRC ]; then
    echo Found $LLVM_SRC, not dowloading it again!
elif [ -d llvm ]; then
    echo Found LLVM, not downloading it again!
else
    echo $WGET $URL/$VERSION/$LLVM_SRC$SUFFIX
    $WGET $URL/$VERSION/$LLVM_SRC$SUFFIX
    tar xf $LLVM_SRC$SUFFIX
    rm $LLVM_SRC$SUFFIX
fi

if [ -d $LLVM_SRC ]; then
    echo Everything looks sane.
    mv $LLVM_SRC llvm
elif [ -d llvm ]; then
    echo Everything looks sane.
else
    echo Problem with LLVM download. Exiting!
    exit
fi

LLVM_SRC=llvm

if [ -d $CURRENT_DIR/$LLVM_SRC/tools ]; then
    cd $CURRENT_DIR/$LLVM_SRC/tools
    echo In tools.
else
    echo Something is wrong with LLVM checkout. Exiting!
    exit 1
fi

if [ -d clang ]; then
    echo Found clang! Not downloading clang again.
else
    $WGET $URL/$VERSION/$CLANG_SRC$SUFFIX
    tar xf $CLANG_SRC$SUFFIX
    rm $CLANG_SRC$SUFFIX
    mv $CLANG_SRC clang
    if [ -d clang ]; then
	echo Everything looks sane.
    else
	echo Problem with clang download. Exiting!
	exit
    fi
fi

cd $CURRENT_DIR

HPVM_DIR=$CURRENT_DIR/$LLVM_SRC/tools/hpvm

if [ ! -d $HPVM_DIR ]; then
  echo Adding HPVM sources to tree
  mkdir -p $HPVM_DIR
  ln -s $CURRENT_DIR/CMakeLists.txt $HPVM_DIR
  ln -s $CURRENT_DIR/include $HPVM_DIR/
  ln -s $CURRENT_DIR/lib $HPVM_DIR/
  ln -s $CURRENT_DIR/projects $HPVM_DIR/
  ln -s $CURRENT_DIR/test $HPVM_DIR/
  ln -s $CURRENT_DIR/tools $HPVM_DIR/
else
  echo $CURRENT_DIR/$LLVM_SRC/tools/hpvm exists.
fi

export LLVM_SRC_ROOT=$CURRENT_DIR/$LLVM_SRC

echo Applying HPVM patches
cd $CURRENT_DIR/llvm_patches
/bin/bash ./construct_patch.sh
/bin/bash ./apply_patch.sh

echo Patches applied.

if [ ! $AUTOMATE == "y" ]; then
  echo
  echo HPVM not installed. Exiting. 
  exit  
fi

echo
echo Now building...

echo Using $NUM_THREADS threads to build HPVM.
echo

cd $CURRENT_DIR

if [ ! -d $BUILD_DIR ]; then
  mkdir -p $BUILD_DIR
fi

if [ ! -d $INSTALL_DIR ]; then
  mkdir -p $INSTALL_DIR
fi

cd $BUILD_DIR
echo cmake ../$LLVM_SRC -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DLLVM_TARGETS_TO_BUILD="X86"  -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR
cmake ../$LLVM_SRC -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DLLVM_TARGETS_TO_BUILD="X86"  -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR

echo make -j$NUM_THREADS
make -j$NUM_THREADS
#make install


if [ -f $BUILD_DIR/tools/hpvm/projects/$HPVM_RT ]; then
    true
else
    echo $BUILD_DIR/tools/hpvm/projects/$HPVM_RT
    echo HPVM not installed properly.
    exit 0
fi

cd $CURRENT_DIR


