#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define ORDER 6

typedef u_int64_t value_t;
typedef u_int64_t key_t;

typedef union leaf
{
    value_t value;
    struct node *next;
} leaf;

typedef struct node
{
    key_t keys[ORDER - 1];
    leaf children[ORDER];

    uint16_t n;
    bool is_leaf;
} node;

void node_init(node *n, bool is_leaf)
{
    n->n = 0;
    n->is_leaf = is_leaf;
}

uint16_t find_index(key_t keys[ORDER - 1], uint16_t n, key_t key)
{
    uint16_t i = 0;
    while (i < n && key > keys[i])
        i++;
    return i;
}

value_t *node_get(node *n, key_t key)
{
    uint16_t i = find_index(n->keys, n->n, key);
    bool eq = n->keys[i] == key;
    if (n->is_leaf)
    {
        if (eq)
            return &n->children[i].value;
        else
            return NULL;
    }
    else
    {
        if (eq)
            return node_get(n->children[i + 1].next, key);
        else
            return node_get(n->children[i].next, key);
    }
}

void node_split(node *n, uint16_t i, node *child)
{
    node *right = (node *)malloc(sizeof(node));
    node_init(right, child->is_leaf);
    int min_deg = ORDER / 2;
    right->n = min_deg;
    // y = child
    // z = right

    for (int j = 0; j < min_deg; j++)
    {
        right->keys[j] = child->keys[j + min_deg - 1];
        right->children[j] = child->children[j + min_deg - 1];
    }

    // if non leaf node also copy last one
    if (!child->is_leaf)
        right->children[min_deg] = child->children[min_deg + min_deg - 1];

    // Reduce the number of keys in y
    child->n = min_deg - 1;

    // Since this node is going to have a new child,
    // create space of new child
    for (int j = n->n; j >= i + 1; j--)
        n->children[j + 1] = n->children[j];

    // Link the new child to this node
    n->children[i + 1].next = right;

    // A key of y will move to this node. Find the location of
    // new key and move all greater keys one space ahead
    for (int j = n->n - 1; j >= i; j--)
        n->keys[j + 1] = n->keys[j];

    // Copy the middle key of y to this node
    n->keys[i] = child->keys[min_deg - 1];

    // Increment count of keys in this node
    n->n++;
}

void node_insert(node *n, key_t key, value_t value)
{
    uint16_t i = find_index(n->keys, n->n, key);
    bool eq = n->keys[i] == key;
    if (n->is_leaf)
    {
        if (eq)
        {
            n->children[i].value = value;
        }
        else
        {
            // shift values to right an insert
            for (int j = n->n; j > i; j--)
            {
                n->keys[j] = n->keys[j - 1];
                n->children[j] = n->children[j - 1];
            }
            n->keys[i] = key;
            n->children[i].value = value;
            n->n++;
        }
    }
    else
    {
        if (eq)
            i++;
        if (n->children[i].next->n == ORDER - 1)
        {
            // child node is full
            node_split(n, i, n->children[i].next);
            if (n->keys[i] < key)
                i++;
        }
        node_insert(n->children[i].next, key, value);
    }
}

typedef struct bptree
{
    node *root;
} bptree;

void bptree_init(bptree *tree)
{
    tree->root = NULL;
}

value_t *bptree_get(bptree *tree, key_t key)
{
    if (tree->root == NULL)
        return NULL;
    else
        return node_get(tree->root, key);
}
void bptree_insert(bptree *tree, key_t key, value_t value)
{
    if (tree->root == NULL)
    {
        tree->root = (node *)malloc(sizeof(node));
        tree->root->keys[0] = key;
        tree->root->children[0].value = value;
        tree->root->is_leaf = true;
        tree->root->n = 1;
    }
    else
    {
        if (tree->root->n == ORDER - 1)
        {
            node *s = (node *)malloc(sizeof(node));
            node_init(s, false);
            s->children[0].next = tree->root;
            node_split(s, 0, tree->root);
            int i = 0;
            if (s->keys[0] < key)
                i++;
            node_insert(s->children[i].next, key, value);

            // Change root
            tree->root = s;
        }
        else
        {
            node_insert(tree->root, key, value);
        }
    }
}

void node_dot(node *n, FILE *fp)
{
    fprintf(fp, "\"node%lu\" [label = \"", (uintptr_t)n);
    for (int i = 0; i < ORDER; i++)
    {
        fprintf(fp, "<f%d>", i);
        if (i < n->n)
        {
            fprintf(fp, "%llu", n->keys[i]);
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
            if (n->children[i].next != NULL)
            {
                node_dot(n->children[i].next, fp);
                char orientation = i != n->n ? 'w' : 'e';
                int src_idx = i != n->n ? i : i - 1;
                fprintf(fp, "\"node%lu\":f%d:s%c -> \"node%lu\":n;", (uintptr_t)n, src_idx, orientation, (uintptr_t)n->children[i].next);
            }
            else
            {
                printf("ERROR: child %d of node [%llu,%llu,..] is null\n", i, n->keys[0], n->keys[1]);
            }
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
    char fn[50];
    for (int i = 0; i < tests; i++)
    {
        bptree_insert(&tree, i, i);
        sprintf(fn, "trees/tree_%d.dot", i);
        bptree_plot(&tree, fn);
    }
    for (int i = 0; i < tests; i++)
    {
        value_t *v = bptree_get(&tree, i);
        if (v == NULL || *v != i)
        {
            printf("ERROR: %d != %llu\n", i, v != NULL ? *v : -1);
        }
    }
}