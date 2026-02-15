#ifndef ES_SMART_PTR_H
#define ES_SMART_PTR_H

#include "../utils/logger.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>


typedef enum {
    PTR_UNIQUE,    
    PTR_SHARED,    
    PTR_WEAK       
} SmartPtrType;


typedef struct SharedControlBlock {
    atomic_int ref_count;      
    atomic_int weak_count;     
    void (*deleter)(void*);    
    void* data;                
    bool is_array;             
} SharedControlBlock;


typedef struct SmartPtr {
    void* ptr;                    
    SharedControlBlock* control;  
    SmartPtrType type;            
    const char* type_name;        
} SmartPtr;


SmartPtr* es_make_shared(const char* type_name, size_t size);
SmartPtr* es_make_shared_array(const char* type_name, size_t count, size_t element_size);
SmartPtr* es_make_unique(const char* type_name, size_t size);
SmartPtr* es_make_unique_array(const char* type_name, size_t count, size_t element_size);


void es_shared_acquire(SmartPtr* ptr);
void es_shared_release(SmartPtr* ptr);
int es_shared_count(const SmartPtr* ptr);


void* es_ptr_get(const SmartPtr* ptr);
void* es_ptr_get_checked(const SmartPtr* ptr, const char* file, int line);


bool es_ptr_is_valid(const SmartPtr* ptr);
bool es_ptr_is_null(const SmartPtr* ptr);


void es_ptr_destroy(SmartPtr* ptr);
void es_ptr_reset(SmartPtr* ptr, void* new_ptr, size_t size);


SmartPtr* es_make_weak(const SmartPtr* shared_ptr);
SmartPtr* es_weak_lock(const SmartPtr* weak_ptr);
bool es_weak_expired(const SmartPtr* weak_ptr);


SmartPtr* es_make_shared_pooled(const char* type_name, size_t size, const char* pool_name);


void es_ptr_dump_stats(void);
size_t es_ptr_get_total_shared_count(void);
size_t es_ptr_get_total_unique_count(void);


#define ES_MAKE_SHARED(type) es_make_shared(#type, sizeof(type))
#define ES_MAKE_SHARED_ARRAY(type, count) es_make_shared_array(#type, count, sizeof(type))
#define ES_MAKE_UNIQUE(type) es_make_unique(#type, sizeof(type))
#define ES_MAKE_UNIQUE_ARRAY(type, count) es_make_unique_array(#type, count, sizeof(type))


#define ES_PTR_GET(ptr) es_ptr_get_checked(ptr, __FILE__, __LINE__)
#define ES_PTR_VALID(ptr) (es_ptr_is_valid(ptr) && !es_ptr_is_null(ptr))


#define ES_PTR_AUTO_DESTROY __attribute__((cleanup(es_ptr_destroy)))

#endif 