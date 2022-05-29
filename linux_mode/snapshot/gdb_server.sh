#!/bin/bash

# get our environmental variables
source ../vars

# start qemu with gdb
gdb \
        -x ../scripts/qemu.py \                         # this script registers the cpu command to dump the cpu state
        --args ${QEMU_SYSTEM_X86_64} \                  # start debug version of qemu 
        -name guest=archlinux,debug-threads=on \
        -blockdev node-name=node-A,driver=qcow2,file.driver=file,file.node-name=file,file.filename=${ARCHLINUX} \
        -device virtio-blk,drive=node-A,id=virtio0 \
        -s \
        -machine dump-guest-core=on \
        -device vmcoreinfo \
        -monitor tcp:127.0.0.1:55555,server,nowait \    # creates the qemu monitor we can nc into
        -m 1024M \                                      # sets image memory, kept low for faster dumps but can be updated
        -net user,hostfwd=tcp::10022-:22 -net nic       # sets up ssh connection on port 10022