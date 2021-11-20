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

#include "bptree_poet.h"
#include "bptree.h"
#include "queries.h"

pthread_mutex_t printmutex;

/* default parameter settings */
static size_t num_threads = 1;
// static size_t num_mget = 1;
static float duration = 10.0;
static char *inputfile = NULL;
static char *log_file = NULL;

/* db structure is global */
bptree_t *db;

/* using sigalarm for timer */
volatile bool stop = false;

void trigger(int sig)
{
    stop = true;
}

static void usage(char *binname)
{
    printf("%s [-t #] [-b #] [-l trace] [-d #] [-h]\n", binname);
    printf("\t-t #: number of working threads, by default %" PRIu64 "\n", num_threads);
    printf("\t-d #: duration of the test in seconds, by default %f\n", duration);
    printf("\t-l  : dataset file\n");
    printf("\t-o  : heartbeats log file\n");
    printf("\t-h  : show usage\n");
}

void benchmark_n_threads(result_t *result, thread_param *tp, query *queries, size_t num_queries, pthread_t *threads, size_t num_threads)
{
    printf("\n\nOne round of benchmark,  %ld threads\n\n", num_threads);
    size_t t;

    for (t = 0; t < num_threads; t++)
    {
        tp[t].queries = queries + t * (num_queries / num_threads);
        tp[t].tid = t;
        tp[t].num_ops = num_queries / num_threads;
        tp[t].num_puts = tp[t].num_gets = tp[t].num_miss = tp[t].num_hits = 0;
        tp[t].time = tp[t].tput = 0.0;
        tp[t].stop = &stop;
        tp[t].db = db;
        int rc = pthread_create(&threads[t], NULL, queries_exec, (void *)&tp[t]);
        if (rc)
        {
            perror("failed: pthread_create\n");
            exit(-1);
        }
    }

    /* Start the timer */
    signal(SIGALRM, trigger);
    alarm(duration);

    //Only once
    result->grand_total_time = 0.0;

    result->total_time = 0.0;
    result->total_tput = 0.0;
    result->total_hits = 0;
    result->total_miss = 0;
    result->total_gets = 0;
    result->total_puts = 0;
    result->num_threads = num_threads;

    for (t = 0; t < num_threads; t++)
    {
        void *status;
        int rc = pthread_join(threads[t], &status);
        if (rc)
        {
            perror("error, pthread_join\n");
            exit(-1);
        }
        result->total_time = (result->total_time > tp[t].time) ? result->total_time : tp[t].time;
        result->total_tput += tp[t].tput;
        result->total_hits += tp[t].num_hits;
        result->total_miss += tp[t].num_miss;
        result->total_gets += tp[t].num_gets;
        result->total_puts += tp[t].num_puts;
    }

    result->grand_total_time += result->total_time;
    stop = false;
}

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        usage(argv[0]);
        exit(-1);
    }

    char ch;
    while ((ch = getopt(argc, argv, "t:d:h:l:o:")) != -1)
    {
        switch (ch)
        {
        case 't':
            num_threads = atoi(optarg);
            break;
        case 'd':
            duration = atof(optarg);
            break;
        case 'l':
            inputfile = optarg;
            break;
        case 'o':
            log_file = optarg;
            break;
        case 'h':
            usage(argv[0]);
            exit(0);
            break;
        default:
            usage(argv[0]);
            exit(-1);
        }
    }

    if (inputfile == NULL)
    {
        usage(argv[0]);
        exit(-1);
    }

    query *queries;
    size_t num_queries = queries_init(&queries, inputfile);

    pthread_t threads[num_threads];
    pthread_mutex_init(&printmutex, NULL);

    thread_param tp[num_threads];

    db = bptree_poet_new(NULL, log_file, false);

    result_t result;
    benchmark_n_threads(&result, tp, queries, num_queries, threads, num_threads);

    printf("total_time = %.2f\n", result.grand_total_time);
    printf("total_tput = %.2f\n", (float)(result.total_gets + result.total_puts) / result.grand_total_time);
    printf("total_tput_get = %.2f\n", (float)(result.total_gets) / result.grand_total_time);
    printf("total_tput_insert = %.2f\n", (float)(result.total_puts) / result.grand_total_time);
    printf("total_hitratio = %.4f\n", (float)result.total_hits / result.total_gets);

    free(queries);
    bptree_poet_free(db);

    printf("bye\n");
    return 0;
}
