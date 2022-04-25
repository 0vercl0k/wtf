import gdb
import os
import kernel
import utils

os.environ['PWNLIB_NOTERM'] = '1'
import pwn

pwn.context.arch = 'amd64'

class FuzzBpkt(gdb.Breakpoint):
	def __init__(self, addr, name, checkname=True, is_mapped=False):

		loc = f'''*{addr}'''

		gdb.Breakpoint.__init__(
			self,
			spec=loc,
			type=gdb.BP_BREAKPOINT
		)
		
		self.program_name = name
		self.checkname = checkname
		
		self.is_mapped = is_mapped
		self.map_counter = 0
		self.vmmap = []
		
		self.save_kernel_exception_handlers()
		
	def stop(self):
	
		if self.checkname and not self.is_my_program():
			return False
	
		if not self.is_mapped:
			self.get_vmmap()
			self.save_base()
			self.is_mapped = True
		
		if self.map_counter >= len(self.vmmap):
			print('vmmaped, ready to dump')
			return True
		
		self.load_one_map_to_vm()
		
		return False
		
	def is_my_program(self):
		curr_program_name = self.get_name()
		return self.program_name in curr_program_name
			
	def save_base(self):
		base = self.get_base()
		name = self.get_name()
		utils.write_to_store({name: hex(base)})
		
	def save_kernel_exception_handlers(self):
		utils.write_to_store({
			"entry_syscall": hex(kernel.entry_syscall), 
			"asm_exc_page_fault": hex(kernel.asm_exc_page_fault),
			"asm_exc_divide_error": hex(kernel.asm_exc_divide_error),
			"force_sigsegv": hex(kernel.force_sigsegv),
			"page_fault_oops": hex(kernel.page_fault_oops)
		})
		
	def get_base(self):
		task_struct = kernel.task.get_task_struct()
		mmap_addr   = kernel.vmmap.get_mmap_struct(task_struct)
		return kernel.vmmap.get_vm_start(mmap_addr)
		
	def get_name(self):
		task_struct = kernel.task.get_task_struct()
		return kernel.task.get_name(task_struct)

	def call_mlockall(self):

		mlockall = '''
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
			mov rdi, 0x7
			syscall
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
		'''

		shellcode = pwn.asm(mlockall)

		rip = kernel.cpu.get_reg('rip')
		shellcode_addr = rip - len(shellcode)

		addr_to_write = shellcode_addr
		
		for b in shellcode:
			kernel.write_byte(addr_to_write, b)
			addr_to_write += 1
			
		kernel.cpu.set_reg('rip', shellcode_addr)
		
	def load_one_map_to_vm(self):
	
		def get_loop(start, end):
			loop = '''
				.init:
					push rdx
					push rdi
					push rsi
					mov rdi, 0x{:x}
					mov rsi, 0x{:x}
				.loop:
					mov rdx, byte [rdi]
					add rdi, 0x1000
					cmp rdi, rsi
					jl  .loop
				.restore:
					pop rsi
					pop rdi
					pop rdx
			'''.format(start, end)
		
			return pwn.asm(loop)
	
		page = self.vmmap[self.map_counter]
		self.map_counter += 1
		
		start, end = page[1], page[2]
		
		print('load 0x{:x} - 0x{:x}'.format(start, end))
		
		loop = get_loop(start, end)
		rip = kernel.cpu.get_reg('rip')
		
		loop_addr = rip - len(loop)
		
		addr_to_write = loop_addr
		
		for b in loop:
			kernel.write_byte(addr_to_write, b)
			addr_to_write += 1
			
		kernel.cpu.set_reg('rip', loop_addr)
		
	def get_vmmap(self):
	
		RWX = 0b111
	
		def walk(mmap_struct):
		
			vmmap = []
		
			curr_mmap_struct = mmap_struct
		
			while curr_mmap_struct:
			
				kernel.vmmap.set_permission(curr_mmap_struct, RWX)
				
				vm_start = kernel.vmmap.get_vm_start(curr_mmap_struct)
				vm_end   = kernel.vmmap.get_vm_end(curr_mmap_struct)
				
				try:
					filename = kernel.vmmap.get_dentry_name(curr_mmap_struct)
				except:
					filename = ""

				print('mmap_struct: 0x{:x}, mmap: 0x{:x} - 0x{:x} {}'.format(curr_mmap_struct, vm_start, vm_end, filename))
				
				vm_private_data = kernel.vmmap.get_vm_private_data(curr_mmap_struct)
				
				if vm_private_data != '<vvar_mapping>':
					vmmap += [[curr_mmap_struct, vm_start, vm_end]]
				
				curr_mmap_struct = kernel.vmmap.get_vm_next(curr_mmap_struct)
				
			return vmmap
	
		task_struct = kernel.task.get_task_struct()
		mmap_struct = kernel.vmmap.get_mmap_struct(task_struct)
		
		self.vmmap = walk(mmap_struct)