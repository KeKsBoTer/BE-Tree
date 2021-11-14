#include "bptree.h"
#include "pthread.h"

typedef struct args_t
{
    int tests;
    bptree *tree;
} args_t;

void *rand_insert(void *args)
{
    args_t *t_args = (args_t *)args;
    srand(0);
    value_t buffer;
    for (int i = 0; i < t_args->tests; i++)
    {
        key_t x = rand();
        sprintf(buffer, "%d", x);
        bptree_insert(t_args->tree, x, buffer);
    }
    return NULL;
}

void *rand_get(void *args)
{
    args_t *t_args = (args_t *)args;
    srand(0);
    value_t buffer;
    for (int i = 0; i < t_args->tests; i++)
    {
        key_t x = rand();
        sprintf(buffer, "%d", x);
        value_t *v = bptree_get(t_args->tree, x);
        if (v != NULL && strcmp((char *)v, buffer))
        {
            printf("ERROR: %s != %s\n", buffer, *v != NULL ? *v : "NULL");
        }
        free(v);
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
    bptree *tree = malloc(sizeof(tree));
    bptree_init(tree);

    printf("inserting...\n");

    int num_threads = 4;
    pthread_t threads[num_threads];

    args_t *args = malloc(sizeof(args_t));
    args->tests = tests;
    args->tree = tree;
    for (int t = 0; t < num_threads; t++)
    {
        if (t % 2 == 0)
            pthread_create(threads + t, NULL, rand_insert, args);
        else
            pthread_create(threads + t, NULL, rand_get, args);
    }

    for (int t = 0; t < num_threads; t++)
        pthread_join(threads[t], NULL);
    printf("done!\n");
    bptree_free(tree);
    free(args);
    free(tree);
}