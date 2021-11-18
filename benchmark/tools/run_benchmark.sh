./tools/set-governor.sh && \
HEARTBEAT_ENABLED_DIR=/tmp LD_LIBRARY_PATH=/usr/local/lib/ \
./bin/bench_client -t 4 -d 30 -l /opt/datasets/test.dat 