/* dummy key-value storage */

#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include "dummy-keystore.h"

int LOOP_GET = 1000;
int LOOP_SET = 10000;

/* START POET & HEARTBEAT */

// HB Interval (in useconds)
#define HB_INTERVAL 10000
int stop_heartbeat = 0;
pthread_t hb_thread_handler;

#define HB_ENERGY_IMPL
#include <heartbeats/hb-energy.h>
#include <heartbeats/heartbeat-accuracy-power.h>
#include <poet/poet.h>
#include <poet/poet_config.h>

#define PREFIX "DUMMYKEY"

#define USE_POET // Power and performance control

heartbeat_t *heart;
poet_state *state;
static poet_control_state_t *control_states;
static poet_cpu_state_t *cpu_states;
unsigned int num_runs = 1000;

void *heartbeat_timer_thread()
{

    int i = 0;
    while (!stop_heartbeat)
    {

        heartbeat_acc(heart, i, 1);
#ifdef USE_POET
        poet_apply_control(state);
#endif
        i++;
        usleep(HB_INTERVAL);
    }
}

void hb_poet_init()
{
    float min_heartrate;
    float max_heartrate;
    int window_size;
    double power_target;
    unsigned int nstates;

    if (getenv(PREFIX "_MIN_HEART_RATE") == NULL)
    {
        min_heartrate = 0.0;
    }
    else
    {
        min_heartrate = atof(getenv(PREFIX "_MIN_HEART_RATE"));
    }
    if (getenv(PREFIX "_MAX_HEART_RATE") == NULL)
    {
        max_heartrate = 100.0;
    }
    else
    {
        max_heartrate = atof(getenv(PREFIX "_MAX_HEART_RATE"));
    }
    if (getenv(PREFIX "_WINDOW_SIZE") == NULL)
    {
        window_size = 30;
    }
    else
    {
        window_size = atoi(getenv(PREFIX "_WINDOW_SIZE"));
    }
    if (getenv(PREFIX "_POWER_TARGET") == NULL)
    {
        power_target = 70;
    }
    else
    {
        power_target = atof(getenv(PREFIX "_POWER_TARGET"));
    }

    if (getenv("HEARTBEAT_ENABLED_DIR") == NULL)
    {
        fprintf(stderr, "ERROR: need to define environment variable HEARTBEAT_ENABLED_DIR (see README)\n");
        exit(1);
    }

    printf("init heartbeat with %f %f %d\n", min_heartrate, max_heartrate, window_size);

    heart = heartbeat_acc_pow_init(window_size, 100, "heartbeat.log",
                                   min_heartrate, max_heartrate,
                                   0, 100,
                                   1, hb_energy_impl_alloc(), power_target, power_target);
    if (heart == NULL)
    {
        fprintf(stderr, "Failed to init heartbeat.\n");
        exit(1);
    }
#ifdef USE_POET
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
    state = poet_init(heart, nstates, control_states, cpu_states, &apply_cpu_config, &get_current_cpu_state, 1, "poet.log");
    if (state == NULL)
    {
        fprintf(stderr, "Failed to init poet.\n");
        exit(1);
    }
#endif
    printf("heartbeat init'd\n");
}

void hb_poet_finish()
{
#ifdef USE_POET
    poet_destroy(state);
    free(control_states);
    free(cpu_states);
#endif
    heartbeat_finish(heart);
    printf("heartbeat finished\n");
}
/* END POET AND HEARTBEAT */

char *dummydata = "This is a dummy data!";

/* create a dummy data structure */
db_t *db_new()
{
    /* init runtime control (e.g., POET) */

    hb_poet_init();

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int rc = pthread_create(&hb_thread_handler, &attr, heartbeat_timer_thread, NULL);
    if (rc)
    {
        perror("failed: HB thread create\n");
        exit(-1);
    }

    return (db_t *)malloc(sizeof(int));
}

/* wrapper of set command */
int db_put(db_t *db_data, char *key, char *val)
{

    volatile int dummy = 0;
    int j;

    /*insert loop*/
    for (j = 0; j < LOOP_SET; j++)
    {
        dummy = dummy >> 1;
        dummy = dummy - 1;
    }

    return 1;
}

/* wrapper of get command */
char *db_get(db_t *db_data, char *key)
{

    volatile int dummy = 0;
    int j;

    /*get loop*/
    for (j = 0; j < LOOP_GET; j++)
    {
        dummy = dummy >> 1;
        dummy = dummy - 1;
    }

    return dummydata;
}

/* wrapper of free command */
int db_free(db_t *db_data)
{

    stop_heartbeat = 1;

    int rc = pthread_join(hb_thread_handler, NULL);
    if (rc)
    {
        perror("error, pthread_join\n");
        exit(-1);
    }

    hb_poet_finish();

    /*free*/
    //free(db_data);

    return 0;
}
