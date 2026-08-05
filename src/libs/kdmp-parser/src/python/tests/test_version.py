#
# This file is part of kdmp-parser project
#
# Released under MIT License, by 0vercl0k - 2023
#
# With contributions from:
# * masthoon - (github.com/masthoon)
# * hugsy - (github.com/hugsy)
#
import kdmp_parser


def test_version():
    assert isinstance(kdmp_parser.version.major, int)
    assert isinstance(kdmp_parser.version.minor, int)
    assert isinstance(kdmp_parser.version.patch, int)
    assert isinstance(kdmp_parser.version.release, str)
