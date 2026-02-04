#include "output_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "es_common.h"

OutputCache g_output_cache = {0};

void es_output_cache_init(void) {
    es_output_cache_clear();
    g_output_cache.enabled = 0;
}

void es_output_cache_cleanup(void) {
    es_output_cache_flush();
    g_output_cache.head = NULL;
    g_output_cache.tail = NULL;
    g_output_cache.count = 0;
    g_output_cache.enabled = 0;
}

void es_output_cache_clear(void) {
    OutputCacheEntry* current = g_output_cache.head;
    while (current) {
        OutputCacheEntry* next = current->next;
        ES_FREE(current->message);
        ES_FREE(current);
        current = next;
    }
    g_output_cache.head = NULL;
    g_output_cache.tail = NULL;
    g_output_cache.count = 0;
}

static void es_output_cache_add_internal(OutputCacheStream stream, const char* format, va_list args) {
    if (!format) {
        return;
    }

    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);

    if (size <= 0) {
        return;
    }

    char* buffer = (char*)ES_MALLOC((size_t)size);
    if (!buffer) {
        if (!g_output_cache.enabled) {
            char fallback_buffer[512];
            va_list args_fallback;
            va_copy(args_fallback, args);
            int fallback_written = vsnprintf(fallback_buffer, sizeof(fallback_buffer), format, args_fallback);
            va_end(args_fallback);

            if (fallback_written > 0) {
                FILE* target = (stream == OUTPUT_CACHE_STDERR) ? stderr : stdout;
                int result = fputs(fallback_buffer, target);
                if (result != EOF) {
                    if (fallback_written >= (int)sizeof(fallback_buffer) - 1) {
                        result = fputs("... [truncated due to memory allocation failure]\n", target);
                    } else {
                        result = fputs("\n", target);
                    }
                }
                if (result == EOF) {
                    const char* error_msg = "[输出错误: 内存分配失败且无法写入输出流]\n";
                    fputs(error_msg, stderr);
                }
                fflush(target);
            }
        }
        return;
    }

    int written = vsnprintf(buffer, (size_t)size, format, args);
    if (written < 0) {
        ES_FREE(buffer);
        return;
    }

    if (!g_output_cache.enabled) {
        FILE* target = (stream == OUTPUT_CACHE_STDERR) ? stderr : stdout;
        int result = fputs(buffer, target);
        if (result != EOF) {
            result = fputs("\n", target);
        }
        if (result == EOF) {
            
            const char* error_msg = "[输出错误: 无法写入输出流]\n";
            fputs(error_msg, stderr);
        }
        fflush(target);  
        ES_FREE(buffer);
        return;
    }

    OutputCacheEntry* new_entry = (OutputCacheEntry*)ES_MALLOC(sizeof(OutputCacheEntry));
    if (!new_entry) {
        FILE* target = (stream == OUTPUT_CACHE_STDERR) ? stderr : stdout;
        int result = fputs(buffer, target);
        if (result != EOF) {
            result = fputs(" [cache entry allocation failed, output directly]\n", target);
        }
        if (result == EOF) {
            const char* error_msg = "[输出错误: 缓存条目分配失败且无法写入输出流]\n";
            fputs(error_msg, stderr);
        }
        fflush(target);
        ES_FREE(buffer);
        return;
    }

    new_entry->message = buffer;
    new_entry->stream = stream;
    new_entry->next = NULL;

    if (!g_output_cache.head) {
        g_output_cache.head = new_entry;
        g_output_cache.tail = new_entry;
    } else {
        g_output_cache.tail->next = new_entry;
        g_output_cache.tail = new_entry;
    }

    g_output_cache.count++;
}

void es_output_cache_add(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    es_output_cache_add_internal(OUTPUT_CACHE_STDOUT, format, copy);
    va_end(copy);
    va_end(args);
}

void es_output_cache_addv(const char* format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    es_output_cache_add_internal(OUTPUT_CACHE_STDOUT, format, copy);
    va_end(copy);
}

void es_output_cache_add_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    es_output_cache_add_internal(OUTPUT_CACHE_STDERR, format, copy);
    va_end(copy);
    va_end(args);
}

void es_output_cache_add_errorv(const char* format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    es_output_cache_add_internal(OUTPUT_CACHE_STDERR, format, copy);
    va_end(copy);
}

void es_output_cache_flush(void) {
    OutputCacheEntry* current = g_output_cache.head;
    while (current) {
        OutputCacheEntry* next = current->next;
        FILE* target = (current->stream == OUTPUT_CACHE_STDERR) ? stderr : stdout;

        if (current->message) {
            int result = fputs(current->message, target);
            if (result != EOF) {
                result = fputs("\n", target);
            }
            if (result == EOF) {
                
                const char* error_msg = "[警告: 缓存条目写入失败]\n";
                fputs(error_msg, stderr);
            }
            fflush(target);
        }
        ES_FREE(current->message);
        ES_FREE(current);
        current = next;
    }
    g_output_cache.head = NULL;
    g_output_cache.tail = NULL;
    g_output_cache.count = 0;
}

int es_output_cache_is_enabled(void) {
    return g_output_cache.enabled;
}

void es_output_cache_set_enabled(int enabled) {
    g_output_cache.enabled = enabled;
}