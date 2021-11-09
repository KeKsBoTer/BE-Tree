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
        if (n->children.values == NULL)
        {
            perror("not enough memory\n");
            exit(-1);
        }
    }
    else
    {
        n->children.next = aligned_alloc(32, sizeof(node) * ORDER);
        if (n->children.next == NULL)
        {
            perror("not enough memory\n");
            exit(-1);
        }
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
 * all spin locks in the clone are unlocked
 * 
 * @param group poiter to group with ORDER nodes
 * @return node* pointer to clone
 */
node *node_clone_group(node *group)
{
    node *clone = aligned_alloc(32, sizeof(node) * ORDER);
    if (clone == NULL)
    {
        perror("not enough memory\n");
        exit(-1);
    }
    memcpy_sized(clone, group, ORDER);
    for (int i = 0; i < ORDER; i++)
        pthread_spin_init(&clone[i].write_lock, 0);
    return clone;
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
void node_insert_no_clone(node *n, key_t key, key_cmp_t cmp_key, value_t value)
{
    while (true)
    {
        uint16_t i = find_index(n->keys, n->n, cmp_key);
        bool eq = n->keys[i] == key;
        if (n->is_leaf)
        {
            if (eq)
            {
                memcpy(&n->children.values[i], value, sizeof(value_t));
            }
            else
            {
                // shift values to right an insert
                memmove_sized(n->keys + i + 1, n->keys + i, n->n - i);
                memmove_sized(n->children.values + i + 1, n->children.values + i, (n->n - i));
                n->keys[i] = key;
                memcpy(&n->children.values[i], value, sizeof(value_t));
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

void save_free(bptree *tree, void *memory)
{
    int pipe_w = tree->free_pipe[1];
    garbage_msg msg = {
        .type = MsgFree,
        .step = tree->global_step,
        .msg.memory = memory,
    };
    if (write(pipe_w, &msg, sizeof(msg)) == -1)
    {
        printf("cannot write to pipe %d\n", pipe_w);
        exit(-1);
    }
}

void save_get_start(bptree *tree, pthread_t thread_id, uint64_t step)
{
    int pipe_w = tree->free_pipe[1];
    garbage_msg msg = {
        .type = MsgGet,
        .step = step,
        .msg.get = {.type = GetStart, .id = thread_id},
    };
    if (write(pipe_w, &msg, sizeof(msg)) == -1)
    {
        printf("cannot write to pipe %d\n", pipe_w);
        exit(-1);
    }
}
void save_get_end(bptree *tree, pthread_t thread_id)
{
    int pipe_w = tree->free_pipe[1];
    garbage_msg msg = {
        .type = MsgGet,
        .step = tree->global_step,
        .msg.get = {.type = GetEnd, .id = thread_id},
    };
    if (write(pipe_w, &msg, sizeof(msg)) == -1)
    {
        printf("cannot write to pipe %d\n", pipe_w);
        exit(-1);
    }
}

void node_insert(node *n, key_t key, key_cmp_t cmp_key, value_t value, node **node_group, pthread_spinlock_t *write_lock, bptree *tree)
{
    uint16_t i = find_index(n->keys, n->n, cmp_key);
    bool eq = n->keys[i] == key;
    if (n->is_leaf)
    {
        if (eq)
        {
            memcpy(&n->children.values[i], value, sizeof(value_t));
            pthread_spin_unlock(write_lock);
        }
        else
        {
            node *clone;
            if (!(tree->root == n))
            {
                // clone entire node group and perform insertion on this clone
                clone = node_clone_group(*node_group);
            }
            else
            {
                // we are in root so only clone current node
                clone = aligned_alloc(32, sizeof(node));
                if (clone == NULL)
                {
                    perror("not enough memory\n");
                    exit(-1);
                }
                memcpy_sized(clone, n, 1);
                pthread_spin_init(&clone->write_lock, 0);
            }
            node *n2 = clone + (n - *node_group);
            // shift values to right an insert
            memmove_sized(n2->keys + i + 1, n2->keys + i, n2->n - i);

            // clone values
            value_t *value_clones = malloc(sizeof(value_t) * (ORDER - 1));
            if (value_clones == NULL)
            {
                perror("not enough memory\n");
                exit(-1);
            }
            memcpy_sized(value_clones, n2->children.values, i);
            value_t *old_values = n2->children.values;
            n2->children.values = value_clones;

            memcpy_sized(n2->children.values + i + 1, old_values + i, (n2->n - i));
            n2->keys[i] = key;
            memcpy(&n2->children.values[i], value, sizeof(value_t));
            n2->n++;

            // change pointer to clone and free old one
            node *old_group = *node_group;
            *node_group = clone;
            save_free(tree, old_group);
            save_free(tree, old_values);
            pthread_spin_unlock(write_lock);
        }
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

            node_insert_no_clone(&(n_clone->children.next[i]), key, cmp_key, value);

            node *old_group = *node_group;
            node *old_children = n->children.next;
            *node_group = n_group_clone;
            n = n_clone;

            save_free(tree, old_group);
            save_free(tree, old_children);
            pthread_spin_unlock(write_lock);
            return;
        }
        pthread_spin_lock(&n->write_lock);
        node_insert(&(n->children.next[i]), key, cmp_key, value, &n->children.next, &n->write_lock, tree);

        // done inserting in the child, release the lock
        pthread_spin_unlock(write_lock);
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

void *garbage_collector(void *args)
{
    int p = *((int *)args);

    pqueue queue;
    pqueue_init(&queue);
    garbage_msg g_msg;
    while (1)
    {
        int n = read(p, &g_msg, sizeof(garbage_msg));
        if (n == -1)
        {
            printf("cannot read from pipe\n");
            exit(-1);
        }
        else if (n == 0)
        {
            pqueue_free(&queue);
            pthread_exit(NULL);
            return NULL;
        }
        switch (g_msg.type)
        {
        case MsgGet:
            switch (g_msg.msg.get.type)
            {
            case GetStart:
                pqueue_get_start(&queue, g_msg.msg.get.id, g_msg.step);
                break;
            case GetEnd:
                pqueue_get_end(&queue, g_msg.msg.get.id);
                break;

            default:
                printf("undefined get message type: %d", g_msg.msg.get.type);
                exit(-1);
                break;
            }
            break;

        case MsgFree:
            pqueue_save_free(&queue, g_msg.msg.memory, g_msg.step);
            break;
        default:
            printf("undefined message type: %d", g_msg.type);
            exit(-1);
            break;
        }
    }
    pqueue_free(&queue);
    pthread_exit(NULL);
    return NULL;
}

void bptree_init(bptree *tree)
{
    tree->root = NULL;
    pthread_spin_init(&tree->write_lock, 0);
    tree->global_step = 0;

    if (pipe(tree->free_pipe) < 0)
    {
        printf("cannot create pipe\n");
        exit(-1);
    }
    pthread_create(&tree->free_thread, NULL, garbage_collector, (void *)tree->free_pipe);
}

value_t *bptree_get(bptree *tree, key_t key)
{
    if (__builtin_expect(tree->root == NULL, 0))
        return NULL;
    else
    {
        pthread_t thread_id = pthread_self();
        uint64_t now = __atomic_fetch_add(&tree->global_step, 1, __ATOMIC_SEQ_CST);
        save_get_start(tree, thread_id, now);

        value_t *result = node_get(tree->root, key);

        save_get_end(tree, thread_id);
        return result;
    }
}

void bptree_insert(bptree *tree, key_t key, value_t value)
{
    pthread_spin_lock(&tree->write_lock);
    if (__builtin_expect(tree->root == NULL, 0))
    {
        node *root = aligned_alloc(32, sizeof(node));
        if (root == NULL)
        {
            perror("not enough memory\n");
            exit(-1);
        }
        node_init(root, true);
        root->keys[0] = key;
        memcpy(&root->children.values[0], value, sizeof(value_t));
        root->n = 1;
        tree->root = root;
        pthread_spin_unlock(&tree->write_lock);
        return;
    }
    else
    {
        key_cmp_t cmp_key = avx_broadcast(key);
        if (tree->root->n == ORDER - 1)
        {
            node *s = aligned_alloc(32, sizeof(node));
            if (s == NULL)
            {
                perror("not enough memory\n");
                exit(-1);
            }
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
            node_insert(&(s->children.next[i]), key, cmp_key, value, &s->children.next, &s->write_lock, tree);
            node *old_root = tree->root;
            // Change root
            tree->root = s;

            pthread_spin_unlock(&tree->write_lock);
            save_free(tree, old_root);
        }
        else
        {
            // IMPORTANT: as you (hello future simon) can maybe see, the unlock for the
            // bptree write lock is missing. The unlock happens within the node_insert method.
            node_insert(tree->root, key, cmp_key, value, &tree->root, &tree->write_lock, tree);
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
    pthread_cancel(tree->free_thread);
    close(tree->free_pipe[0]);
    close(tree->free_pipe[1]);
    pthread_spin_destroy(&tree->write_lock);
}