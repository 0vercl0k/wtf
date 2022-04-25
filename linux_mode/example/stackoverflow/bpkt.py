import sys 
import os

from fuzzbpkt import *

FuzzBpkt('0x55555555526d', 'stackoverflow', is_mapped=False)

gdb.execute('target remote localhost:1234')