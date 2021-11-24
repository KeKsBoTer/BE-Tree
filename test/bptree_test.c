#include <stdbool.h>
#include "bptree.h"
#include "pthread.h"

typedef struct args_t
{
    int tests;
    bptree_t *tree;
} args_t;

void *rand_insert(void *args)
{
    args_t *t_args = (args_t *)args;
    srand(0);
    for (int i = 0; i < t_args->tests; i++)
    {
        bp_key_t x = rand();
        bptree_insert(t_args->tree, x, (value_t)x);
    }
    return NULL;
}

void *rand_get(void *args)
{
    args_t *t_args = (args_t *)args;
    srand(0);
    for (int i = 0; i < t_args->tests; i++)
    {
        bp_key_t x = rand();
        value_t v;
        bool found = bptree_get(t_args->tree, x, &v);
        if (found && x != v)
        {
            printf("ERROR: %ld != %ld\n", x, v);
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int tests = 1000;
    bool use_avx2 = false;
    switch (argc)
    {
    case 3:
        use_avx2 = atoi(argv[2]);
        if (use_avx2 != 1 && use_avx2 != 0)
        {
            printf("cannot convert '%s' to bool (0 or 1)\n", argv[2]);
            exit(1);
        }
        if (use_avx2)
            printf("avx2 is on!\n");
        else
            printf("avx2 is off!\n");
    case 2:
        tests = atoi(argv[1]);
        if (tests == 0)
        {
            printf("cannot convert '%s' to integer\n", argv[1]);
            exit(1);
        }
    }

    bptree_t *tree = malloc(sizeof(bptree_t));
    bptree_init(tree, use_avx2);

    float insert_ratio = 0.05;

    int num_threads = 2;
    pthread_t threads[num_threads];

    args_t *args_insert = malloc(sizeof(args_t));
    args_insert->tests = (int)(tests * insert_ratio);
    args_insert->tree = tree;

    printf("inserting %d...\n", (int)(tests * insert_ratio));

    args_t *args_get = malloc(sizeof(args_t));
    args_get->tests = (int)(tests * (1 - insert_ratio));
    args_get->tree = tree;

    printf("retrieving %d...\n", (int)(tests * (1 - insert_ratio)));

    for (int t = 0; t < num_threads; t++)
    {
        if (t % 2 == 0)
        {
            pthread_create(threads + t, NULL, rand_insert, args_insert);
            // pthread_join(threads[t], NULL);
        }
        else
        {
            pthread_create(threads + t, NULL, rand_get, args_get);
            // pthread_join(threads[t], NULL);
        }
    }

    for (int t = 0; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    printf("done!\n");
    bptree_free(tree);
    free(args_get);
    free(args_insert);
    free(tree);
}