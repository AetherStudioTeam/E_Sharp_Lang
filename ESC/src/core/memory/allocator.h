#ifndef ES_MEMORY_H
#define ES_MEMORY_H

#include "../utils/logger.h"
#include "../platform/platform.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION es_mutex_t;
#else
#include <pthread.h>
typedef pthread_mutex_t es_mutex_t;
#endif


typedef enum {
    MEM_PROTECT_NONE = 0,
    MEM_PROTECT_READ = 1 << 0,
    MEM_PROTECT_WRITE = 1 << 1,
    MEM_PROTECT_EXECUTE = 1 << 2,
    MEM_PROTECT_ALL = MEM_PROTECT_READ | MEM_PROTECT_WRITE | MEM_PROTECT_EXECUTE
} MemoryProtectionFlags;


typedef struct MemoryBlock {
    void* user_ptr;           
    void* actual_ptr;         
    size_t user_size;         
    size_t actual_size;       
    uint32_t magic;           
    MemoryProtectionFlags protection;
    bool is_freed;            
    const char* file;         
    int line;                 
    struct MemoryBlock* next;
    struct MemoryBlock* prev;
} MemoryBlock;


typedef struct MemorySafetyManager {
    MemoryBlock* blocks;      
    size_t block_count;       
    es_mutex_t mutex;    
    bool poison_enabled;      
    bool canary_enabled;      
} MemorySafetyManager;


extern MemorySafetyManager* g_memory_safety;


void es_memory_safety_init(void);
void es_memory_safety_cleanup(void);


void* es_safe_malloc(size_t size, const char* file, int line);
void* es_safe_calloc(size_t count, size_t size, const char* file, int line);
void* es_safe_realloc(void* ptr, size_t size, const char* file, int line);
char* es_safe_strdup(const char* str, const char* file, int line);


void es_safe_free(void* ptr, const char* file, int line);


bool es_bounds_check(const void* array, size_t index, size_t element_size, size_t array_size);
bool es_buffer_check(const void* buffer, size_t offset, size_t size, size_t buffer_size);


bool es_memory_protect(void* ptr, MemoryProtectionFlags flags);
bool es_memory_unprotect(void* ptr);


void es_memory_poison(void* ptr, size_t size);
bool es_memory_is_poisoned(const void* ptr, size_t size);


void es_set_canary(MemoryBlock* block);
bool es_check_canary(const MemoryBlock* block);


bool es_memory_ptr_is_valid(const void* ptr);
bool es_ptr_is_freed(const void* ptr);
MemoryBlock* es_ptr_get_block(const void* ptr);


bool es_ptr_is_dangling(const void* ptr);
void es_ptr_track_free(void* ptr);


bool es_ptr_check_double_free(const void* ptr, const char* file, int line);


void es_memory_dump_blocks(void);
size_t es_memory_get_total_usage(void);
size_t es_memory_get_block_count(void);
void es_memory_dump_stats(void);


void es_memory_report_error(const char* error_type, const void* ptr, 
                           const char* file, int line, const char* format, ...);


bool es_memory_validate_heap(void);
bool es_memory_check_integrity(void);
void es_memory_scan_corruption(void);


#define ES_SAFE_MALLOC(size) es_safe_malloc(size, __FILE__, __LINE__)
#define ES_SAFE_CALLOC(count, size) es_safe_calloc(count, size, __FILE__, __LINE__)
#define ES_SAFE_REALLOC(ptr, size) es_safe_realloc(ptr, size, __FILE__, __LINE__)
#define ES_SAFE_STRDUP(str) es_safe_strdup(str, __FILE__, __LINE__)
#define ES_SAFE_FREE(ptr) es_safe_free(ptr, __FILE__, __LINE__)


#define ES_BOUNDS_CHECK(array, index, array_size) \
    es_bounds_check(array, index, sizeof(*(array)), array_size)

#define ES_BUFFER_CHECK(buffer, offset, size, buffer_size) \
    es_buffer_check(buffer, offset, size, buffer_size)


#define ES_ARRAY_GET(array, index, array_size) \
    (ES_BOUNDS_CHECK(array, index, array_size) ? &((array)[index]) : NULL)

#define ES_ARRAY_SET(array, index, value, array_size) \
    do { \
        if (ES_BOUNDS_CHECK(array, index, array_size)) { \
            (array)[index] = (value); \
        } else { \
            es_memory_report_error("ARRAY_BOUNDS", array, __FILE__, __LINE__, \
                                 "Index %zu out of bounds [0, %zu)", \
                                 (size_t)(index), (size_t)(array_size)); \
        } \
    } while(0)


#define ES_MEMORY_PTR_VALID(ptr) es_memory_ptr_is_valid(ptr)
#define ES_PTR_NOT_FREED(ptr) (!es_ptr_is_freed(ptr))
#define ES_PTR_NOT_DANGLING(ptr) (!es_ptr_is_dangling(ptr))


void es_memory_safety_enable_poison(bool enable);
void es_memory_safety_enable_canary(bool enable);
void es_memory_safety_set_strict_mode(bool strict);

#endif 