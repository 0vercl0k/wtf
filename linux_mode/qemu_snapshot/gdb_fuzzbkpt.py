# Jason Crowder - February 2024
# imports
import gdb
import os
import re
import sys
import inspect
import time
import socket
import select
import struct
import subprocess
import pathlib

currentdir = pathlib.Path(inspect.getfile(inspect.currentframe()))
currentdir = currentdir.absolute().parent
sys.path.insert(0, str(currentdir.parent / "qemu_snapshot"))

import gdb_utils

# set environmental variables
os.environ["PWNLIB_NOTERM"] = "1"
import pwn

# set architecture
pwn.context.arch = "amd64"

SYMSTORE_FILENAME = pathlib.Path("symbol-store.json")
RAW_FILENAME = pathlib.Path("raw")
DMP_FILENAME = pathlib.Path("mem.dmp")
REGS_JSON_FILENAME = pathlib.Path("regs.json")


class dump_file:
    PAGE_SIZE = 0x1000

    HEADER64_SIZE = 8248
    EXPECTED_SIGNATURE = b"PAGE"
    EXPECTED_VALID_DUMP = b"DU64"
    BMPDUMP = 5

    BMP_HEADER64_SIZE = 56
    BMP_EXPECTED_SIGNATURE = b"SDMP"
    BMP_EXPECTED_VALID_DUMP = b"DUMP"

    def convert_raw_to_dmp(out_filename: pathlib.Path):
        dump_size = RAW_FILENAME.stat().st_size
        pages_count = int(dump_size / dump_file.PAGE_SIZE)
        bitmap_size = int(pages_count / 8)

        out_file = out_filename.open("wb")

        print(f"Converting raw file '{RAW_FILENAME}' to dump file '{out_filename}'")

        d = dump_file.EXPECTED_SIGNATURE
        assert len(d) == 4

        d += dump_file.EXPECTED_VALID_DUMP
        assert len(d) == 8

        d += b"\x00" * (3992 - len(d))
        assert len(d) == 3992

        d += struct.pack("<L", dump_file.BMPDUMP)
        assert len(d) == 3996

        d += b"\x00" * (8192 - len(d))
        assert len(d) == 8192

        # BMP_HEADER64
        b = dump_file.BMP_EXPECTED_SIGNATURE
        assert len(b) == 4

        b += dump_file.BMP_EXPECTED_VALID_DUMP
        assert len(b) == 8

        b += b"\x00" * (32 - len(b))
        assert len(b) == 32

        # FirstPage
        b += struct.pack("<Q", dump_file.HEADER64_SIZE + bitmap_size)
        assert len(b) == 40

        # Pad
        b += b"\x00" * (48 - len(b))
        assert len(b) == 48

        # Pages
        b += struct.pack("<Q", pages_count)
        assert len(b) == 56

        b += b"\x00" * (dump_file.BMP_HEADER64_SIZE - len(b))
        assert len(b) == dump_file.BMP_HEADER64_SIZE

        d += b
        assert len(d) == dump_file.HEADER64_SIZE

        # bitmap
        d += b"\xff" * bitmap_size

        out_file.write(d)

        with RAW_FILENAME.open("rb") as f:
            while in_bytes := f.read(10 * 2**20):
                out_file.write(in_bytes)

        out_file.close()

        RAW_FILENAME.unlink()
        print("Done")


class qemu_monitor:
    HOSTNAME = "localhost"
    PORT = 55555

    def setup_sock():
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((qemu_monitor.HOSTNAME, qemu_monitor.PORT))
        return s

    def wait_ready(s):
        data_buf = b""
        sock_list = [s]
        while True:
            r_socks, _, _ = select.select(sock_list, [], [])
            for sock in r_socks:
                d = sock.recv(4096)
                if not d:
                    raise Exception("Disconnected")
                data_buf += d
            if data_buf.endswith(b"\n(qemu) "):
                break
            time.sleep(0.1)
        return data_buf[: -len(b"(qemu) ")]

    def write_phys_mem_file_to_disk():
        print(f"Connecting to Qemu monitor at {qemu_monitor.HOSTNAME}:{qemu_monitor.PORT}")
        s = qemu_monitor.setup_sock()
        print("Connected")
        qemu_monitor.wait_ready(s)

        print(f"Instructing Qemu to dump physical memory into '{RAW_FILENAME}'")
        s.send(f"pmemsave 0 0xffffffff {RAW_FILENAME}\n".encode())
        qemu_monitor.wait_ready(s)
        print("Done")


class kernel:
    @staticmethod
    def parse_and_eval(cmd):
        # if an old version of gdb we can use parse and eval
        if gdb.VERSION.startswith("6.8.50.2009"):
            return gdb.parse_and_eval(cmd)

        # otherwise we have to do this :/
        gdb.execute("set logging redirect on")
        gdb.execute(
            "set logging enabled"
        )  # older versions of gdb do not support set logging enabled on so we need to use this.
        gdb.execute(f"print {cmd}")
        gdb.execute("set logging enabled off")
        return gdb.history(0)

    # reads the qword at an address
    read_qword = lambda addr: int(
        str(gdb.execute(f"""x/gx {addr}""", to_string=True)).split()[-1], base=16
    )

    read_byte = lambda addr: int(
        str(gdb.execute(f"""x/1bx {addr}""", to_string=True)).split()[-1], base=16
    )

    # writes a byte to an address
    write_byte = lambda addr, b: gdb.execute(
        f"""set *((char*)({addr}))={b}""", to_string=True
    )

    # evaluates a command and extracts the hex address from the returned string
    @staticmethod
    def parse_variable(cmd):
        def search_hex(s):
            match = re.search("0x[a-fA-F0-9]+", s)
            if match:
                return match.group()
            return ""

        raw = str(kernel.parse_and_eval(cmd))
        raw_hex = search_hex(raw)
        val = int(raw) if raw_hex == "" else int(raw_hex, base=16)
        return val

    # gets structures related to the cpu
    class cpu:
        get_reg = lambda reg: kernel.parse_variable(f"""${reg}""")
        set_reg = lambda reg, val: gdb.execute(f"""set ${reg}={val}""", to_string=True)

        # gets structures related to the task

    class task:
        @staticmethod
        def get_name():
            task_struct = str(kernel.parse_and_eval("&$lx_current()"))

            process_name = str(
                kernel.parse_and_eval(f"((struct task_struct*){task_struct})->comm")
            )

            process_name = process_name.replace('"', "")
            process_name = process_name[: process_name.find("\\0")]

            return process_name


# Fuzzing Breakpoint Class, wrapper for gdb breakpoint
class FuzzBkpt(gdb.Breakpoint):
    # intializes the breakpoint
    def __init__(
        self,
        target_dir,
        addr,
        program_name,
        checkname=True,
        bp_hits_required=1,
        target_base=0x555555554000,
        sym_path=None,
    ):
        target_syms_dict = {}

        if sym_path:
            text_offset = self.get_text_offset(sym_path)

            gdb.execute(f"add-symbol-file {sym_path} {target_base + text_offset}")

            target_syms_list = self.get_target_syms(sym_path)
            for name, rva in target_syms_list:
                va = target_base + rva
                target_syms_dict[name] = hex(va)

        target_syms_dict[program_name] = hex(target_base)

        gdb_utils.write_to_store(target_syms_dict)

        print("Removing '{REGS_JSON_FILENAME}' file if it exists...")
        REGS_JSON_FILENAME.unlink(missing_ok=True)

        # convert address into format that gdb takes: break *0xFFFF
        loc = f"""*{addr}"""

        # intialize the gdb breakpoint
        gdb.Breakpoint.__init__(self, spec=loc, type=gdb.BP_HARDWARE_BREAKPOINT)

        target_dir = pathlib.Path(os.environ["WTF"]) / "targets" / target_dir
        target_dir = target_dir.absolute().resolve()
        print(f"Using '{target_dir}' as target directory")
        self.target_dir = target_dir

        print(f"mkdir '{target_dir}'")
        target_dir.mkdir(exist_ok=True)

        dirs = ("crashes", "inputs", "outputs", "state")
        for d in dirs:
            new_dir = self.target_dir / d
            print(f"mkdir '{new_dir}'")
            new_dir.mkdir(exist_ok=True)

        # set the program name and whether or not we should check the name
        self.program_name = program_name
        self.checkname = checkname
        self.bp_hits = 0
        self.bp_hits_required = bp_hits_required

        self.got_base = False

        # save the addresses for the kernel exception handlers
        self.save_kernel_exception_handlers()

        self.orig_bytes = None
        self.start_orig_rip = None

        self.mlock = False

        self.did_snapshot = False

    # function that is called when the breakpoint is hit
    def stop(self):
        if self.did_snapshot:
            return False

        print(f"In right process? {self.is_my_program()}")
        if self.checkname and not self.is_my_program():
            return False
        self.bp_hits += 1
        if self.bp_hits < self.bp_hits_required:
            print(f"Hit bp {self.bp_hits} time, but need to hit it {self.bp_hits_required} times")
            return False

        if not self.mlock:
            self.call_mlockall()
            self.mlock = True
            return False

        self.restore_orig_bytes()

        def wait_for_cpu_regs_dump():
            print("In the QEMU tab, press Ctrl+C, run the `cpu` command")
            while not REGS_JSON_FILENAME.exists():
                time.sleep(1)

            file_size = REGS_JSON_FILENAME.stat()
            # Make sure entirety of regs file has been written
            while True:
                time.sleep(1)
                new_file_size = REGS_JSON_FILENAME.stat()
                if file_size == new_file_size:
                    break
                file_size = new_file_size
            print(f"Detected cpu registers dumped to '{REGS_JSON_FILENAME}'")
            # sleep for a few seconds to allow Qemu to continue
            time.sleep(3)

        wait_for_cpu_regs_dump()

        qemu_monitor.write_phys_mem_file_to_disk()

        out_filename = self.target_dir / "state" / DMP_FILENAME
        dump_file.convert_raw_to_dmp(out_filename)

        files = (REGS_JSON_FILENAME, SYMSTORE_FILENAME)
        for f in files:
            dst = self.target_dir / "state" / f
            print(f"mv '{f}' '{dst}'")
            f.replace(dst)
        print("Snapshotting complete")
        self.did_snapshot = True
        return True

    # checks if the current program is the program we wanted
    #   does this by checking if the program names match
    def is_my_program(self):
        curr_program_name = self.get_name()
        return self.program_name in curr_program_name

    def get_target_syms(self, target_file):
        syms = []
        output = subprocess.check_output(["nm", target_file]).decode().split("\n")
        for line in output:
            try:
                (rva, t, name) = line.split(" ")
                rva = int(rva, 16)
            except ValueError:
                continue

            if t not in ("t", "T"):
                continue

            syms.append((name, rva))

        return syms

    def get_text_offset(self, target_file):
        output = (
            subprocess.check_output(["readelf", "-S", target_file]).decode().split("\n")
        )

        text_info_line = None
        for line in filter(lambda x: ".text" in x, output):
            text_info_line = line
            break

        # Example:
        #   [16] .text             PROGBITS         0000000000001100  00001100
        offset = int(text_info_line.split()[-2], 16)

        return offset

    # saves all of the kernel exception handlers
    #   used for context switch or crashes
    def save_kernel_exception_handlers(self):
        entry_syscall = kernel.parse_variable("entry_SYSCALL_64")
        asm_exc_page_fault = kernel.parse_variable("asm_exc_page_fault")
        asm_exc_divide_error = kernel.parse_variable("asm_exc_divide_error")
        force_sigsegv = kernel.parse_variable("force_sigsegv")
        page_fault_oops = kernel.parse_variable("page_fault_oops")

        gdb_utils.write_to_store(
            {
                "entry_syscall": hex(entry_syscall),
                "asm_exc_page_fault": hex(asm_exc_page_fault),
                "asm_exc_divide_error": hex(asm_exc_divide_error),
                "force_sigsegv": hex(force_sigsegv),
                "page_fault_oops": hex(page_fault_oops),
            }
        )

    # gets the name of the current running process
    def get_name(self):
        return kernel.task.get_name()

    def save_orig_bytes(self, start_addr, num_bytes):
        if self.orig_bytes is None:
            print(f"Saving {num_bytes} bytes at 0x{start_addr:x}")
            self.orig_bytes = []
            self.start_orig_rip = start_addr
            addr_to_read = start_addr
            for i in range(num_bytes):
                self.orig_bytes.append(kernel.read_byte(addr_to_read))
                addr_to_read += 1
        elif start_addr < self.start_orig_rip:
            num_bytes = self.start_orig_rip - start_addr
            print(
                f"Saving {num_bytes} bytes from 0x{start_addr:x} to 0x{self.start_orig_rip:x}"
            )
            self.start_orig_rip = start_addr
            addr_to_read = start_addr
            prepend_bytes = []
            for i in range(num_bytes):
                prepend_bytes.append(kernel.read_byte(addr_to_read))
                addr_to_read += 1
            self.orig_bytes = prepend_bytes + self.orig_bytes

    def restore_orig_bytes(self):
        if self.orig_bytes is None:
            return
        addr_to_write = self.start_orig_rip
        print(f"Restoring {len(self.orig_bytes)} bytes at 0x{addr_to_write:x}")
        for b in self.orig_bytes:
            kernel.write_byte(addr_to_write, b)
            addr_to_write += 1
        print("Restored")

    def call_mlockall(self):
        print("Calling mlockall")

        # assembly code to call mlock. Saves and restores all registers so they are
        #   unaffected by the call.
        mlockall = """
            push rax
            push rbx
            push rcx
            push rdx
            push rbp
            push rdi
            push rsi
            push r8
            push r9
            push r10
            push r11
            push r12
            push r13
            push r14
            push r15
            mov rax, 0x97
            mov rdi, 0x3
            syscall
         """

        # Used to detect when mlockall fails. You will see your target stop like this:
        # $ ./a.out
        # Press enter.
        #
        # Trace/breakpoint trap (core dumped)
        mlockall += """
            test eax, eax
            jz no_err
            int3
        no_err:
        """

        mlockall += """
            pop r15
            pop r14
            pop r13
            pop r12
            pop r11
            pop r10
            pop r9
            pop r8
            pop rsi
            pop rdi
            pop rbp
            pop rdx
            pop rcx
            pop rbx
            pop rax
        """

        # assemble the shellcode
        shellcode = pwn.asm(mlockall)

        # gets the current instruction pointer
        rip = kernel.cpu.get_reg("rip")

        # get the address for the shellcode
        shellcode_addr = rip - len(shellcode)

        self.save_orig_bytes(shellcode_addr, len(shellcode))

        # write the shellcode

        # run the original code that was ahead of the ip so that we can restore it.
        addr_to_write = shellcode_addr
        for b in shellcode:
            kernel.write_byte(addr_to_write, b)
            addr_to_write += 1

        # set the instruction pointer to the start of the shellcode
        kernel.cpu.set_reg("rip", shellcode_addr)
