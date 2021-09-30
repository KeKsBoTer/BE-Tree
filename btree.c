#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define ORDER 5

typedef uint16_t partial_key;
typedef int i_value;

typedef struct node
{
    partial_key *keys;
    i_value *values;
    int t; // Minimum degree (defines the range for number of keys)
    struct node **children;
    int n; // Current number of keys
    bool leaf;
} node;

void node_init(node *n, int t, bool is_leafe)
{
    n->t = t;
    n->leaf = is_leafe;
    n->keys = (partial_key *)malloc((2 * t - 1) * sizeof(partial_key));
    n->values = (i_value *)malloc((2 * t - 1) * sizeof(i_value));
    n->children = (node **)malloc((2 * t) * sizeof(node **));
    n->n = 0;
}

void node_traverse(node *n)
{
    int i;
    for (i = 0; i < n->n; i++)
    {
        // If this is not leaf, then before printing key[i],
        // traverse the subtree rooted with child C[i].
        if (n->leaf == false)
            node_traverse(n->children[i]);
        printf("%d ", n->keys[i]);
    }

    // Print the subtree rooted with last child
    if (n->leaf == false)
        node_traverse(n->children[i]);
}

i_value *node_get(node *n, partial_key key)
{
    int i = 0;
    while (i < n->n && key > n->keys[i])
        i++;

    // If the found key is equal to k, return this node
    if (n->keys[i] == key)
        return &n->values[i];

    // If key is not found here and this is a leaf node
    if (n->leaf == true)
        return NULL;

    // Go to the appropriate child
    return node_get(n->children[i], key);
}

void node_split_child(node *n, int i, node *y)
{
    // Create a new node which is going to store (t-1) keys
    // of y
    node *z = (node *)malloc(sizeof(node));
    node_init(z, y->t, y->leaf);
    z->n = n->t - 1;

    // Copy the last (t-1) keys of y to z
    for (int j = 0; j < n->t - 1; j++)
    {
        z->keys[j] = y->keys[j + n->t];
        z->values[j] = y->values[j + n->t];
    }

    // Copy the last t children of y to z
    if (y->leaf == false)
    {
        for (int j = 0; j < n->t; j++)
            z->children[j] = y->children[j + n->t];
    }

    // Reduce the number of keys in y
    y->n = n->t - 1;

    // Since this node is going to have a new child,
    // create space of new child
    for (int j = n->n; j >= i + 1; j--)
        n->children[j + 1] = n->children[j];

    // Link the new child to this node
    n->children[i + 1] = z;

    // A key of y will move to this node. Find the location of
    // new key and move all greater keys one space ahead
    for (int j = n->n - 1; j >= i; j--)
    {
        n->keys[j + 1] = n->keys[j];
        n->values[j + 1] = n->values[j];
    }

    // Copy the middle key of y to this node
    n->keys[i] = y->keys[n->t - 1];
    n->values[i] = y->values[n->t - 1];

    // Increment count of keys in this node
    n->n++;
}

void node_insert_non_full(node *n, partial_key key, i_value value)
{
    // Initialize index as index of rightmost element
    int i = n->n - 1;

    // If this is a leaf node
    if (n->leaf == true)
    {
        // The following loop does two things
        // a) Finds the location of new key to be inserted
        // b) Moves all greater keys to one place ahead
        while (i >= 0 && n->keys[i] > key)
        {
            n->keys[i + 1] = n->keys[i];
            n->values[i + 1] = n->values[i];
            i--;
        }

        // Insert the new key at found location
        n->keys[i + 1] = key;
        n->values[i + 1] = value;
        n->n++;
    }
    else // If this node is not leaf
    {
        // Find the child which is going to have the new key
        while (i >= 0 && n->keys[i] > key)
            i--;

        // See if the found child is full
        if (n->children[i + 1]->n == 2 * n->t - 1)
        {
            // If the child is full, then split it
            node_split_child(n, i + 1, n->children[i + 1]);

            // After split, the middle key of C[i] goes up and
            // C[i] is splitted into two.  See which of the two
            // is going to have the new key
            if (n->keys[i + 1] < key)
                i++;
        }
        node_insert_non_full(n->children[i + 1], key, value);
    }
}

void node_dot(node *n, FILE *fp)
{
    fprintf(fp, "\"node%d\" [label = \"", (int)n);
    for (int i = 0; i < n->t * 2; i++)
    {
        fprintf(fp, "<f%d>", i);
        if (i < n->n)
        {
            fprintf(fp, "%d", n->keys[i]);
        }
        if (i != n->t * 2 - 1)
        {
            fprintf(fp, " | ");
        }
    }
    fprintf(fp, "\" shape = \"record\"];\n");
    if (!n->leaf)
        for (int i = 0; i < n->n + 1; i++)
        {
            node_dot(n->children[i], fp);
            char orientation = i != n->n ? 'w' : 'e';
            int src_idx = i != n->n ? i : i - 1;
            fprintf(fp, "\"node%d\":f%d:s%c -> \"node%d\":n;", (int)n, src_idx, orientation, (int)n->children[i]);
        }
}

void node_free(node *n)
{
    if (!n->leaf)
    {
        for (int i = 0; i < n->n + 1; i++)
        {
            node_free(n->children[i]);
        }
    }
    free(n);
}

typedef struct btree
{
    node *root;
    int t;
} btree;

void btree_init(btree *tree, int order)
{
    tree->root = NULL;
    tree->t = order;
}
void btree_traverse(btree *tree)
{
    if (tree->root != NULL)
        node_traverse(tree->root);
}

void btree_insert(btree *tree, partial_key key, i_value value)
{
    // If tree is empty
    if (tree->root == NULL)
    {
        // Allocate memory for root
        tree->root = (node *)malloc(sizeof(node));
        node_init(tree->root, tree->t, true);
        tree->root->keys[0] = key;
        tree->root->values[0] = value;
        tree->root->n = 1; // Update number of keys in root
    }
    else // If tree is not empty
    {
        // If root is full, then tree grows in height
        if (tree->root->n == 2 * tree->t - 1)
        {
            // Allocate memory for new root
            node *s = (node *)malloc(sizeof(node));
            node_init(s, tree->t, false);

            // Make old root as child of new root
            s->children[0] = tree->root;

            // Split the old root and move 1 key to the new root
            node_split_child(s, 0, tree->root);

            // New root has two children now.  Decide which of the
            // two children is going to have new key
            int i = 0;
            if (s->keys[0] < key)
                i++;
            node_insert_non_full(s->children[i], key, value);

            // Change root
            tree->root = s;
        }
        else // If root is not full, call insertNonFull for root
            node_insert_non_full(tree->root, key, value);
    }
}

i_value *btree_get(btree *tree, partial_key key)
{
    if (tree->root != NULL)
    {
        return node_get(tree->root, key);
    }
    else
    {
        return NULL;
    }
}
void btree_free(btree *tree)
{
    if (tree->root != NULL)
        node_free(tree->root);
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
    btree_init(&tree, ORDER);

    printf("inserting ");
    for (int i = 0; i < test_values; i += 2)
    {
        printf("%d, ", i);
        btree_insert(&tree, i, i * 2);
    }
    for (int i = 1; i < test_values; i += 2)
    {
        printf("%d, ", i);
        btree_insert(&tree, i, i * 2);
    }
    btree_plot(&tree, "trees/final.dot");
    printf("\n");

    i_value *v;
    for (int i = 0; i < test_values; i++)
    {
        v = btree_get(&tree, i);
        if (v == NULL || *v != i * 2)
            printf("ERROR: %d: %d\n", i, v == NULL ? -1 : *v);
    }
    btree_free(&tree);
    return 0;
}