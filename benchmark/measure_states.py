import csv
import time
import subprocess


def read_cpu_config():

    config = {}

    with open("config/cpu_config", "r") as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            id = row["#id"]
            del row["#id"]
            config[id] = row
    return config


if __name__ == '__main__':
    from os.path import join
    import argparse

    parser = argparse.ArgumentParser(description='B+Tree Benchmarking script')
    parser.add_argument('out-folder', dest="out_folder", type=str,
                        help='an integer for the accumulator')
    parser.add_argument('--avx2', dest='avx2', action='store_true', default=False,
                        help='use avx2 accelerated bptree')

    args = parser.parse_args()

    out_folder = "measurements"

    # set scaling_governor to userspace
    subprocess.run(["echo", "userspace", "|", "tee",
                    "/sys/devices/system/cpu/*/cpufreq/scaling_governor"], shell=True).check_returncode()

    def set_cpu_freq(freq):
        result = subprocess.run(
            ["cpupower", "frequency-set", "-f", str(freq)], stdout=subprocess.PIPE)
        result.check_returncode()

        time.sleep(3)

        # check if cpu frequency change was successfull
        result = subprocess.run(
            ["cat", "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq"], stdout=subprocess.PIPE)
        current_freq = int(result.stdout.decode("utf-8"))
        if freq != current_freq:
            raise Exception(
                f"setting CPU frequency changed (target=={freq},current_freq={current_freq})")
        print(f"changed CPU frequency to {freq}")

    config = read_cpu_config()

    for state_id, params in config.items():
        num_threads = int(params["cores"])+1
        set_cpu_freq(int(params["freq"]))
        result = subprocess.run([
            "bin/bench_store",
            "-t", str(num_threads),
            "-d", "10",
            "-l", "/opt/datasets/test.dat",
            "-o", join(out_folder, "heartbeat_{state_id}.log")],
            env={"HEARTBEAT_ENABLED_DIR": "/tmp",
                 "LD_LIBRARY_PATH": "/usr/local/lib/"},
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        result.check_returncode()
        print(f"done with #{state_id}")
