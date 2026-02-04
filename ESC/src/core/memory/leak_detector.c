#include "leak_detector.h"
#include "../utils/es_common.h"
#include "../utils/es_string.h"
#include "../utils/logger.h"
#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


extern char *strrchr(const char *s, int c);
extern size_t strlen(const char *s);


MemoryLeakDetector g_memory_leak_detector = {0};


void es_memory_leak_detector_init(void) {
    if (!ES_LEAK_DETECTION_ENABLED) return;

    g_memory_leak_detector.capacity = 1024;
    g_memory_leak_detector.allocations = ES_MALLOC(sizeof(MemoryAllocationInfo) * g_memory_leak_detector.capacity);
    g_memory_leak_detector.count = 0;
    g_memory_leak_detector.total_allocated = 0;
    g_memory_leak_detector.total_freed = 0;
    g_memory_leak_detector.start_time = (uint64_t)time(NULL);
    g_memory_leak_detector.enabled = 1;

    if (!g_memory_leak_detector.allocations) {
        fprintf(stderr, "[ES_MEMORY_ERROR] Failed to initialize memory leak detector\n");
        fflush(stderr);
        g_memory_leak_detector.enabled = 0;
        return;
    }

}


void es_memory_leak_detector_cleanup(void) {
    if (!ES_LEAK_DETECTION_ENABLED || !g_memory_leak_detector.enabled) return;


    es_report_memory_leaks();


    if (g_memory_leak_detector.allocations) {
        ES_FREE(g_memory_leak_detector.allocations);
        g_memory_leak_detector.allocations = NULL;
    }

    g_memory_leak_detector.capacity = 0;
    g_memory_leak_detector.count = 0;
    g_memory_leak_detector.enabled = 0;


}


static void expand_allocation_array(void) {
    size_t new_capacity = g_memory_leak_detector.capacity * 2;
    MemoryAllocationInfo* new_allocations = ES_REALLOC(g_memory_leak_detector.allocations,
                                                    sizeof(MemoryAllocationInfo) * new_capacity);

    if (!new_allocations) {
        fprintf(stderr, "[ES_MEMORY_ERROR] Failed to expand memory leak detector array\n");
        fflush(stderr);
        return;
    }

    g_memory_leak_detector.allocations = new_allocations;
    g_memory_leak_detector.capacity = new_capacity;

}


void es_record_allocation(void* ptr, size_t size, const char* file, int line,
                         const char* function, const char* type) {
    if (!ES_LEAK_DETECTION_ENABLED || !g_memory_leak_detector.enabled || !ptr) return;


    if (g_memory_leak_detector.count >= g_memory_leak_detector.capacity) {
        expand_allocation_array();
    }


    MemoryAllocationInfo* info = &g_memory_leak_detector.allocations[g_memory_leak_detector.count];
    info->ptr = ptr;
    info->size = size;
    info->file = file;
    info->line = line;
    info->function = function;
    info->timestamp = (uint64_t)time(NULL);
    info->type = type;

    g_memory_leak_detector.count++;
    g_memory_leak_detector.total_allocated += size;

}


void es_record_deallocation(void* ptr, const char* file, int line, const char* function) {
    (void)file;
    (void)line;
    (void)function;
    if (!ES_LEAK_DETECTION_ENABLED || !g_memory_leak_detector.enabled || !ptr) return;


    for (size_t i = 0; i < g_memory_leak_detector.count; i++) {
        if (g_memory_leak_detector.allocations[i].ptr == ptr) {

            g_memory_leak_detector.total_freed += g_memory_leak_detector.allocations[i].size;


            if (i < g_memory_leak_detector.count - 1) {
                g_memory_leak_detector.allocations[i] =
                    g_memory_leak_detector.allocations[g_memory_leak_detector.count - 1];
            }

            g_memory_leak_detector.count--;

            return;
        }
    }


}


void es_report_memory_leaks(void) {
    if (!ES_LEAK_DETECTION_ENABLED || !g_memory_leak_detector.enabled) return;

    if (g_memory_leak_detector.count == 0) {
        return;
    }


    size_t total_leaked = 0;
    for (size_t i = 0; i < g_memory_leak_detector.count; i++) {
        total_leaked += g_memory_leak_detector.allocations[i].size;
    }


    fprintf(stderr, "\n[ES_MEMORY_LEAK_REPORT]\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Memory Leak Summary:\n");
    fprintf(stderr, "  Total allocations: %zu\n", g_memory_leak_detector.total_allocated);
    fprintf(stderr, "  Total freed: %zu\n", g_memory_leak_detector.total_freed);
    fprintf(stderr, "  Current usage: %zu bytes\n", g_memory_leak_detector.total_allocated - g_memory_leak_detector.total_freed);
    fprintf(stderr, "  Leaked blocks: %zu\n", g_memory_leak_detector.count);
    fprintf(stderr, "  Total leaked: %zu bytes\n", total_leaked);
    fprintf(stderr, "========================================\n\n");
    fflush(stderr);


    if (g_memory_leak_detector.count > 0) {
        fprintf(stderr, "Detailed Leak Report:\n");
        fprintf(stderr, "========================================\n");
        fflush(stderr);

        for (size_t i = 0; i < g_memory_leak_detector.count; i++) {
            MemoryAllocationInfo* info = &g_memory_leak_detector.allocations[i];


            const char* filename = strrchr(info->file, '/');
            if (!filename) filename = strrchr(info->file, '\\');
            filename = filename ? filename + 1 : info->file;

            fprintf(stderr, "Leak #%zu:\n", i + 1);
        fprintf(stderr, "  Address: %p\n", info->ptr);
        fprintf(stderr, "  Size: %zu bytes\n", info->size);
        fprintf(stderr, "  Type: %s\n", info->type);
        fprintf(stderr, "  Location: %s:%d\n", filename, info->line);
        fprintf(stderr, "  Function: %s\n", info->function);
        fprintf(stderr, "  Timestamp: %llu\n", (unsigned long long)info->timestamp);
        fprintf(stderr, "\n");
        }

        fprintf(stderr, "========================================\n");
    }
}


void es_get_memory_stats(size_t* current_usage, size_t* peak_usage,
                        size_t* total_allocations, size_t* total_leaks) {
    if (!ES_LEAK_DETECTION_ENABLED || !g_memory_leak_detector.enabled) {
        if (current_usage) *current_usage = 0;
        if (peak_usage) *peak_usage = 0;
        if (total_allocations) *total_allocations = 0;
        if (total_leaks) *total_leaks = 0;
        return;
    }

    if (current_usage) *current_usage = g_memory_leak_detector.total_allocated - g_memory_leak_detector.total_freed;
    if (peak_usage) *peak_usage = g_memory_leak_detector.total_allocated;
    if (total_allocations) *total_allocations = g_memory_leak_detector.total_allocated;
    if (total_leaks) *total_leaks = g_memory_leak_detector.count;
}


void* es_malloc_leak_detected(size_t size, const char* file, int line) {
    void* ptr = ES_MALLOC(size);
    if (ptr) {
        es_record_allocation(ptr, size, file, line, __func__, "malloc");
    }
    return ptr;
}

void* es_calloc_leak_detected(size_t count, size_t size, const char* file, int line) {
    void* ptr = ES_CALLOC(count, size);
    if (ptr) {
        es_record_allocation(ptr, count * size, file, line, __func__, "calloc");
    }
    return ptr;
}

void* es_realloc_leak_detected(void* ptr, size_t size, const char* file, int line) {

    if (ptr) {
        es_record_deallocation(ptr, file, line, __func__);
    }

    void* new_ptr = ES_REALLOC(ptr, size);
    if (new_ptr) {
        es_record_allocation(new_ptr, size, file, line, __func__, "realloc");
    }

    return new_ptr;
}

char* es_strdup_leak_detected(const char* str, const char* file, int line) {
    if (!str) return NULL;

    size_t len = strlen(str);
    char* new_str = ES_MALLOC(len + 1);
    if (new_str) {
        ES_STRCPY_S(new_str, len + 1, str);
        es_record_allocation(new_str, len + 1, file, line, __func__, "strdup");
    }
    return new_str;
}

void es_free_leak_detected(void* ptr, const char* file, int line) {
    if (ptr) {
        es_record_deallocation(ptr, file, line, __func__);
    }
    ES_FREE(ptr);
}