#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <immintrin.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include "bptree.h"
#include "spinlock.h"

#define atomic_add(a, b) __atomic_fetch_add(a, b, __ATOMIC_RELAXED)
#define atomic_sub(a, b) __atomic_fetch_sub(a, b, __ATOMIC_RELAXED)
#define atomic_exchange(a, b) __atomic_exchange_n(a, b, __ATOMIC_RELAXED)

node_t *node_create(bool is_leaf)
{
    node_t *n = aligned_alloc(32, sizeof(node_t));
    n->n = 0;
    n->is_leaf = is_leaf;
    for (int i = 0; i < ORDER - 1; i++)
        n->keys[i] = KEY_T_MAX;
    n->rc_cnt = 0;
    return n;
}

node_t *node_clone(node_t *node)
{
    node_t *clone = aligned_alloc(32, sizeof(node_t));
    memcpy_sized(clone, node, 1);
    clone->rc_cnt = 0;
    return clone;
}

static inline node_t *node_access(node_t **node, uint64_t *inc_ops)
{

#ifdef BPTREE_SECURE_NODE_ACCESS
    atomic_add(inc_ops, 1);
#endif

    node_t *n = *node;
    atomic_add(&n->rc_cnt, 1);

#ifdef BPTREE_SECURE_NODE_ACCESS
    atomic_sub(inc_ops, 1);
#endif

    return n;
}

static inline void exit_node(node_t *node)
{
    atomic_sub(&node->rc_cnt, 1);
}

static inline uint64_t cmp(__m256i x_vec, key_t *y_ptr)
{
    __m256i y_vec = _mm256_load_si256((__m256i *)y_ptr);
    __m256i mask = _mm256_cmpgt_epi(x_vec, y_vec);
    return _mm256_movemask((__m256)mask);
}

uint16_t find_index_avx2(key_t keys[ORDER - 1], int size, __m256i key)
{
    uint64_t mask = cmp(key, keys);
    // TODO check if conditional is faster / slower
    if (size > REG_VALUES)
        mask += cmp(key, &keys[REG_VALUES]) << REG_VALUES;
    int i = __builtin_ffs(~mask) - 1;
    return i;
}

uint16_t find_index(key_t keys[ORDER - 1], int size, key_t key)
{
    int i = 0;
    while (i < size && key > keys[i])
        i++;
    return i;
}

bool node_get(node_t *n, key_t key, value_t *result, uint64_t *inc_ops, bool use_avx2)
{
    __m256i cmp_key;
    if (use_avx2)
        cmp_key = avx_broadcast(key);

    while (true)
    {
        uint16_t i;
        if (use_avx2)
            i = find_index_avx2(n->keys, n->n, cmp_key);
        else
            i = find_index(n->keys, n->n, key);

        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            if (eq)
                *result = n->children.values[i];
            exit_node(n);
            return eq;
        }
        else
        {
            if (eq)
                i++;

            node_t *old = n;
            n = node_access(&n->children.nodes[i], inc_ops);
            exit_node(old);
        }
    }
}

void delayed_free(node_t *node, uint64_t *inc_ops)
{
    do
    {
#ifdef BPTREE_SECURE_NODE_ACCESS
        // wait until all reference counter operations are done
        while (*inc_ops > 0)
            ;
#endif
        // wait until reference counter of node is done to zero
    } while (node->rc_cnt > 0);
    free(node);
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

node_t *node_insert(node_t *n, key_t key, value_t value, node_t **free_after, uint64_t *inc_ops, bool use_avx2)
{
    uint16_t i;
    if (use_avx2)
        i = find_index_avx2(n->keys, n->n, avx_broadcast(key));
    else
        i = find_index(n->keys, n->n, key);

    bool eq = n->keys[i] == key;
    if (n->is_leaf)
    {
        if (eq)
        {
            n->children.values[i] = value;
            return NULL;
        }
        else
        {
            node_t *n_clone = node_clone(n);
            // shift values to right an insert
            memmove_sized(n_clone->keys + i + 1, n_clone->keys + i, n_clone->n - i);
            memmove_sized(n_clone->children.values + i + 1, n_clone->children.values + i, n_clone->n - i);

            n_clone->keys[i] = key;
            n_clone->children.values[i] = value;
            n_clone->n++;
            return n_clone;
        }
    }
    else
    {
        if (eq)
            i++;

        node_t *to_split = n->children.nodes[i];

        // lock node we are eventually going to split
        // to make sure all possible ongoing insert
        // operations one this node are done

        if (to_split->n == ORDER - 1)
        {
            node_t *n_clone = node_clone(n);
            node_t *to_split_clone = node_clone(to_split);
            n_clone->children.nodes[i] = to_split_clone;

            node_split(n_clone, i, to_split_clone);

            if (n_clone->keys[i] < key)
                i++;
            node_t *next = n_clone->children.nodes[i];

            if (next != to_split)
                *free_after = to_split;

            node_t *free_after_2 = NULL;
            node_t *new_next = node_insert(next, key, value, &free_after_2, inc_ops, use_avx2);
            if (new_next != NULL)
            {
                node_t *old_next = atomic_exchange(&n_clone->children.nodes[i], new_next);
                delayed_free(old_next, inc_ops);
                if (free_after_2 != NULL)
                    delayed_free(free_after_2, inc_ops);
            }
            return n_clone;
        }
        else
        {

            node_t *next = n->children.nodes[i];

            node_t *free_after_2 = NULL;
            node_t *new_next = node_insert(next, key, value, &free_after_2, inc_ops, use_avx2);
            if (new_next != NULL)
            {
                node_t *old_next = atomic_exchange(&n->children.nodes[i], new_next);
                delayed_free(old_next, inc_ops);
                if (free_after_2 != NULL)
                    delayed_free(free_after_2, inc_ops);
            }
            return NULL;
        }
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

void bptree_init(bptree_t *tree, bool use_avx2)
{
    tree->root = NULL;
    pthread_spin_init(&tree->lock, 0);
    tree->inc_ops = 0;
    tree->use_avx2 = use_avx2;
}

bool bptree_get(bptree_t *tree, key_t key, value_t *result)
{
    bool found = false;
    if (tree->root != NULL)
    {
        node_t *root = node_access(&tree->root, &tree->inc_ops);
        found = node_get(root, key, result, &tree->inc_ops, tree->use_avx2);
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
    }
    else
    {
        if (tree->root->n == ORDER - 1)
        {
            node_t *s = node_create(false);
            s->children.nodes[0] = node_clone(tree->root);

            node_split(s, 0, s->children.nodes[0]);
            int i = 0;
            if (s->keys[0] < key)
                i++;
            node_t *next = s->children.nodes[i];

            node_t *free_after = NULL;
            node_t *new_next = node_insert(next, key, value, &free_after, &tree->inc_ops, tree->use_avx2);
            if (new_next != NULL)
            {
                node_t *old_next = atomic_exchange(&s->children.nodes[i], new_next);
                delayed_free(old_next, &tree->inc_ops);
                if (free_after != NULL)
                    delayed_free(free_after, &tree->inc_ops);
            }

            // Change root
            node_t *old_root = atomic_exchange(&tree->root, s);
            delayed_free(old_root, &tree->inc_ops);
        }
        else
        {
            node_t *free_after = NULL;
            node_t *new_root = node_insert(tree->root, key, value, &free_after, &tree->inc_ops, tree->use_avx2);
            if (new_root != NULL)
            {
                node_t *old_root = atomic_exchange(&tree->root, new_root);
                delayed_free(old_root, &tree->inc_ops);
                if (free_after != NULL)
                    delayed_free(free_after, &tree->inc_ops);
            }
        }
    }
    pthread_spin_unlock(&tree->lock);
}

void bptree_free(bptree_t *tree)
{
    if (tree->root != NULL)
        node_free(tree->root);
}