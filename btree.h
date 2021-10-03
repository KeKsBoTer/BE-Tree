#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <x86intrin.h>

/**
 * @brief size of the keys in the binary tree in bits
 */
#define KEY_SIZE 16

// define macros for the AVX functions based on the KEY_SIZE

#if KEY_SIZE == 8
typedef uint8_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi8(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi8(a)

#elif KEY_SIZE == 16
typedef uint16_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi16(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi16(a)

#elif KEY_SIZE == 32
typedef uint32_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi32(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi32(a)

#elif KEY_SIZE == 64
typedef uint64_t partial_key;
#define _mm256_cmpgt_epi(a, b) _mm256_cmpgt_epi64(a, b)
#define _mm256_set1_epi(a) _mm256_set1_epi64x(a)

#else
#error KEY_SIZE has to be 8, 16, 32 or 64
#endif

/**
 * @brief type of the values with the btree
 */
typedef int i_value;

/**
 * @brief a node within a btree
 */
typedef struct node
{
    /** list of keys within the node**/
    partial_key *keys;
    i_value *values;
    struct node **children;
    /** Minimum degree (defines the range for number of keys) **/
    int min_deg; //
    /** Current number of keys / values within the node **/
    int num_keys;
    /** indicator if a node is a leaf node **/
    bool leaf;
} node;

/**
 * @brief create a node struct together with keys, values and children array
 * as a continouns block of memory
 * 
 * @param min_deg minimum number of elements in non leaf and root nodes (node size = min_deg * 2)
 * @param is_leaf indicates whether node is a leaf node
 * @return node* 32-bit aligned pointer to node struct
 */
node *node_create(int min_deg, bool is_leaf);

/**
 * @brief finds index of key within a list. 
 * Uses SIMD instructions if they are enabled during compile time
 * 
 * @param keys pointer to start of key
 * @param size number of keys
 * @param key key to search for
 * @return unsigned int location of key within keys array
 */
unsigned int find_index(partial_key *keys, int size, partial_key key);

i_value *node_get(node *n, partial_key key);
void node_split_child(node *n, int i, node *y);
void node_insert_non_full(node *n, partial_key key, i_value value);
void node_free(node *n);

typedef int64_t btree_key;
typedef int64_t btree_key_hash;
typedef struct btree
{
    node *root;
    /** minimum number of keys per node in tree **/
    int t;
} btree;

void btree_init(btree *tree, int t);

i_value *btree_get(btree *tree, btree_key key);
void btree_insert(btree *tree, btree_key key, i_value value);

/** returns the order of the tree (minimum number of elements per node * 2 + 1) **/
int order(btree *tree);

/** frees memory allocated for tree. tree pointer itself is not freed**/
void btree_free(btree *tree);

btree_key_hash hash_key(btree_key key);