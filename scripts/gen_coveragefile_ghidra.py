#Output a module coverage file for wtf to %USERPROFILE%/ghidra_scripts
#@author huwwp
#@category fuzzing

from ghidra.program.model.block import BasicBlockModel
import json
import os

program = getCurrentProgram()
program_name = program.getName().rsplit(".",1)[0]
base_address = program.getImageBase().getOffset()
block_iterator = BasicBlockModel(program).getCodeBlocks(monitor)
block = block_iterator.next()

address_list = []

while block:
    address_list.append(block.minAddress.getOffset() - base_address)
    block = block_iterator.next()

json_object = {
	'name': program_name,
	'addresses': address_list
}

file = open(os.environ['USERPROFILE'] + "/ghidra_scripts/" + program_name + ".cov", "w")
json.dump(json_object, file)
file.close()
"""
