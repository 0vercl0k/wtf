#!/bin/bash

DEPTH="--depth 1"

# Default to v5.15, latest version of Linux that's supported in fuzzbkpt.py
# Earliest Linux version tested is v5.15
# Latest Linux version tested is 6.7.0-rc3

# Parse the command arguments
# --kernel-version v5.4
# --with-kasan
# --full
# -h | --help
while [[ $# -gt 0 ]]; do
  case $1 in
    --kernel-version)
      VERSION="$2"
      # Shift past the argument
      shift
      # Shift past the value
      shift
      ;;
    --with-kasan)
      KASAN=1
      # Shift past the argument
      shift
      ;;
    --full)
      DEPTH=
      # Shift past the argument
      shift
      ;;
    -h|--help)
        echo "Usage: "
        echo "./init.sh [--kernel-version <linux_branch>] [--with-kasan]"
        echo "Example:"
        echo "./init.sh"
        echo "./init.sh --kernel-version v5.4 --with-kasan"
        exit 0
      ;;
    *)
      echo "Unknown argument: $1 | Options [--kernel-version|--with-kasan]"
      exit 0
      ;;
  esac
done

# Immediately stop execution if an error occurs
set -e

# Use GCC 9 for Ubuntu 22 and GCC 8 for everything else
if cat /etc/*rel* | grep "Ubuntu 22"; then
        GCC=9
else
        GCC=8
fi


download_prereqs() {
    # Ensure prereqs are installed
    sudo apt install -y gcc-$GCC g++-$GCC clang make ninja-build debootstrap libelf-dev \
         libssl-dev pkg-config flex bison gdb

    sudo apt-get install -y libglib2.0-dev libpixman-1-dev python3-pip cmake

    pip3 install pwntools

    # If there isn't a bookworm script for debootstrap (like in Ubuntu 18.04), copy
    # over the bullseye script as it is the same
    if [ ! -f /usr/share/debootstrap/scripts/bookworm ]; then
        sudo cp /usr/share/debootstrap/scripts/bullseye /usr/share/debootstrap/scripts/bookworm
    fi
}

# Download and build an Linux image for use in QEMU snapshots
download_linux() {
    # If the bzImage already exists, no need to rebuild
    if [ -f ./linux/arch/x86/boot/bzImage ]; then
        return
    fi

    # If no specific linux kernel given, download the entire kernel
    if [ -z "$VERSION" ]; then
        echo "Downloading latest linux kernel"
        git clone $DEPTH https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
    else
        echo "Downloading kernel version: $VERSION"
        git clone $DEPTH --branch "$VERSION" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
    fi

    pushd linux
    make defconfig
    echo CONFIG_CONFIGFS_FS=y            >> .config
    echo CONFIG_SECURITYFS=y             >> .config
    echo CONFIG_DEBUG_INFO=y             >> .config
    echo CONFIG_DEBUG_INFO_DWARF4=y      >> .config
    echo CONFIG_RELOCATABLE=n            >> .config
    echo CONFIG_RANDOMIZE_BASE=n         >> .config
    echo CONFIG_GDB_SCRIPTS=y            >> .config
    echo CONFIG_DEBUG_INFO_REDUCED=n     >> .config

    # Only enable KASAN if asked to
    if [[ "$KASAN" ]]; then
        echo CONFIG_KASAN=y >> .config
    fi

    # If gcc is not in the path already, set gcc to the active gcc
    if ! which gcc; then
        sudo ln -s `which gcc-$GCC` /usr/bin/gcc
    fi

    yes "" | make -j`nproc` bzImage
    make scripts_gdb
    popd
}

download_qemu() {
    if [ -f ./qemu/build/qemu-system-x86_64 ]; then
        return
    fi

    # Set up Qemu with debug build
    git clone -b v7.1.0 https://github.com/qemu/qemu
    pushd qemu
    mkdir build
    cd build
    CXXFLAGS="-g" CFLAGS="-g" ../configure --cpu=x86_64 --target-list="x86_64-softmmu x86_64-linux-user"
    make
    popd
}

init_debian_image() {
    pushd image
    ./create-image.sh
    popd
}

download_prereqs

download_qemu

# Pass the command line arguments to check for specific kernel version
download_linux $*

init_debian_image
