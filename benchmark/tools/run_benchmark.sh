#!/bin/bash

./tools/set-governor.sh && \
HEARTBEAT_ENABLED_DIR=/tmp LD_LIBRARY_PATH=/usr/local/lib/ \
./bin/bench_store_poet -t 6 -d 20 -l /opt/datasets/dataset_95_32bit.dat -o $1 -a $2