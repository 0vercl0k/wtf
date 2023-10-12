#!/usr/bin/python3

"""Merge coverage traces from multiple files into one.

@m4drat - 2023
"""


import argparse
from pathlib import Path
from alive_progress import alive_bar


def read_file(file_name: str) -> list:
    with open(file_name, mode="r", encoding="ascii") as cov_file:
        return cov_file.read().splitlines()


def merge_coverage_files(file_names: list, disable_progress: bool = False) -> set:
    merged: set = set()
    with alive_bar(
        len(file_names), title="Merging coverage traces", disable=disable_progress
    ) as bar:
        for file_name in file_names:
            merged.update(read_file(file_name))
            bar()

    return merged


def main():
    p = argparse.ArgumentParser()
    p.add_argument(
        "--coverage-dir",
        type=Path,
        help="Path to directory containing coverage traces",
        required=True,
    )
    p.add_argument(
        "--output",
        type=Path,
        help="Path to merged coverage trace",
        default="merged_coverage.trace",
    )
    args = p.parse_args()

    cov_merged = merge_coverage_files(list(args.coverage_dir.glob("*.trace")))
    cov_lines = map(lambda cov_entry: cov_entry + "\n", cov_merged)

    with open(args.output, "w", encoding="ascii") as merged:
        merged.writelines(cov_lines)


if __name__ == "__main__":
    main()
