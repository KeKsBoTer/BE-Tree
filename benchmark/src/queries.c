#include <stdio.h>
#include <pthread.h>
#include <inttypes.h>
#include "queries.h"
#include "bptree_poet.h"

/* init all queries from the ycsb trace file before issuing them */
size_t queries_init(query **queries, char *filename)
{
    FILE *input;

    input = fopen(filename, "rb");
    if (input == NULL)
    {
        perror("can not open file");
        perror(filename);
        exit(1);
    }

    size_t key_len, val_len, num_queries;
    int n;
    n = fread(&key_len, sizeof(key_len), 1, input);
    if (n != 1)
        perror("fread error");

    char error_buffer[1024];
    if (key_len != NKEY)
    {
        sprintf(error_buffer, "NKEY (%d) != key_len (%ld) in dataset %s", NKEY, key_len, filename);
        perror(error_buffer);
    }

    n = fread(&val_len, sizeof(val_len), 1, input);
    if (n != 1)
        perror("fread error");

    if (key_len != NKEY)
    {
        sprintf(error_buffer, "NVAL (%d) != val_len (%ld) in dataset %s", NVAL, val_len, filename);
        perror(error_buffer);
    }

    n = fread(&num_queries, sizeof(num_queries), 1, input);
    if (n != 1)
        perror("fread error");

    printf("trace(%s):\n", filename);
    printf("\tkey_len = %zu\n", key_len);
    printf("\tval_len = %zu\n", val_len);
    printf("\tnum_queries = %zu\n", num_queries);
    printf("\n");

    *queries = (query *)malloc(sizeof(query) * num_queries);
    if (*queries == NULL)
    {
        perror("not enough memory to init queries\n");
        exit(-1);
    }

    size_t num_read;
    num_read = fread(*queries, sizeof(query), num_queries, input);
    if (num_read < num_queries)
    {
        fprintf(stderr, "num_read: %zu\n", num_read);
        perror("can not read all queries\n");
        fclose(input);
        exit(-1);
    }

    fclose(input);
    printf("queries_init...done\n");
    return num_queries;
}

/* Calculate the second difference*/
static double timeval_diff(struct timeval *start,
                           struct timeval *end)
{
    double r = end->tv_sec - start->tv_sec;

    /* Calculate the microsecond difference */
    if (end->tv_usec > start->tv_usec)
        r += (end->tv_usec - start->tv_usec) / 1000000.0;
    else if (end->tv_usec < start->tv_usec)
        r -= (start->tv_usec - end->tv_usec) / 1000000.0;
    return r;
}

/* executing queries at each thread */
void *queries_exec(void *param)
{
    /* get the key-value store structure */
    struct timeval tv_s, tv_e;

    thread_param *p = (thread_param *)param;

    printf("start benching using thread%" PRIu64 "\n", p->tid);

    query *queries = p->queries;
    p->time = 0;

    /* Strictly obey the timer */
    while (!*p->stop)
    {
        gettimeofday(&tv_s, NULL); // start timing
        for (size_t i = 0; i < p->num_ops; i++)
        {
            enum query_types type = queries[i].type;
            bp_key_t key = *((bp_key_t *)queries[i].hashed_key);
            if (type == query_put)
            {
                bptree_poet_insert(p->db, key, (value_t)key);
                p->num_puts++;
            }
            else if (type == query_get)
            {
                value_t val;
                bool found = bptree_poet_get(p->db, key, &val);
                p->num_gets++;
                if (!found)
                {
                    // cache miss, put something (garbage) in cache
                    p->num_miss++;
                    bptree_insert(p->db, key, (value_t)key);
                }
                else
                {
                    p->num_hits++;
                }
            }
            else
            {
                fprintf(stderr, "unknown query type\n");
            }

            /* Strictly obey the timer */
            if (*p->stop)
                break;
        }
        gettimeofday(&tv_e, NULL); // stop timing
        p->time += timeval_diff(&tv_s, &tv_e);
    }

    size_t nops = p->num_gets + p->num_puts;
    p->tput = nops / p->time;

    printf("thread%" PRIu64 " gets %" PRIu64 " items in %.2f sec \n",
           p->tid, nops, p->time);
    printf("#put = %zu, #get = %zu\n", p->num_puts, p->num_gets);
    printf("#miss = %zu, #hits = %zu\n", p->num_miss, p->num_hits);
    printf("hitratio = %.4f\n", (float)p->num_hits / p->num_gets);
    printf("tput = %.2f\n", p->tput);
    printf("\n");

    printf("queries_exec...done\n");
    return NULL;
}
