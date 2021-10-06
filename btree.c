/** implementation based on https://www.geeksforgeeks.org/insert-operation-in-b-tree/ **/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <x86intrin.h>

#include "btree.h"
#include "MurmurHash3/MurmurHash3.h"
#include "bitmap.h"

#define HASH_KEY

#define memcpy_sized(dst, src, n) memcpy(dst, src, (n) * sizeof(*(dst)))
#define memmove_sized(dst, src, n) memmove(dst, src, (n) * sizeof(*(dst)))
#define memset_sized(dst, value, n) memset(dst, value, (n) * sizeof(*(dst)))

node *node_create(int min_deg, bool is_leaf)
{
    int node_size = sizeof(node);
    // rounding up to multiple of 32 bits to ensure 32-bit alignment for keys
    node_size = (node_size + 31) / 32 * 32;
    int order = 2 * min_deg; // order of the tree
    int keys_size = (order - 1) * sizeof(partial_key);
    int values_size = (order - 1) * sizeof(node_value);
    int children_size = order * sizeof(node **);
    int total_size = node_size + keys_size + values_size + children_size;

    // allocate all memory in one block and align with 32 bits to ensure
    // keys have 32 bits alignment for simd operations
    uint8_t *buffer = (uint8_t *)aligned_alloc(32, (total_size + 31) / 32 * 32);
    node *n = (node *)buffer;
    n->min_deg = min_deg;
    n->leaf = is_leaf;
    n->num_keys = 0;
    n->keys = (partial_key *)(buffer + node_size);
    n->values = (node_value *)(buffer + node_size + keys_size);
    n->children = (node **)(buffer + node_size + keys_size + values_size);
    n->tree_end = 0;
    return n;
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

i_value *node_get(node *n, partial_key *keys, int num_keys)
{
    partial_key key = keys[0];
    while (true)
    {
        unsigned int i = find_index(n->keys, n->num_keys, key);

        // If the found key is equal to k, return this node
        if (n->keys[i] == key && i < n->num_keys)
        {
            if (get_bit(&n->tree_end, i) == 0)
            {
                return &n->values[i].pair.value;
            }
            else
            {
                return node_get(n->values[i].next->root, keys + 1, num_keys - 1);
            }
        }

        // If key is not found here and this is a leaf node
        if (n->leaf == true)
            return NULL;

        // Go to the appropriate child
        n = n->children[i];
    }
}

void node_split_child(node *n, int i, node *y)
{
    // Create a new node which is going to store (t-1) keys
    // of y
    node *right = node_create(y->min_deg, y->leaf);
    right->num_keys = n->min_deg - 1;

    // Copy the last (t-1) keys of y to z
    memcpy_sized(right->keys, y->keys + n->min_deg, n->min_deg - 1);
    memcpy_sized(right->values, y->values + n->min_deg, n->min_deg - 1);
    right->tree_end = y->tree_end;
    shift_right(&right->tree_end, n->min_deg);

    // Copy the last t children of y to z
    if (y->leaf == false)
    {
        memcpy_sized(right->children, y->children + n->min_deg, n->min_deg);
    }

    // Reduce the number of keys in y

    for (int i = n->min_deg; i < y->num_keys; i++)
    {
        if (get_bit(&y->tree_end, i) == 1)
            clear_bit(&y->tree_end, i);
    }
    y->num_keys = n->min_deg - 1;

    // Since this node is going to have a new child,
    // create space of new child
    memmove_sized(n->children + i + 2, n->children + i + 1, n->num_keys - i);

    // Link the new child to this node
    n->children[i + 1] = right;

    // A key of y will move to this node. Find the location of
    // new key and move all greater keys one space ahead
    memmove_sized(n->keys + 1 + i, n->keys + i, n->num_keys - i);
    memmove_sized(n->values + 1 + i, n->values + i, n->num_keys - i);
    shift_one_left(&n->tree_end, i);

    // Copy the middle key of y to this node
    n->keys[i] = y->keys[n->min_deg - 1];
    n->values[i] = y->values[n->min_deg - 1];
    if (get_bit(&y->tree_end, n->min_deg - 1) == 0)
    {
        clear_bit(&n->tree_end, i);
    }
    else
    {
        set_bit(&n->tree_end, i);
    }

    // Increment count of keys in this node
    n->num_keys++;
}

void node_insert_non_full(node *n, partial_key *keys, int num_keys, i_value value, btree_key_hash full_key)
{
    partial_key key = keys[0];
    while (true)
    {
        // Initialize index as index of rightmost element
        int i = find_index(n->keys, n->num_keys, key);

        if (n->keys[i] == key && i < n->num_keys)
        {
            if (get_bit(&n->tree_end, i) == 0)
            {
                if (num_keys == 1)
                {
                    // reached end of subtrees
                    n->values[i].pair.value = value;
                }
                else
                {
                    // TODO this can be done more efficient
                    btree *subtree = (btree *)malloc(sizeof(btree));
                    btree_init(subtree, n->min_deg);
                    // insert current value into new subtree
                    partial_key *p_keys = (partial_key *)&n->values[i].pair.key;
                    int full_key_size = sizeof(btree_key_hash) / sizeof(partial_key);
                    btree_insert_partial(subtree, p_keys + (full_key_size - (num_keys - 1)), num_keys - 1, n->values[i].pair.value, n->values[i].pair.key);

                    btree_insert_partial(subtree, keys + 1, num_keys - 1, value, full_key);
                    n->values[i].next = subtree;
                    set_bit(&n->tree_end, i);
                }
            }
            else
            {
                btree_insert_partial(n->values[i].next, keys + 1, num_keys - 1, value, full_key);
            }
            return;
        }
        // If this is a leaf node
        if (n->leaf == true)
        {
            // Moves all greater keys to one place ahead

            memmove_sized(n->keys + i, n->keys + i - 1, n->num_keys - (i - 1));
            memmove_sized(n->values + i, n->values + i - 1, n->num_keys - (i - 1));

            // shift value flags accordingly
            // note: we use shift left since index 0 is least significant bit in bitmap
            shift_one_left(&n->tree_end, i);

            // Insert the new key at found location
            n->keys[i] = key;
            n->values[i].pair.key = full_key;
            n->values[i].pair.value = value;
            n->num_keys++;
            clear_bit(&n->tree_end, i);
            return;
        }

        // See if the found child is full
        if (n->children[i]->num_keys == 2 * n->min_deg - 1)
        {
            // TODO check if try find fist and then insert is faster
            // If the child is full, then split it
            node_split_child(n, i, n->children[i]);

            // After split, the middle key of n->children[i] goes up and
            // n->children[i] is splitted into two.  See which of the two
            // is going to have the new key
            if (n->keys[i] < key)
                i++;

            if (n->keys[i] == key)
                // in rare cases we make a unecessary splits
                // in this cases we need to retry the insertion for the current node
                // this happens for about 0,0025% of all insertions
                continue;
        }
        // set node to child for next iteration
        // exception: node that was split
        //
        // if (n->keys[i] != key)
        n = n->children[i];
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
    for (int i = 0; i < n->num_keys; i++)
    {
        if (get_bit(&n->tree_end, i) == 1)
        {
            btree_free(n->values[i].next);
            free(n->values[i].next);
        }
    }
    free(n);
}

void btree_init(btree *tree, int t)
{
    tree->root = NULL;
    tree->t = t;
}

void btree_insert(btree *tree, btree_key key, i_value value)
{
    btree_key_hash hashed_key = hash_key(key);
    partial_key *p_keys = (partial_key *)&hashed_key;
    int num_partial_keys = sizeof(btree_key_hash) / sizeof(partial_key);

    btree_insert_partial(tree, p_keys, num_partial_keys, value, hashed_key);
}

void btree_insert_partial(btree *tree, partial_key *keys, int num_keys, i_value value, btree_key_hash full_key)
{
    // If tree is empty
    if (tree->root == NULL)
    {
        // Allocate memory for root
        tree->root = node_create(tree->t, true);

        tree->root->keys[0] = keys[0];
        tree->root->values[0].pair.key = full_key;
        tree->root->values[0].pair.value = value;
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
            if (s->keys[0] < keys[0])
                i++;
            node_insert_non_full(s->children[i], keys, num_keys, value, full_key);

            // Change root
            tree->root = s;
        }
        else // If root is not full, call insertNonFull for root
            node_insert_non_full(tree->root, keys, num_keys, value, full_key);
    }
}

i_value *btree_get(btree *tree, btree_key key)
{
    btree_key_hash hashed_key = hash_key(key);
    partial_key *p_keys = (partial_key *)&hashed_key;
    int num_partial_keys = sizeof(btree_key_hash) / sizeof(partial_key);

    if (tree->root != NULL)
    {
        return node_get(tree->root, p_keys, num_partial_keys);
    }
    else
    {
        return NULL;
    }
}

int order(btree *tree)
{
    return tree->t * 2 + 1;
}

void btree_free(btree *tree)
{
    if (tree->root != NULL)
        node_free(tree->root);
}

btree_key_hash hash_key(btree_key key)
{
#ifdef HASH_EKY
    uint64_t hashed_key[2];
    //MurmurHash3_x64_128(&key, sizeof(btree_key), 42, &hashed_key);
    // truncate hash to 64 bit
    // this is the way, according to https://security.stackexchange.com/questions/97377/secure-way-to-shorten-a-hash
    return hashed_key[0];
#else
    return key;
#endif
}