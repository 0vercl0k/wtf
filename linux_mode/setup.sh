#!/bin/bash

# install dependencies
sudo apt-get install -y ninja-build pkg-config libglib2.0-dev libpixman-1-dev python3-pip clang cmake

# pip install
pip3 install pwntools

# Set up Qemu with debug build
git clone https://github.com/qemu/qemu 
cd qemu
mkdir build 
cd build
CXXFLAGS="-g" CFLAGS="-g" ../configure --cpu=x86_64 --target-list="x86_64-softmmu x86_64-linux-user" 
make

# compile raw2dmp
cd ../../raw2dmp
make