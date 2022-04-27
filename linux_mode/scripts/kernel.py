import gdb
import re

read_qword = lambda addr: int(str(gdb.execute(f'''x/gx {addr}''', to_string=True)).split()[1], base=16)
write_byte = lambda addr, b:      gdb.execute(f'''set *((char*)({addr}))={b}''', to_string=True)

def parse_variable(cmd):
	def search_hex(s):
		match = re.search("0x[a-fA-F0-9]+", s)
		if match:
			return match.group()
		else:
			return ""

	raw = str(gdb.parse_and_eval(cmd))
	return int(search_hex(raw), base=16)

get_current_task_offset   = lambda : parse_variable('&current_task')
get_cpu_offset            = lambda : parse_variable('__per_cpu_offset[0]')

entry_syscall        = parse_variable('entry_SYSCALL_64')
asm_exc_page_fault   = parse_variable('asm_exc_page_fault')
asm_exc_divide_error = parse_variable('asm_exc_divide_error')
force_sigsegv        = parse_variable('force_sigsegv')
page_fault_oops      = parse_variable('page_fault_oops')

class cpu:
	get_reg = lambda reg: parse_variable(f'''${reg}''')
	set_reg = lambda reg, val: gdb.execute(f'''set ${reg}={val}''', to_string=True)

class task:
	get_task_struct = lambda: read_qword(get_cpu_offset() + get_current_task_offset())
	get_name        = lambda task_struct_addr: str(gdb.parse_and_eval(f'''((struct task_struct*){task_struct_addr})->comm''')).replace('"', '').replace('\\000', '')

class vmmap:
	get_vm_private_data = lambda mmap_struct: str(gdb.parse_and_eval(f''' (*(struct vm_area_struct*)({mmap_struct}))->vm_private_data ''')).split()[1]
	
	get_mmap_struct = lambda task_struct: parse_variable(f'''((struct task_struct*){task_struct})->mm->mmap->vm_mm->mmap''')
	get_vm_next     = lambda mmap_struct: parse_variable(f'''(*(struct vm_area_struct*)({mmap_struct}))->vm_next''')
	get_vm_start    = lambda mmap_struct: parse_variable(f'''(*(struct vm_area_struct*)({mmap_struct}))->vm_start''')
	get_vm_end      = lambda mmap_struct: parse_variable(f'''(*(struct vm_area_struct*)({mmap_struct}))->vm_end''')
	#get_permission  = lambda mmap_struct: parse_variable(f'''(*(struct vm_area_struct*)({mmap_struct}))->vm_page_prot->pgprot''')
	get_permission  = lambda mmap_struct: parse_variable(f'''(*(struct vm_area_struct*)({mmap_struct}))->vm_flags''')
	get_dentry_name = lambda mmap_struct: str(gdb.parse_and_eval(f'''(*(struct vm_area_struct*)({mmap_struct}))->vm_file->f_path->dentry->d_name->name''')).split()[1]

	def set_permission(mmap_struct, flag):
		old_permission = vmmap.get_permission(mmap_struct)
		new_permission = old_permission | flag
		gdb.execute(f'''set (*(struct vm_area_struct*)({mmap_struct}))->vm_flags={new_permission}''', to_string=True)
		#gdb.execute(f'''set (*(struct vm_area_struct*)({mmap_struct}))->vm_page_prot->pgprot={new_permission}''', to_string=True)
		
