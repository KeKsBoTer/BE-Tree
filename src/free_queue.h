#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// garbage collection stuff

typedef struct __attribute__((__packed__)) garbage_msg
{
    enum __attribute__((__packed__))
    {
        MsgGet,
        MsgFree
    } type;
    uint64_t step;
    union
    {
        struct __attribute__((__packed__))
        {
            pthread_t id;
            enum __attribute__((__packed__)) get_type_e
            {
                GetStart,
                GetEnd
            } type;
        } get;
        void *memory;
    } msg;
} garbage_msg;

// linked list with largest step as fist element
typedef struct free_node
{
    uint64_t step;
    void *memory;
    struct free_node *next;
} free_node;

void free_node_insert(free_node *node, void *memory, uint64_t step);

// linked list with smallest step as fist element
typedef struct pq_node
{
    uint64_t step;
    pthread_t id;
    struct pq_node *next;
} pq_node;

void pq_node_insert(pq_node *node, pthread_t id, uint64_t step);
void pq_node_remove(pq_node *node, pthread_t id);
void pq_node_free(pq_node *node);

typedef struct pqueue
{
    pq_node *get_queue;
    free_node *free_queue;
} pqueue;

void pqueue_init(pqueue *queue);
void pqueue_get_start(pqueue *queue, pthread_t id, uint64_t step);
void pqueue_get_end(pqueue *queue, pthread_t id);
void pqueue_save_free(pqueue *queue, void *memory, uint64_t step);
void pqueue_free_before(pqueue *queue, uint64_t step);
void pqueue_free(pqueue *queue);