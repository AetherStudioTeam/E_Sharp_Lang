#define _POSIX_C_SOURCE 200809L
#include "allocator.h"
#include "../utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>


extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern size_t strlen(const char *s);


#define MEMORY_MAGIC 0xDEADBEEF
#define CANARY_VALUE 0xCAFEBABE
#define POISON_VALUE 0xDD
#define FREED_MAGIC  0xFREEDEAD


MemorySafetyManager* g_memory_safety = NULL;


static void add_memory_block(MemoryBlock* block) {
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    block->next = g_memory_safety->blocks;
    block->prev = NULL;
    if (g_memory_safety->blocks) {
        g_memory_safety->blocks->prev = block;
    }
    g_memory_safety->blocks = block;
    g_memory_safety->block_count++;
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
}

static void remove_memory_block(MemoryBlock* block) {
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        g_memory_safety->blocks = block->next;
    }
    
    if (block->next) {
        block->next->prev = block->prev;
    }
    
    g_memory_safety->block_count--;
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
}


void es_memory_safety_init(void) {
    if (g_memory_safety) return;
    
    g_memory_safety = malloc(sizeof(MemorySafetyManager));
    if (!g_memory_safety) {
        exit(1);
    }
    
    g_memory_safety->blocks = NULL;
    g_memory_safety->block_count = 0;
    g_memory_safety->poison_enabled = true;
    g_memory_safety->canary_enabled = true;
    
#ifdef _WIN32
    InitializeCriticalSection(&g_memory_safety->mutex);
#else
    if (pthread_mutex_init(&g_memory_safety->mutex, NULL) != 0) {
        free(g_memory_safety);
        g_memory_safety = NULL;
        exit(1);
    }
#endif

}


void es_memory_safety_cleanup(void) {
    if (!g_memory_safety) return;
    
    
    
    if (g_memory_safety->block_count > 0) {
        es_memory_dump_blocks();
    }
    
    
    MemoryBlock* current = g_memory_safety->blocks;
    while (current) {
        MemoryBlock* next = current->next;
        
        if (!current->is_freed) {
        }
        
        free(current->actual_ptr);
        free(current);
        current = next;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_destroy(&g_memory_safety->mutex);
#endif
    free(g_memory_safety);
    g_memory_safety = NULL;
    
}


void* es_safe_malloc(size_t size, const char* file, int line) {
    if (!g_memory_safety) {
        es_memory_safety_init();
    }
    
    if (size == 0) return NULL;
    
    
    size_t actual_size = size;
    if (g_memory_safety->canary_enabled) {
        actual_size += 2 * sizeof(uint32_t);  
    }
    
    
    void* actual_ptr = malloc(actual_size);
    if (!actual_ptr) {
        es_memory_report_error("MALLOC_FAILED", NULL, file, line, 
                            "Failed to allocate %zu bytes", size);
        return NULL;
    }
    
    
    MemoryBlock* block = malloc(sizeof(MemoryBlock));
    if (!block) {
        free(actual_ptr);
        return NULL;
    }
    
    
    void* user_ptr = actual_ptr;
    if (g_memory_safety->canary_enabled) {
        user_ptr = (char*)actual_ptr + sizeof(uint32_t);  
    }
    
    
    block->actual_ptr = actual_ptr;
    block->user_ptr = user_ptr;
    block->user_size = size;
    block->actual_size = actual_size;
    block->magic = MEMORY_MAGIC;
    block->protection = MEM_PROTECT_READ | MEM_PROTECT_WRITE;
    block->is_freed = false;
    block->file = file;
    block->line = line;
    
    
    if (g_memory_safety->canary_enabled) {
        es_set_canary(block);
    }
    
    add_memory_block(block);

    
    return user_ptr;
}


void* es_safe_calloc(size_t count, size_t size, const char* file, int line) {
    if (count == 0 || size == 0) return NULL;
    
    void* ptr = es_safe_malloc(count * size, file, line);
    if (ptr) {
        memset(ptr, 0, count * size);
    }
    return ptr;
}


void* es_safe_realloc(void* old_ptr, size_t new_size, const char* file, int line) {
    if (!old_ptr) {
        return es_safe_malloc(new_size, file, line);
    }
    
    if (new_size == 0) {
        es_safe_free(old_ptr, file, line);
        return NULL;
    }
    
    
    MemoryBlock* old_block = es_ptr_get_block(old_ptr);
    if (!old_block) {
        es_memory_report_error("INVALID_REALLOC", old_ptr, file, line,
                            "Attempt to realloc untracked pointer %p", old_ptr);
        return NULL;
    }
    
    
    void* new_ptr = es_safe_malloc(new_size, file, line);
    if (!new_ptr) {
        return NULL;
    }
    
    
    size_t copy_size = (old_block->user_size < new_size) ? old_block->user_size : new_size;
    memcpy(new_ptr, old_ptr, copy_size);
    
    
    if (new_size > copy_size) {
        memset((char*)new_ptr + copy_size, 0, new_size - copy_size);
    }
    
    
    es_safe_free(old_ptr, file, line);
    
    return new_ptr;
}


char* es_safe_strdup(const char* str, const char* file, int line) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* copy = (char*)es_safe_malloc(len + 1, file, line);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}


void es_safe_free(void* ptr, const char* file, int line) {
    if (!ptr) return;
    
    
    if (es_ptr_check_double_free(ptr, file, line)) {
        return;
    }
    
    
    MemoryBlock* block = es_ptr_get_block(ptr);
    if (!block) {
        es_memory_report_error("INVALID_FREE", ptr, file, line,
                            "Attempt to free untracked pointer %p", ptr);
        return;
    }
    
    
    if (!es_check_canary(block)) {
        es_memory_report_error("CANARY_CORRUPTED", ptr, file, line,
                            "Memory canary corrupted for block %p", ptr);
    }
    
    
    if (es_memory_is_poisoned(ptr, block->user_size)) {
        es_memory_report_error("USE_AFTER_FREE", ptr, file, line,
                            "Use after free detected for block %p", ptr);
    }
    
    
    block->is_freed = true;
    
    
    if (g_memory_safety->poison_enabled) {
        es_memory_poison(ptr, block->user_size);
    }
    
    
    remove_memory_block(block);

    
    
    free(block->actual_ptr);
    free(block);
}


bool es_bounds_check(const void* array, size_t index, size_t element_size, size_t array_size) {
    if (!array || element_size == 0) return false;
    
    if (index >= array_size) {
        return false;
    }
    
    return true;
}


bool es_buffer_check(const void* buffer, size_t offset, size_t size, size_t buffer_size) {
    if (!buffer || size == 0) return false;
    
    if (offset + size > buffer_size) {
        return false;
    }
    
    return true;
}


bool es_ptr_check_double_free(const void* ptr, const char* file, int line) {
    if (!ptr) return false;
    
    MemoryBlock* block = es_ptr_get_block(ptr);
    if (block && block->is_freed) {
        es_memory_report_error("DOUBLE_FREE", ptr, file, line,
                            "Double free detected for block %p", ptr);
        return true;
    }
    
    return false;
}


MemoryBlock* es_ptr_get_block(const void* ptr) {
    if (!g_memory_safety || !ptr) return NULL;
    
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    MemoryBlock* current = g_memory_safety->blocks;
    while (current) {
        if (current->user_ptr == ptr) {
#ifdef _WIN32
            LeaveCriticalSection(&g_memory_safety->mutex);
#else
            pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
            return current;
        }
        current = current->next;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
    return NULL;
}


bool es_memory_ptr_is_valid(const void* ptr) {
    if (!ptr) return false;
    
    MemoryBlock* block = es_ptr_get_block(ptr);
    if (!block) return false;
    
    
    if (block->magic != MEMORY_MAGIC) return false;
    if (block->is_freed) return false;
    if (!es_check_canary(block)) return false;
    
    return true;
}


void es_set_canary(MemoryBlock* block) {
    if (!block || !g_memory_safety->canary_enabled) return;
    
    uint32_t* front_canary = (uint32_t*)block->actual_ptr;
    uint32_t* back_canary = (uint32_t*)((char*)block->user_ptr + block->user_size);
    
    *front_canary = CANARY_VALUE;
    *back_canary = CANARY_VALUE;
}


bool es_check_canary(const MemoryBlock* block) {
    if (!block || !g_memory_safety->canary_enabled) return true;
    
    uint32_t front_canary = *(uint32_t*)block->actual_ptr;
    uint32_t back_canary = *(uint32_t*)((char*)block->user_ptr + block->user_size);
    
    return front_canary == CANARY_VALUE && back_canary == CANARY_VALUE;
}


void es_memory_poison(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    memset(ptr, POISON_VALUE, size);
}


bool es_memory_is_poisoned(const void* ptr, size_t size) {
    if (!ptr || size == 0) return false;
    
    const unsigned char* bytes = (const unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != POISON_VALUE) return false;
    }
    return true;
}


void es_memory_report_error(const char* error_type, const void* ptr, 
                           const char* file, int line, const char* format, ...) {
    fprintf(stderr, "[%s] %s:%d", error_type, file, line);
    if (ptr) fprintf(stderr, " ptr=%p", ptr);
    
    va_list args;
    va_start(args, format);
    fputc(' ', stderr);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    
    #ifdef ES_DEBUG
    fflush(stderr);
    __debugbreak();
    #endif
}


void es_memory_dump_blocks(void) {
    if (!g_memory_safety) return;
    
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    printf("\n=== Memory Blocks Dump ===\n");
    printf("Total blocks: %zu\n", g_memory_safety->block_count);
    
    MemoryBlock* current = g_memory_safety->blocks;
    int count = 0;
    while (current) {
        printf("Block %d:\n", count++);
        printf("  User ptr: %p\n", current->user_ptr);
        printf("  Actual ptr: %p\n", current->actual_ptr);
        printf("  Size: %zu bytes\n", current->user_size);
        printf("  Status: %s\n", current->is_freed ? "FREED" : "ACTIVE");
        printf("  Location: %s:%d\n", current->file, current->line);
        printf("  Magic: 0x%08X\n", current->magic);
        
        if (g_memory_safety->canary_enabled) {
            printf("  Canary: %s\n", es_check_canary(current) ? "OK" : "CORRUPTED");
        }
        
        current = current->next;
    }
    
    printf("=========================\n");
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
}


size_t es_memory_get_total_usage(void) {
    if (!g_memory_safety) return 0;
    
    size_t total = 0;
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    MemoryBlock* current = g_memory_safety->blocks;
    while (current) {
        if (!current->is_freed) {
            total += current->user_size;
        }
        current = current->next;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
    return total;
}

size_t es_memory_get_block_count(void) {
    return g_memory_safety ? g_memory_safety->block_count : 0;
}


void es_memory_dump_stats(void) {
    printf("\n=== Memory Safety Statistics ===\n");
    printf("Active blocks: %zu\n", es_memory_get_block_count());
    printf("Total usage: %zu bytes\n", es_memory_get_total_usage());
    printf("Poison detection: %s\n", 
           g_memory_safety->poison_enabled ? "enabled" : "disabled");
    printf("Canary protection: %s\n", 
           g_memory_safety->canary_enabled ? "enabled" : "disabled");
    printf("=================================\n");
}