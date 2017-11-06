#ifndef _NGX_TABLE_H_INCLUDED_
#define _NGX_TABLE_H_INCLUDED_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define CASE_MASK 0xbfbfbfbf
#define GEO_LEN  60

#define TABLE_HASH_SIZE 32
#define TABLE_INDEX_MASK 0x1f
#define TABLE_HASH(key)  (TABLE_INDEX_MASK & *(unsigned char *)(key))
#define TABLE_INDEX_IS_INITIALIZED(t, i) ((t)->index_initialized & (1 << (i)))
#define TABLE_SET_INDEX_INITIALIZED(t, i) ((t)->index_initialized |= (1 << (i)))

typedef int (*compare_func)(const void *, const void *);
#define COMPUTE_KEY_CHECKSUM(key, checksum)    \
{                                              \
	const char *k = (key);                     \
	uint32_t c = (uint32_t)*k;         \
	(checksum) = c;                            \
	(checksum) <<= 8;                          \
	if (c) {                                   \
		c = (uint32_t)*++k;                \
		checksum |= c;                         \
	}                                          \
	(checksum) <<= 8;                          \
	if (c) {                                   \
		c = (uint32_t)*++k;                \
		checksum |= c;                         \
	}                                          \
	(checksum) <<= 8;                          \
	if (c) {                                   \
		c = (uint32_t)*++k;                \
		checksum |= c;                         \
	}                                          \
	checksum &= CASE_MASK;                     \
}

typedef struct {
	char key[GEO_LEN];
	void *value;
	const char *attr;
	int hash;
	uint32_t key_checksum;
}ngx_table_entry_t;


typedef struct {
	ngx_table_entry_t *entries;
    ngx_slab_pool_t *pool;
	int index_first[TABLE_HASH_SIZE];
	int index_last[TABLE_HASH_SIZE];
	ngx_uint_t nelts;
	int elt_size;
	ngx_uint_t nalloc;
	int index_initialized;
    compare_func comp;
}ngx_table_t;

int ngx_is_empty_table(ngx_table_t *t);
void ngx_table_reindex(ngx_table_t *t);
ngx_table_t *ngx_table_make(ngx_slab_pool_t *pool, int nelts, compare_func cfunc);
void ngx_table_clear(ngx_table_t *t);
void * ngx_table_get(ngx_table_t *t, const char *key, const void *cvalue);
void ngx_table_add(ngx_table_t *t, const char *key, void *cvalue);
void * ngx_table_del(ngx_table_t *t, const char *key, void *cvalue);
void *ngx_table_set(ngx_table_t *t, const char *key, void *cvalue);

#endif
