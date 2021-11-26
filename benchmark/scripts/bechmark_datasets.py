import sys
import subprocess
import os
import glob
from os import path
from utils import set_scaling_governor, set_cpu_freq


def benchmark(dataset: str, log_folder: str):
    name = path.basename(dataset).split(".")[0]
    log_file = path.join(log_folder, f"heartbeat_{name}.log")
    result = subprocess.run([
        "bin/bench_store",
        "-t", str(4),
        "-d", "60",
        "-a", str(1),
        "-l", dataset,
        "-o", log_file],
        env={"HEARTBEAT_ENABLED_DIR": "/tmp",
             "LD_LIBRARY_PATH": "/usr/local/lib/"},
        stdout=subprocess.PIPE)
    result.check_returncode()


if __name__ == '__main__':
    set_scaling_governor()
    set_cpu_freq(2100000)

    for ds_fn in glob.glob(f"/opt/datasets/64/*.dat"):
        print("starting benchmark:", ds_fn)
        benchmark(ds_fn, "experiments/datasets_64")
