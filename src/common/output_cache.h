#ifndef OUTPUT_CACHE_H
#define OUTPUT_CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef enum {
    OUTPUT_CACHE_STDOUT = 0,
    OUTPUT_CACHE_STDERR = 1
} OutputCacheStream;

typedef struct OutputCacheEntry {
    char* message;
    OutputCacheStream stream;
    struct OutputCacheEntry* next;
} OutputCacheEntry;

typedef struct {
    OutputCacheEntry* head;
    OutputCacheEntry* tail;
    int count;
    int enabled;
} OutputCache;

extern OutputCache g_output_cache;

void es_output_cache_init(void);
void es_output_cache_cleanup(void);
void es_output_cache_clear(void);
void es_output_cache_add(const char* format, ...);
void es_output_cache_addv(const char* format, va_list args);
void es_output_cache_add_error(const char* format, ...);
void es_output_cache_add_errorv(const char* format, va_list args);
void es_output_cache_flush(void);
int es_output_cache_is_enabled(void);
void es_output_cache_set_enabled(int enabled);


int es_printf(const char* format, ...);
int es_print_format(const char* format, ...);
int es_fprintf(FILE* stream, const char* format, ...);

#endif