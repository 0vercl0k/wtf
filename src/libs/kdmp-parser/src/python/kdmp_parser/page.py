from typing import Generator
from ._kdmp_parser import (  # type: ignore
    PageSize as size,
    PageAlign as align,
    PageOffset as offset,
    KernelDumpParser as _KernelDumpParser,
)

VALID_PAGE_SIZES = (0x1000, 0x20_0000, 0x4000_0000)


class PageIterator:
    """Access a dump pages"""

    def __init__(self, dump: _KernelDumpParser) -> None:
        self.__dump = dump

    def __getitem__(self, physical_address: int) -> bytearray:
        raw_page = self.__dump.GetPhysicalPage(physical_address)
        if not raw_page:
            raise IndexError
        return bytearray(raw_page)

    def __iter__(self) -> Generator[int, None, None]:
        return self.keys()

    def __contains__(self, addr: int) -> bool:
        return addr in self.keys()

    def __len__(self) -> int:
        return len(list(self.keys()))

    def keys(self) -> Generator[int, None, None]:
        for page_addr in self.__dump.GetPhysmem():
            yield page_addr

    def values(self) -> Generator[bytearray, None, None]:
        for page_addr in self.__dump.GetPhysmem():
            yield self[page_addr]

    def items(self) -> Generator["tuple[int, bytearray]", None, None]:
        for page_addr in self.__dump.GetPhysmem():
            yield page_addr, self[page_addr]
