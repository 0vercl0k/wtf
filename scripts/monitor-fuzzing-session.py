#!/usr/bin/python3

"""Monitors a fuzzing session and once in a while generates a BB coverage report.

@m4drat - 2023
"""

import os
import json
import time
import shutil
import argparse
import subprocess
from pathlib import Path
from typing import List, Any, Dict
from merge_coverage_traces import merge_coverage_files


def generate_coverage_trace(
    wtf: Path, target_fuzzer: str, coverage_reports_dir: Path, inputs_dir: Path, instr_limit: int
) -> bool:
    """Generate a BB coverage trace for a given testcase.

    Args:
        wtf (Path): Path to the WTF binary
        target_fuzzer (str): Name of the target fuzzer
        coverage_reports_dir (Path): Path where to store the generated coverage trace
        inputs_dir (Path): Path to the testcases directory
        instr_limit (int): Maximum number of instructions to execute

    Returns:
        bool: True if coverage traces were generated successfully, False otherwise
    """

    # Generate coverage trace
    p = subprocess.run(
        [
            wtf,
            "run",
            "--name",
            target_fuzzer,
            "--backend=bochscpu",
            "--state=state",
            f"--limit={instr_limit}",
            f"--input={inputs_dir.absolute()}",
            f"--trace-path={coverage_reports_dir}",
            "--trace-type=cov",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )

    return p.returncode == 0


def generate_coverage_traces(
    wtf: Path, target_fuzzer: str, coverage_traces_dir: Path, new_testcases: set, instr_limit: int
) -> List[Path]:
    """Generate BB coverage traces for new testcases.

    Args:
        wtf (Path): Path to the WTF binary
        target_fuzzer (str): Name of the target fuzzer
        coverage_traces_dir (Path): Path where to store the generated coverage traces
        new_testcases (set): Set of new testcases
        instr_limit (int): Maximum number of instructions to execute

    Returns:
        List[Path]: List of generated coverage traces
    """

    coverage_traces: List[Path] = []

    if len(new_testcases) == 0:
        return coverage_traces

    # Copy new testcases to the inputs directory
    testcases_dir = Path(".") / "monitor-inputs"
    testcases_dir.mkdir(exist_ok=True)

    for testcase in new_testcases:
        shutil.copy2(testcase, testcases_dir)

        coverage_report_path = coverage_traces_dir / f"{testcase.name}.trace"
        coverage_traces.append(coverage_report_path)

    if generate_coverage_trace(wtf, target_fuzzer, coverage_traces_dir, testcases_dir, instr_limit) is not True:
        print(f'Failed to generate coverage traces for "{testcases_dir}"')

    # Remove copied testcases
    shutil.rmtree(testcases_dir)

    return coverage_traces


def extract_execs_sec(log_file: Path = Path("master.log")) -> int:
    try:
        last_line = next(reversed(list(open(log_file, "r", encoding="ascii"))))
        return int(float(last_line.rstrip().split(" ")[8]))
    except Exception as e:
        print(f"Failed to extract execs/sec from {log_file}: {e}")
        return 0


def monitor_coverage(
    wtf: Path,
    target_fuzzer: str,
    coverage_traces: Path,
    aggregated_coverage: Path,
    output: Path,
    monitor_interval: int, 
    instr_limit: int
):
    """Monitor a fuzzing session and once in a while generate a BB coverage report.

    Args:
        wtf (Path): Path to the WTF binary
        target_fuzzer (str): Name of the target fuzzer
        coverage_traces (Path): Path where to store generated coverage traces
        aggregated_coverage (Path): Path where to store the aggregated coverage trace
        output (Path): Path to the stats output file
        monitor_interval (int): Interval in seconds between each stats update
        instr_limit (int): Maximum number of instructions to execute
    """

    aggregated_coverage.touch()
    processed_outputs: set = set()
    merged_coverage: set = set()

    stats: Dict[str, Any] = {
        "start_time": time.time(),
        "stats": [
            {
                "relative_timestamp": 0,
                "coverage": 0,
                "crashes": 0,
                "execs_sec": 0,
                "corpus_size": 0,
            }
        ],
    }

    try:
        while True:
            outputs = set(Path(".").glob("outputs/*"))
            new_outputs = outputs - processed_outputs

            timestamp = time.time()

            new_coverage_traces = generate_coverage_traces(
                wtf, target_fuzzer, coverage_traces, new_outputs, instr_limit
            )

            if len(new_coverage_traces) > 0:
                merged_coverage = merge_coverage_files(
                    [aggregated_coverage] + new_coverage_traces, True
                )
            coverage_delta = len(merged_coverage) - stats["stats"][-1]["coverage"]

            total_crashes = len(list(Path(".").glob("crashes/*")))
            crashes_delta = total_crashes - stats["stats"][-1]["crashes"]
            execs_sec = extract_execs_sec()

            print(
                f"[{time.ctime(timestamp)}] Coverage: {len(merged_coverage)} (+{coverage_delta}), "
                f"Crashes: {total_crashes} (+{crashes_delta}), "
                f"Testcases: {len(outputs)} +({len(new_outputs)}), "
                f"Execs/sec: {execs_sec}, Corpus size: {len(processed_outputs)}"
            )

            entry = {
                "relative_timestamp": timestamp - stats["start_time"],
                "coverage": len(merged_coverage),
                "crashes": total_crashes,
                "execs_sec": execs_sec,
                "corpus_size": len(processed_outputs),
            }
            stats["stats"].append(entry)

            with open(output, "w", encoding="ascii") as stats_file:
                json.dump(stats, stats_file, indent=4)

            # Update set of processed outputs
            processed_outputs.update(new_outputs)

            # Update aggregated coverage
            if len(new_coverage_traces) > 0:
                aggregated_coverage.write_text("\n".join(merged_coverage))

            time.sleep(monitor_interval)
    except KeyboardInterrupt:
        # Finalize stats
        with open(output, "w", encoding="ascii") as stats_file:
            json.dump(stats, stats_file, indent=4)

        print("Monitoring stopped!")


def main():
    p = argparse.ArgumentParser()
    p.add_argument(
        "--wtf",
        type=Path,
        help="Path to the WTF binary",
        required=True,
    )
    p.add_argument(
        "--target-dir",
        type=Path,
        help="Path to the target directory",
        required=True,
    )
    p.add_argument(
        "--target-fuzzer",
        type=str,
        help="Name of the target fuzzer",
        required=True,
    )
    p.add_argument(
        "--coverage-traces-dir",
        type=Path,
        help="Relative path where to store the coverage traces",
        default="monitor-coverage-traces",
    )
    p.add_argument(
        "--aggregated-coverage",
        type=Path,
        help="Relative path where to store the aggregated coverage trace",
        default="aggregated_coverage.trace",
    )
    p.add_argument(
        "--output",
        type=Path,
        help="Relative path to the stats output file",
        default="bb_coverage.json",
    )
    p.add_argument(
        "--monitor-interval",
        type=int,
        help="Interval in seconds between two stats updates",
        default=30,
    )
    p.add_argument(
        "--instr-limit",
        type=int,
        help="Maximum instructions executed",
        required=True
    )
    args = p.parse_args()

    if not args.target_dir.exists():
        print(f"Target directory {args.target_dir} does not exist")
        exit(1)

    # Make sure we are in the target directory
    os.chdir(args.target_dir)

    # Remove previous artifacts
    if args.aggregated_coverage.exists():
        args.aggregated_coverage.unlink()

    if args.coverage_traces_dir.exists():
        shutil.rmtree(args.coverage_traces_dir, ignore_errors=True)

    if not args.coverage_traces_dir.exists():
        args.coverage_traces_dir.mkdir()

    monitor_coverage(
        args.wtf,
        args.target_fuzzer,
        args.coverage_traces_dir,
        args.aggregated_coverage,
        args.output,
        args.monitor_interval,
        args.instr_limit,
    )


if __name__ == "__main__":
    main()
