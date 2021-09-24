#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define ORDER 5

// macros for memcpy and memset for non byte values
#define memcpy_sized(dst, src, n) memcpy(dst, src, (n) * sizeof(*(dst)))
#define memmove_sized(dst, src, n) memmove(dst, src, (n) * sizeof(*(dst)))
#define memset_sized(dst, value, n) memset(dst, value, (n) * sizeof(*(dst)))

typedef uint16_t partial_key;
typedef int i_value;

typedef struct node
{
    partial_key keys[ORDER - 1];
    // number of key-value pairs in the node
    int num_keys;
    // flag to mark leafe nodes
    bool is_leafe;
    struct node *children[ORDER];

    i_value values[ORDER - 1];
    struct node *parent;
} node;

void init_node(node *n, node *parent)
{
    n->is_leafe = true;
    n->parent = parent;
    // TODO only for debug
    memset_sized(n->keys, 0, ORDER - 1);
    memset_sized(n->values, 0, ORDER - 1);
    memset_sized(n->children, 0, ORDER);
}

int find_index(node *n, partial_key key)
{
    for (int i = 0; i < n->num_keys; i++)
    {
        if (n->keys[i] >= key)
        {
            return i;
        }
    }
    return n->num_keys;
}

i_value *node_get(node *n, partial_key key)
{
    int idx = find_index(n, key);
    if (n->keys[idx] == key)
        return &n->values[idx];

    if (n->children[idx] != NULL)
        return node_get(n->children[idx], key);
    else
        return NULL;
}

typedef struct split
{
    partial_key key;
    i_value value;
    node *left;
    node *right;
} split;

// inserts a key value pair into a key and value buffer at a given position
// shifts values from the index to the right
void node_insert_pair(partial_key *keys, i_value *values, int size, int idx, partial_key key, i_value value)
{
    // move keys and values to the right
    memmove_sized(keys + idx + 1, keys + idx, size - idx);
    memmove_sized(values + idx + 1, values + idx, size - idx);
    keys[idx] = key;
    values[idx] = value;
}

void node_split(node *n, int idx, split s, node **root);

void node_promote(node *n, split s, node **root)
{
    if (n == NULL)
    {
        n = (node *)malloc(sizeof(node));
        init_node(n, NULL);
        n->is_leafe = false;
        n->keys[0] = s.key;
        n->values[0] = s.value;
        n->num_keys = 1;
        n->children[0] = s.left;
        n->children[1] = s.right;
        s.left->parent = n;
        s.right->parent = n;
        *root = n;
        return;
    }
    if (n->num_keys == ORDER - 1)
    {
        int idx = find_index(n, s.key);
        node_split(n, idx, s, root);
    }
    else
    {
        int idx = find_index(n, s.key);

        node_insert_pair(n->keys, n->values, n->num_keys, idx, s.key, s.value);
        s.left->parent = n;
        s.right->parent = n;
        // shoft next pointers to the right
        memmove(n->children + idx + 2, n->children + idx + 1, (n->num_keys - idx) * sizeof(node *));
        n->children[idx] = s.left;
        n->children[idx + 1] = s.right;
        n->num_keys++;
        n->is_leafe = false;
    }
}

// inserts the value into the node and then performs a split
// the median of the new node is then promoted to its parent node
// with the two nodes - created from the split - as children.
// pointer to root is required if n == root and root needs to be changed after promotion.
void node_split(node *n, int idx, split s, node **root)
{
    node *parent = n->parent;
    // use current node as left one
    node *right = (node *)malloc(sizeof(node));
    init_node(right, NULL);

    // split node into two nodes
    // create buffers for moving stuff around
    // TODO do it without the buffers (they are not needed, just make it simpler for now)
    partial_key key_buffer[ORDER];
    i_value value_buffer[ORDER];
    memcpy_sized(key_buffer, n->keys, ORDER - 1);
    memcpy_sized(value_buffer, n->values, ORDER - 1);
    node_insert_pair((partial_key *)key_buffer, (i_value *)value_buffer, ORDER - 1, idx, s.key, s.value);

    node *left = n;
    // copy keys and values into new nodes
    int median_i = ORDER / 2;
    memcpy_sized(left->keys, key_buffer, median_i);
    memcpy_sized(left->values, value_buffer, median_i);
    memcpy_sized(right->keys, key_buffer + median_i + 1, ORDER - median_i - 1);
    memcpy_sized(right->values, value_buffer + median_i + 1, ORDER - median_i - 1);
    n->num_keys = median_i;
    right->num_keys = ORDER - median_i - 1;

    memcpy_sized(right->children, n->children + median_i + 1, ORDER - 1 - median_i);

    if (s.left != NULL)
    {
        if (idx < median_i)
        {
            // was inserted into left node
            left->children[idx] = s.left;
            left->children[idx + 1] = s.right;
            right->is_leafe = false;
        }
        else if (idx > median_i)
        {
            // was inserted into right node
            right->children[ORDER - idx] = s.left;
            right->children[ORDER - idx + 1] = s.right;
            right->is_leafe = false;
        }
        else
        {
            // s.key is beeing promoted again
            printf("not implemented :(\n");
            exit(1);
        }
    }

    split s_new = {key_buffer[median_i], value_buffer[median_i], left, right};
    node_promote(parent, s_new, root);
}

void node_put(node *n, partial_key key, i_value value, node **root)
{
    int idx = find_index(n, key);
    if (n->keys[idx] == key)
    {
        n->values[idx] = value;
        return;
    }
    if (n->is_leafe)
    {
        // if node is sfull rearange values
        if (n->num_keys == ORDER - 1)
        {
            split s = {key, value, NULL, NULL};
            node_split(n, idx, s, root);
        }
        // if node has empty spots, insert key
        else
        {
            node_insert_pair(n->keys, n->values, n->num_keys, idx, key, value);
            n->num_keys++;
        }
    }
    else
    {
        // not a leafe, insert into children
        node_put(n->children[idx], key, value, root);
    }
}

void node_dot(node *n, FILE *fp)
{
    fprintf(fp, "\"node%d\" [label = \"", (int)n);
    for (int i = 0; i < ORDER - 1; i++)
    {
        fprintf(fp, "<f%d>", i);
        if (i < n->num_keys)
        {
            fprintf(fp, "%d", n->keys[i]);
        }
        if (i != ORDER - 2)
        {
            fprintf(fp, " | ");
        }
    }
    fprintf(fp, "\" shape = \"record\"];\n");
    if (!n->is_leafe)
        for (int i = 0; i < n->num_keys + 1; i++)
        {
            node_dot(n->children[i], fp);
            char orientation = i != n->num_keys ? 'w' : 'e';
            int src_idx = i != n->num_keys ? i : i - 1;
            fprintf(fp, "\"node%d\":f%d:s%c -> \"node%d\":n;", (int)n, src_idx, orientation, (int)n->children[i]);
        }
}

void free_node(node *n)
{
    if (!n->is_leafe)
    {
        for (int i = 0; i < n->num_keys + 1; i++)
        {
            free_node(n->children[i]);
        }
    }
    free(n);
}

typedef struct btree
{
    node *root;
} btree;

void init_btree(btree *tree)
{
    tree->root = NULL;
}

// searches for the given key in the tree
// pointer to value is returns
// NULL is returned if value does not exist
i_value *tree_get(btree *tree, partial_key key)
{
    if (tree->root == NULL)
    {
        return NULL;
    }
    return node_get(tree->root, key);
}

// inserts value for given key into the tree
// if key already exists within the tree the value is overridden
void tree_put(btree *tree, partial_key key, i_value value)
{
    if (tree->root == NULL)
    {
        tree->root = (node *)malloc(sizeof(node));
        init_node(tree->root, NULL);

        tree->root->keys[0] = key;
        tree->root->values[0] = value;
        tree->root->num_keys = 1;
    }
    else
    {
        node_put(tree->root, key, value, &tree->root);
    }
}

// frees nodes in tree
// IMPORTANT: does not free the tree struct itself (it could live on the function stack)
void free_tree(btree *tree)
{
    if (tree->root != NULL)
    {
        free_node(tree->root);
    }
}

void tree_dot(btree *tree, FILE *fp)
{
    fprintf(fp, "digraph g {\ngraph [ rankdir = \"TP\"];\n");
    if (tree->root != NULL)
        node_dot(tree->root, fp);
    fprintf(fp, "\n}");
}

void plot_tree(btree *tree, const char *filename)
{
    // print tree
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        printf("file can't be opened\n");
        exit(1);
    }
    tree_dot(tree, fp);
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
    init_btree(&tree);

    printf("inserting ");
    for (int i = 0; i < test_values; i += 2)
    {
        printf("%d, ", i);
        tree_put(&tree, i, i * 2);
    }
    for (int i = 1; i < test_values; i += 2)
    {
        printf("%d, ", i);
        plot_tree(&tree, "before.dot");
        tree_put(&tree, i, i * 2);
        plot_tree(&tree, "after.dot");
    }
    printf("\n");

    i_value *v;
    for (int i = 0; i < test_values; i++)
    {
        v = tree_get(&tree, i);
        if (v == NULL || *v != i * 2)
            printf("ERROR: %d: %d\n", i, v == NULL ? -1 : *v);
    }
    free_tree(&tree);
    return 0;
}