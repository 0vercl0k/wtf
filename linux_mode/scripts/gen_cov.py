import idaapi
import idautils
import idc
import json
import pathlib

idaapi.auto_wait()
img_base = idaapi.get_imagebase()
addrs = set()

filepath = pathlib.Path(idc.get_input_file_path())

print("img_base: 0x{:x}".format(img_base))

for fea in idautils.Functions():
    for b in idaapi.FlowChart(idaapi.get_func(fea)):
        ea = b.start_ea
        flags = idaapi.get_full_flags(ea)
        is_code = idaapi.is_code(flags)
        rva = ea - img_base
        if is_code:
            addrs.add(rva)

cov = {
        'name': filepath.with_suffix('').name,
        'addresses': sorted(addrs)
}

outfile = filepath.with_suffix('.cov')
with open(outfile, 'w') as fd:
    json.dump(cov, fd)
