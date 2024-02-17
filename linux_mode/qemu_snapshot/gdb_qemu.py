# Jason Crowder - February 2024
# imports
import gdb, json, os, sys

sys.path.insert(1, os.path.dirname(os.path.abspath(__file__)))

import gdb_utils

REGS_JSON_FILENAME = "regs.json"

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
            return (
                f"""(   (CPUX86State*)  ( (CPUState*)({cpu_state}) )->env_ptr   )->"""
                + x
            )

        data = {}

        data["rax"] = str(gdb.parse_and_eval(get_reg("regs[0]")))
        data["rcx"] = str(gdb.parse_and_eval(get_reg("regs[1]")))
        data["rdx"] = str(gdb.parse_and_eval(get_reg("regs[2]")))
        data["rbx"] = str(gdb.parse_and_eval(get_reg("regs[3]")))
        data["rsp"] = str(gdb.parse_and_eval(get_reg("regs[4]")))
        data["rbp"] = str(gdb.parse_and_eval(get_reg("regs[5]")))
        data["rsi"] = str(gdb.parse_and_eval(get_reg("regs[6]")))
        data["rdi"] = str(gdb.parse_and_eval(get_reg("regs[7]")))
        data["r8"] = str(gdb.parse_and_eval(get_reg("regs[8]")))
        data["r9"] = str(gdb.parse_and_eval(get_reg("regs[9]")))
        data["r10"] = str(gdb.parse_and_eval(get_reg("regs[10]")))
        data["r11"] = str(gdb.parse_and_eval(get_reg("regs[11]")))
        data["r12"] = str(gdb.parse_and_eval(get_reg("regs[12]")))
        data["r13"] = str(gdb.parse_and_eval(get_reg("regs[13]")))
        data["r14"] = str(gdb.parse_and_eval(get_reg("regs[14]")))
        data["r15"] = str(gdb.parse_and_eval(get_reg("regs[15]")))

        data["rip"] = str(gdb.parse_and_eval(get_reg("eip")))

        data["rflags"] = str(gdb.parse_and_eval(get_reg("eflags")))

        data["dr0"] = str(gdb.parse_and_eval(get_reg("dr[0]")))
        data["dr1"] = str(gdb.parse_and_eval(get_reg("dr[1]")))
        data["dr2"] = str(gdb.parse_and_eval(get_reg("dr[2]")))
        data["dr3"] = str(gdb.parse_and_eval(get_reg("dr[3]")))
        data["dr6"] = str(gdb.parse_and_eval(get_reg("dr[6]")))
        data["dr7"] = str(gdb.parse_and_eval(get_reg("dr[7]")))

        def update_attr(val, limit):
            # Satisfy wtf sanity checks
            # https://github.com/0vercl0k/wtf/blob/main/src/wtf/utils.cc#L237
            val = val >> 8
            return val | ((limit & 0xF0000) >> 8)

        limit = gdb.parse_and_eval(get_reg("segs[0].limit"))
        attr = update_attr(gdb.parse_and_eval(get_reg("segs[0].flags")), limit)
        data["es"] = {
            "present": True,
            "selector": str(gdb.parse_and_eval(get_reg("segs[0].selector"))),
            "base": str(gdb.parse_and_eval(get_reg("segs[0].base"))),
            "limit": str(limit),
            "attr": str(attr),
        }

        limit = gdb.parse_and_eval(get_reg("segs[1].limit"))
        attr = update_attr(gdb.parse_and_eval(get_reg("segs[1].flags")), limit)
        data["cs"] = {
            "present": True,
            "selector": str(gdb.parse_and_eval(get_reg("segs[1].selector"))),
            "base": str(gdb.parse_and_eval(get_reg("segs[1].base"))),
            "limit": str(limit),
            "attr": str(attr),
        }

        limit = gdb.parse_and_eval(get_reg("segs[2].limit"))
        attr = update_attr(gdb.parse_and_eval(get_reg("segs[2].flags")), limit)
        data["ss"] = {
            "present": True,
            "selector": str(gdb.parse_and_eval(get_reg("segs[2].selector"))),
            "base": str(gdb.parse_and_eval(get_reg("segs[2].base"))),
            "limit": str(limit),
            "attr": str(attr),
        }

        limit = gdb.parse_and_eval(get_reg("segs[3].limit"))
        attr = update_attr(gdb.parse_and_eval(get_reg("segs[3].flags")), limit)
        data["ds"] = {
            "present": True,
            "selector": str(gdb.parse_and_eval(get_reg("segs[3].selector"))),
            "base": str(gdb.parse_and_eval(get_reg("segs[3].base"))),
            "limit": str(limit),
            "attr": str(attr),
        }

        limit = gdb.parse_and_eval(get_reg("segs[4].limit"))
        attr = update_attr(gdb.parse_and_eval(get_reg("segs[4].flags")), limit)
        data["fs"] = {
            "present": True,
            "selector": str(gdb.parse_and_eval(get_reg("segs[4].selector"))),
            "base": str(gdb.parse_and_eval(get_reg("segs[4].base"))),
            "limit": str(limit),
            "attr": str(attr),
        }

        limit = gdb.parse_and_eval(get_reg("segs[5].limit"))
        attr = update_attr(gdb.parse_and_eval(get_reg("segs[5].flags")), limit)
        data["gs"] = {
            "present": True,
            "selector": str(gdb.parse_and_eval(get_reg("segs[5].selector"))),
            "base": str(gdb.parse_and_eval(get_reg("segs[5].base"))),
            "limit": str(limit),
            "attr": str(attr),
        }

        limit = gdb.parse_and_eval(get_reg("tr.limit"))
        attr = update_attr(gdb.parse_and_eval(get_reg("tr.flags")), limit)
        # https://github.com/awslabs/snapchange/blob/a3db58d2545a34a18fcf3128d403deb0f78b3bba/src/cmdline.rs#L1047
        # Ensure TR.access rights has the 64-bit busy TSS enabled
        attr |= 0xB
        data["tr"] = {
            "present": True,
            "selector": str(gdb.parse_and_eval(get_reg("tr.selector"))),
            "base": str(gdb.parse_and_eval(get_reg("tr.base"))),
            "limit": str(limit),
            "attr": str(attr),
        }

        limit = gdb.parse_and_eval(get_reg("ldt.limit"))
        attr = update_attr(gdb.parse_and_eval(get_reg("ldt.flags")), limit)
        data["ldtr"] = {
            "present": True,
            "selector": str(gdb.parse_and_eval(get_reg("ldt.selector"))),
            "base": str(gdb.parse_and_eval(get_reg("ldt.base"))),
            "limit": str(limit),
            "attr": str(attr),
        }

        data["tsc"] = str(gdb.parse_and_eval(get_reg("tsc")))

        data["sysenter_cs"] = str(gdb.parse_and_eval(get_reg("sysenter_cs")))
        data["sysenter_esp"] = str(gdb.parse_and_eval(get_reg("sysenter_esp")))
        data["sysenter_eip"] = str(gdb.parse_and_eval(get_reg("sysenter_eip")))

        data["pat"] = str(gdb.parse_and_eval(get_reg("pat")))

        data["efer"] = str(gdb.parse_and_eval(get_reg("efer")))

        data["star"] = str(gdb.parse_and_eval(get_reg("star")))
        data["lstar"] = str(gdb.parse_and_eval(get_reg("lstar")))

        data["cstar"] = str(gdb.parse_and_eval(get_reg("cstar")))
        data["fmask"] = str(gdb.parse_and_eval(get_reg("fmask")))
        data["kernel_gs_base"] = str(gdb.parse_and_eval(get_reg("kernelgsbase")))
        data["tsc_aux"] = str(gdb.parse_and_eval(get_reg("tsc_aux")))

        data["mxcsr"] = str(gdb.parse_and_eval(get_reg("mxcsr")))

        data["cr0"] = str(gdb.parse_and_eval(get_reg("cr[0]")))
        data["cr2"] = str(gdb.parse_and_eval(get_reg("cr[2]")))
        data["cr3"] = str(gdb.parse_and_eval(get_reg("cr[3]")))
        data["cr4"] = str(gdb.parse_and_eval(get_reg("cr[4]")))
        data["cr8"] = "0x0"

        data["xcr0"] = str(gdb.parse_and_eval(get_reg("xcr0")))

        data["gdtr"] = {
            "base": str(gdb.parse_and_eval(get_reg("gdt.base"))),
            "limit": str(gdb.parse_and_eval(get_reg("gdt.limit"))),
        }

        data["idtr"] = {
            "base": str(gdb.parse_and_eval(get_reg("idt.base"))),
            "limit": str(gdb.parse_and_eval(get_reg("idt.limit"))),
        }

        data["fpop"] = str(gdb.parse_and_eval(get_reg("fpop")))

        data["apic_base"] = "0xfee00900"
        data["sfmask"] = "0x4700"
        data["fpcw"] = "0x27f"
        data["fpsw"] = "0x0"
        data["fptw"] = "0x0"
        data["mxcsr_mask"] = "0x0"
        data["fpst"] = [{"fraction": 0, "exp": 0}] * 8

        # writes the data dictionary to the file
        json.dump(data, f)

        # updates entry_syscall in symbol-store.json
        gdb_utils.write_to_store({"entry_syscall": hex(data["lstar"])})

    # function that gets called when the cpu command has been called
    def invoke(self, args, from_tty):
        global cpu_state

        self.dont_repeat()

        argv = gdb.string_to_argv(args)

        print("cpu_state: 0x%x" % cpu_state)
        print("Writing register information to %s" % REGS_JSON_FILENAME)

        # dump the cpu state to regs.json
        with open(REGS_JSON_FILENAME, "w") as f:
            self.dump(f)

        print("Done...continuing debuggee")
        gdb.execute("continue")


DumpCPUStateCommand()

# When HW acceleration is enabled
QemuBkpt("kvm_cpu_exec")

# When SW emulation is used
QemuBkpt("cpu_exec")

gdb.execute("continue")