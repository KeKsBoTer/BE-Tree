#include <stdio.h>
#include "./btree.h"

//#define SAVE_TREES

/**
 * @brief writes node (and its children) to a .dot dile
 * 
 * @param n reference to node struct
 * @param fp file to write to
 */
void node_dot(node *n, FILE *fp)
{
    fprintf(fp, "\"node%lu\" [label = \"", (uintptr_t)n);
    for (int i = 0; i < n->min_deg * 2; i++)
    {
        fprintf(fp, "<f%d>", i);
        if (i < n->num_keys)
        {
            fprintf(fp, "%d", n->keys[i]);
            fprintf(fp, "(%lld)", n->values[i].pair.value);
        }
        if (i != n->min_deg * 2 - 1)
        {
            fprintf(fp, " | ");
        }
    }
    fprintf(fp, "\" shape = \"record\"];\n");
    if (!n->leaf)
        for (int i = 0; i < n->num_keys + 1; i++)
        {
            if (n->children[i] != NULL)
            {
                node_dot(n->children[i], fp);
                char orientation = i != n->num_keys ? 'w' : 'e';
                int src_idx = i != n->num_keys ? i : i - 1;
                fprintf(fp, "\"node%lu\":f%d:s%c -> \"node%lu\":n;", (uintptr_t)n, src_idx, orientation, (uintptr_t)n->children[i]);
            }
            else
            {
                printf("ERROR: child %d of node [%d,%d,..] is null\n", i, n->keys[0], n->keys[1]);
            }
        }
}

void btree_dot(btree *tree, FILE *fp)
{
    fprintf(fp, "digraph g {\ngraph [ rankdir = \"TP\"];\n");
    if (tree->root != NULL)
        node_dot(tree->root, fp);
    fprintf(fp, "\n}");
}

void btree_plot(btree *tree, const char *filename)
{
    // print tree
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        printf("file can't be opened\n");
        exit(1);
    }
    btree_dot(tree, fp);
    fclose(fp);
}

int main(int argc, char *argv[])
{
    int test_values = 60000;
    switch (argc)
    {
    case 2:
        test_values = atoi(argv[1]);
        if (test_values == 0)
        {
            printf("cannot convert '%s' to integer\n", argv[1]);
            exit(1);
        }
    }
    srand(0);
    btree tree;
    int order = 48 * 256 / (sizeof(partial_key) * 8) + 1;
    btree_init(&tree, order / 2);
    printf("inserting %d pairs...\n", test_values);
#ifdef SAVE_TREES
    char fn[100];
#endif
    for (int i = 0; i < test_values; i++)
    {
        i_value x = rand() - RAND_MAX / 2;

        btree_insert(&tree, x, x);
        if (i % (test_values / 10) == 0)
        {
            printf("progress: %d%%\n", (int)(100 * ((float)i) / test_values));
        }
#ifdef SAVE_TREES
        sprintf(fn, "trees/tree_%d.dot", x);
        btree_plot(&tree, fn);
#endif
    }
#ifdef SAVE_TREES
    btree_plot(&tree, "trees/final.dot");
#endif
    printf("done!\n");

    srand(0);

    printf("testing...\n");
    i_value *v;
    for (int i = 0; i < test_values; i++)
    {
        i_value x = rand() - RAND_MAX / 2;

        v = btree_get(&tree, x);
        if (i % (test_values / 10) == 0)
        {
            printf("progress: %d%%\n", (int)(100 * ((float)i) / test_values));
        }
        if (v == NULL || *v != x)
            printf("ERROR: %lld: %lld\n", x, v == NULL ? -1 : *v);
    }
    btree_free(&tree);
    printf("done!\n");

#ifdef COUNT_REGISTER_FILL
    printf("register fill: %f (%llu, %llu)\n", 100 * ((float)find_index_fill) / (find_index_cnt * KEY_SIZE), find_index_fill, find_index_cnt);
#endif
    return 0;
}