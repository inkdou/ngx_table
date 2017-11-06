#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_mq.h>
#include <ngx_shradix_tree.h>
#include <ngx_table.h>
#include <ngx_proc_task_cache.h>

static void *table_push(ngx_table_t *t);
int ngx_is_empty_table(ngx_table_t *t)
{
    return ((t == NULL) || t->nelts == 0);	
}

ngx_table_t *ngx_table_make(ngx_slab_pool_t *pool, int nelts, compare_func cfunc)
{
    ngx_table_t *t = (ngx_table_t *)ngx_slab_alloc(pool, sizeof(ngx_table_t));
    if(nelts < 1)
        nelts = 1;
    t->entries = ngx_slab_alloc(pool, nelts * sizeof(ngx_table_entry_t));
    t->elt_size = sizeof(ngx_table_entry_t);
    t->nelts = 0;
    t->nalloc = nelts;
    t->pool = pool;
    t->index_initialized = 0;
    t->comp = cfunc;
    return t;
}

static void *table_push(ngx_table_t *t)
{
    ngx_table_entry_t *tmp;

    if (t->nelts == t->nalloc) {
        int new_size = (t->nalloc <= 0) ? 1 : t->nalloc * 2;
        ngx_table_entry_t *new_data;

        tmp = t->entries;
        new_data = ngx_slab_alloc(t->pool, t->elt_size * new_size);

        memcpy(new_data, t->entries, t->nalloc * t->elt_size);
        ngx_slab_free(t->pool, tmp);

        t->entries = new_data;
        t->nalloc = new_size;
    }

    ++t->nelts;
    return t->entries + (t->nelts - 1);
}


void ngx_table_reindex(ngx_table_t *t)
{
    ngx_uint_t i;
    int hash;
    ngx_table_entry_t *next_elt = (ngx_table_entry_t *) t->entries;

    t->index_initialized = 0;
    for (i = 0; i < t->nelts; i++, next_elt++) {
        hash = TABLE_HASH(next_elt->key);
        t->index_last[hash] = i;
        if (!TABLE_INDEX_IS_INITIALIZED(t, hash)) {
            t->index_first[hash] = i;
            TABLE_SET_INDEX_INITIALIZED(t, hash);
        }
    }
}

void ngx_table_clear(ngx_table_t *t)
{
    t->nelts = 0;
    t->index_initialized = 0;
}

void * ngx_table_get(ngx_table_t *t, const char *key, const void *cvalue)
{
    ngx_table_entry_t *next_elt;
    ngx_table_entry_t *end_elt;
    uint32_t checksum;
    int hash;

    if (key == NULL) {
        return NULL;
    }

    hash = TABLE_HASH(key);
    if (!TABLE_INDEX_IS_INITIALIZED(t, hash)) {
        return NULL;
    }
    COMPUTE_KEY_CHECKSUM(key, checksum);
    next_elt = ((ngx_table_entry_t *) t->entries) + t->index_first[hash];;
    end_elt = ((ngx_table_entry_t *) t->entries) + t->index_last[hash];

    for (; next_elt <= end_elt; next_elt++) {
        if ((checksum == next_elt->key_checksum) &&
            !strcasecmp(next_elt->key, key)) {
            if(NULL != t->comp) {
                if(NGX_OK == t->comp(next_elt->value, cvalue))
                    return next_elt->value;
            }else {
                return next_elt->value;
            }
        }
    }

    return NULL;
}
void *ngx_table_set(ngx_table_t *t, const char *key,
    void *cvalue)
{
    ngx_table_entry_t *next_elt;
    ngx_table_entry_t *end_elt;
    uint32_t checksum;
    int hash;

    COMPUTE_KEY_CHECKSUM(key, checksum);
    hash = TABLE_HASH(key);
    if (!TABLE_INDEX_IS_INITIALIZED(t, hash)) {
        return NULL;
    }
    next_elt = ((ngx_table_entry_t *) t->entries) + t->index_first[hash];;
    end_elt = ((ngx_table_entry_t *) t->entries) + t->index_last[hash];

    for (; next_elt <= end_elt; next_elt++) {
        if ((checksum == next_elt->key_checksum) &&
            !strcasecmp(next_elt->key, key) && 
            (NGX_OK == t->comp(next_elt->value, cvalue))) {
            next_elt->value = cvalue;
            return next_elt->value;
        }
    }

    return NULL;
}

void ngx_table_add(ngx_table_t *t, const char *key,
    void *val)
{
    ngx_table_entry_t *entry;
    uint32_t checksum;
    int hash;

    hash = TABLE_HASH(key);
    t->index_last[hash] = t->nelts;
    if (!TABLE_INDEX_IS_INITIALIZED(t, hash)) {
        t->index_first[hash] = t->nelts;
        TABLE_SET_INDEX_INITIALIZED(t, hash);
    }
    COMPUTE_KEY_CHECKSUM(key, checksum);
    entry = (ngx_table_entry_t *) table_push(t);
    strncpy(entry->key, key, GEO_LEN);
    entry->value = val;
    entry->key_checksum = checksum;
}

void * ngx_table_del(ngx_table_t *t, const char *key, void *cvalue) {
    uint32_t checksum;
    int hash;
    int must_reindex = 0;
    ngx_table_entry_t *next_elt, *end_elt;
    ngx_table_entry_t *table_end;
    ngx_table_entry_t *dst_elt;
    ngx_table_entry_t *current_elt = NULL;
    void *value = NULL;
    printf("fdafd\n");

    hash = TABLE_HASH(key);
    if (!TABLE_INDEX_IS_INITIALIZED(t, hash)) {
        return NULL;
    }

    COMPUTE_KEY_CHECKSUM(key, checksum);
    next_elt = ((ngx_table_entry_t *) t->entries) + t->index_first[hash];;
    end_elt = ((ngx_table_entry_t *) t->entries) + t->index_last[hash];

    for (; next_elt <= end_elt; next_elt++) {
        printf("inkey=%s, key=%s\n",(char*)next_elt->key, key);
        if ((checksum == next_elt->key_checksum) &&
            !strcasecmp(next_elt->key, key)) {
            if(t->comp != NULL) {
                if(NGX_OK == t->comp(next_elt->value, cvalue)) {
                    table_end = ((ngx_table_entry_t *)t->entries) + t->nelts;
                    t->nelts--;
                    dst_elt = next_elt;
                    current_elt = dst_elt;
                    for(next_elt++; next_elt <= table_end; next_elt++) {
                        *dst_elt++ = *next_elt;
                        must_reindex = 1;
                    }
                }
            }else {
                table_end = ((ngx_table_entry_t *)t->entries) + t->nelts;
                t->nelts--;
                dst_elt = next_elt;
                current_elt = dst_elt;
                value = current_elt->value;
                for(next_elt++; next_elt <= table_end; next_elt++) {
                    *dst_elt++ = *next_elt;
                    must_reindex = 1;
                }
            }
        }
    }

    if(must_reindex) {
        ngx_table_reindex(t);
    }

    return value;
}

