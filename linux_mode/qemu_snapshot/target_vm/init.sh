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
    -h | --help)
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

download_prereqs() {
    # Ensure prereqs are installed
    sudo apt install -y gcc-11 g++-11 clang make ninja-build debootstrap libelf-dev \
        libssl-dev pkg-config flex bison gdb libc6 lsb-release software-properties-common

    sudo apt-get install -y python3-venv libglib2.0-dev libpixman-1-dev python3-pip cmake

    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    sudo add-apt-repository -y "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-20 main"

    # Update and install
    sudo apt update
    sudo apt install -y clang-20 lldb-20 lld-20

    # Set as default (optional) - disabled to use gcc instead of clang
    sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100
    sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100
    sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-20 100

    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100
    python3 -m venv venv

    # activate python venv
    source venv/bin/activate
    pip3 install pwntools setuptools lief tomli
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
    echo CONFIG_CONFIGFS_FS=y >>.config
    echo CONFIG_SECURITYFS=y >>.config
    echo CONFIG_DEBUG_INFO=y >>.config
    echo CONFIG_DEBUG_INFO_DWARF4=y >>.config
    echo CONFIG_RELOCATABLE=n >>.config
    echo CONFIG_RANDOMIZE_BASE=n >>.config
    echo CONFIG_GDB_SCRIPTS=y >>.config
    echo CONFIG_DEBUG_INFO_REDUCED=n >>.config

    # Only enable KASAN if asked to
    if [[ "$KASAN" ]]; then
        echo CONFIG_KASAN=y >>.config
    fi

    # If gcc is not in the path already, set gcc to the active gcc
    if ! which gcc; then
        sudo ln -s $(which gcc-$GCC) /usr/bin/gcc
    fi

    yes "" | make -j$(nproc) bzImage
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
    CC=gcc CXX=g++ CXXFLAGS="-g" CFLAGS="-g -w" ../configure --cpu=x86_64 --target-list="x86_64-softmmu x86_64-linux-user"
    make
    popd
}

init_debian_image() {
    pushd image
    ./create-image.sh
    popd
}

# Add the current user to the kvm group; QEMU uses the KVM api (/dev/kvm) for acceleration.
sudo usermod -aG kvm `whoami`

download_prereqs
sudo -v

download_qemu
sudo -v

# Pass the command line arguments to check for specific kernel version
download_linux $*
sudo -v

init_debian_image
