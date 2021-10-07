#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <x86intrin.h>

#define memcpy_sized(dst, src, n) memcpy(dst, src, (n) * sizeof(*(dst)))
#define memmove_sized(dst, src, n) memmove(dst, src, (n) * sizeof(*(dst)))
#define memset_sized(dst, value, n) memset(dst, value, (n) * sizeof(*(dst)))

/**
 * @brief size of the keys in the binary tree in bits
 */
#define KEY_SIZE 32

// define macros for the AVX functions based on the KEY_SIZE

#if KEY_SIZE == 8
typedef int8_t key_t;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi8(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi8(a)

#elif KEY_SIZE == 16
typedef int16_t key_t;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi16(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi16(a)

#elif KEY_SIZE == 32
typedef int32_t key_t;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi32(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi32(a)

#elif KEY_SIZE == 64
typedef int64_t key_t;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi64(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi64x(a)

#else
#error KEY_SIZE has to be 8, 16, 32 or 64
#endif

#define ORDER 128

typedef u_int64_t value_t;

typedef union leaf
{
    value_t value;
    struct node *next;
} leaf;

typedef struct node
{
    key_t keys[ORDER - 1];
    leaf children[ORDER];
    /** number of keys in node **/
    uint16_t n;
    /** marks node as leaf **/
    bool is_leaf;
} node;

node *node_create(bool is_leaf)
{
    node *n = (node *)aligned_alloc(32, (sizeof(node) + 31) / 32 * 32);
    n->n = 0;
    n->is_leaf = is_leaf;
    return n;
}

#if __AVX2__
uint16_t find_index(key_t keys[ORDER - 1], int size, __m256i key)
{
    int idx = 0;
    int rv = 256 / KEY_SIZE;
    int num_iter = (size + rv - 1) / rv;
    __m256i b, cmp;
    // number of values that fit into the register
    int bb = KEY_SIZE / 8;

    unsigned int g_mask;

    // mask for last iteration (if register is only partially filled in last iteration)
    int offset = size - (num_iter - 1) * rv;
    uint32_t last_msk = (u_int32_t)(((u_int64_t)1 << ((offset * bb))) - 1);

    int j = 0;
    do
    {
        b = _mm256_load_si256((__m256i const *)(keys + j * rv));
        cmp = _mm256_cmpgt_epi(key, b);
        g_mask = _mm256_movemask_epi8(cmp);
        if (j == num_iter - 1)
            g_mask &= last_msk;
        if (g_mask != -1)
            break;
        j++;
    } while (j < num_iter);
    idx = __builtin_ffs(~g_mask) / bb;
    return idx + j * rv;
}
#else
uint16_t find_index(key_t keys[ORDER - 1], int size, key_t key)
{
    int i = 0;
    while (i < size && key > keys[i])
        i++;
    return i;
}
#endif

value_t *node_get(node *n, key_t key)
{
#ifdef __AVX2__
    __m256i cmp_key = _mm256_set1_epi(key);
#else
    key_t cmp_key = key;
#endif
    while (true)
    {
        uint16_t i = find_index(n->keys, n->n, cmp_key);
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
                n = n->children[i + 1].next;
            else
                n = n->children[i].next;
        }
    }
}

void node_insert(node *n, key_t key, value_t value);

void node_split(node *n, uint16_t i, node *child)
{
    node *right = node_create(child->is_leaf);
    int min_deg = ORDER / 2;
    // is we split child split value has to be reinserted into right node
    // k makes sure all new values in the node are moved one to the right
    int k = child->is_leaf ? 1 : 0;

    right->n = min_deg - 1 + k;
    if (k == 1)
    {
        right->keys[0] = child->keys[min_deg - 1];
        right->children[0].value = child->children[min_deg - 1].value;
    }

    for (int j = 0; j < right->n + 1; j++)
    {
        right->keys[j + k] = child->keys[j + min_deg];
        right->children[j + k] = child->children[j + min_deg];
    }

    // if non leaf node also copy last one
    if (!child->is_leaf)
    {
        right->children[min_deg - 1 + k] = child->children[min_deg + min_deg - 1];
    }

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
#ifdef __AVX2__
    __m256i cmp_key = _mm256_set1_epi(key);
#else
    key_t cmp_key = key;
#endif
    uint16_t i = find_index(n->keys, n->n, cmp_key);
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
            node_split(n, i, n->children[i].next);
            if (n->keys[i] < key)
                i++;
        }
        node_insert(n->children[i].next, key, value);
    }
}

void node_free(node *n)
{
    if (!n->is_leaf)
    {
        for (int i = 0; i < n->n + 1; i++)
        {
            node_free(n->children[i].next);
        }
    }
    free(n);
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
    if (__builtin_expect(tree->root == NULL, 0))
        return NULL;
    else
        return node_get(tree->root, key);
}
void bptree_insert(bptree *tree, key_t key, value_t value)
{
    if (__builtin_expect(tree->root == NULL, 0))
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
            node *s = node_create(false);
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

void bptree_free(bptree *tree)
{
    if (tree->root != NULL)
        node_free(tree->root);
}

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
    for (int i = 0; i < tests; i++)
    {
        key_t x = rand();
        bptree_insert(&tree, x, x);
    }
    srand(0);
    for (int i = 0; i < tests; i++)
    {
        key_t x = rand();
        value_t *v = bptree_get(&tree, x);
        if (v == NULL || *v != x)
        {
            printf("ERROR: %lld != %llu\n", x, v != NULL ? *v : -1);
        }
    }
    bptree_free(&tree);
}