#pragma once
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <immintrin.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>

#ifndef __USE_XOPEN2K
#include "spinlock.h"
#endif

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
// workaround for 16 bit. Use 8 bit extraction and only use every second value
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

typedef char value_t[200];

#ifdef __AVX2__
#define key_cmp_t __m256i
#define avx_broadcast(a) _mm256_set1_epi(a)
#else
#define key_cmp_t key_t
#define avx_broadcast(a) (a)
#endif

// reference counting struct for poiter
typedef struct rc_ptr_t
{
    atomic_uint cnt;
    void *ptr;
} __attribute__((aligned(32))) rc_ptr_t;

void rc_ptr_init(rc_ptr_t *rc, void *ptr);

static inline void rc_ptr_inc(rc_ptr_t *rc);
static inline void rc_ptr_dec(rc_ptr_t *rc);

static inline void rc_ptr_free(rc_ptr_t *rc);
typedef struct node_t
{
    key_t keys[ORDER - 1];

    // children / values
    rc_ptr_t *children;

    pthread_spinlock_t write_lock;
    /** number of keys in node **/
    uint16_t n;
    /** marks node as leaf **/
    bool is_leaf;

} __attribute__((aligned(32))) node_t;

typedef struct bptree_t
{
    // reference to root node
    rc_ptr_t *_Atomic root;
    pthread_spinlock_t write_lock;
} bptree_t;

void node_init(node_t *n, bool is_leaf);
#if __AVX2__
uint16_t find_index(key_t keys[ORDER - 1], int size, __m256i key);
#else
uint16_t find_index(key_t keys[ORDER - 1], int size, key_t key);
#endif

value_t *node_get(node_t *n, key_t key, rc_ptr_t *rc);

void node_split(node_t *n, uint16_t i, node_t *child);

void node_free(node_t *node);

void node_insert(node_t *n, key_t key, key_cmp_t cmp_key, value_t value, rc_ptr_t *_Atomic *node_group, pthread_spinlock_t *write_lock, bptree_t *tree);

// b+ tree stuff

void bptree_init(bptree_t *tree);

value_t *bptree_get(bptree_t *tree, key_t key);

void bptree_insert(bptree_t *tree, key_t key, value_t value);

void bptree_free(bptree_t *tree);
