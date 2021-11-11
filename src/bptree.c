#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <immintrin.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#ifndef __USE_XOPEN2K
#include "spinlock.h"
#endif

#include "bptree.h"

rc_ptr_t *rc_nodes_create(int n)
{
    node_t *nodes = aligned_alloc(32, sizeof(node_t) * n + sizeof(rc_ptr_t));
    if (nodes == NULL)
    {
        perror("not enough memory\n");
        exit(-1);
    }
    rc_ptr_t *rc = (rc_ptr_t *)(nodes + n);
    rc->cnt = 0;
    rc->ptr.nodes = nodes;
    return rc;
}

rc_ptr_t *rc_values_create()
{
    value_t *values = malloc(sizeof(value_t[ORDER - 1]) + sizeof(rc_ptr_t));
    rc_ptr_t *rc = (rc_ptr_t *)(values + ORDER - 1);
    rc->cnt = 0;
    rc->ptr.values = values;
    return rc;
}
void node_init(node_t *n, bool is_leaf)
{
    n->n = 0;
    n->is_leaf = is_leaf;
    // set all keys to max value as default to avoid masking when compare with AVX
    for (int i = 0; i < ORDER - 1; i++)
        n->keys[i] = KEY_T_MAX;
    // point children to memory allocated for it after node struct
    if (n->is_leaf)
        n->children = rc_values_create();
    else
        n->children = rc_nodes_create(ORDER);

    if (n->children->ptr.values == NULL)
    {
        perror("not enough memory\n");
        exit(-1);
    }
    pthread_spin_init(&n->write_lock, 0);
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

value_t *node_get(node_t *n, key_t key, rc_ptr_t *rc)
{
    // convert to avx value if supported
    key_cmp_t cmp_key = avx_broadcast(key);
    while (true)
    {
        rc_ptr_inc(rc);
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            value_t *result = NULL;
            if (eq)
                result = &n->children->ptr.values[i];
            rc_ptr_dec(rc);
            // TODO return copy here, for memory safty (data could be freed)!
            return result;
        }
        else
        {
            rc_ptr_t *old_rc = rc;
            rc = n->children;
            if (eq)
                i++;
            n = &(n->children->ptr.nodes[i]);
            rc_ptr_dec(old_rc);
        }
    }
}

void node_split(node_t *n, uint16_t i, node_t *child)
{
    // Since this node is going to have a new child,
    // create space of new child
    memmove_sized(n->children->ptr.nodes + i + 2, n->children->ptr.nodes + i + 1, n->n - i);

    // we are storing the new right node in the children of the current node
    node_t *right = &(n->children->ptr.nodes[i + 1]);
    node_init(right, child->is_leaf);

    int min_deg = (ORDER + ORDER % 2) / 2; // divide by two and ceil
    // is we split child split value has to be reinserted into right node
    // k makes sure all new values in the node are moved one to the right
    int k = child->is_leaf ? 1 : 0;

    right->n = (ORDER - min_deg - 1) + k;
    // copy vales (leaf node) or child nodes
    if (child->is_leaf)
        memcpy_sized(right->children->ptr.values, child->children->ptr.values + min_deg - 1, right->n);
    else
        memcpy_sized(right->children->ptr.nodes, child->children->ptr.nodes + min_deg, right->n + 1);

    // move keys to new right node
    memcpy_sized(right->keys, child->keys + min_deg - k, right->n + k);

    // A key of y will move to this node. Find the location of
    // new key and move all greater keys one space ahead
    memmove_sized(n->keys + i + 1, n->keys + i, n->n - i);

    // Copy the middle key of y to this node
    n->keys[i] = child->keys[min_deg - 1];

    // Increment count of keys in this node
    n->n++;
}

/**
 * @brief clones a a node group by copying its values
 * all spin locks in the clone are unlocked
 * 
 * @param group poiter to group with ORDER nodes
 * @param n number of nodes to clone
 * @return node* pointer to clone
 */
rc_ptr_t *node_clone_group(node_t *group, int n)
{
    rc_ptr_t *rc_nodes = rc_nodes_create(n);
    memcpy_sized(rc_nodes->ptr.nodes, group, n);
    for (int i = 0; i < n; i++)
    {
        pthread_spin_init(&rc_nodes->ptr.nodes[i].write_lock, 0);
    }
    return rc_nodes;
}

/**
 * @brief Insets a value into a node without cloning the node (to allow for concurret get access)
 * this is used to insert into an already cloned node.
 * 
 * @param n node that is inserted to 
 * @param key key used for insertion
 * @param cmp_key key used for comparison. Is of type __m256i if AVX2 is enables, otherwise same as key.
 * @param value value assosiated with the key
 */
void node_insert_no_clone(node_t *n, key_t key, key_cmp_t cmp_key, value_t value, bptree *tree)
{
    uint16_t i = find_index(n->keys, n->n, cmp_key);
    bool eq = n->keys[i] == key;
    if (n->is_leaf)
    {
        if (eq)
        {
            memcpy(&n->children->ptr.values[i], value, sizeof(value_t));
        }
        else
        {
            // shift values to right an insert
            memmove_sized(n->keys + i + 1, n->keys + i, n->n - i);
            memmove_sized(n->children->ptr.values + i + 1, n->children->ptr.values + i, (n->n - i));
            n->keys[i] = key;
            memcpy(&n->children->ptr.values[i], value, sizeof(value_t));
            n->n++;
        }
        return;
    }
    else
    {
        if (eq)
            i++;
        if (n->children->ptr.nodes[i].n == ORDER - 1)
        {
            node_t *to_split = &(n->children->ptr.nodes[i]);
            node_split(n, i, to_split);

            // update number of elements in node that was just split
            int min_deg = (ORDER + ORDER % 2) / 2;
            to_split->n = min_deg - 1;

            if (n->keys[i] < key)
                i++;
        }
        node_insert(&(n->children->ptr.nodes[i]), key, cmp_key, value, &n->children, &n->write_lock, tree);
    }
}

void node_insert(node_t *n, key_t key, key_cmp_t cmp_key, value_t value, rc_ptr_t **node_group, pthread_spinlock_t *write_lock, bptree *tree)
{
    uint16_t i = find_index(n->keys, n->n, cmp_key);
    bool eq = n->keys[i] == key;
    if (n->is_leaf)
    {
        if (eq)
        {
            memcpy(&n->children->ptr.values[i], value, sizeof(value_t));
            pthread_spin_unlock(write_lock);
        }
        else
        {
            rc_ptr_t *clone;

            int n_block = 1;
            if (tree->root->ptr.nodes != n)
                n_block = ORDER;

            clone = node_clone_group((*node_group)->ptr.nodes, n_block);
            node_t *n_clone = clone->ptr.nodes + (n - (*node_group)->ptr.nodes);

            // shift values to right an insert
            memmove_sized(n_clone->keys + i + 1, n_clone->keys + i, n_clone->n - i);

            // clone values
            rc_ptr_t *value_clones = rc_values_create();
            memcpy_sized(value_clones->ptr.values, n_clone->children->ptr.values, i);

            rc_ptr_t *old_values = n_clone->children;
            n_clone->children = value_clones;

            memcpy_sized(n_clone->children->ptr.values + i + 1, old_values->ptr.values + i, (n_clone->n - i));
            n_clone->keys[i] = key;
            memcpy(&n_clone->children->ptr.values[i], value, sizeof(value_t));
            n_clone->n++;

            // change pointer to clone and free old one
            rc_ptr_t *old_group = *node_group;
            *node_group = clone;
            rc_ptr_free(old_group);
            rc_ptr_free(old_values);
            pthread_spin_unlock(write_lock);
        }
        return;
    }
    else
    {
        if (eq)
            i++;
        node_t *current = &n->children->ptr.nodes[i];
        if (current->n == ORDER - 1)
        {
            rc_ptr_t *n_group_clone = node_clone_group((*node_group)->ptr.nodes, ORDER);
            rc_ptr_t *c_group_clone = node_clone_group(n->children->ptr.nodes, ORDER);
            node_t *n_clone = n_group_clone->ptr.nodes + (n - (*node_group)->ptr.nodes);

            n_clone->children = c_group_clone;

            node_t *to_split = &(n_clone->children->ptr.nodes[i]);
            node_split(n_clone, i, to_split);

            // update number of elements in node that was just split
            int min_deg = (ORDER + ORDER % 2) / 2;
            to_split->n = min_deg - 1;

            if (n_clone->keys[i] < key)
                i++;

            node_insert_no_clone(&(c_group_clone->ptr.nodes[i]), key, cmp_key, value, tree);

            rc_ptr_t *old_group = *node_group;
            rc_ptr_t *old_children = n->children;
            *node_group = n_group_clone;
            n = n_clone;

            rc_ptr_free(old_group);
            rc_ptr_free(old_children);
            pthread_spin_unlock(write_lock);
            return;
        }
        pthread_spin_lock(&n->write_lock);

        node_insert(current, key, cmp_key, value, &n->children, &n->write_lock, tree);

        // done inserting in the child, release the lock
        pthread_spin_unlock(write_lock);
    }
}

void node_free(node_t *n)
{
    pthread_spin_destroy(&n->write_lock);
    for (int i = 0; i < n->n + 1; i++)
    {
        if (!n->children->ptr.nodes[i].is_leaf)
            node_free(&(n->children->ptr.nodes[i]));
    }
    rc_ptr_free(n->children);
}

void bptree_init(bptree *tree)
{
    tree->root = NULL;
    pthread_spin_init(&tree->write_lock, 0);
}

value_t *bptree_get(bptree *tree, key_t key)
{
    if (__builtin_expect(tree->root == NULL, 0))
        return NULL;
    else
    {
        return node_get(tree->root->ptr.nodes, key, tree->root);
    }
}

void bptree_insert(bptree *tree, key_t key, value_t value)
{
    pthread_spin_lock(&tree->write_lock);
    if (tree->root == NULL)
    {
        rc_ptr_t *root_rc = rc_nodes_create(1);
        node_t *root = root_rc->ptr.nodes;
        node_init(root, true);
        root->keys[0] = key;
        memcpy(&root->children->ptr.values[0], value, sizeof(value_t));
        root->n = 1;

        tree->root = root_rc;

        pthread_spin_unlock(&tree->write_lock);
        return;
    }
    else
    {
        key_cmp_t cmp_key = avx_broadcast(key);
        if (tree->root->ptr.nodes->n == ORDER - 1)
        {
            rc_ptr_t *s_rc = rc_nodes_create(1);
            node_t *s = s_rc->ptr.nodes;
            node_init(s, false);

            node_split(s, 0, tree->root->ptr.nodes);

            s->children->ptr.nodes[0] = *tree->root->ptr.nodes;
            pthread_spin_init(&s->children->ptr.nodes[0].write_lock, 0);

            // Reduce the number of keys in old root element
            int min_deg = (ORDER + ORDER % 2) / 2;
            s->children->ptr.nodes[0].n = min_deg - 1;

            int i = 0;
            if (s->keys[0] < key)
                i++;

            node_insert(&(s->children->ptr.nodes[i]), key, cmp_key, value, &s->children, &s->write_lock, tree);

            rc_ptr_t *old_root = tree->root;
            // Change root
            tree->root = s_rc;

            pthread_spin_unlock(&tree->write_lock);
            rc_ptr_free(old_root);
        }
        else
        {
            // IMPORTANT: as you (hello future simon) can maybe see, the unlock for the
            // bptree write lock is missing. The unlock happens within the node_insert method.
            node_insert(tree->root->ptr.nodes, key, cmp_key, value, &tree->root, &tree->write_lock, tree);
        }
    }
}

void bptree_free(bptree *tree)
{
    if (tree->root != NULL)
    {
        if (!tree->root->ptr.nodes->is_leaf)
            node_free(tree->root->ptr.nodes);
        rc_ptr_free(tree->root);
    }
}