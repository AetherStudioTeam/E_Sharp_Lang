#include "es_standalone.h"

#ifdef _WIN32
#include <windows.h>

static HANDLE es_heap = NULL;

static HANDLE es_get_heap(void) {
    if (!es_heap) {
        es_heap = GetProcessHeap();
    }
    return es_heap;
}
#endif

void* es_malloc(int size) {
    if (size <= 0) return NULL;
#ifdef _WIN32
    return HeapAlloc(es_get_heap(), 0, size);
#else
    return malloc(size);
#endif
}

void* es_calloc(int num, int size) {
    if (num <= 0 || size <= 0) return NULL;
    int total = num * size;
    void* ptr = es_malloc(total);
    if (ptr) {
        es_memset(ptr, 0, total);
    }
    return ptr;
}

void* es_realloc(void* ptr, int new_size) {
    if (!ptr) return es_malloc(new_size);
    if (new_size <= 0) {
        es_free(ptr);
        return NULL;
    }
#ifdef _WIN32
    return HeapReAlloc(es_get_heap(), 0, ptr, new_size);
#else
    return realloc(ptr, new_size);
#endif
}

void es_free(void* ptr) {
    if (!ptr) return;
#ifdef _WIN32
    HeapFree(es_get_heap(), 0, ptr);
#else
    free(ptr);
#endif
}

int es_memcmp(const void* s1, const void* s2, int n);
void* es_memcpy(void* dest, const void* src, int n);
void* es_memmove(void* dest, const void* src, int n);
void* es_memset(void* s, int c, int n);
