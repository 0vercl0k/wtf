# WTF: I can fuzz ELF files with this?

A little while ago I was shown [this blog post](https://habr-com.translate.goog/en/company/dsec/blog/664230/?_x_tr_sl=auto&_x_tr_tl=en) where someone had done some work in order to get WTF working with ELF files. The blog talked about using qemu to take snapshots of a program and included code in order to convert the dump from qemu into a format that was readable by WTF. The blog seemed very promising so I started trying to replicate the work done by the blog on other ELF files. During that process I updated the code so that it would run with the most recent version of WTF without needing a custom build and updated the python scripts to work with the newer versions of GDB. I also documented and automated the process for setting up the tools needed to take snapshots and setting up the fuzzer, all of which are outlined below.

## Limitations 

### Only runs on a linux machine

Since the mem.dmp file created by the dump and convert process is not an actual mem.dmp file the windows build of WTF will error out while trying to read the file since it seems to use winapi to parse the dump file. On linux it seems to only parse what is needed. This is not really too much of an issue since for faster fuzzing people will use KVM anyways and someone needs linux to take the snapshot in the first place.

### ASLR needs to be turned off

The way the scripts are currently set up, ASLR needs to be turned off. This was done by Gayanov for convenience and could be changed in future releases.

### Tested for Bochscpu

I have only tested this process for Bochscpu. Gayanov's repository also contained some changes to KVM for the CS register, but I was unable to test whether or not that was required since I do not have KVM set up on my end. 

## Setting up the environment

In order to set up the environment for taking the snapshots you need to run `setup.sh`. This script installs all of the tools that you need to compile qemu, run the python scripts, and compile WTF. Then the script clones qemu and builds the debug version of qemu for x86_64 so that we can take our snapshot. And then finally, the script compiles raw2dmp, a program that is used to convert the raw dump from the qemu monitor to a mem.dmp file for WTF. 

While the script is running, update the `CUR_PATH` variable in `vars` to be the absolute path to the linux_mode directory. The vars file sets variables that are then used in various other programs such as the path to the debug build of qemu, the kernel image and qcow, and the python scripts in the scripts directory. 

And the final step for setting up the environment is downloading the [image](https://github.com/Kasimir123/wtf/releases/download/v0.4/vmlinux-5.17.4-arch1.tar.gz) and [qcow](https://github.com/Kasimir123/wtf/releases/download/v0.4/archlinux-root.qcow2.tar.gz) for qemu and putting them in the linux_mode directory. The image is the same as from the blog post, the qcow has been stripped clean from examples and can be used as a clean slate for anyone's fuzzing projects. The user for the system is `root` and the password is `password`. If you change the names for either of these files remember to update it inside of the vars file.

## Taking a snapshot

Once you have your environment set up its time to get a snapshot so that we can start fuzzing. As an easy example I made a [small test file](https://gist.github.com/Kasimir123/4dbd12793177192051bb01f31a37930e) that can be compiled to test taking a snapshot and writing a fuzzer. 

The first step of the snapshot process is trying to figure out where to put your breakpoint. In order to do this you can use gdb on your machine. If you compiled the test I would recommend setting a breakpoint before the call to `print_char`. This can be done after loading the compiled test binary into gdb and running:

```
break test.c:28
run
disassemble
```

At which point you would see something like:

```
=> 0x00005555555551d8 <+27>:    movsbl -0x1(%rbp),%eax
   0x00005555555551dc <+31>:    mov    %eax,%edi
   0x00005555555551de <+33>:    call   0x555555555169 <print_char>
```

Now you can update `bpkt.py` to reflect these new values. For our example, `break_address` would become `'0x00005555555551de'` and `file_name` would become `'test'`. 

Now that you have the breakpoint set its time to upload the file to the qemu image. To do this you will need 2 terminal tabs. In the first run the `gdb_server.sh` script. And in the second run `qemu_file_upload.sh` with the path to the file to upload. It may take a little bit since the image needs to boot up but you will then be prompted for a password at which point you can enter `password`. This will put your file into the home directory of the root user. You can verify that the file has been uploaded by ssh'ing into the image with `ssh root@localhost -p 10022`.

Once the file is uploaded it is time to actually take our snapshot. For this you will need 4 terminal tabs. In the first tab run the `gdb_server.sh` script. In the second tab run the `gdb_client.sh` script. In the third tab ssh into the qemu image and run the executable that you want to snapshot. At this point you should see a lot of text in the second tab, wait till this says "vmmaped, ready to dump". Then go to your fourth terminal tab and nc into the qemu monitor with `nc localhost 55555`. Once the monitor is loaded run `pmemsave 0 0xffffffff raw` and wait for the dump to finish. Once the dump is complete, go back to the first terminal window and press ctrl + c. You should see a gdb prompt, at which point type `cpu` and press enter. Once complete you can exit out of the terminal tabs.

Once everything is closed run the `move_to_fuzzer.sh` script with the name of your target as the command line argument. This will convert the raw file into a mem.dmp file and will move mem.dmp, regs.json, and symbol-store.json into `wtf/targets/<name you specified>`. This script also created all the required folders needed for WTF to run.

## Harnessing, Compiling, and Fuzzing 

Now that the snapshot is done we can move to the fun part, writing our harness. Writing the harness for ELF files is the same process as writing harnesses for windows executables so I will just provide the [harness for the test file](https://gist.github.com/Kasimir123/965074d873639e3964976abe896d07de). Put this file into `wtf/src/wtf` and then go into our target folder. Now we just need to run `recompile_wtf.sh` which will build the elf build of WTF and place it into the target folder. For our test file we need to create an input so lets just run `echo 'a' >> inputs/a` and then remove the newline at the end of the file. 

Now that we have everything set up we can start our server and fuzzer:

```
./wtf master --max_len=1 --runs=10000000 --target . --name test
```

```
./wtf fuzz --backend=bochscpu --name test --limit 100000000
```

When run we can see that it finds all of the crashes!!

If you want to compile for windows, simply run `cmake --build . --target clean` and remove the cached cmake files in the build folder.