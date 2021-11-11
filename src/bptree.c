#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include "bptree.h"

void node_init(node_t *n, bool is_leaf)
{
    n->n = 0;
    n->is_leaf = is_leaf;
    // set all keys to max value as default to avoid masking when compare with AVX
    for (int i = 0; i < ORDER - 1; i++)
        n->keys[i] = KEY_T_MAX;
    // point children to memory allocated for it after node struct
    if (n->is_leaf)
    {
        value_t *values = malloc(sizeof(value_t[ORDER - 1]) + sizeof(rc_ptr_t));
        if (values == NULL)
        {
            perror("not enough memory\n");
            exit(EXIT_FAILURE);
        }
        n->children = (rc_ptr_t *)(values + (ORDER - 1));
        rc_ptr_init(n->children, values);
    }
    else
    {
        node_t *nodes = aligned_alloc(32, sizeof(node_t) * ORDER + sizeof(rc_ptr_t));
        if (nodes == NULL)
        {
            perror("not enough memory\n");
            exit(EXIT_FAILURE);
        }
        n->children = (rc_ptr_t *)(nodes + ORDER);
        rc_ptr_init(n->children, nodes);
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
        // rc_ptr_inc(rc);
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            value_t *result = NULL;
            if (eq)
            {
                value_t *values = (value_t *)n->children->ptr;
                result = &values[i];
            }
            // rc_ptr_dec(rc);
            return result;
        }
        else
        {
            rc_ptr_t *old_rc = rc;
            rc = n->children;
            node_t *children = n->children->ptr;
            if (eq)
                i++;
            n = &(children[i]);
            // rc_ptr_dec(old_rc);
        }
    }
}

void node_split(node_t *n, uint16_t i, node_t *child)
{
    node_t *n_children = (node_t *)n->children->ptr;
    // Since this node is going to have a new child,
    // create space of new child
    memmove_sized(n_children + i + 2, n_children + i + 1, n->n - i);

    // we are storing the new right node in the children of the current node
    node_t *right = &(n_children[i + 1]);
    node_init(right, child->is_leaf);

    int min_deg = (ORDER + ORDER % 2) / 2; // divide by two and ceil
    // is we split child split value has to be reinserted into right node
    // k makes sure all new values in the node are moved one to the right
    int k = child->is_leaf ? 1 : 0;

    right->n = (ORDER - min_deg - 1) + k;
    // copy vales (leaf node) or child nodes
    if (child->is_leaf)
    {
        // TODO maybe make this prettier
        value_t *right_values = (value_t *)right->children->ptr;
        value_t *child_values = (value_t *)child->children->ptr;
        memcpy_sized(right_values, child_values + min_deg - 1, right->n);
    }
    else
    {
        node_t *right_children = (node_t *)right->children->ptr;
        node_t *child_children = (node_t *)child->children->ptr;
        memcpy_sized(right_children, child_children + min_deg, right->n + 1);
    }

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
 * @return node* pointer to clone
 */
rc_ptr_t *_Atomic node_clone_group(node_t *group)
{
    node_t *nodes_clone = aligned_alloc(32, sizeof(node_t) * ORDER + sizeof(rc_ptr_t));
    if (nodes_clone == NULL)
    {
        perror("not enough memory\n");
        exit(EXIT_FAILURE);
    }
    // TODO use memcpysized?
    memcpy(nodes_clone, group, sizeof(node_t) * ORDER);
    for (int i = 0; i < ORDER; i++)
        pthread_spin_init(&nodes_clone[i].write_lock, 0);

    rc_ptr_t *_Atomic group_clone = (rc_ptr_t *)(nodes_clone + ORDER);
    rc_ptr_init(group_clone, nodes_clone);
    return group_clone;
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
void node_insert_no_clone(node_t *n, key_t key, key_cmp_t cmp_key, value_t value)
{
    while (true)
    {
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            value_t *n_values = (value_t *)n->children->ptr;
            if (eq)
            {
                memcpy(&n_values[i], value, sizeof(value_t));
            }
            else
            {
                // shift values to right an insert
                memmove_sized(n->keys + i + 1, n->keys + i, n->n - i);
                memmove_sized(n_values + i + 1, n_values + i, (n->n - i));
                n->keys[i] = key;
                memcpy(&n_values[i], value, sizeof(value_t));
                n->n++;
            }
            return;
        }
        else
        {
            node_t *n_children = (node_t *)n->children->ptr;
            if (eq)
                i++;
            if (n_children[i].n == ORDER - 1)
            {
                node_t *to_split = &(n_children[i]);
                node_split(n, i, to_split);

                // update number of elements in node that was just split
                int min_deg = (ORDER + ORDER % 2) / 2;
                to_split->n = min_deg - 1;

                if (n->keys[i] < key)
                    i++;
            }
            n = &(n_children[i]);
        }
    }
}

void node_insert(node_t *n, key_t key, key_cmp_t cmp_key, value_t value, rc_ptr_t *_Atomic *node_group, pthread_spinlock_t *write_lock, bptree_t *tree)
{
    uint16_t i = find_index(n->keys, n->n, cmp_key);
    bool eq = n->keys[i] == key;
    if (n->is_leaf)
    {
        if (eq)
        {
            value_t *n_values = (value_t *)n->children->ptr;
            memcpy(&n_values[i], value, sizeof(value_t));
            pthread_spin_unlock(write_lock);
        }
        else
        {
            rc_ptr_t *_Atomic clone;
            node_t *group = (node_t *)(*node_group)->ptr;
            if (tree->root->ptr != n)
            {
                // clone entire node group and perform insertion on this clone
                clone = node_clone_group(group);
            }
            else
            {
                // we are in root so only clone current node
                node_t *root = aligned_alloc(32, sizeof(node_t) + sizeof(rc_ptr_t));
                if (root == NULL)
                {
                    perror("not enough memory\n");
                    exit(EXIT_FAILURE);
                }
                memcpy_sized(root, n, 1);
                clone = (rc_ptr_t *)(root + 1);
                rc_ptr_init(clone, root);
                pthread_spin_init(&root->write_lock, 0);
            }
            node_t *clone_group = (node_t *)clone->ptr;
            node_t *n_clone = clone_group + (n - group);
            // shift values to right an insert
            memmove_sized(n_clone->keys + i + 1, n_clone->keys + i, n_clone->n - i);

            // clone values
            value_t *value_clones = malloc(sizeof(value_t) * (ORDER - 1) + sizeof(rc_ptr_t));
            if (value_clones == NULL)
            {
                perror("not enough memory\n");
                exit(EXIT_FAILURE);
            }
            rc_ptr_t *new_values = (rc_ptr_t *)(value_clones + sizeof(value_t) * (ORDER - 1));
            rc_ptr_init(new_values, value_clones);

            value_t *n_clone_values = (value_t *)n_clone->children->ptr;
            memcpy_sized(value_clones, n_clone_values, i);
            rc_ptr_t *old_values = n_clone->children;
            n_clone->children = new_values;

            value_t *old_values_values = (value_t *)old_values->ptr;
            memcpy_sized(value_clones + i + 1, old_values_values + i, (n_clone->n - i));
            n_clone->keys[i] = key;
            memcpy(&value_clones[i], value, sizeof(value_t));
            n_clone->n++;

            // change pointer to clone and free old one
            rc_ptr_t *old_group = atomic_exchange(node_group, clone);
            rc_ptr_free(old_group);

            // TODO free with reference counting
            rc_ptr_free(old_values);
            pthread_spin_unlock(write_lock);
        }
        return;
    }
    else
    {
        if (eq)
            i++;
        node_t *n_children = (node_t *)n->children->ptr;
        if (n_children[i].n == ORDER - 1)
        {
            rc_ptr_t *_Atomic n_group_clone = node_clone_group((node_t *)(*node_group)->ptr);
            rc_ptr_t *_Atomic c_group_clone = node_clone_group(n_children);

            node_t *node_group_nodes = (node_t *)(*node_group)->ptr;
            node_t *n_group_clone_nodes = (node_t *)n_group_clone->ptr;

            node_t *n_clone = n_group_clone_nodes + (n - node_group_nodes);
            // TODO this overrides counter, check
            n_clone->children = c_group_clone;

            node_t *n_clone_children = (node_t *)n_clone->children->ptr;
            node_t *to_split = &(n_clone_children[i]);
            node_split(n_clone, i, to_split);

            // update number of elements in node that was just split
            int min_deg = (ORDER + ORDER % 2) / 2;
            to_split->n = min_deg - 1;

            if (n_clone->keys[i] < key)
                i++;

            node_insert_no_clone(&(n_clone_children[i]), key, cmp_key, value);

            // swap pointers and save previous pointer to old_group
            rc_ptr_t *old_group = atomic_exchange(node_group, n_group_clone);

            rc_ptr_free(old_group);
            // TODO free with reference counting
            free(n->children->ptr); // free old children

            pthread_spin_unlock(write_lock);
            return;
        }
        pthread_spin_lock(&n->write_lock);
        node_insert(&(n_children[i]), key, cmp_key, value, &n->children, &n->write_lock, tree);

        // done inserting in the child, release the lock
        pthread_spin_unlock(write_lock);
    }
}

void node_free(node_t *n)
{
    pthread_spin_destroy(&n->write_lock);
    node_t *n_children = (node_t *)n->children->ptr;
    for (int i = 0; i < n->n + 1; i++)
    {
        if (!n_children[i].is_leaf)
            node_free(&(n_children[i]));
    }
    rc_ptr_free(n->children);
}

void bptree_init(bptree_t *tree)
{
    tree->root = NULL;
    pthread_spin_init(&tree->write_lock, 0);
}

value_t *bptree_get(bptree_t *tree, key_t key)
{
    if (__builtin_expect(tree->root == NULL, 0))
        return NULL;
    else
    {
        return node_get(tree->root->ptr, key, &tree->root);
    }
}

void bptree_insert(bptree_t *tree, key_t key, value_t value)
{
    pthread_spin_lock(&tree->write_lock);
    if (__builtin_expect(tree->root == NULL, 0))
    {
        // TODO make multiple of 32
        node_t *root = aligned_alloc(32, sizeof(node_t) + sizeof(rc_ptr_t));
        if (root == NULL)
        {
            perror("not enough memory\n");
            exit(EXIT_FAILURE);
        }
        node_init(root, true);
        root->keys[0] = key;
        value_t *root_values = (value_t *)root->children->ptr;
        memcpy(&root_values[0], value, sizeof(value_t));
        root->n = 1;
        rc_ptr_t *root_node = (rc_ptr_t *)(root + 1);
        rc_ptr_init(root_node, root);
        atomic_store(&tree->root, root_node);
        pthread_spin_unlock(&tree->write_lock);
        return;
    }
    else
    {
        key_cmp_t cmp_key = avx_broadcast(key);
        node_t *root = tree->root->ptr;
        if (root->n == ORDER - 1)
        {
            node_t *s = aligned_alloc(32, sizeof(node_t) + sizeof(rc_ptr_t));
            if (s == NULL)
            {
                perror("not enough memory\n");
                exit(-1);
            }
            node_init(s, false);

            node_split(s, 0, root);

            rc_ptr_t *new_root = (rc_ptr_t *)(s + 1);
            rc_ptr_init(new_root, s);

            node_t *s_children = (node_t *)s->children->ptr;
            s_children[0] = *root;
            pthread_spin_init(&s_children[0].write_lock, 0);

            // Reduce the number of keys in old root element
            int min_deg = (ORDER + ORDER % 2) / 2;
            s_children[0].n = min_deg - 1;

            int i = 0;
            if (s->keys[0] < key)
                i++;
            node_insert(&(s_children[i]), key, cmp_key, value, &s->children, &s->write_lock, tree);

            // update root to new
            rc_ptr_t *old_root = atomic_exchange(&tree->root, new_root);
            pthread_spin_unlock(&tree->write_lock);

            // wait for all get requests to finish
            rc_ptr_free(old_root);
        }
        else
        {
            // IMPORTANT: as you (hello future simon) can maybe see, the unlock for the
            // bptree write lock is missing. The unlock happens within the node_insert method.
            node_insert(root, key, cmp_key, value, &tree->root, &tree->write_lock, tree);
        }
    }
}

void bptree_free(bptree_t *tree)
{
    if (tree->root != NULL)
    {
        node_t *root = (node_t *)tree->root->ptr;
        if (!root->is_leaf)
            node_free(root);
        rc_ptr_free(tree->root);
    }
    pthread_spin_destroy(&tree->write_lock);
}

void rc_ptr_init(rc_ptr_t *rc, void *ptr)
{
    rc->cnt = 0;
    rc->ptr = ptr;
}

static inline void rc_ptr_inc(rc_ptr_t *rc)
{
    atomic_fetch_add_explicit(&rc->cnt, 1, memory_order_relaxed);
}

static inline void rc_ptr_dec(rc_ptr_t *rc)
{
    atomic_fetch_sub_explicit(&rc->cnt, 1, memory_order_relaxed);
}

static inline void rc_ptr_free(rc_ptr_t *rc)
{
    while (rc->cnt > 0)
        ;
    free(rc->ptr);
}