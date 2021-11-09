#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "free_queue.h"

// garbage collection stuff

void pq_node_insert(pq_node *node, pthread_t id, uint64_t step)
{
    pq_node *new = malloc(sizeof(pq_node));
    new->step = step;
    new->id = id;
    while (true)
    {
        if (node->next == NULL)
        {
            node->next = new;
            new->next = NULL;
            return;
        }
        if (node->next->step > step)
        {
            new->next = node->next;
            node->next = new;
            return;
        }
        node = node->next;
    }
}

void pq_node_remove(pq_node *node, pthread_t id)
{
    while (true)
    {
        if (node->next == NULL)
        {
            return;
        }
        if (node->next->id == id)
        {
            node->next = node->next->next;
            return;
        }
        node = node->next;
    }
}

void pq_node_free(pq_node *node)
{
    do
    {
        pq_node *next = node->next;
        free(node);
        node = next;
    } while (node != NULL);
}

void pqueue_init(pqueue *queue)
{
    queue->get_queue = NULL;
    queue->free_queue = NULL;
}

void pqueue_get_start(pqueue *queue, pthread_t id, uint64_t step)
{
    if (queue->get_queue == NULL)
    {
        queue->get_queue = malloc(sizeof(pq_node));
        queue->get_queue->step = step;
        queue->get_queue->id = id;
        queue->get_queue->next = NULL;
    }
    else
    {
        if (step < queue->get_queue->step)
        {
            // replace first element if it smaller then root
            pq_node *new = malloc(sizeof(pq_node));
            new->step = step;
            new->id = id;
            new->next = queue->get_queue;
            queue->get_queue = new;
        }
        else
        {
            pq_node_insert(queue->get_queue, id, step);
        }
    }
}

void pqueue_get_end(pqueue *queue, pthread_t id)
{
    uint64_t earlierst = 0;
    if (queue->get_queue != NULL)
    {
        if (queue->get_queue->id == id)
        {
            // special case: remove first element in queue
            pq_node *root = queue->get_queue;
            queue->get_queue = queue->get_queue->next;
            free(root);
            if (queue->get_queue != NULL)
            {
                // earlierst started get request is first in queue
                earlierst = queue->get_queue->step;
            }
        }
        else
        {
            pq_node_remove(queue->get_queue, id);
            earlierst = queue->get_queue->step;
        }
    }
    pqueue_free_before(queue, earlierst);
}

void free_node_free(free_node *node)
{
    do
    {
        free_node *next = node->next;
        free(node->memory);
        free(node);
        node = next;
    } while (node != NULL);
}

void pqueue_free(pqueue *queue)
{
    if (queue->get_queue != NULL)
    {
        pq_node_free(queue->get_queue);
    }
    if (queue->free_queue != NULL)
    {
        free_node_free(queue->free_queue);
    }
}

void pqueue_save_free(pqueue *queue, void *memory, uint64_t step)
{
    if (queue->get_queue == NULL)
    {
        // no reason to wait if there is no ongoing get operation
        free(memory);
    }
    else
    {
        if (queue->free_queue == NULL)
        {
            queue->free_queue = malloc(sizeof(free_node));
            queue->free_queue->memory = memory;
            queue->free_queue->step = step;
            queue->free_queue->next = NULL;
        }
        else
        {
            if (step > queue->free_queue->step)
            {
                // replace first element if its step is larger then root
                free_node *new = malloc(sizeof(free_node));
                new->step = step;
                new->memory = memory;
                new->next = queue->free_queue;
                queue->free_queue = new;
            }
            else
            {
                free_node_insert(queue->free_queue, memory, step);
            }
        }
    }
}

void free_node_insert(free_node *node, void *memory, uint64_t step)
{
    free_node *new = malloc(sizeof(free_node));
    new->step = step;
    new->memory = memory;
    while (true)
    {
        if (node->next == NULL)
        {
            node->next = new;
            new->next = NULL;
            return;
        }
        if (node->next->step < step)
        {
            new->next = node->next;
            node->next = new;
            return;
        }
        node = node->next;
    }
}

// free all memory that was requested to be freed before
// a given step
void pqueue_free_before(pqueue *queue, uint64_t step)
{
    if (queue->free_queue != NULL)
    {
        if (queue->free_queue->step < step)
        {
            free_node_free(queue->free_queue);
            queue->free_queue = NULL;
        }
        else
        {
            free_node *node = queue->free_queue;
            while (node->next != NULL)
            {
                if (node->next->step < step)
                {
                    free_node_free(node->next);
                    node->next = NULL;
                    return;
                }
                node = node->next;
            }
        }
    }
}