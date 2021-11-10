#include "bptree.h"
// #include "plot.h"
#include "stop_watch.h"

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
    bptree_t tree;
    bptree_init(&tree);
    srand(0);
    sw_init();
    sw_start();
    value_t buffer;
    // printf("inserting %d random values...\n", tests);
    // char fn[50];
    for (int i = 0; i < tests; i++)
    {
        key_t x = rand();
        sprintf(buffer, "%d", x);
        bptree_insert(&tree, x, buffer);
        // sprintf(fn, "trees/tree_%d.dot", i);
        // bptree_plot(&tree, fn);
    }
    srand(0);
    // printf("done!\nretrieving...\n");
    sw_stop();
    printf("insert:\t%fs\n", readTotalSeconds());
    sw_start();
    for (int i = 0; i < tests; i++)
    {
        key_t x = rand();
        sprintf(buffer, "%d", x);
        value_t *v = bptree_get(&tree, x);
        if (v == NULL || strcmp((char *)v, buffer))
        {
            printf("ERROR: %s != %s\n", buffer, *v != NULL ? *v : "NULL");
        }
    }
    sw_stop();
    printf("get:\t%fs\n", readTotalSeconds());
    // printf("done!\n");
    bptree_free(&tree);
}