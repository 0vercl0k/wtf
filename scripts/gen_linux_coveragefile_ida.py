import json
import pathlib
import idaapi
import idautils
import idc

def main():
    idaapi.auto_wait()
    img_base = idaapi.get_imagebase()
    filepath = pathlib.Path(idc.get_input_file_path())
    addrs = set()
    for fea in idautils.Functions():
        for b in idaapi.FlowChart(idaapi.get_func(fea)):
            ea = b.start_ea
            is_code = idaapi.is_code(idaapi.get_full_flags(ea))
            rva = ea - img_base
            addrs.add(rva)

    cov = {
        'name': filepath.name,
        'addresses': sorted(addrs)
    }

    outfile = filepath.with_suffix('.cov')
    with open(outfile, 'w') as fd:
        json.dump(cov, fd)

    print(f'Done, {outfile}')

if __name__ == '__main__':
    main()
