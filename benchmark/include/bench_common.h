#ifndef _BENCH_COMMON_H_
#define _BENCH_COMMON_H_

/* bench result */
typedef struct __attribute__((__packed__))
{
    double grand_total_time;
    double total_tput;
    double total_time;
    size_t total_hits;
    size_t total_miss;
    size_t total_gets;
    size_t total_puts;
    size_t num_threads;
} result_t;
#endif
