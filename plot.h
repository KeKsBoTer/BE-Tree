
void node_dot(node *n, FILE *fp)
{
    fprintf(fp, "\"node%lx\" [label = \"", (uintptr_t)n);
    for (int i = 0; i <= ORDER; i++)
    {
        fprintf(fp, "<f%d>", i);
        if (i < n->n)
        {
            fprintf(fp, "%d", n->keys[i]);
            if (n->is_leaf)
                fprintf(fp, "(%lld)", n->children[i].value);
        }
        if (i != ORDER - 1)
        {
            fprintf(fp, " | ");
        }
    }
    fprintf(fp, "\" shape = \"record\"];\n");
    if (!n->is_leaf)
        for (int i = 0; i < n->n + 1; i++)
        {
            node_dot(n->children[i].next, fp);
            char orientation = i != n->n ? 'w' : 'e';
            int src_idx = i != n->n ? i : i - 1;
            fprintf(fp, "\"node%lx\":f%d:s%c -> \"node%lx\":n;", (uintptr_t)n, src_idx, orientation, (uintptr_t)n->children[i].next);
        }
}

void bptree_dot(bptree *tree, FILE *fp)
{
    fprintf(fp, "digraph g {\ngraph [ rankdir = \"TP\"];\n");
    if (tree->root != NULL)
        node_dot(tree->root, fp);
    fprintf(fp, "\n}");
}

void bptree_plot(bptree *tree, const char *filename)
{
    // print tree
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        printf("file can't be opened\n");
        exit(1);
    }
    bptree_dot(tree, fp);
    fclose(fp);
}
