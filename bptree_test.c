#include "bptree.h"
#include "plot.h"

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
    bptree tree;
    bptree_init(&tree);
    srand(0);
    printf("inserting %d random values...\n", tests);
    for (int i = 0; i < tests; i++)
    {
        key_t x = rand();
        bptree_insert(&tree, x, x);
    }
    srand(0);
    printf("done!\nretrieving...\n");
    for (int i = 0; i < tests; i++)
    {
        key_t x = rand();
        value_t *v = bptree_get(&tree, x);
        if (v == NULL || *v != x)
        {
            printf("ERROR: %d != %lld\n", x, v != NULL ? *v : -1);
        }
    }
    bptree_free(&tree);
    printf("done!\n");
}