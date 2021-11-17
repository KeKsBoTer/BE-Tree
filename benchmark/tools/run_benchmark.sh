export HEARTBEAT_ENABLED_DIR=/tmp
./tools/set-governor.sh
LD_LIBRARY_PATH=/usr/local/lib/ ./bin/bench_client -t 4 -d 20 -l /opt/datasets/test.dat 