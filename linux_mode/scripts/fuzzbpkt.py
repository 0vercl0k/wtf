# imports
import gdb, os, kernel, utils, pwn

# set environmental variables
os.environ['PWNLIB_NOTERM'] = '1'

# set architecture
pwn.context.arch = 'amd64'

# Fuzzing Breakpoint Class, wrapper for gdb breakpoint
class FuzzBpkt(gdb.Breakpoint):

	# intializes the breakpoint
	def __init__(self, addr, name, checkname=True, is_mapped=False):

		# convert address into format that gdb takes: break *0xFFFF
		loc = f'''*{addr}'''

		# intialize the gdb breakpoint
		gdb.Breakpoint.__init__(
			self,
			spec=loc,
			type=gdb.BP_BREAKPOINT
		)
		
		# set the program name and whether or not we should check the name
		self.program_name = name
		self.checkname = checkname
		
		# set whether or not the program is mapped and initializes some data 
		self.is_mapped = is_mapped
		self.map_counter = 0
		self.vmmap = []
		
		# save the addresses for the kernel exception handlers
		self.save_kernel_exception_handlers()
		
	# function that is called when the breakpoint is hit
	def stop(self):
	
		# if we want to check the program name and the program that was stopped isn't 
		# 	the program we want then return false
		if self.checkname and not self.is_my_program():
			return False
	
		# if the program is not mapped then map it
		if not self.is_mapped:
			self.get_vmmap()
			self.save_base()
			self.is_mapped = True
		
		# if all pages have been loaded then we can dump the data
		if self.map_counter >= len(self.vmmap):
			print('vmmaped, ready to dump')
			return True
		
		# load a map
		self.load_one_map_to_vm()
		
		return False
		
	# checks if the current program is the program we wanted
	# 	does this by checking if the program names match
	def is_my_program(self):
		curr_program_name = self.get_name()
		return self.program_name in curr_program_name
			
	# saves the base address of the process to the symbol store
	def save_base(self):
		base = self.get_base()
		name = self.get_name()
		utils.write_to_store({name: hex(base)})
		
	# saves all of the kernel exception handlers
	#	used for context switch or crashes
	def save_kernel_exception_handlers(self):
		utils.write_to_store({
			"entry_syscall": hex(kernel.entry_syscall), 
			"asm_exc_page_fault": hex(kernel.asm_exc_page_fault),
			"asm_exc_divide_error": hex(kernel.asm_exc_divide_error),
			"force_sigsegv": hex(kernel.force_sigsegv),
			"page_fault_oops": hex(kernel.page_fault_oops)
		})
		
	# gets the base address of the process
	def get_base(self):
		task_struct = kernel.task.get_task_struct()
		mmap_addr   = kernel.vmmap.get_mmap_struct(task_struct)
		return kernel.vmmap.get_vm_start(mmap_addr)

	# gets the name of the current running process
	def get_name(self):
		task_struct = kernel.task.get_task_struct()
		return kernel.task.get_name(task_struct)

	# calls mlock
	def call_mlockall(self):

		# assembly code to call mlock. Saves and restores all registers so they are
		# 	unaffected by the call.
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

		# assemble the shellcode
		shellcode = pwn.asm(mlockall)

		# gets the current instruction pointer
		rip = kernel.cpu.get_reg('rip')

		# get the address for the shellcode
		shellcode_addr = rip - len(shellcode)

		# write the shellcode
		# TODO: may want to save the original code in case the fuzzing case needs to 
		# run the original code that was ahead of the ip so that we can restore it.
		addr_to_write = shellcode_addr
		for b in shellcode:
			kernel.write_byte(addr_to_write, b)
			addr_to_write += 1
			
		# set the instruction pointer to the start of the shellcode
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
	
		# get the next unloaded page and increment the counter
		page = self.vmmap[self.map_counter]
		self.map_counter += 1
		
		# get the start and end of the page
		start, end = page[1], page[2]
		
		# print out progress
		print('load 0x{:x} - 0x{:x}'.format(start, end))
		
		# gets the assembly for the start and end of the page
		loop = get_loop(start, end)

		# get the instruction pointer
		rip = kernel.cpu.get_reg('rip')
		
		# write the loop to memory
		loop_addr = rip - len(loop)
		addr_to_write = loop_addr
		for b in loop:
			kernel.write_byte(addr_to_write, b)
			addr_to_write += 1
			
		# set the instruction pointer to the loop code address and let it run
		kernel.cpu.set_reg('rip', loop_addr)
		
	def get_vmmap(self):
	
		# permissions for read, write, and execute
		RWX = 0b111

		# checks to see if page is missing perms
		def is_page_guard(perm):
			return perm & 0b111 == 0
	
		# walk the mmap struct
		def walk(mmap_struct):
		
			# initialize an empty array
			vmmap = []
		
			# set the current mmap struct to the one passed to the function
			curr_mmap_struct = mmap_struct
		
			# while we still have a current mmap struct
			while curr_mmap_struct:

				# gets the original perms of the page
				old_permission = kernel.vmmap.get_permission(curr_mmap_struct)
			
				# sets the permissions to read, write, and execute
				kernel.vmmap.set_permission(curr_mmap_struct, RWX)
				
				# gets the start and end of the page
				vm_start = kernel.vmmap.get_vm_start(curr_mmap_struct)
				vm_end   = kernel.vmmap.get_vm_end(curr_mmap_struct)
				
				# tries to get the dentry name
				try:
					filename = kernel.vmmap.get_dentry_name(curr_mmap_struct)
				except:
					filename = ""

				# print out the progress of the mapping
				print('mmap_struct: 0x{:x}, mmap: 0x{:x} - 0x{:x}, prot: 0x{:x} {}'.format(curr_mmap_struct, vm_start, vm_end, old_permission, filename))
				
				# gets the private data of the map
				vm_private_data = kernel.vmmap.get_vm_private_data(curr_mmap_struct)
				
				# if private data is not vvar_mapping and there is no page guard then add the page to vmap
				if vm_private_data != '<vvar_mapping>' and not is_page_guard(old_permission):
					vmmap += [[curr_mmap_struct, vm_start, vm_end]]
				# otherwise skip
				else:
					print("^skipped")
				
				# try to get the next page
				curr_mmap_struct = kernel.vmmap.get_vm_next(curr_mmap_struct)
				
			# return vmmap
			return vmmap
	
		# gets the mmap struct
		task_struct = kernel.task.get_task_struct()
		mmap_struct = kernel.vmmap.get_mmap_struct(task_struct)
		
		# set vmmap to the results of walking the page
		self.vmmap = walk(mmap_struct)