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
        key_t x = rand();
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
        key_t x = rand();
        value_t *v = bptree_get(t_args->tree, x);
        if (v != NULL && x != *v)
        {
            printf("ERROR: %d != %ld\n", x, v != NULL ? (uint64_t)*v : 0);
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int tests = 10;
    switch (argc)
    {
    case 2:
        tests = atoi(argv[1]);
        if (tests == 0)
        {
            printf("cannot convert '%s' to integer\n", argv[1]);
            exit(1);
        }
    }
    bptree_t *tree = malloc(sizeof(bptree_t));
    bptree_init(tree);

    printf("inserting %d...\n", tests);

    int num_threads = 4;
    pthread_t threads[num_threads];

    args_t *args = malloc(sizeof(args_t));
    args->tests = tests;
    args->tree = tree;
    for (int t = 0; t < num_threads; t++)
    {
        if (t % 2 == 0)
        {
            pthread_create(threads + t, NULL, rand_insert, args);
        }
        else
        {
            pthread_create(threads + t, NULL, rand_get, args);
        }
    }

    for (int t = 0; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    printf("done!\n");
    bptree_free(tree);
    free(args);
    free(tree);
}