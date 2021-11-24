import csv
import time
import subprocess
from itertools import product
from multiprocessing import cpu_count
from utils import set_cpu_freq, set_scaling_governor, system_cpu_config

if __name__ == '__main__':
    from os.path import join
    import argparse

    parser = argparse.ArgumentParser(description='B+Tree Benchmarking script')
    parser.add_argument('out_folder', type=str,
                        help='folder')
    parser.add_argument('--avx2', action='store_true',
                        help='use avx2 accelerated bptree')

    args = parser.parse_args()

    out_folder = args.out_folder

    # set scaling_governor to userspace
    set_scaling_governor()

    config = system_cpu_config()

    with open("config/cpu_config", "w") as f:
        writer = csv.DictWriter(f, delimiter="\t", fieldnames=[
                                "#id", "freq", "cores"])
        writer.writerows(config.values())

    for state_id, params in config.items():
        num_threads = int(params["cores"])+1
        set_cpu_freq(int(params["freq"]))
        result = subprocess.run([
            "bin/bench_store",
            "-t", str(num_threads),
            "-d", "10",
            "-l", "/opt/datasets/dataset_95_32bit.dat",
            "-o", join(out_folder, f"heartbeat_{state_id}.log")],
            env={"HEARTBEAT_ENABLED_DIR": "/tmp",
                 "LD_LIBRARY_PATH": "/usr/local/lib/"},
            stdout=subprocess.PIPE)
        result.check_returncode()
        print(f"done with #{state_id}")
