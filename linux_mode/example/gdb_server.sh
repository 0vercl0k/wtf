#!/bin/bash

source vars

gdb \
	-x ../scripts/qemu.py \
	--args ${QEMU_SYSTEM_X86_64} \
	-name guest=archlinux,debug-threads=on \
	-blockdev node-name=node-A,driver=qcow2,file.driver=file,file.node-name=file,file.filename=${ARCHLINUX} \
	-device virtio-blk,drive=node-A,id=virtio0 \
	-s \
	-machine dump-guest-core=on \
	-device vmcoreinfo \
	-m 1024M 
