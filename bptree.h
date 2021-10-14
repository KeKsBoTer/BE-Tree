
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <immintrin.h>

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

#define ORDER (16)

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

node *node_create(bool is_leaf);
#if __AVX2__
uint16_t find_index(key_t keys[ORDER - 1], int size, __m256i key);
#else
uint16_t find_index(key_t keys[ORDER - 1], int size, key_t key);
#endif

value_t *node_get(node *n, key_t key);

void node_split(node *n, uint16_t i, node *child);

void node_insert(node *n, key_t key, value_t value);

void node_free(node *n);
typedef struct bptree
{
    node *root;
} bptree;

void bptree_init(bptree *tree);

value_t *bptree_get(bptree *tree, key_t key);

void bptree_insert(bptree *tree, key_t key, value_t value);

void bptree_free(bptree *tree);
