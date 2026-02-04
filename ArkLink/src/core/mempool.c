

#include "internal/mempool.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>



struct Mempool {
    MempoolBlock* blocks;
    MempoolBlock* current;
    size_t total_allocated;
    size_t total_used;
    size_t block_count;
};

Mempool* mempool_create(void) {
    Mempool* pool = (Mempool*)malloc(sizeof(Mempool));
    if (!pool) return NULL;
    
    memset(pool, 0, sizeof(Mempool));
    
    MempoolBlock* block = (MempoolBlock*)malloc(sizeof(MempoolBlock));
    if (!block) {
        free(pool);
        return NULL;
    }
    
    block->data = (uint8_t*)malloc(MEMPOOL_BLOCK_SIZE);
    if (!block->data) {
        free(block);
        free(pool);
        return NULL;
    }
    
    block->next = NULL;
    block->used = 0;
    block->capacity = MEMPOOL_BLOCK_SIZE;
    
    pool->blocks = block;
    pool->current = block;
    pool->block_count = 1;
    
    return pool;
}

void mempool_destroy(Mempool* pool) {
    if (!pool) return;
    
    MempoolBlock* block = pool->blocks;
    while (block) {
        MempoolBlock* next = block->next;
        free(block->data);
        free(block);
        block = next;
    }
    
    free(pool);
}

void* mempool_alloc(Mempool* pool, size_t size) {
    if (!pool || size == 0) return NULL;
    
    
    size = (size + 7) & ~7;
    
    
    if (size > MEMPOOL_BLOCK_SIZE / 4) {
        
        MempoolBlock* block = (MempoolBlock*)malloc(sizeof(MempoolBlock));
        if (!block) return NULL;
        
        block->data = (uint8_t*)malloc(size);
        if (!block->data) {
            free(block);
            return NULL;
        }
        
        
        block->next = pool->blocks;
        block->used = size;
        block->capacity = size;
        pool->blocks = block;
        pool->total_allocated += size;
        pool->total_used += size;
        
        return block->data;
    }
    
    
    if (pool->current->used + size > pool->current->capacity) {
        
        MempoolBlock* block = (MempoolBlock*)malloc(sizeof(MempoolBlock));
        if (!block) return NULL;
        
        block->data = (uint8_t*)malloc(MEMPOOL_BLOCK_SIZE);
        if (!block->data) {
            free(block);
            return NULL;
        }
        
        block->next = NULL;
        block->used = 0;
        block->capacity = MEMPOOL_BLOCK_SIZE;
        
        pool->current->next = block;
        pool->current = block;
        pool->block_count++;
    }
    
    void* ptr = pool->current->data + pool->current->used;
    pool->current->used += size;
    pool->total_allocated += size;
    pool->total_used += size;
    
    return ptr;
}

void* mempool_alloc_aligned(Mempool* pool, size_t size, size_t align) {
    if (!pool || size == 0 || align == 0) return NULL;
    
    
    if ((align & (align - 1)) != 0) return NULL;
    
    
    uintptr_t current = (uintptr_t)(pool->current->data + pool->current->used);
    uintptr_t aligned = (current + align - 1) & ~(align - 1);
    size_t padding = aligned - current;
    
    
    if (padding > 0) {
        mempool_alloc(pool, padding);
    }
    
    return mempool_alloc(pool, size);
}

void mempool_reset(Mempool* pool) {
    if (!pool) return;
    
    MempoolBlock* block = pool->blocks;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    
    pool->current = pool->blocks;
    pool->total_used = 0;
}

void mempool_stats(Mempool* pool, size_t* total, size_t* used, size_t* blocks) {
    if (!pool) {
        if (total) *total = 0;
        if (used) *used = 0;
        if (blocks) *blocks = 0;
        return;
    }
    
    if (total) *total = pool->total_allocated;
    if (used) *used = pool->total_used;
    if (blocks) *blocks = pool->block_count;
}

void* mempool_get_base(Mempool* pool) {
    if (!pool || !pool->blocks) return NULL;
    return pool->blocks->data;
}



typedef struct ObjHeader {
    struct ObjHeader* next;
} ObjHeader;

struct ObjPool {
    void* free_list;
    size_t obj_size;
    size_t objs_per_block;
    Mempool* pool;
    size_t allocated_count;
    size_t freed_count;
};

ObjPool* objpool_create(size_t obj_size, size_t objs_per_block) {
    if (obj_size < sizeof(void*)) obj_size = sizeof(void*);
    
    ObjPool* pool = (ObjPool*)calloc(1, sizeof(ObjPool));
    if (!pool) return NULL;
    
    pool->obj_size = obj_size;
    pool->objs_per_block = objs_per_block > 0 ? objs_per_block : 64;
    pool->free_list = NULL;
    pool->pool = mempool_create();
    
    if (!pool->pool) {
        free(pool);
        return NULL;
    }
    
    return pool;
}

void objpool_destroy(ObjPool* pool) {
    if (!pool) return;
    mempool_destroy(pool->pool);
    free(pool);
}

void* objpool_alloc(ObjPool* pool) {
    if (!pool) return NULL;
    
    
    if (pool->free_list) {
        ObjHeader* obj = (ObjHeader*)pool->free_list;
        pool->free_list = obj->next;
        pool->allocated_count++;
        return obj;
    }
    
    
    void* obj = mempool_alloc(pool->pool, pool->obj_size);
    if (obj) {
        pool->allocated_count++;
    }
    return obj;
}

void objpool_free(ObjPool* pool, void* obj) {
    if (!pool || !obj) return;
    
    
    ObjHeader* header = (ObjHeader*)obj;
    header->next = (ObjHeader*)pool->free_list;
    pool->free_list = header;
    pool->freed_count++;
}

void objpool_stats(ObjPool* pool, size_t* allocated, size_t* freed) {
    if (!pool) {
        if (allocated) *allocated = 0;
        if (freed) *freed = 0;
        return;
    }
    
    if (allocated) *allocated = pool->allocated_count;
    if (freed) *freed = pool->freed_count;
}




typedef struct StringEntry {
    struct StringEntry* next;
    uint32_t hash;
    char* str;
} StringEntry;

struct StringPool {
    StringEntry** buckets;          
    size_t bucket_count;            
    size_t string_count;            
    size_t total_string_bytes;      
};


static uint32_t fnv1a_hash(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

StringPool* stringpool_create(size_t initial_capacity) {
    StringPool* pool = (StringPool*)calloc(1, sizeof(StringPool));
    if (!pool) return NULL;
    
    pool->bucket_count = initial_capacity > 16 ? initial_capacity : 16;
    pool->buckets = (StringEntry**)calloc(pool->bucket_count, sizeof(StringEntry*));
    
    if (!pool->buckets) {
        free(pool);
        return NULL;
    }
    
    return pool;
}

void stringpool_destroy(StringPool* pool) {
    if (!pool) return;
    
    
    for (size_t i = 0; i < pool->bucket_count; i++) {
        StringEntry* entry = pool->buckets[i];
        while (entry) {
            StringEntry* next = entry->next;
            free(entry->str);
            free(entry);
            entry = next;
        }
    }
    
    free(pool->buckets);
    free(pool);
}

const char* stringpool_intern(StringPool* pool, const char* str) {
    if (!pool || !str) return NULL;
    
    
    const char* existing = stringpool_find(pool, str);
    if (existing != NULL) {
        return existing;
    }
    
    
    uint32_t hash = fnv1a_hash(str);
    size_t idx = hash & (pool->bucket_count - 1);
    
    
    StringEntry* entry = (StringEntry*)malloc(sizeof(StringEntry));
    if (!entry) return NULL;
    
    size_t len = strlen(str) + 1;
    entry->str = (char*)malloc(len);
    if (!entry->str) {
        free(entry);
        return NULL;
    }
    
    memcpy(entry->str, str, len);
    entry->hash = hash;
    entry->next = pool->buckets[idx];
    pool->buckets[idx] = entry;
    
    pool->string_count++;
    pool->total_string_bytes += len;
    
    return entry->str;
}

const char* stringpool_find(StringPool* pool, const char* str) {
    if (!pool || !str || pool->string_count == 0) return NULL;
    
    uint32_t hash = fnv1a_hash(str);
    size_t idx = hash & (pool->bucket_count - 1);
    
    StringEntry* entry = pool->buckets[idx];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->str, str) == 0) {
            return entry->str;
        }
        entry = entry->next;
    }
    
    return NULL;
}

void stringpool_stats(StringPool* pool, size_t* count, size_t* total_bytes, size_t* hash_capacity) {
    if (!pool) {
        if (count) *count = 0;
        if (total_bytes) *total_bytes = 0;
        if (hash_capacity) *hash_capacity = 0;
        return;
    }
    
    if (count) *count = pool->string_count;
    if (total_bytes) *total_bytes = pool->total_string_bytes;
    if (hash_capacity) *hash_capacity = pool->bucket_count;
}
