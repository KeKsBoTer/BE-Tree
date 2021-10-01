#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <x86intrin.h>

#define memcpy_sized(dst, src, n) memcpy(dst, src, (n) * sizeof(*(dst)))
#define memmove_sized(dst, src, n) memmove(dst, src, (n) * sizeof(*(dst)))
#define memset_sized(dst, value, n) memset(dst, value, (n) * sizeof(*(dst)))

#define KEY_SIZE 32
#if KEY_SIZE == 16
typedef int16_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi16(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi16(a)
#else
typedef int32_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi32(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi32(a)
#endif

typedef int i_value;

typedef struct node
{
    partial_key *keys;
    i_value *values;
    struct node **children;
    uint8_t min_deg;  // Minimum degree (defines the range for number of keys)
    uint8_t num_keys; // Current number of keys
    bool leaf;
} node;

node *node_create(int min_deg, bool is_leaf)
{
    int node_size = sizeof(node);
    int keys_size = (2 * min_deg - 1) * sizeof(partial_key);
    int values_size = (2 * min_deg - 1) * sizeof(i_value);
    int children_size = (2 * min_deg) * sizeof(node **);
    int total_size = node_size + keys_size + values_size + children_size;

    // allocate all memory in one block and align with 32 bits to ensure
    // keys have 32 bits alignment for simd operations
    uint8_t *buffer = (uint8_t *)aligned_alloc(32, (total_size + 31) / 32 * 32);
    node *n = (node *)buffer;
    n->min_deg = min_deg;
    n->leaf = is_leaf;
    n->num_keys = 0;
    n->keys = (partial_key *)(buffer + node_size);
    n->values = (i_value *)(buffer + node_size + keys_size);
    n->children = (node **)(buffer + node_size + keys_size + values_size);
    return n;
}

void print_byte_as_bits(int val)
{
    printf("0b");
    for (int i = 16; 0 <= i; i--)
    {
        printf("%c", (val & (1 << i * 2)) ? '1' : '0');
    }
}

unsigned int find_index(partial_key *keys, int size, partial_key key)
{
#if __AVX2__

    int key_size = sizeof(key) * 8;

    int idx = 0;
    int num_iter = (size + key_size - 1) / key_size;
    __m256i a = _mm256_set1_epi(key);
    __m256i b, cmp;
    // number of bits per register in the result
    // see _mm256_movemask_epi8 documentation
    int bb = key_size / 8;
    // number of values that fit into the register
    int rv = 256 / key_size;

    for (int j = 0; j < num_iter; j++)
    {
        b = _mm256_load_si256((__m256i const *)(keys + j * rv));
        cmp = _mm256_cmpgt_epi(a, b);
        unsigned int g_mask = _mm256_movemask_epi8(cmp);
        int offset = (j + 1) * rv > size ? size - (j * rv) : rv;
        unsigned int bit_mask = ~((unsigned int)0) >> (32 - offset * bb);
        // get index of first larger key in current window
        int ab = _lzcnt_u32(g_mask & bit_mask);
        int x = rv - ab / bb;
        idx += x;
        if (x < rv)
            break;
    }
    return idx;
#else
    int i = 0;
    while (i < size && key > keys[i])
        i++;
    return i;
#endif
}

i_value *node_get(node *n, partial_key key)
{
    unsigned int i = find_index(n->keys, n->num_keys, key);

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
    node *z = node_create(y->min_deg, y->leaf);
    z->num_keys = n->min_deg - 1;

    // Copy the last (t-1) keys of y to z
    memcpy_sized(z->keys, y->keys + n->min_deg, n->min_deg - 1);
    memcpy_sized(z->values, y->values + n->min_deg, n->min_deg - 1);

    // Copy the last t children of y to z
    if (y->leaf == false)
    {
        memcpy_sized(z->children, y->children + n->min_deg, n->min_deg);
    }

    // Reduce the number of keys in y
    y->num_keys = n->min_deg - 1;

    // Since this node is going to have a new child,
    // create space of new child
    memmove_sized(n->children + 1, y->children, n->num_keys - i);

    // Link the new child to this node
    n->children[i + 1] = z;

    // A key of y will move to this node. Find the location of
    // new key and move all greater keys one space ahead
    memmove_sized(z->keys + 1, y->keys, n->num_keys - i);
    memmove_sized(z->values + 1, y->values, n->num_keys - i);

    // Copy the middle key of y to this node
    n->keys[i] = y->keys[n->min_deg - 1];
    n->values[i] = y->values[n->min_deg - 1];

    // Increment count of keys in this node
    n->num_keys++;
}

void node_insert_non_full(node *n, partial_key key, i_value value)
{
    // Initialize index as index of rightmost element
    int i = n->num_keys - 1;

    // If this is a leaf node
    if (n->leaf == true)
    {
        // The following does two things
        // a) Finds the location of new key to be inserted
        // b) Moves all greater keys to one place ahead
        i = find_index(n->keys, n->num_keys, key) - 1;
        memmove_sized(n->keys + i + 1, n->keys + i, n->num_keys - i);
        memmove_sized(n->values + i + 1, n->values + i, n->num_keys - i);

        // Insert the new key at found location
        n->keys[i + 1] = key;
        n->values[i + 1] = value;
        n->num_keys++;
    }
    else // If this node is not leaf
    {
        // Find the child which is going to have the new key
        i = find_index(n->keys, n->num_keys, key) - 1;

        // See if the found child is full
        if (n->children[i + 1]->num_keys == 2 * n->min_deg - 1)
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
    for (int i = 0; i < n->min_deg * 2; i++)
    {
        fprintf(fp, "<f%d>", i);
        if (i < n->num_keys)
        {
            fprintf(fp, "%d", n->keys[i]);
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

void node_free(node *n)
{
    if (!n->leaf)
    {
        for (int i = 0; i < n->num_keys + 1; i++)
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

void btree_init(btree *tree, int t)
{
    tree->root = NULL;
    tree->t = t;
}

void btree_insert(btree *tree, partial_key key, i_value value)
{
    // If tree is empty
    if (tree->root == NULL)
    {
        // Allocate memory for root
        tree->root = node_create(tree->t, true);

        tree->root->keys[0] = key;
        tree->root->values[0] = value;
        tree->root->num_keys = 1; // Update number of keys in root
    }
    else // If tree is not empty
    {
        // If root is full, then tree grows in height
        if (tree->root->num_keys == 2 * tree->t - 1)
        {
            // Allocate memory for new root
            node *s = node_create(tree->t, false);

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