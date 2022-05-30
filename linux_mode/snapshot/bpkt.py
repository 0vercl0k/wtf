# imports
import sys, os

# import fuzzing breakpoint
from fuzzbpkt import *

# address to break on, found using gdb
break_address = ''

# name of the file in which to break
file_name = ''

# create the breakpoint for the executable specified
FuzzBpkt(break_address, file_name, is_mapped=False)

# connect to the remote gdb server
gdb.execute('target remote localhost:1234')