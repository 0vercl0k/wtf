#!/usr/bin/python3

from termcolor import colored
from subprocess import Popen, PIPE, DEVNULL, TimeoutExpired
import yaml
import argparse
import pathlib
import os
import sys
import time


def start_node(
    target_dir: pathlib.Path,
    seed: int,
    node_name: str,
    node_conf: dict,
    master: bool = False,
) -> Popen:
    """Start a node.

    Args:
        target_dir (pathlib.Path): Path to the target directory
        seed (int): Seed to use
        node_name (str): Node name
        node_conf (dict): Node configuration
        master (bool, optional): Whether this node is the master. Defaults to False.

    Returns:
        Popen: Popen object for the started node
    """

    wtf_bin = "wtf.exe" if os.name == "nt" else "wtf"

    if master:
        args = [
            wtf_bin,
            "master",
            f"--runs={node_conf['runs']}",
            f"--max_len={node_conf['max_len']}",
            f"--name={node_conf['name']}",
            f"--target={target_dir.absolute()}",
            f"--inputs={node_conf.get('inputs', target_dir / 'inputs')}",
            f"--outputs={node_conf.get('outputs', target_dir / 'inputs')}",
            f"--seed={seed}",
        ]
    else:
        # Check whether the backend is bochscpu, bxcpu or 0 (also bochscpu)

        if node_conf["backend"] in ["bochscpu", "bxcpu", "0"]:
            args = [
                wtf_bin,
                "fuzz",
                f"--backend={node_conf['backend']}",
                f"--edges={node_conf['edges']}",
                f"--compcov={node_conf['compcov']}",
                f"--laf={node_conf['laf']}",
            ]

            if "laf-allowed-ranges" in node_conf:
                args.append(f"--laf-allowed-ranges={node_conf['laf-allowed-ranges']}")

            args += [
                f"--name={node_conf['name']}",
                f"--target={target_dir.absolute()}",
                f"--limit={node_conf['limit']}",
                f"--seed={seed}",
            ]
            print(args)

        elif node_conf["backend"] in ["kvm", "whv"]:
            if node_conf["limit"] > 15:
                print(
                    f"{colored('WARNING', 'yellow')}: The limit looks too high for this backend, remember that the limit for KVM/WHV is the number of seconds to run the fuzzing session for"
                )

            args = [
                wtf_bin,
                "fuzz",
                f"--backend={node_conf['backend']}",
                f"--name={node_conf['name']}",
                f"--target={target_dir.absolute()}",
                f"--limit={node_conf['limit']}",
                f"--seed={seed}",
            ]

        else:
            print(f"Unknown backend: {node_conf['backend']}")
            exit(1)

    print(f"Starting node: {node_name} with args: {args}")

    # Start the node
    output = None if master else DEVNULL
    return Popen(args, stdout=output, stderr=output, bufsize=1, universal_newlines=True)


def main():
    p = argparse.ArgumentParser()
    p.add_argument(
        "config",
        type=pathlib.Path,
        help="Path to the configuration file",
    )
    args = p.parse_args()

    with open(args.config, "r", encoding="ascii") as conf_stream:
        try:
            config = yaml.safe_load(conf_stream)
        except yaml.YAMLError as exc:
            print(exc)
            exit(1)

    target_dir = pathlib.Path(config.get("target-dir", os.getcwd())).resolve()
    seed = config.get("seed", 0)
    run_for = config.get("run-for", 60 * 60 * 24 * 365 * 10)

    subprocesses = []

    # Start the master node
    if "master" not in config:
        print("No master node defined")
        exit(1)

    subprocesses.append(start_node(target_dir, seed, "master", config["master"], master=True))
    master = subprocesses[0]

    # Wait for master node to finish
    try:
        if master.wait(10) != 0:
            print("Master node failed to start!")
            exit(1)
    except TimeoutExpired:
        print("Master node succesfully started!")

    # Start all secondary nodes
    if "nodes" not in config:
        print("No secondary nodes defined")
        exit(1)

    for node_name in config["nodes"]:
        node_conf = config["nodes"][node_name]
        subprocesses.append(start_node(target_dir, seed, node_name, node_conf))

        last = subprocesses[-1]
        try:
            if last.wait(2) != 0:
                print(f"Fuzzer-node: {node_name} failed to start!")
                exit(1)
        except TimeoutExpired:
            print(f"Fuzzer-node: {node_name} succesfully started!")

    # Wait for master node to finish
    try:
        master.wait(run_for)
    except TimeoutExpired:
        print("Timeout expired, killing all nodes")
        master.kill()
    except KeyboardInterrupt:
        print("Killing all nodes")
        master.kill()
    finally:
        # Wait for all nodes to finish
        for node in subprocesses[1:]:
            node.wait()


if __name__ == "__main__":
    main()
