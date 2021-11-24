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

#define memcpy_sized(dst, src, n) memcpy(dst, src, (n) * sizeof(*(dst)))
#define memmove_sized(dst, src, n) memmove(dst, src, (n) * sizeof(*(dst)))

#define SIMD_REGISTER_SIZE 256
#define DCACHE_LINESIZE 512

/**
 * @brief size of the keys in the binary tree in bits
 */
#define KEY_SIZE 32

#define REG_VALUES ((SIMD_REGISTER_SIZE) / (KEY_SIZE))

// define macros for the AVX functions based on the KEY_SIZE

#if KEY_SIZE == 8
typedef int8_t key_t;
#define KEY_T_MAX INT8_MAX
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi8(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi8(a)
#define _mm256_movemask(a) _mm256_movemask_epi8(a)

#elif KEY_SIZE == 16
typedef int16_t key_t;
#define KEY_T_MAX INT16_MAX
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi16(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi16(a)
#define _mm256_movemask(a) _pext_u32(_mm256_movemask_epi8(a), 0xAAAAAAAA) // 0xA = 0b1010

#elif KEY_SIZE == 32
typedef int32_t key_t;
#define KEY_T_MAX INT32_MAX
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi32(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi32(a)
#define _mm256_movemask(a) _mm256_movemask_ps(a)

#elif KEY_SIZE == 64
typedef int64_t key_t;
#define KEY_T_MAX INT64_MAX
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi64(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi64x(a)
#define _mm256_movemask(a) _mm256_movemask_pd(a)

#else
#error KEY_SIZE has to be 8, 16, 32 or 64
#endif

#define ORDER (DCACHE_LINESIZE / KEY_SIZE + 1)

typedef u_int64_t value_t;

#define avx_broadcast(a) _mm256_set1_epi(a)

// #define BPTREE_SECURE_NODE_ACCESS

typedef struct node_t
{
    key_t keys[ORDER - 1];
    union
    {
        value_t values[ORDER];
        struct node_t *nodes[ORDER];
    } children;

    // reference counting counter
    // number of get operations that currently
    // access this node
    uint64_t __attribute__((aligned(8))) rc_cnt;

    /** number of keys in node **/
    uint16_t n;
    /** marks node as leaf **/
    bool is_leaf;
} __attribute__((aligned(32))) node_t;

node_t *node_create(bool is_leaf);
uint16_t find_index_avx2(key_t keys[ORDER - 1], int size, __m256i key);
uint16_t find_index(key_t keys[ORDER - 1], int size, key_t key);

bool node_get(node_t *n, key_t key, value_t *result, uint64_t *inc_ops, bool use_avx2);

void node_split(node_t *n, uint16_t i, node_t *child);

node_t *node_insert(node_t *n, key_t key, value_t value, node_t **free_after, uint64_t *inc_ops, bool use_avx2);

void node_free(node_t *n);
typedef struct bptree_t
{
    node_t *root;
    pthread_spinlock_t lock;
    uint64_t __attribute__((aligned(8))) inc_ops;
    bool use_avx2;
} bptree_t;

void bptree_init(bptree_t *tree, bool use_avx2);

bool bptree_get(bptree_t *tree, key_t key, value_t *result);

void bptree_insert(bptree_t *tree, key_t key, value_t value);

void bptree_free(bptree_t *tree);
