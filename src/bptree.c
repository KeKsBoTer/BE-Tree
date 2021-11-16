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

node_t *node_clone(node_t *node)
{
    node_t *clone = aligned_alloc(32, sizeof(node_t));
    memcpy_sized(clone, node, 1);
    pthread_spin_init(&clone->lock, 0);
    return clone;
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

bool node_get(node_t *n, key_t key, value_t *result)
{
    key_cmp_t cmp_key = avx_broadcast(key);
    while (true)
    {
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            if (eq)
                *result = n->children.values[i];
            pthread_spin_unlock(&n->lock);
            return eq;
        }
        else
        {
            if (eq)
                i++;

            node_t *next = n->children.nodes[i];
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

    // is we split the child. its values have to be reinserted into right node
    // k makes sure all new values in the node are moved one to the right
    int k = child->is_leaf ? 1 : 0;

    right->n = (ORDER - min_deg - 1) + k;

    if (child->is_leaf)
        memcpy_sized(right->children.values, child->children.values + min_deg - 1, right->n);
    else
        memcpy_sized(right->children.nodes, child->children.nodes + min_deg, right->n + 1);

    memcpy_sized(right->keys, child->keys + min_deg - k, right->n + k);

    // Reduce the number of keys in y
    child->n = min_deg - 1;

    memmove_sized(n->children.nodes + i + 2, n->children.nodes + i + 1, n->n - i);
    // Link the new child to this node
    n->children.nodes[i + 1] = right;

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

node_t *node_insert(node_t *n, key_t key, value_t value)
{
    key_cmp_t cmp_key = avx_broadcast(key);
    uint16_t i = find_index(n->keys, n->n, cmp_key);
    bool eq = n->keys[i] == key;
    if (n->is_leaf)
    {
        node_t *n_clone = node_clone(n);
        if (eq)
        {
            n_clone->children.values[i] = value;
        }
        else
        {
            // shift values to right an insert
            memmove_sized(n_clone->keys + i + 1, n_clone->keys + i, n->n - i);
            memmove_sized(n_clone->children.values + i + 1, n_clone->children.values + i, n_clone->n - i);

            n_clone->keys[i] = key;
            n_clone->children.values[i] = value;
            n_clone->n++;
        }
        return n_clone;
    }
    else
    {
        node_t *n_clone = NULL;
        if (eq)
            i++;

        node_t *to_split = n->children.nodes[i];

        // lock node we are eventually going to split
        // to make sure all possible ongoing insert
        // operations one this node are done
        pthread_spin_lock(&to_split->lock);
        bool need_split = to_split->n == ORDER - 1;
        pthread_spin_unlock(&to_split->lock);

        if (need_split)
        {
            n_clone = node_clone(n);
            node_t *to_split_clone = node_clone(to_split);
            node_split(n_clone, i, to_split_clone);
            if (n_clone->keys[i] < key)
                i++;
            // TODO free to_split
        }

        node_t *next = n->children.nodes[i];
        pthread_spin_lock(&next->lock);

        if (n_clone == NULL)
            pthread_spin_unlock(&n->lock);

        node_t *new_next = node_insert(next, key, value);
        if (new_next != NULL)
        {
            node_t *old_next = __atomic_exchange_n(&n->children.nodes[i], new_next, __ATOMIC_RELAXED);
            pthread_spin_unlock(&next->lock);
            free(old_next);
        }
        return n_clone;
    }
}

void node_free(node_t *n)
{
    if (!n->is_leaf)
    {
        for (int i = 0; i < n->n + 1; i++)
            node_free(n->children.nodes[i]);
    }
    free(n);
}

void bptree_init(bptree_t *tree)
{
    tree->root = NULL;
    pthread_spin_init(&tree->lock, 0);
}

bool bptree_get(bptree_t *tree, key_t key, value_t *result)
{
    pthread_spin_lock(&tree->lock);
    bool found = false;
    if (tree->root != NULL)
    {
        pthread_spin_lock(&tree->root->lock);
        pthread_spin_unlock(&tree->lock);
        found = node_get(tree->root, key, result);
    }
    else
    {
        pthread_spin_unlock(&tree->lock);
    }
    return found;
}

void bptree_insert(bptree_t *tree, key_t key, value_t value)
{
    pthread_spin_lock(&tree->lock);
    if (tree->root == NULL)
    {
        node_t *root = node_create(true);
        root->keys[0] = key;
        root->children.values[0] = value;
        root->n = 1;
        __atomic_store_n(&tree->root, root, __ATOMIC_RELAXED);
        pthread_spin_unlock(&tree->lock);
    }
    else
    {
        pthread_spin_lock(&tree->root->lock);
        if (tree->root->n == ORDER - 1)
        {
            node_t *s = node_create(false);
            s->children.nodes[0] = node_clone(tree->root);

            node_split(s, 0, s->children.nodes[0]);
            int i = 0;
            if (s->keys[0] < key)
                i++;
            node_t *next = s->children.nodes[i];

            node_t *new_next = node_insert(next, key, value);
            if (new_next != NULL)
                next = new_next;

            // Change root
            node_t *old_root = __atomic_exchange_n(&tree->root, s, __ATOMIC_RELAXED);
            free(old_root);
            pthread_spin_unlock(&tree->lock);
        }
        else
        {
            pthread_spin_unlock(&tree->lock);
            node_t *new_root = node_insert(tree->root, key, value);
            if (new_root != NULL)
            {
                node_t *old_root = __atomic_exchange_n(&tree->root, new_root, __ATOMIC_RELAXED);
                pthread_spin_unlock(&tree->root->lock);
                free(old_root);
            }
        }
    }
}

void bptree_free(bptree_t *tree)
{
    if (tree->root != NULL)
        node_free(tree->root);
}