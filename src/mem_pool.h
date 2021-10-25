typedef u_int64_t value_t;

typedef struct value_pool
{
    int n;
    int i;
    int num_pages;
    int values_per_page;
    int elm_size;
    void **pages;
} value_pool;

void value_pool_grow(value_pool *pool);
void value_pool_init(value_pool *pool, int elm_size);

void value_pool_free(value_pool *pool);

value_t *value_pool_alloc(value_pool *pool);