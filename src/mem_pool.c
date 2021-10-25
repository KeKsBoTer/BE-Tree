#include <unistd.h>
#include <stdlib.h>
#include "mem_pool.h"

void value_pool_grow(value_pool *pool)
{
    pool->num_pages++;
    pool->pages = realloc(pool->pages, sizeof(value_t *) * pool->num_pages);
    pool->pages[pool->num_pages - 1] = malloc(sizeof(value_t[pool->elm_size]) * pool->values_per_page);
    pool->n = pool->num_pages * pool->values_per_page;
    pool->i = 0;
}

void value_pool_init(value_pool *pool, int elm_size)
{
    pool->num_pages = 0;
    pool->elm_size = elm_size;
    pool->values_per_page = sysconf(_SC_PAGESIZE) / sizeof(value_t[elm_size]);
    value_pool_grow(pool);
}

void value_pool_free(value_pool *pool)
{
    for (int i = 0; i < pool->num_pages; i++)
        free(pool->pages[i]);
    free(pool->pages);
}

value_t *value_pool_alloc(value_pool *pool)
{
    if (pool->i == pool->values_per_page)
    {
        value_pool_grow(pool);
    }

    value_t *last_page = pool->pages[pool->num_pages - 1];
    value_t *v = last_page + pool->i * pool->elm_size;
    pool->i++;
    return v;
}