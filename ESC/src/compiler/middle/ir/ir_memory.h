#ifndef ES_IR_MEMORY_H
#define ES_IR_MEMORY_H

#include "../../../core/utils/es_common.h"


#define DEFAULT_POOL_SIZE (4 * 1024)


typedef struct EsIRMemoryPool {
    char* buffer;           
    size_t size;            
    size_t used;            
    struct EsIRMemoryPool* next;  
} EsIRMemoryPool;


typedef struct EsIRMemoryArena {
    EsIRMemoryPool* current_pool;  
    size_t pool_size;               
    size_t total_allocated;         
    size_t pool_count;              
} EsIRMemoryArena;


EsIRMemoryArena* es_ir_arena_create(size_t pool_size);


void es_ir_arena_destroy(EsIRMemoryArena* arena);


void* es_ir_arena_alloc(EsIRMemoryArena* arena, size_t size);


char* es_ir_arena_strdup(EsIRMemoryArena* arena, const char* str);


void es_ir_arena_reset(EsIRMemoryArena* arena);


void es_ir_arena_get_stats(EsIRMemoryArena* arena, size_t* total_allocated, size_t* pool_count);

#endif 