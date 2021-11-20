#pragma once
#include <stdbool.h>
#include "bptree.h"

/*
 * size of the key in bytes
 */
#define NKEY 4
#define NVAL 24

/* type of each query */
enum query_types
{
    query_put = 0,
    query_get,
    query_del,
};

/* 
 * format of each query, it has a key and a type and we don't care
 * the value
 */
typedef struct __attribute__((__packed__))
{
    char hashed_key[NKEY];
    char type;
} query;

typedef struct
{
    size_t tid;
    query *queries;
    size_t num_ops;
    size_t num_puts;
    size_t num_gets;
    size_t num_miss;
    size_t num_hits;
    double tput;
    double time;
    volatile bool *stop;
    bptree_t *db;
} thread_param;

size_t queries_init(query **queries, char *filename);
void *queries_exec(void *param);

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
