#!/bin/bash


# get our environmental variables
export LINUX_MODE_BASE=../
export WTF=${LINUX_MODE_BASE}../
PYTHON_SITE_PACKAGES=$(echo ../qemu_snapshot/target_vm/venv/lib/python3.*/site-packages/)
export PYTHONPATH=${PYTHONPATH}:${PYTHON_SITE_PACKAGES}
export PYTHONPATH=${PYTHONPATH}:${LINUX_MODE_BASE}/qemu_snapshot
export KERNEL=${LINUX_MODE_BASE}qemu_snapshot/target_vm/linux/vmlinux
export LINUX_GDB=${LINUX_MODE_BASE}qemu_snapshot/target_vm/linux/scripts/gdb/vmlinux-gdb.py

# initialize gdb
gdb \
    ${KERNEL} \
    -q \
    -ex "set pagination off" \
    -iex "add-auto-load-safe-path ${LINUX_GDB}" \
    -ex "set confirm off" \
    -ex "target remote localhost:1234" \
    -ex "python import sys; sys.path.insert(0, '${LINUX_MODE_BASE}qemu_snapshot/target_vm/linux/scripts/gdb')" \
    -ex "source ${LINUX_GDB}" \
    -x ./bkpt.py \
    -ex continue
