


#ifndef ArkLink_MEMPOOL_H
#define ArkLink_MEMPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#define MEMPOOL_BLOCK_SIZE (64 * 1024)


typedef struct MempoolBlock {
    struct MempoolBlock* next;
    uint8_t* data;
    size_t used;
    size_t capacity;
} MempoolBlock;


typedef struct {
    MempoolBlock* blocks;           
    MempoolBlock* current;          
    size_t total_allocated;         
    size_t total_used;              
    size_t block_count;             
} Mempool;


typedef struct {
    void* free_list;                
    size_t obj_size;                
    size_t objs_per_block;          
    Mempool* pool;                  
    size_t allocated_count;         
    size_t freed_count;             
} ObjPool;


typedef struct StringPool StringPool;




Mempool* mempool_create(void);


void mempool_destroy(Mempool* pool);


void* mempool_alloc(Mempool* pool, size_t size);


void* mempool_alloc_aligned(Mempool* pool, size_t size, size_t align);


void mempool_reset(Mempool* pool);


void mempool_stats(Mempool* pool, size_t* total, size_t* used, size_t* blocks);


void* mempool_get_base(Mempool* pool);




ObjPool* objpool_create(size_t obj_size, size_t objs_per_block);


void objpool_destroy(ObjPool* pool);


void* objpool_alloc(ObjPool* pool);


void objpool_free(ObjPool* pool, void* obj);


void objpool_stats(ObjPool* pool, size_t* allocated, size_t* freed);




StringPool* stringpool_create(size_t initial_capacity);


void stringpool_destroy(StringPool* pool);



const char* stringpool_intern(StringPool* pool, const char* str);


const char* stringpool_find(StringPool* pool, const char* str);


void stringpool_stats(StringPool* pool, size_t* count, size_t* total_bytes, size_t* hash_capacity);

#endif 
