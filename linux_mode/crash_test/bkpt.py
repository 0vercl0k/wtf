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
