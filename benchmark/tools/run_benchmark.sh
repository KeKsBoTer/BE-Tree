./tools/set-governor.sh && \
HEARTBEAT_ENABLED_DIR=/tmp LD_LIBRARY_PATH=/usr/local/lib/ \
./bin/bench_store_poet -t 4 -d 10 -l /opt/datasets/test.dat 