import time
import subprocess
from itertools import product
from multiprocessing import cpu_count


def system_cpu_freqs():
    result = subprocess.run(
        ["cat", "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies"], stdout=subprocess.PIPE)
    steps = reversed(
        list(map(int, result.stdout.decode().rstrip().split(" "))))
    return steps


def system_cpu_config():

    steps = system_cpu_freqs()
    num_cores = cpu_count()
    config = {}

    for i, (n, step) in enumerate(product(range(num_cores), steps)):
        config[i] = {
            "#id": i,
            "freq": step,
            "cores": n
        }

    return config


def set_scaling_governor():
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
