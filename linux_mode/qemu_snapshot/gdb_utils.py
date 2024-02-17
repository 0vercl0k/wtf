# Jason Crowder - February 2024
# This file contains shared code between Python modules used for Qemu and the
# Linux kernel
import json, os.path

# symbol store filename
SYMBOL_STORE = "symbol-store.json"


# write data to the symbol store file
def write_to_store(content):
    # if the file doesn't exist then create it
    if not os.path.isfile(SYMBOL_STORE):
        with open(SYMBOL_STORE, "w"):
            pass

    data = {}

    # read the symbol store data into data variable
    with open(SYMBOL_STORE, "r") as f:
        try:
            data = json.load(f)
        except:
            pass

    # update the dictionary
    data.update(content)

    # write the data to the symbol store file
    with open(SYMBOL_STORE, "w") as f:
        json.dump(data, f)
