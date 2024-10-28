#!/bin/bash
$PWD/QEMU/build/qemu-system-x86_64 \
        -m 4G \
        -smp 1 \
        -kernel $PWD/linux/arch/x86_64/boot/bzImage \
        -append "console=ttyS0 root=/dev/sda earlyprintk=serial noapic ibpb=off ibrs=off kpti=0 l1tf=off mds=off mitigations=off no_stf_barrier noibpb noibrs pcil" \
        -machine type=pc,accel=kvm \
        -drive file=$PWD/image/bookworm.img \
        -net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
        -net nic,model=e1000 \
        -nographic \
        -pidfile vm.pid \
        2>&1 | tee vm.log

# machine type=q35,accel=kvm,dump-guest-core=on
