#!/bin/bash

# get our environmental variables
export LINUX_MODE_BASE=../
export GDB_QEMU_PY_SCRIPT=${LINUX_MODE_BASE}qemu_snapshot/gdb_qemu.py
export QEMU=${LINUX_MODE_BASE}qemu_snapshot/target_vm/qemu/build/qemu-system-x86_64
export KERNEL=${LINUX_MODE_BASE}qemu_snapshot/target_vm/linux/arch/x86_64/boot/bzImage
export IMAGE=${LINUX_MODE_BASE}qemu_snapshot/target_vm/image/bookworm.img

gdb \
    --ex "set confirm off" \
    --ex "starti" \
    --ex "handle SIGUSR1 noprint nostop" \
    -x ${GDB_QEMU_PY_SCRIPT} \
    --args ${QEMU} \
    -m 2G \
    -smp 1 \
    -kernel ${KERNEL} \
    -append "console=ttyS0 root=/dev/sda earlyprintk=serial noapic ibpb=off ibrs=off kpti=0 l1tf=off mds=off mitigations=off no_stf_barrier noibpb noibrs pcil" \
    -machine type=pc,accel=kvm \
    -drive file=${IMAGE} \
    -net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
    -monitor tcp:127.0.0.1:55555,server,nowait \
    -s \
    -net nic,model=e1000 \
    -nographic \
    -pidfile vm.pid \
    2>&1 | tee vm.log
