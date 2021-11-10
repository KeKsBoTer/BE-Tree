#pragma once
#include <stdio.h>
#include <stdatomic.h>
#include "free_queue.h"

#define MSG_STACK_SIZE 65536

struct lstack_node
{
    void *value;
    struct lstack_node *next;
};

struct lstack_head
{
    uintptr_t aba;
    struct lstack_node *node;
};

typedef struct
{
    struct lstack_node *node_buffer;
    _Atomic struct lstack_head head, free;
    _Atomic size_t size;
} lstack_t;

int lstack_init(lstack_t *lstack, size_t max_size);

void lstack_free(lstack_t *lstack);
int lstack_push(lstack_t *lstack, void *value);
void *lstack_pop(lstack_t *lstack);