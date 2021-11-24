#pragma once
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <immintrin.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include "spinlock.h"

#define SIMD_REGISTER_SIZE sizeof(__m256i)

// cache-line width of processor in bytes
#define DCACHE_LINESIZE 64

/**
 * @brief size of the keys in the binary tree in bytes
 */
#define KEY_SIZE 4

// number of values that can fit into one AVX2 register
#define NUM_REG_VALUES ((SIMD_REGISTER_SIZE) / (KEY_SIZE))

// define macros for the AVX functions based on the KEY_SIZE

#if KEY_SIZE == 1
typedef int8_t bp_key_t;
#define KEY_T_MAX INT8_MAX
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi8(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi8(a)
#define _mm256_movemask(a) _mm256_movemask_epi8((__m256i)a)

#elif KEY_SIZE == 2
typedef int16_t bp_key_t;
#define KEY_T_MAX INT16_MAX
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi16(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi16(a)
// there is no 16 bits version of movemask
// so we use the 8-bit version and then gather every second bit
#define _mm256_movemask(a) _pext_u32(_mm256_movemask_epi8((__m256i)a), 0xAAAAAAAA) // 0xA = 0b1010

#elif KEY_SIZE == 4
typedef int32_t bp_key_t;
#define KEY_T_MAX INT32_MAX
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi32(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi32(a)
#define _mm256_movemask(a) _mm256_movemask_ps((__m256)a)

#elif KEY_SIZE == 8
typedef int64_t bp_key_t;
#define KEY_T_MAX INT64_MAX
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi64(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi64x(a)
#define _mm256_movemask(a) _mm256_movemask_pd((__m256d)a)

#else
#error KEY_SIZE has to be 1,2,4 or 8
#endif

#define ORDER (DCACHE_LINESIZE / KEY_SIZE + 1)

// value type in b+tree
typedef u_int64_t value_t;

// defines whether "super" version of node access is used.
// A counter in bptree is used to keep track of ongoing
// node_access operations and a node is only freed
// when this counter is down to zero.
// #define BPTREE_SECURE_NODE_ACCESS

// a node within the b+tree
typedef struct node_t
{
    // array of keys within this node
    bp_key_t keys[ORDER - 1];

    // array of node pointer (children) or values
    union
    {
        value_t values[ORDER];
        struct node_t *nodes[ORDER];
    } children;

    // reference counting counter
    // number of get operations that currently
    // access this node
    uint64_t __attribute__((aligned(8))) rc_cnt;

    // number of keys in node
    uint16_t n;

    // marks node as leaf
    bool is_leaf;
} __attribute__((aligned(32))) node_t;

/**
 * @brief Allocates the memory for a new node and initializes it.
 * keys within the node are set to KEY_T_MAX.
 * rc_cnt is set to zero.
 * 
 * @param is_leaf marks whether the node is a leaf or itermediate node
 * @return node_t* pointer to created node
 */
node_t *node_create(bool is_leaf);

/**
 * @brief finds the value for key within the node and its children 
 * 
 * @param n a node
 * @param key query key
 * @param result destination where the value is stored
 * @param inc_ops reference to counter (see BPTREE_SECURE_NODE_ACCESS)
 * @param use_avx2 whether to use AVX2 accelerated version of find_index
 * @return true if key was found
 * @return false else
 */
bool node_get(node_t *n, bp_key_t key, value_t *result, uint64_t *inc_ops, bool use_avx2);

/**
 * @brief inserts a key and its value into a bptree node.
 * The function clones the node (or its children) before the insert and
 * performs the insert on the clone. Poiter to this clone is returned
 * 
 * @param n node to insert it to
 * @param key 
 * @param value 
 * @param free_after function may stores a pointer to a node here. This node can be freed afterwards.
 * @param inc_ops reference to counter (see BPTREE_SECURE_NODE_ACCESS)
 * @param use_avx2 whether to use AVX2 accelerated version of find_index
 * @return node_t* clone of n that was inserted to. (NULL if no insertion happend)
 */
node_t *node_insert(node_t *n, bp_key_t key, value_t value, node_t **free_after, uint64_t *inc_ops, bool use_avx2);

// Frees memory allocated by a nodes children
// Does not free the node n inself.
void node_free(node_t *n);

typedef struct bptree_t
{
    node_t *root;
    pthread_spinlock_t lock;
    uint64_t __attribute__((aligned(8))) inc_ops;
    bool use_avx2;
} bptree_t;

/**
 * @brief initilizes a b+tree 
 * 
 * @param tree pointer to tree
 * @param use_avx2 whether to use avx2 acceleration of not
 */
void bptree_init(bptree_t *tree, bool use_avx2);

/**
 * @brief finds the value for a key
 * 
 * @param tree a bptree
 * @param key query key
 * @param result destination where the value is stored
 * @return true if key was found
 * @return false else
 */
bool bptree_get(bptree_t *tree, bp_key_t key, value_t *result);

// inserts a key-value pair or updates a keys value
void bptree_insert(bptree_t *tree, bp_key_t key, value_t value);

// frees memory allocated by the tree
// does not free the bptree_t struct itself
void bptree_free(bptree_t *tree);
