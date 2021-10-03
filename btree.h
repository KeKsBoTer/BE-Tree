#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <x86intrin.h>

#define KEY_SIZE 8

#if KEY_SIZE == 8
typedef int8_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi8(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi8(a)

#elif KEY_SIZE == 16
typedef int16_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi16(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi16(a)

#elif KEY_SIZE == 32
typedef int32_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi32(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi32(a)

#elif KEY_SIZE == 64
typedef int64_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi64(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi64x(a)

#else
#error KEY_SIZE has to be 8, 16, 32 or 64
#endif

typedef int i_value;

typedef struct node
{
    partial_key *keys;
    i_value *values;
    struct node **children;
    uint8_t min_deg;  // Minimum degree (defines the range for number of keys)
    uint8_t num_keys; // Current number of keys
    bool leaf;
} node;

node *node_create(int min_deg, bool is_leaf);

unsigned int find_index(partial_key *keys, int size, partial_key key);

i_value *node_get(node *n, partial_key key);

void node_split_child(node *n, int i, node *y);

void node_insert_non_full(node *n, partial_key key, i_value value);
void node_dot(node *n, FILE *fp);

void node_free(node *n);

typedef struct btree
{
    node *root;
    int t;
} btree;

void btree_init(btree *tree, int t);

void btree_insert(btree *tree, partial_key key, i_value value);
i_value *btree_get(btree *tree, partial_key key);
void btree_free(btree *tree);