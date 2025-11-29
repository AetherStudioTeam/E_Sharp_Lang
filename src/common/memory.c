#include "es_common.h"
#include "memory_leak_detector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void* es_malloc_checked(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    if (ptr == NULL && size > 0) {
        fprintf(stderr, "[ES_MEMORY_ERROR] %s:%d: Failed to allocate %zu bytes\n",
                file, line, size);
        fflush(stderr);
        exit(1);
    }

    #ifdef DEBUG

    #endif


    if (ES_LEAK_DETECTION_ENABLED && g_memory_leak_detector.enabled) {
        es_record_allocation(ptr, size, file, line, __func__, "malloc");
    }

    return ptr;
}

void* es_calloc_checked(size_t count, size_t size, const char* file, int line) {
    void* ptr = calloc(count, size);
    if (ptr == NULL && count > 0 && size > 0) {
        fprintf(stderr, "[ES_MEMORY_ERROR] %s:%d: Failed to allocate %zu bytes\n",
                file, line, count * size);
        fflush(stderr);
        exit(1);
    }

    #ifdef DEBUG

    #endif


    if (ES_LEAK_DETECTION_ENABLED && g_memory_leak_detector.enabled) {
        es_record_allocation(ptr, count * size, file, line, __func__, "calloc");
    }

    return ptr;
}

void* es_realloc_checked(void* ptr, size_t size, const char* file, int line) {

    if (ES_LEAK_DETECTION_ENABLED && g_memory_leak_detector.enabled && ptr) {
        es_record_deallocation(ptr, file, line, __func__);
    }

    void* new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size > 0) {
        fprintf(stderr, "[ES_MEMORY_ERROR] %s:%d: Failed to reallocate %zu bytes\n",
                file, line, size);
        fflush(stderr);
        exit(1);
    }

    #ifdef DEBUG

    #endif


    if (ES_LEAK_DETECTION_ENABLED && g_memory_leak_detector.enabled && new_ptr) {
        es_record_allocation(new_ptr, size, file, line, __func__, "realloc");
    }

    return new_ptr;
}

char* es_strdup_checked(const char* str, const char* file, int line) {
    if (str == NULL) {
        fprintf(stderr, "[ES_MEMORY_ERROR] %s:%d: Attempted to duplicate NULL string\n",
                file, line);
        fflush(stderr);
        exit(1);
    }

    char* new_str = strdup(str);
    if (new_str == NULL) {
        fprintf(stderr, "[ES_MEMORY_ERROR] %s:%d: Failed to duplicate string of length %zu\n",
                file, line, strlen(str));
        fflush(stderr);
        exit(1);
    }

    #ifdef DEBUG

    #endif


    if (ES_LEAK_DETECTION_ENABLED && g_memory_leak_detector.enabled) {
        es_record_allocation(new_str, strlen(str) + 1, file, line, __func__, "strdup");
    }

    return new_str;
}


void es_free_checked(void* ptr, const char* file, int line) {
    if (ptr == NULL) {
        #ifdef DEBUG

    #endif
        return;
    }


    #ifdef DEBUG
     if ((uintptr_t)ptr < 0x1000) {

         return;
     }
     #endif

    #ifdef DEBUG

    #endif


    if (ES_LEAK_DETECTION_ENABLED && g_memory_leak_detector.enabled) {
        es_record_deallocation(ptr, file, line, __func__);
    }

    free(ptr);
}