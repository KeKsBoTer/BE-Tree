#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <immintrin.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "bptree.h"

node_t *node_create(bool is_leaf)
{
    node_t *n = aligned_alloc(32, sizeof(node_t));
    n->n = 0;
    n->is_leaf = is_leaf;
    for (int i = 0; i < ORDER - 1; i++)
        n->keys[i] = KEY_T_MAX;
    pthread_spin_init(&n->lock, 0);
    return n;
}

#if __AVX2__

static inline uint64_t cmp(__m256i x_vec, key_t *y_ptr)
{
    __m256i y_vec = _mm256_load_si256((__m256i *)y_ptr);
    __m256i mask = _mm256_cmpgt_epi(x_vec, y_vec);
    return _mm256_movemask((__m256)mask);
}

uint16_t find_index(key_t keys[ORDER - 1], int size, __m256i key)
{
    uint64_t mask = cmp(key, keys);
    // TODO check if conditional is faster / slower
    if (size > REG_VALUES)
        mask += cmp(key, &keys[REG_VALUES]) << REG_VALUES;
    int i = __builtin_ffs(~mask) - 1;
    return i;
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

value_t *node_get(node_t *n, key_t key)
{
    key_cmp_t cmp_key = avx_broadcast(key);
    while (true)
    {
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            value_t *result = NULL;
            if (eq)
                result = &n->children[i].value;
            pthread_spin_unlock(&n->lock);
            return result;
        }
        else
        {
            if (eq)
                i++;

            node_t *next = n->children[i].node;
            pthread_spin_lock(&next->lock);
            pthread_spin_unlock(&n->lock);
            n = next;
        }
    }
}

void node_split(node_t *n, uint16_t i, node_t *child)
{
    node_t *right = node_create(child->is_leaf);

    int min_deg = (ORDER + ORDER % 2) / 2;
    // is we split child split value has to be reinserted into right node
    // k makes sure all new values in the node are moved one to the right
    int k = child->is_leaf ? 1 : 0;

    right->n = (ORDER - min_deg - 1) + k;

    if (child->is_leaf)
        memcpy_sized(&right->children[0].value, &child->children[min_deg - 1].value, right->n);
    else
        memcpy_sized(&right->children[0].node, &child->children[min_deg].node, right->n + 1);

    memcpy_sized(right->keys, child->keys + min_deg - k, right->n + k);

    // Reduce the number of keys in y
    child->n = min_deg - 1;

    memmove_sized(&n->children[i + 2].node, &n->children[i + 1].node, n->n - i);
    // Link the new child to this node
    n->children[i + 1].node = right;

    // A key of y will move to this node. Find the location of
    // new key and move all greater keys one space ahead
    memmove_sized(n->keys + i + 1, n->keys + i, n->n - i);

    // Copy the middle key of y to this node
    n->keys[i] = child->keys[min_deg - 1];

    for (int j = child->n; j < ORDER - 1; j++)
        child->keys[j] = KEY_T_MAX;

    // Increment count of keys in this node
    n->n++;
}

void node_insert(node_t *n, key_t key, value_t value)
{
    key_cmp_t cmp_key = avx_broadcast(key);
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
            memmove_sized(n->keys + i + 1, n->keys + i, n->n - i);
            memmove_sized(&n->children[i + 1].value, &n->children[i].value, n->n - i);

            n->keys[i] = key;
            n->children[i].value = value;
            n->n++;
        }
        pthread_spin_unlock(&n->lock);
    }
    else
    {
        if (eq)
            i++;

        node_t *to_split = n->children[i].node;
        pthread_spin_lock(&to_split->lock);
        if (to_split->n == ORDER - 1)
        {
            node_split(n, i, to_split);
            if (n->keys[i] < key)
                i++;
        }
        pthread_spin_unlock(&to_split->lock);

        node_t *next = n->children[i].node;

        pthread_spin_lock(&next->lock);
        pthread_spin_unlock(&n->lock);

        node_insert(next, key, value);
    }
}

void node_free(node_t *n)
{
    if (!n->is_leaf)
    {
        for (int i = 0; i < n->n + 1; i++)
            node_free(n->children[i].node);
    }
    free(n);
}

void bptree_init(bptree_t *tree)
{
    tree->root = NULL;
    pthread_spin_init(&tree->lock, 0);
}

value_t *bptree_get(bptree_t *tree, key_t key)
{
    pthread_spin_lock(&tree->lock);
    value_t *result = NULL;
    if (tree->root != NULL)
    {
        pthread_spin_lock(&tree->root->lock);
        pthread_spin_unlock(&tree->lock);
        result = node_get(tree->root, key);
    }
    else
    {
        pthread_spin_unlock(&tree->lock);
    }
    return result;
}
void bptree_insert(bptree_t *tree, key_t key, value_t value)
{
    pthread_spin_lock(&tree->lock);
    if (tree->root == NULL)
    {
        tree->root = node_create(true);
        tree->root->keys[0] = key;
        tree->root->children[0].value = value;
        tree->root->n = 1;
        pthread_spin_unlock(&tree->lock);
    }
    else
    {
        if (tree->root->n == ORDER - 1)
        {
            node_t *s = node_create(false);
            s->children[0].node = tree->root;
            node_split(s, 0, tree->root);
            int i = 0;
            if (s->keys[0] < key)
                i++;
            node_t *next = s->children[i].node;

            pthread_spin_lock(&next->lock);
            node_insert(next, key, value);

            // Change root
            tree->root = s;
            pthread_spin_unlock(&tree->lock);
        }
        else
        {
            pthread_spin_lock(&tree->root->lock);
            pthread_spin_unlock(&tree->lock);
            node_insert(tree->root, key, value);
        }
    }
}

void bptree_free(bptree_t *tree)
{
    if (tree->root != NULL)
        node_free(tree->root);
}