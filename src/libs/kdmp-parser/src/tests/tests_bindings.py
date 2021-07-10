import sys
sys.path.append(sys.argv[1])

from kdmp import Dump, FullDump, BMPDump

dmp = Dump(sys.argv[2])
assert(dmp.type() == FullDump or dmp.type() == BMPDump)

ctx = dmp.context()
dtb = ctx['dtb'] & ~0xfff # remove PCID

assert(ctx['rip'] == 0xfffff805108776a0)
assert(dtb == 0x6d4000)

page = dmp.get_physical_page(0x5000)
assert(page[0x34:0x38] == b'MSFT')

assert(dmp.virt_translate(0xfffff78000000000) == 0x0000000000c2f000)
assert(dmp.virt_translate(0xfffff80513370000) == 0x000000003d555000)

assert(dmp.get_virtual_page(0xfffff78000000000) == dmp.get_physical_page(0x0000000000c2f000))
assert(dmp.get_virtual_page(0xfffff80513370000) == dmp.get_physical_page(0x000000003d555000))

v = 0xfffff80513568000
assert(dmp.get_virtual_page(v) == dmp.get_physical_page(dmp.virt_translate(v)))

print("Python tests: All good!")
sys.exit(0)