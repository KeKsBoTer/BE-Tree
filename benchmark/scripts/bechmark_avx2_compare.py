import subprocess
from utils import set_scaling_governor, system_cpu_config, set_cpu_freq


def benchmark(freq, num_threads, use_avx2: bool):
    set_cpu_freq(freq)
    log_file = f"experiments/{'with_avx2' if use_avx2 else 'without_avx2'}/heartbeat_{freq}.log"
    print(log_file)
    result = subprocess.run([
        "bin/bench_store",
        "-t", str(num_threads),
        "-d", "10",
        "-a", str(int(use_avx2)),
        "-l", "/opt/datasets/dataset_95_32bit.dat",
        "-o", log_file],
        env={"HEARTBEAT_ENABLED_DIR": "/tmp",
             "LD_LIBRARY_PATH": "/usr/local/lib/"},
        stdout=subprocess.PIPE)
    result.check_returncode()


if __name__ == '__main__':
    # set scaling_governor to userspace
    set_scaling_governor()

    config = system_cpu_config()

    for state_id, params in config.items():
        num_threads = int(params["cores"])+1
        # only test with maximum load for better difference in numbers
        if num_threads == 4:
            benchmark(int(params["freq"]), num_threads, True)
            print(f"done with #{state_id} (with avx2)")
            benchmark(int(params["freq"]), num_threads, False)
            print(f"done with #{state_id} (without avx2)")
