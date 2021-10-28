#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include "bptree.h"

void node_init(node *n, bool is_leaf)
{
    n->n = 0;
    n->is_leaf = is_leaf;
    // set all keys to max value as default to avoid masking when compare with AVX
    for (int i = 0; i < ORDER - 1; i++)
        n->keys[i] = KEY_T_MAX;
    // point children to memory allocated for it after node struct
    if (n->is_leaf)
    {
        n->children.values = malloc(sizeof(value_t[ORDER - 1]));
    }
    else
    {
        n->children.next = aligned_alloc(32, sizeof(node) * ORDER);
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

value_t *node_get(node *n, key_t key)
{
    // convert to avx value if supported
    key_cmp_t cmp_key = avx_broadcast(key);
    while (true)
    {
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            if (eq)
                return &n->children.values[i];
            else
                return NULL;
        }
        else
        {
            if (eq)
                n = &(n->children.next[i + 1]);
            else
                n = &(n->children.next[i]);
        }
    }
}

void node_split(node *n, uint16_t i, node *child)
{
    // Since this node is going to have a new child,
    // create space of new child
    memmove_sized(n->children.next + i + 2, n->children.next + i + 1, n->n - i);

    // we are storing the new right node in the children of the current node
    node *right = &(n->children.next[i + 1]);
    node_init(right, child->is_leaf);

    int min_deg = (ORDER + ORDER % 2) / 2; // divide by two and ceil
    // is we split child split value has to be reinserted into right node
    // k makes sure all new values in the node are moved one to the right
    int k = child->is_leaf ? 1 : 0;

    right->n = (ORDER - min_deg - 1) + k;
    // copy vales (leaf node) or child nodes
    if (child->is_leaf)
        memcpy_sized(right->children.values, child->children.values + min_deg - 1, right->n);
    else
        memcpy_sized(right->children.next, child->children.next + min_deg, right->n + 1);

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
 * 
 * @param group poiter to group with ORDER nodes
 * @return node* pointer to clone
 */
node *node_clone_group(node *group)
{
    node *clone = aligned_alloc(32, sizeof(node) * ORDER);
    memcpy_sized(clone, group, ORDER);
    return clone;
}

void node_insert_no_clone(node *n, key_cmp_t cmp_key, value_t value)
{
    key_t key = *((key_t *)(&cmp_key));
    while (true)
    {
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            if (eq)
            {
                n->children.values[i] = value;
            }
            else
            {
                // shift values to right an insert
                memmove_sized(n->keys + i + 1, n->keys + i, n->n - i);
                memmove_sized(n->children.values + i + 1, n->children.values + i, (n->n - i));
                n->keys[i] = key;
                n->children.values[i] = value;
                n->n++;
            }
            return;
        }
        else
        {
            if (eq)
                i++;
            if (n->children.next[i].n == ORDER - 1)
            {
                node *to_split = &(n->children.next[i]);
                node_split(n, i, to_split);

                // update number of elements in node that was just split
                int min_deg = (ORDER + ORDER % 2) / 2;
                to_split->n = min_deg - 1;

                if (n->keys[i] < key)
                    i++;
            }
            n = &(n->children.next[i]);
        }
    }
}

void node_insert(node *n, key_t key, value_t value, node **node_group, pthread_spinlock_t *write_lock, bool is_root)
{
    // convert to avx value if supported
    key_cmp_t cmp_key = avx_broadcast(key);

    while (true)
    {
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            if (eq)
            {
                n->children.values[i] = value;
            }
            else
            {
                node *clone;
                if (!is_root)
                {
                    // clone entire node group and perform insertion on this clone
                    clone = node_clone_group(*node_group);
                }
                else
                {
                    // we are in root so only clone current node
                    clone = aligned_alloc(32, sizeof(node));
                    memcpy_sized(clone, n, 1);
                }
                n = clone + (n - *node_group);
                // shift values to right an insert
                memmove_sized(n->keys + i + 1, n->keys + i, n->n - i);

                // clone values
                value_t *value_clones = malloc(sizeof(value_t) * (ORDER - 1));
                memcpy_sized(value_clones, n->children.values, i);
                value_t *old_values = n->children.values;
                n->children.values = value_clones;

                memcpy_sized(n->children.values + i + 1, old_values + i, (n->n - i));
                n->keys[i] = key;
                n->children.values[i] = value;
                n->n++;

                // change pointer to clone and free old one
                node *old_group = *node_group;
                *node_group = clone;

                free(old_group);
                free(old_values);
            }
            pthread_spin_unlock(write_lock);
            return;
        }
        else
        {
            if (eq)
                i++;
            if (n->children.next[i].n == ORDER - 1)
            {
                node *n_group_clone = node_clone_group(*node_group);
                node *c_group_clone = node_clone_group(n->children.next);
                node *n_clone = n_group_clone + (n - *node_group);
                n_clone->children.next = c_group_clone;

                node *to_split = &(n_clone->children.next[i]);
                node_split(n_clone, i, to_split);

                // update number of elements in node that was just split
                int min_deg = (ORDER + ORDER % 2) / 2;
                to_split->n = min_deg - 1;

                if (n_clone->keys[i] < key)
                    i++;

                node_insert_no_clone(&(n_clone->children.next[i]), cmp_key, value);

                node *old_group = *node_group;
                node *old_children = n->children.next;
                *node_group = n_group_clone;
                n = n_clone;

                pthread_spin_unlock(write_lock);
                free(old_group);
                free(old_children);
                return;
            }
            // done inserting in the child, release the lock
            pthread_spin_unlock(write_lock);

            // require lock for children since they are accessed next
            write_lock = &n->write_lock;
            pthread_spin_lock(write_lock);

            // insert in child in next iteration (tail recursion)
            node_group = &n->children.next;
            n = &(n->children.next[i]);
            is_root = false;
        }
    }
}

void node_free(node *n)
{
    pthread_spin_destroy(&n->write_lock);
    for (int i = 0; i < n->n + 1; i++)
    {
        if (!n->children.next[i].is_leaf)
            node_free(&(n->children.next[i]));
    }
    free(n->children.next);
}

void bptree_init(bptree *tree)
{
    tree->root = NULL;
    pthread_spin_init(&tree->write_lock, 0);
    pqueue_init(&tree->get_queue);
    tree->global_step = 0;
}

value_t *bptree_get(bptree *tree, key_t key)
{
    if (__builtin_expect(tree->root == NULL, 0))
        return NULL;
    else
    {
        return node_get(tree->root, key);
    }
}

void bptree_insert(bptree *tree, key_t key, value_t value)
{
    pthread_spin_lock(&tree->write_lock);
    if (__builtin_expect(tree->root == NULL, 0))
    {
        node *root = aligned_alloc(32, sizeof(node));
        node_init(root, true);
        root->keys[0] = key;
        root->children.values[0] = value;
        root->n = 1;
        tree->root = root;
        pthread_spin_unlock(&tree->write_lock);
    }
    else
    {
        if (tree->root->n == ORDER - 1)
        {
            node *s = aligned_alloc(32, sizeof(node));
            node_init(s, false);

            node_split(s, 0, tree->root);

            s->children.next[0] = *tree->root;
            pthread_spin_init(&s->children.next[0].write_lock, 0);

            // Reduce the number of keys in old root element
            int min_deg = (ORDER + ORDER % 2) / 2;
            s->children.next[0].n = min_deg - 1;

            int i = 0;
            if (s->keys[0] < key)
                i++;
            node_insert(&(s->children.next[i]), key, value, &s->children.next, &s->write_lock, false);
            node *old_root = tree->root;
            // Change root
            tree->root = s;

            pthread_spin_unlock(&tree->write_lock);
            free(old_root);
        }
        else
        {
            // IMPORTANT: as you (hello future simon) can maybe see, the unlock for the
            // bptree write lock is missing. The unlock happens within the node_insert method.
            node_insert(tree->root, key, value, &tree->root, &tree->write_lock, true);
        }
    }
}

void bptree_free(bptree *tree)
{
    if (tree->root != NULL)
    {
        if (!tree->root->is_leaf)
            node_free(tree->root);
        free(tree->root);
    }
    pqueue_free(&tree->get_queue);
    pthread_spin_destroy(&tree->write_lock);
}