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

#include "bptree_poet.h"
#include "bptree.h"

// POET / HEARBEAT related stuff

#define PREFIX "BPTREE"

heartbeat_t *heart;
poet_state *state;

static poet_control_state_t *control_states;
static poet_cpu_state_t *cpu_states;

int heartbeats_counter;

bool use_poet = false;

void hb_poet_init(const char *poet_log_name, const char *heartbeats_log_name, bool _use_poet)
{
    use_poet = _use_poet;
    float target_heartrate = 190.0;
    int window_size = 50;
    heartbeats_counter = 0;

    if (getenv(PREFIX "_TARGET_HEART_RATE") != NULL)
    {
        target_heartrate = atof(getenv(PREFIX "_TARGET_HEART_RATE"));
    }

    if (getenv(PREFIX "_WINDOW_SIZE") != NULL)
    {
        window_size = atoi(getenv(PREFIX "_WINDOW_SIZE"));
    }

    if (getenv("HEARTBEAT_ENABLED_DIR") == NULL)
    {
        fprintf(stderr, "ERROR: need to define environment variable HEARTBEAT_ENABLED_DIR (see README)\n");
        exit(1);
    }

    printf("init heartbeat with %f %d\n", target_heartrate, window_size);

    heart = heartbeat_acc_pow_init(window_size,
                                   100, heartbeats_log_name,
                                   // min and max target rate is the same
                                   target_heartrate, target_heartrate,
                                   0, 100,
                                   1, hb_energy_impl_alloc(),
                                   10, 10);
    if (heart == NULL)
    {
        fprintf(stderr, "Failed to init heartbeat.\n");
        exit(1);
    }
    if (use_poet)
    {
        unsigned int nstates;

        if (get_control_states("config/control_config", &control_states, &nstates))
        {
            fprintf(stderr, "Failed to load control states.\n");
            exit(1);
        }
        if (get_cpu_states("config/cpu_config", &cpu_states, &nstates))
        {
            fprintf(stderr, "Failed to load cpu states.\n");
            exit(1);
        }
        state = poet_init(heart, nstates, control_states, cpu_states, &apply_cpu_config, &get_current_cpu_state, 1, poet_log_name);
        if (state == NULL)
        {
            fprintf(stderr, "Failed to init poet.\n");
            exit(1);
        }
        printf("poet init'd\n");
    }
    printf("heartbeat init'd\n");
}

void hb_poet_finish()
{
    if (use_poet)
    {
        poet_destroy(state);
        free(control_states);
        free(cpu_states);
    }
    heartbeat_finish(heart);
    printf("heartbeat finished\n");
}

/* create a dummy data structure */
bptree_t *bptree_poet_new(const char *poet_log_name, const char *heartbeats_log_name, bool use_poet)
{
    hb_poet_init(poet_log_name, heartbeats_log_name, use_poet);

    bptree_t *bptree = malloc(sizeof(bptree_t));
    bptree_init(bptree, true);

    return bptree;
}

void register_heartbeat()
{
    if (heartbeats_counter % 10000 == 0)
    {
        heartbeat_acc(heart, heartbeats_counter, 1);
        if (use_poet)
            poet_apply_control(state);
    }
    heartbeats_counter++;
}

/* wrapper of set command */
int bptree_poet_insert(bptree_t *bptree, key_t key, value_t val)
{
    register_heartbeat();
    bptree_insert(bptree, key, val);
    return 1;
}

/* wrapper of get command */
bool bptree_poet_get(bptree_t *bptree, key_t key, value_t *result)
{
    register_heartbeat();
    return bptree_get(bptree, key, result);
}

/* wrapper of free command */
int bptree_poet_free(bptree_t *bptree)
{
    hb_poet_finish();

    bptree_free(bptree);
    free(bptree);

    return 0;
}
