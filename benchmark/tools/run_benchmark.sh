#!/bin/bash

./tools/set-governor.sh && \
HEARTBEAT_ENABLED_DIR=/tmp LD_LIBRARY_PATH=/usr/local/lib/ \
./bin/bench_store_poet -t 4 -d 30 -l /opt/datasets/dataset_95_32bit.dat -o $1