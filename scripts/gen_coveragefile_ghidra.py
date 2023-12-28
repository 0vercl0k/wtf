#Output a module coverage file for wtf
#@author huwwp
#@category fuzzing

from ghidra.program.model.block import BasicBlockModel
import json

program = getCurrentProgram()
output_path = program.getExecutablePath().rsplit("/",1)[0]
program_name = program.getName().rsplit(".",1)[0]
base_address = program.getImageBase().getOffset()
block_iterator = BasicBlockModel(program).getCodeBlocks(monitor)
block = block_iterator.next()

address_list = []

while block:
    address_list.append(block.minAddress.getOffset() - base_address)
    block = block_iterator.next()

json_object = {
    'name': program_name.replace("-", "_"),
    'addresses': address_list
}

file = open(output_path + "/" + program_name + ".cov", "w")
json.dump(json_object, file)
file.close()
