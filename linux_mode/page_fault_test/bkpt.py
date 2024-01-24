# imports
import sys, os

# import fuzzing breakpoint
from gdb_fuzzbkpt import *

target_dir = 'linux_page_fault_test'

break_address = 'page_fault_test'

# name of the file in which to break
file_name = 'a.out'

# create the breakpoint for the executable specified
FuzzBkpt(target_dir, break_address, file_name, bp_hits_required=1, sym_path=file_name)
