# Jason Crowder - February 2024
# This file contains shared code between Python modules used for Qemu and the
# Linux kernel
import json, pathlib

# symbol store filename
SYMBOL_STORE = pathlib.Path("symbol-store.json")


# write data to the symbol store file
def write_to_store(content):
    # if the file doesn't exist then create it
    if not SYMBOL_STORE.exists():
        SYMBOL_STORE.write_text("{}")

    # read the symbol store data into data variable
    data = json.loads(SYMBOL_STORE.read_text("utf-8"))

    # update the dictionary
    data.update(content)

    # write the data to the symbol store file
    SYMBOL_STORE.write_text(json.dumps(data))
