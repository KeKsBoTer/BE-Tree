# Benchmarking Code

Code for benchmarking the B+Tree.
This code also adds [POET](https://people.cs.uchicago.edu/%7Eckimes/poet) integration to the B+Tree to allow for energy aware runtime adjustments.

## Build

All C code for the benchmarks can be build with a single `make` command.

## Run Benchmarks

```
$ cd benchmark
$ sudo bash benchmark/tools/run_benchmark.sh <log_dir>
```
Note: This must be run as root in order to allow for CPU frequency adjustments.