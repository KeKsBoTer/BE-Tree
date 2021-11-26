# Benchmarking Code

Code for benchmarking the B+Tree.
This code also adds [POET](https://people.cs.uchicago.edu/%7Eckimes/poet) integration to the B+Tree to allow for energy aware runtime adjustments.

## Running the Bechmarks

Make sure you have POET and Heartsbeats installed.

First build the benchmark:
```
$ make 
````

Note: make sure you build the B+ Tree first

Generate the Dataset(s) using instructions in `benchmark/generation/README.md`.
Dataset configurations used for the paper can be found in `benchmark/generation/workloads`.

### POET Benchmark

Run the POET benchmark (must be run as root):
```
$ ./tools/set-governor.sh 
$ export HEARTBEAT_ENABLED_DIR=/tmp 
$ export LD_LIBRARY_PATH=/usr/local/lib/ 
$ ./bin/bench_store_poet -t <num_threads> -d <duration (seconds)> -l <dataset_file> -o <log_dir> -a <avx2 on/off (1/0)>
```


### POET States 

Make sure you have python3 installed and the requirements found in `benchmark/scripts/requirements.txt`.

Calculate CPU performance states for your own system:

```
$ sudo python3 scripts/bechmark_avx2_compare.py <log_folder>  [--avx2]
$ python3 scripts/poet_gen_control_config.py
```

### AVX2 Compare

Make sure you have python3 installed and the requirements found in `benchmark/scripts/requirements.txt`.

Run the benchmark to compare AVX2 vs no AVX2:
```
$ sudo python3 scripts/bechmark_avx2_compare.py <key_size (32 or 64)>
```

Note: needs to be run as root because CPU frequency controll is applied
