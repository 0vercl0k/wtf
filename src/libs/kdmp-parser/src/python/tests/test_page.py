#
# This file is part of kdmp-parser project
#
# Released under MIT License, by 0vercl0k - 2023
#
# With contributions from:
# * masthoon - (github.com/masthoon)
# * hugsy - (github.com/hugsy)
#
import kdmp_parser.page


def test_page():
    assert callable(kdmp_parser.page.align)
    assert callable(kdmp_parser.page.offset)
    assert isinstance(kdmp_parser.page.size, int)
    assert kdmp_parser.page.size in kdmp_parser.page.VALID_PAGE_SIZES
