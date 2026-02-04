#include "ir_memory.h"
#include <string.h>


#define MIN_ALIGNMENT 8


static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}


static EsIRMemoryPool* create_pool(size_t size) {
    EsIRMemoryPool* pool = (EsIRMemoryPool*)ES_MALLOC(sizeof(EsIRMemoryPool));
    if (!pool) return NULL;
    
    pool->buffer = (char*)ES_MALLOC(size);
    if (!pool->buffer) {
        ES_FREE(pool);
        return NULL;
    }
    
    pool->size = size;
    pool->used = 0;
    pool->next = NULL;
    
    return pool;
}


static void destroy_pool(EsIRMemoryPool* pool) {
    if (!pool) return;
    
    if (pool->buffer) {
        ES_FREE(pool->buffer);
    }
    ES_FREE(pool);
}


EsIRMemoryArena* es_ir_arena_create(size_t pool_size) {
    EsIRMemoryArena* arena = (EsIRMemoryArena*)ES_MALLOC(sizeof(EsIRMemoryArena));
    if (!arena) return NULL;
    
    if (pool_size == 0) {
        pool_size = DEFAULT_POOL_SIZE;
    }
    
    arena->current_pool = create_pool(pool_size);
    if (!arena->current_pool) {
        ES_FREE(arena);
        return NULL;
    }
    
    arena->pool_size = pool_size;
    arena->total_allocated = 0;
    arena->pool_count = 1;
    
    return arena;
}


void es_ir_arena_destroy(EsIRMemoryArena* arena) {
    if (!arena) return;
    
    EsIRMemoryPool* pool = arena->current_pool;
    while (pool) {
        EsIRMemoryPool* next = pool->next;
        destroy_pool(pool);
        pool = next;
    }
    
    ES_FREE(arena);
}


void* es_ir_arena_alloc(EsIRMemoryArena* arena, size_t size) {
    if (!arena || size == 0) return NULL;
    
    
    size = align_size(size, MIN_ALIGNMENT);
    
    EsIRMemoryPool* pool = arena->current_pool;
    
    
    if (pool->used + size > pool->size) {
        
        EsIRMemoryPool* new_pool = create_pool(arena->pool_size > size ? arena->pool_size : size * 2);
        if (!new_pool) return NULL;
        
        new_pool->next = pool;
        arena->current_pool = new_pool;
        arena->pool_count++;
        pool = new_pool;
    }
    
    
    void* result = pool->buffer + pool->used;
    pool->used += size;
    arena->total_allocated += size;
    
    return result;
}


char* es_ir_arena_strdup(EsIRMemoryArena* arena, const char* str) {
    if (!arena || !str) return NULL;
    
    size_t len = strlen(str);
    char* result = (char*)es_ir_arena_alloc(arena, len + 1);
    if (!result) return NULL;
    
    memcpy(result, str, len + 1);
    return result;
}


void es_ir_arena_reset(EsIRMemoryArena* arena) {
    if (!arena) return;
    
    
    EsIRMemoryPool* pool = arena->current_pool;
    while (pool) {
        pool->used = 0;
        pool = pool->next;
    }
    
    arena->total_allocated = 0;
}


void es_ir_arena_get_stats(EsIRMemoryArena* arena, size_t* total_allocated, size_t* pool_count) {
    if (!arena) return;
    
    if (total_allocated) {
        *total_allocated = arena->total_allocated;
    }
    
    if (pool_count) {
        *pool_count = arena->pool_count;
    }
}