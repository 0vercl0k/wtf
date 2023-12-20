"""
Root module of `kdmp_parser` Python package.
"""

import enum
import pathlib
from typing import Optional, Union

#
# `_kdmp_parser` is the C++ module. It contains the port of all C++ classes/enums/etc. in their
# original form. The Python package part provides more Pythonic APIs.
#
from ._kdmp_parser import (  # type: ignore
    version,
    DumpType_t as _DumpType_t,
    KernelDumpParser as _KernelDumpParser,
    CONTEXT as __CONTEXT,
    HEADER64 as __HEADER64,
)

from .page import (
    PageIterator as _PageIterator,
)


class DumpType(enum.IntEnum):
    FullDump = _DumpType_t.FullDump
    KernelDump = _DumpType_t.KernelDump
    BMPDump = _DumpType_t.BMPDump
    MiniDump = _DumpType_t.MiniDump
    KernelMemoryDump = _DumpType_t.KernelMemoryDump
    KernelAndUserMemoryDump = _DumpType_t.KernelAndUserMemoryDump
    CompleteMemoryDump = _DumpType_t.CompleteMemoryDump


class KernelDumpParser:
    def __init__(self, path: Union[str, pathlib.Path]):
        """Parse a kernel dump file

        Args:
            path (pathlib.Path|str): Path to the kernel dump file
        """
        if isinstance(path, str):
            path = pathlib.Path(path)

        if not isinstance(path, pathlib.Path):
            raise TypeError

        if not path.exists():
            raise ValueError

        self.__dump = _KernelDumpParser()
        if not self.__dump.Parse(str(path.absolute())):
            raise RuntimeError(f"Invalid kernel dump file: {path}")

        self.filepath = path
        self.context: __CONTEXT = self.__dump.GetContext()
        self.directory_table_base: int = self.__dump.GetDirectoryTableBase() & ~0xFFF
        self.type = DumpType(self.__dump.GetDumpType())
        self.header: __HEADER64 = self.__dump.GetDumpHeader()
        self.pages = _PageIterator(self.__dump)
        return

    def __repr__(self) -> str:
        return f"KernelDumpParser({self.filepath}, {self.type})"

    def read_physical_page(self, physical_address: int) -> Optional[bytearray]:
        """Read a physical page from the memory dump

        Args:
            physical_address (int): The physical address to read. Note that no alignment
            of this parameter is assumed.

        Returns:
            Optional[bytearray]: The bytes in the page if found, None otherwise
        """
        raw_page = self.__dump.GetPhysicalPage(physical_address)
        if not raw_page:
            return None

        return bytearray(raw_page)

    def read_virtual_page(
        self, virtual_address: int, directory_table_base: Optional[int] = 0
    ) -> Optional[bytearray]:
        """Read a virtual page from the memory dump

        Args:
            virtual_address (int): _description_
            directory_table_base (Optional[int]): if given, corresponds to the DirectoryTableBase value

        Returns:
            Optional[bytearray]: The bytes in the page if found, None otherwise
        """
        raw_page = self.__dump.GetVirtualPage(virtual_address, directory_table_base)
        if not raw_page:
            return None

        return bytearray(raw_page)

    def translate_virtual(
        self, virtual_address: int, directory_table_base: Optional[int] = 0
    ) -> Optional[int]:
        """Translate a virtual address to physical. A directory table base can be optionally
        provided

        Args:
            virtual_address (int): _description_
            directory_table_base (Optional[int]): if given, corresponds to the DirectoryTableBase
            value

        Returns:
            Optional[int]: If found, return the physical address to the virtual address. None
            otherwise
        """
        return self.__dump.VirtTranslate(virtual_address, directory_table_base)
