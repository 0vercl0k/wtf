#!/bin/bash

# get our environmental variables
export LINUX_MODE_BASE=../
export WTF=${LINUX_MODE_BASE}../
export PYTHONPATH=${PYTHONPATH}:${LINUX_MODE_BASE}/qemu_snapshot
export KERNEL=${LINUX_MODE_BASE}qemu_snapshot/target_vm/linux/vmlinux
export LINUX_GDB=${LINUX_MODE_BASE}qemu_snapshot/target_vm/linux/scripts/gdb/vmlinux-gdb.py

# initialize gdb
gdb \
    ${KERNEL} \
    -q \
    -iex "add-auto-load-safe-path ${LINUX_GDB}" \
    -ex "set confirm off" \
    -ex "target remote localhost:1234" \
    -x ./bkpt.py \
    -ex continue
