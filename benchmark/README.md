# Benchmark codes
### Credits: https://github.com/efficient/memc3 . Modified for the course project by Ibrahim Umar.
---


1. The benchmark program is the ```bench_client.c```. As it is, it will only benchmark using the dummy key-value storage engine ```example/dummy-keystore.c```.

2. You can use the template inside ```example/dummy-keystore.c``` and ```example/dummy-keystore.h``` to integrate your key-value store library.

3. The datasets are provided in the ```/opt/datasets``` path. You may use any of the provided files.

4. Use ```make``` to compile the benchmark.

4. Now act as root (POET needs to access RAPL library using root): ```sudo bash```

5. Set the environment variable for the libheartbeat: ```export HEARTBEAT_ENABLED_DIR=/tmp```

6. Make sure you are running the 'userspace' CPU governor: ```../tools/set-governor.sh```

7. To run : ```./bench_client -t <no. of threads> -d <duration>  -l <path to dataset>```

8. The dummy key-value store library uses POET, please note that POET and heartbeat library are installed in the ```/usr/local``` path. Therefore,you don't need to reinstall it.
