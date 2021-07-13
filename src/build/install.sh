# Axel '0vercl0k' Souchet - July 9 2020
# adduser over --disabled-password
sudo apt update
sudo apt install -y cmake cpu-checker clang-11 gdb g++-10 ninja-build screen
sudo usermod -a -G kvm `whoami`
git clone https://github.com/0vercl0k/wtf.git
cd wtf/src/build
CC=clang-11 CXX=clang++-11 ./build-release.sh