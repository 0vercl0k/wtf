#!/bin/bash

# get the current path
CUR_PATH=$(pwd)

# cd into the build directory
cd ../../src/build/

# build wtf with the compiler flag
export CC=clang
export CXX=clang++
export CFLAGS='-fsanitize=address -DELF_COMPILATION=1'
export CXXFLAGS='-fsanitize=address -DELF_COMPILATION=1'
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja && cmake --build .

# copy wtf into our path
cp wtf $CUR_PATH/