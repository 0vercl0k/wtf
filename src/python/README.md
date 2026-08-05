# Python building for `kdmp-parser`

![Build status](https://github.com/0vercl0k/kdmp-parser/workflows/Builds/badge.svg)

This C++ library parses Windows kernel [full](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/complete-memory-dump) dumps (`.dump /f` in WinDbg), [BMP](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/active-memory-dump) dumps (`.dump /ka` in WinDbg) as well as more recent dump types that were introduced in ~2022.

![parser](https://github.com/0vercl0k/kdmp-parser/raw/master/pics/parser.jpg)

The library supports loading 64-bit dumps and provides read access to things like:

- The context record,
- The exception record,
- The bugcheck parameters,
- The physical memory.

The Python bindings were authored by [hugsy](https://github.com/hugsy) & [masthoon](https://github.com/masthoon). Refer to the [project page on Github](https://github.com/0vercl0k/kdmp-parser) for documentation, issues and pull requests.

## Installing from PyPI

The easiest way is simply to:

```bash
pip install kdmp_parser
```

## Installing using PIP

Run the following after installing [CMake](https://cmake.org/) and [Python](https://python.org/) 3.8+ / `pip`:
```
cd src/python
pip install requirements.txt
pip install .
```

To create a wheel pacakge:
```
cd src/python
pip wheel .
```

## Usage

### Get context, print the program counter

```python
import kdmp_parser
dmp = kdmp_parser.KernelDumpParser("full.dmp")
assert dmp.type == kdmp_parser.DumpType.FullDump
print(f"Dump RIP={dmp.context.Rip:#x}")
```

### Read a virtual memory page at address pointed by RIP

```python
import kdmp_parser
dmp = kdmp_parser.KernelDumpParser("full.dmp")
dmp.read_virtual_page(dmp.context.Rip)
```

### Explore the physical memory

```python
import kdmp_parser
dmp = kdmp_parser.KernelDumpParser("full.dmp")
pml4 = dmp.directory_table_base
print(f"{pml4=:#x}")
dmp.read_physical_page(pml4)
```

### Translate a virtual address into a physical address

```python
import kdmp_parser
dmp = kdmp_parser.KernelDumpParser("full.dmp")
VA = dmp.context.Rip
PA = dmp.translate_virtual(VA)
print(f"{VA=:#x} -> {PA=:#x}")
```

# Authors

* Axel '[@0vercl0k](https://twitter.com/0vercl0k)' Souchet

# Contributors

[ ![contributors-img](https://contrib.rocks/image?repo=0vercl0k/kdmp-parser) ](https://github.com/0vercl0k/kdmp-parser/graphs/contributors)
