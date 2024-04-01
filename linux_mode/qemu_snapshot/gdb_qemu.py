# Jason Crowder - February 2024
# imports
import gdb, json, sys, pathlib

sys.path.insert(1, str(pathlib.Path(__file__).parent))

import gdb_utils

REGS_JSON_FILENAME = pathlib.Path("regs.json")

cpu_state = 0


class QemuBkpt(gdb.Breakpoint):
    def __init__(self, function):
        gdb.Breakpoint.__init__(
            self, function=function, type=gdb.BP_HARDWARE_BREAKPOINT
        )

    def stop(self):
        global cpu_state
        cpu_state = gdb.parse_and_eval("cpu")


# creates the cpu command used to dump the cpu state
class DumpCPUStateCommand(gdb.Command):
    # function init
    def __init__(self):
        super(DumpCPUStateCommand, self).__init__(
            "cpu", gdb.COMMAND_USER, gdb.COMPLETE_FILENAME
        )

    # dump state to the file passed to the function
    def dump(self, f):
        # grabs a register
        def get_reg(x):
            global cpu_state
            return gdb.parse_and_eval(
                f"""((CPUX86State*)((CPUState*)({cpu_state}))->env_ptr)->{x}"""
            )

        data = {}

        data["rax"] = hex(get_reg("regs[0]"))
        data["rcx"] = hex(get_reg("regs[1]"))
        data["rdx"] = hex(get_reg("regs[2]"))
        data["rbx"] = hex(get_reg("regs[3]"))
        data["rsp"] = hex(get_reg("regs[4]"))
        data["rbp"] = hex(get_reg("regs[5]"))
        data["rsi"] = hex(get_reg("regs[6]"))
        data["rdi"] = hex(get_reg("regs[7]"))
        data["r8"] = hex(get_reg("regs[8]"))
        data["r9"] = hex(get_reg("regs[9]"))
        data["r10"] = hex(get_reg("regs[10]"))
        data["r11"] = hex(get_reg("regs[11]"))
        data["r12"] = hex(get_reg("regs[12]"))
        data["r13"] = hex(get_reg("regs[13]"))
        data["r14"] = hex(get_reg("regs[14]"))
        data["r15"] = hex(get_reg("regs[15]"))

        data["rip"] = hex(get_reg("eip"))
        data["rflags"] = hex(get_reg("eflags"))
        data["dr0"] = hex(get_reg("dr[0]"))
        data["dr1"] = hex(get_reg("dr[1]"))
        data["dr2"] = hex(get_reg("dr[2]"))
        data["dr3"] = hex(get_reg("dr[3]"))
        data["dr6"] = hex(get_reg("dr[6]"))
        data["dr7"] = hex(get_reg("dr[7]"))

        def update_attr(val, limit):
            # Satisfy wtf sanity checks
            # https://github.com/0vercl0k/wtf/blob/main/src/wtf/utils.cc#L237
            val = val >> 8
            return val | ((limit & 0xF0000) >> 8)

        limit = get_reg("segs[0].limit")
        attr = update_attr(get_reg("segs[0].flags"), limit)
        data["es"] = {
            "present": True,
            "selector": hex(get_reg("segs[0].selector")),
            "base": hex(get_reg("segs[0].base")),
            "limit": hex(limit),
            "attr": hex(attr),
        }

        limit = get_reg("segs[1].limit")
        attr = update_attr(get_reg("segs[1].flags"), limit)
        data["cs"] = {
            "present": True,
            "selector": hex(get_reg("segs[1].selector")),
            "base": hex(get_reg("segs[1].base")),
            "limit": hex(limit),
            "attr": hex(attr),
        }

        limit = get_reg("segs[2].limit")
        attr = update_attr(get_reg("segs[2].flags"), limit)
        data["ss"] = {
            "present": True,
            "selector": hex(get_reg("segs[2].selector")),
            "base": hex(get_reg("segs[2].base")),
            "limit": hex(limit),
            "attr": hex(attr),
        }

        limit = get_reg("segs[3].limit")
        attr = update_attr(get_reg("segs[3].flags"), limit)
        data["ds"] = {
            "present": True,
            "selector": hex(get_reg("segs[3].selector")),
            "base": hex(get_reg("segs[3].base")),
            "limit": hex(limit),
            "attr": hex(attr),
        }

        limit = get_reg("segs[4].limit")
        attr = update_attr(get_reg("segs[4].flags"), limit)
        data["fs"] = {
            "present": True,
            "selector": hex(get_reg("segs[4].selector")),
            "base": hex(get_reg("segs[4].base")),
            "limit": hex(limit),
            "attr": hex(attr),
        }

        limit = get_reg("segs[5].limit")
        attr = update_attr(get_reg("segs[5].flags"), limit)
        data["gs"] = {
            "present": True,
            "selector": hex(get_reg("segs[5].selector")),
            "base": hex(get_reg("segs[5].base")),
            "limit": hex(limit),
            "attr": hex(attr),
        }

        limit = get_reg("tr.limit")
        attr = update_attr(get_reg("tr.flags"), limit)
        # https://github.com/awslabs/snapchange/blob/a3db58d2545a34a18fcf3128d403deb0f78b3bba/src/cmdline.rs#L1047
        # Ensure TR.access rights has the 64-bit busy TSS enabled
        attr |= 0xB
        data["tr"] = {
            "present": True,
            "selector": hex(get_reg("tr.selector")),
            "base": hex(get_reg("tr.base")),
            "limit": hex(limit),
            "attr": hex(attr),
        }

        limit = get_reg("ldt.limit")
        attr = update_attr(get_reg("ldt.flags"), limit)
        data["ldtr"] = {
            "present": True,
            "selector": hex(get_reg("ldt.selector")),
            "base": hex(get_reg("ldt.base")),
            "limit": hex(limit),
            "attr": hex(attr),
        }

        data["tsc"] = hex(get_reg("tsc"))

        data["sysenter_cs"] = hex(get_reg("sysenter_cs"))
        data["sysenter_esp"] = hex(get_reg("sysenter_esp"))
        data["sysenter_eip"] = hex(get_reg("sysenter_eip"))

        data["pat"] = hex(get_reg("pat"))

        data["efer"] = hex(get_reg("efer"))

        data["star"] = hex(get_reg("star"))
        data["lstar"] = hex(get_reg("lstar"))

        data["cstar"] = hex(get_reg("cstar"))
        data["fmask"] = hex(get_reg("fmask"))
        data["kernel_gs_base"] = hex(get_reg("kernelgsbase"))
        data["tsc_aux"] = hex(get_reg("tsc_aux"))

        data["mxcsr"] = hex(get_reg("mxcsr"))

        data["cr0"] = hex(get_reg("cr[0]"))
        data["cr2"] = hex(get_reg("cr[2]"))
        data["cr3"] = hex(get_reg("cr[3]"))
        data["cr4"] = hex(get_reg("cr[4]"))
        data["cr8"] = "0x0"

        data["xcr0"] = hex(get_reg("xcr0"))

        data["gdtr"] = {
            "base": hex(get_reg("gdt.base")),
            "limit": hex(get_reg("gdt.limit")),
        }

        data["idtr"] = {
            "base": hex(get_reg("idt.base")),
            "limit": hex(get_reg("idt.limit")),
        }

        data["fpop"] = hex(get_reg("fpop"))

        data["apic_base"] = "0xfee00900"
        data["sfmask"] = "0x4700"
        data["fpcw"] = "0x27f"
        data["fpsw"] = "0x0"
        data["fptw"] = "0xffff"
        data["mxcsr_mask"] = "0x0"
        data["fpst"] = [{"fraction": "0x0", "exp": "0x0"}] * 8

        # writes the data dictionary to the file
        json.dump(data, f)

        # updates entry_syscall in symbol-store.json
        gdb_utils.write_to_store({"entry_syscall": data["lstar"]})

    # function that gets called when the cpu command has been called
    def invoke(self, args, from_tty):
        global cpu_state

        self.dont_repeat()

        print(f"cpu_state: 0x{cpu_state}")
        print(f"Writing register information to '{REGS_JSON_FILENAME}'")

        # dump the cpu state to regs.json
        with REGS_JSON_FILENAME.open("w") as f:
            self.dump(f)

        print("Done...continuing debuggee")
        gdb.execute("continue")


DumpCPUStateCommand()

# When HW acceleration is enabled
QemuBkpt("kvm_cpu_exec")

# When SW emulation is used
QemuBkpt("cpu_exec")

gdb.execute("continue")
