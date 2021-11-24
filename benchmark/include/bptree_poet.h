#define _GNU_SOURCE
#include <getopt.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sched.h>
#include <pthread.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <inttypes.h>
#include <signal.h>

#define HB_ENERGY_IMPL
#include <heartbeats/hb-energy.h>
#include <heartbeats/heartbeat-accuracy.h>
#include <poet/poet.h>
#include <poet/poet_config.h>

#include "bptree.h"

/* create a dummy data structure */
bptree_t *bptree_poet_new(const char *poet_log_name, const char *heartbeats_log_name, bool use_poet, bool use_avx2);

/* wrapper of set command */
int bptree_poet_insert(bptree_t *bptree, key_t key, value_t val);

/* wrapper of get command */
bool bptree_poet_get(bptree_t *bptree, key_t key, value_t *result);

/* wrapper of free command */
int bptree_poet_free(bptree_t *bptree);