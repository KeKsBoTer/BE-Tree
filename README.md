# Energy Aware SIMD Accelerated B+ Tree 

Building the B+ Tree (no POET)
```
$ make
```

Runnning a benchmark that checks correctness (inserts random values and checks result of get)
```
$ ./bin/bptree_test <number of values> <avx2 on/off (0/1)>
```

Informations about running the benchmarks (with POET integration) can be found in `benchmark/README.md`