#include <stdio.h>
#include "./btree.h"

void node_dot(node *n, FILE *fp)
{
    fprintf(fp, "\"node%d\" [label = \"", (int)n);
    for (int i = 0; i < n->min_deg * 2; i++)
    {
        fprintf(fp, "<f%d>", i);
        if (i < n->num_keys)
        {
            fprintf(fp, "%lld", n->keys[i]);
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
            node_dot(n->children[i], fp);
            char orientation = i != n->num_keys ? 'w' : 'e';
            int src_idx = i != n->num_keys ? i : i - 1;
            fprintf(fp, "\"node%d\":f%d:s%c -> \"node%d\":n;", (int)n, src_idx, orientation, (int)n->children[i]);
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
    int test_values = 20;
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
    btree tree;
    int order = 256 / (sizeof(partial_key) * 8) + 1;
    btree_init(&tree, order / 2);

    printf("inserting...");
    for (int i = 0; i < test_values; i += 2)
    {
        int x = i - (1 << 15);
        btree_insert(&tree, x, x * 2);
    }
    for (int i = 1; i < test_values; i += 2)
    {
        int x = i - (1 << 15);
        btree_insert(&tree, x, x * 2);
    }
    btree_plot(&tree, "trees/final.dot");
    printf("done!\n");

    printf("testing...");
    i_value *v;
    for (int i = 0; i < test_values; i++)
    {
        int x = i - (1 << 15);
        v = btree_get(&tree, x);
        if (v == NULL || *v != x * 2)
            printf("ERROR: %d: %d\n", x, v == NULL ? -1 : *v);
    }
    btree_free(&tree);
    printf("done!\n");
    return 0;
}