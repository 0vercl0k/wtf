# WTF: I can fuzz ELF files with this?

This provides Linux ELF userland snapshotting support based on previous work by
[Kasimir](https://github.com/0vercl0k/wtf/pull/102) and scripts from [Snapchange](https://github.com/awslabs/snapchange/tree/main/qemu_snapshot)

## Limitations 

### Only runs on a Linux machine

Since the mem.dmp file created by the dump and convert process is not an actual
mem.dmp file the Windows build of WTF will error out while trying to read the
file since it seems to use winapi to parse the dump file. On Linux it seems to
only parse what is needed. This is not really too much of an issue since for
faster fuzzing people will use KVM anyways and someone needs Linux to take the
snapshot in the first place.

### Guest virtual machine RAM is limited to 2 GB

Currently, the snapshotting scripts are unable to create snapshots that are
readable by wtf when the target VM has more than 2 GB of memory.

### Symbolizing

TODO

## Setting up the environment

Change into the `linux_mode/qemu_snapshot` directory and run `setup.sh`:

```
user@pc:/wtf/linux_mode/qemu_snapshot$ ./setup.sh
```

This script installs all pre-requisite tools, compiles qemu, and builds a target
virtual machine consisting of a Linux kernel and disk image.

## Taking a snapshot

Create a subdirectory in `linux_mode` for your snapshot and create a `bkpt.py`
file, like [linux_mode/crash_test/bkpt.py](crash_test/bkpt.py):

```py
# imports
import sys, os

# import fuzzing breakpoint
from gdb_fuzzbkpt import *

target_dir = 'linux_crash_test'

# address to break on, found using gdb
break_address = 'do_crash_test'

# name of the file in which to break
file_name = 'a.out'

# create the breakpoint for the executable specified
FuzzBkpt(target_dir, break_address, file_name, sym_path=file_name)
```

* `target_dir` - subdirectory in `targets` to save the snapshot data
* `break_address` - address to break on. This can be a hardcoded address or a symbol if `sym_path` is provided
* `file_name` - name of the file in the target VM associated with the breakpoint
* `sym_path` - optional argument if you'd like symbols to be loaded

Start the virtual machine in one tab while in the snapshot subdirectory by running `../qemu_snapshot/gdb_server.sh`:

```console
user@pc:/wtf/linux_mode/crash_test$ ../qemu_snapshot/gdb_server.sh
```

In a separate tab, scp the target file to the target VM. With `crash_test` this can be done by first compiling the target file:

```console
user@pc:/wtf/linux_mode/crash_test$ gcc test.c
```

Then transfer the target file to the VM:

```console
user@pc:/wtf/linux_mode/crash_test$ pushd ../qemu_snapshot/target_vm
user@pc:/wtf/linux_mode/qemu_snapshot/target_vm$ ./scp.sh ../../crash_test/a.out
a.out                                                                                     100%   16KB   1.2MB/s   00:00

```

Go back to the `crash_test` directory.

```console
user@pc:/wtf/linux_mode/qemu_snapshot/target_vm$ popd
/wtf/linux_mode/crash_test
user@pc:/wtf/linux_mode/crash_test$
```

Now, run `../qemu_snapshot/gdb_client.sh`:

```console
user@pc:/wtf/linux_mode/crash_test$ ../qemu_snapshot/gdb_client.sh 
```

In the first tab, log in to the Linux machine (user `root`) and run the target file:

```console
linux login: root
Linux linux 6.7.0-rc3 #1 SMP PREEMPT_DYNAMIC Thu Nov 30 18:38:29 UTC 2023 x86_64

The programs included with the Debian GNU/Linux system are free software;
the exact distribution terms for each program are described in the
individual files in /usr/share/doc/*/copyright.

Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
permitted by applicable law.
A valid context for root could not be obtained.
Last login: Fri Dec  1 21:21:22 UTC 2023 on ttyS0
root@linux:~# ./a.out

Enter some input.
d
```

Once the breakpoint is hit, the second tab will start the snapshotting process:

```console
Continuing.
In right process? True
Calling mlockall
Saving 67 bytes at 555555555146
In right process? True
Restoring 67 bytes at 0x555555555146
Restored
In the Qemu tab, press Ctrl+C, run the `cpu` command
```

Once the second tab indicates to run the `cpu` command, press Ctrl+C and run the `cpu` command from the first tab:

```console
Thread 1 "qemu-system-x86" received signal SIGINT, Interrupt.
0x00007ffff77a4ebe in __ppoll (fds=0x5555568337d0, nfds=8, timeout=<optimized out>, timeout@entry=0x7fffffffdea0, sigmask=si
42	../sysdeps/unix/sysv/linux/ppoll.c: No such file or directory.
(gdb) cpu
cpu_state: 0x55555681e240
done...continuing debuggee
```

The second tab will detect once the first tab has finished executing the `cpu` command and continue creating a snapshot for the target VM.

Once the second tab indicates that snapshotting is complete, the target VM can be terminated.

```console
In the Qemu tab, press Ctrl+C, run the `cpu` command
Detected cpu registers dumped to regs.json
Connecting to Qemu monitor at localhost:55555
Connected
Instructing Qemu to dump physical memory to file raw
Done
Converting raw file raw to dump file /wtf/targets/linux_crash_test/state/mem.dmp
Done
mv regs.json /wtf/targets/linux_crash_test/state/regs.json
mv symbol-store.json /wtf/targets/linux_crash_test/state/symbol-store.json
Snapshotting complete

Breakpoint 1, 0x0000555555555189 in do_crash_test ()
(gdb)
```

## Harnessing and Fuzzing 

Writing harnesses is the same process as writing harnesses for Windows executables. Example harnesses for crash_test and page_fault_test are present in [src/wtf/fuzzer_linux_crash_test.cc](../src/wtf/fuzzer_linux_crash_test.cc) and [src/wtf/fuzzer_linux_page_fault_test.cc](../src/wtf/fuzzer_linux_page_fault_test.cc).

Now that we have everything set up we can start our server and fuzzer:

Provide a seed input:

```console
user@pc:/wtf/targets/linux_crash_test$ echo a>inputs/a
```

Run the master:

```console
user@pc:/wtf/targets/linux_crash_test$ ../../src/build/wtf master --name linux_crash_test --max_len=10
```

Run the fuzzee and note that crashes are found quickly.

```console
user@pc:/wtf/targets/linux_crash_test$ ../../src/build/wtf fuzz --backend=bochscpu --name linux_crash_test
Setting @fptw to 0xff'ff.
The debugger instance is loaded with 16 items
Setting debug register status to zero.
Setting debug register status to zero.
Setting mxcsr_mask to 0xffbf.
Dialing to tcp://localhost:31337/..
#113174 cov: 47 exec/s: 11.3k lastcov: 2.0s crash: 1782 timeout: 0 cr3: 0 uptime: 10.0s
```

To fuzz with KVM, create a coverage breakpoints file by loading the target file in IDA and running [scripts/gen_linux_coveragefile_ida.py](../scripts/gen_linux_coveragefile_ida.py). Transfer the coverage breakpoints file to the `coverage` subfolder in the target's directory. For example, for `linux_crash_test` transfer the coverage breakpoint file to `targets/linux_crash_test/coverage/a.cov`. Once transferred, KVM can be used for fuzzing:

```console
user@pc:/wtf/targets/linux_crash_test$ sudo ../../src/build/wtf fuzz --backend=kvm --name linux_crash_test
Setting @fptw to 0xff'ff.
The debugger instance is loaded with 16 items
Parsing coverage/a.cov..
Applied 44 code coverage breakpoints
Setting debug register status to zero.
Setting debug register status to zero.
Setting mxcsr_mask to 0xffbf.
Resolved breakpoint 0xffffffff82001240 at GPA 0x2001240 aka HVA 0x564428d2afe0
Resolved breakpoint 0xffffffff82000ff0 at GPA 0x2000ff0 aka HVA 0x564428d2cda0
Resolved breakpoint 0xffffffff81099dc0 at GPA 0x1099dc0 aka HVA 0x564428d2db80
Resolved breakpoint 0xffffffff810708e0 at GPA 0x10708e0 aka HVA 0x564428d2e6b0
Resolved breakpoint 0x5555555551e7 at GPA 0x972c1e7 aka HVA 0x564428d32117
Dialing to tcp://localhost:31337/..
#24348 cov: 8 exec/s: 2.4k lastcov: 3.0s crash: 871 timeout: 0 cr3: 0 uptime: 10.0s
```
