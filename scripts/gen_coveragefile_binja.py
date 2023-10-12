"""Generate a coverage file for WTF using Binary Ninja.

@australeo - 2023
"""
from json import dump
from pathlib import Path
from binaryninja import PluginCommand, interaction

def generate_coverage_file(bv):
    # bv.file.filename: 'C:/path/to/binary.bndb'
    name = Path(bv.file.filename).stem
    name = name.replace("-", "_")

    bb_list = []

    for block in bv.basic_blocks:
        bb_list.append(block.start - bv.start)

    json_object = {
        "name": name,
        "addresses": bb_list
    }

    output_file = interaction.get_save_filename_input("Filename: ", ".cov", name + ".cov")

    with open(output_file, "w", encoding="utf-8") as file:
        dump(json_object, file)

def register_plugin():
    PluginCommand.register("Generate WTF coverage file", \
        "Generate WTF coverage file", generate_coverage_file)

register_plugin()
