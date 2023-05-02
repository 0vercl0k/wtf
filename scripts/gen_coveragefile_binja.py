"""Generate a coverage file for WTF using Binary Ninja.

@australeo - 2023
"""
import json
import os
from binaryninja import PluginCommand, basicblock, interaction

def generate_coverage_file(bv):
    # bv.file.filename: 'C:/path/to/binary.bndb'
    name = bv.file.filename.split("/")[-1].split('.')[0]

    bb_list = []

    for block in bv.basic_blocks:
        bb_list.append(block.start - bv.start)

    json_object = {
        "name": name,
        "addresses": bb_list
    }

    output_file = interaction.get_save_filename_input("Filename: ", ".cov", name + ".cov")

    with open(output_file, "w", encoding="utf-8") as file:
        json.dump(json_object, file)

def register_plugin():
    PluginCommand.register("Generate WTF coverage file", \
        "Generate WTF coverage file", generate_coverage_file)

register_plugin()
