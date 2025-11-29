#include "output_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

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
        free(current->message);
        free(current);
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

    char* buffer = (char*)malloc((size_t)size);
    if (!buffer) {



        if (!g_output_cache.enabled) {

            char fallback_buffer[512];
            va_list args_fallback;
            va_copy(args_fallback, args);
            int fallback_written = vsnprintf(fallback_buffer, sizeof(fallback_buffer), format, args_fallback);
            va_end(args_fallback);

            if (fallback_written > 0) {
                FILE* target = (stream == OUTPUT_CACHE_STDERR) ? stderr : stdout;

                if (fputs(fallback_buffer, target) == EOF) {

                } else {


                    if (fallback_written >= (int)sizeof(fallback_buffer) - 1) {
                        if (fputs("... [truncated due to memory allocation failure]\n", target) == EOF) {

                        }
                    } else {
                        if (fputs("\n", target) == EOF) {

                        }
                    }
                }
            }
        }

        return;
    }

    int written = vsnprintf(buffer, (size_t)size, format, args);
    if (written < 0) {

        free(buffer);
        return;
    }

    if (!g_output_cache.enabled) {
        FILE* target = (stream == OUTPUT_CACHE_STDERR) ? stderr : stdout;

        if (fputs(buffer, target) == EOF) {

        } else {

            if (fputs("\n", target) == EOF) {

            }
        }
        free(buffer);
        return;
    }

    OutputCacheEntry* new_entry = (OutputCacheEntry*)malloc(sizeof(OutputCacheEntry));
    if (!new_entry) {


        FILE* target = (stream == OUTPUT_CACHE_STDERR) ? stderr : stdout;

        if (fputs(buffer, target) == EOF) {

        } else {

            if (fputs(" [cache entry allocation failed, output directly]\n", target) == EOF) {

            }
        }
        free(buffer);
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
            if (fputs(current->message, target) == EOF) {

            } else {

                if (fputs("\n", target) == EOF) {

                }
            }
            if (fflush(target) != 0) {

            }
        }
        free(current->message);
        free(current);
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



static int es_format_and_output(FILE* stream, const char* format, va_list args) {
    if (!format) {
        return -1;
    }


    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);


    if (size <= 0) {
        return -1;
    }


    char* buffer = malloc((size_t)size);
    if (!buffer) {

        if (!g_output_cache.enabled && stream) {
            va_list args_direct;
            va_copy(args_direct, args);
            int result = vfprintf(stream, format, args_direct);
            va_end(args_direct);
            return result;
        }
        return -1;
    }


    int written = vsnprintf(buffer, (size_t)size, format, args);


    if (written < 0) {
        free(buffer);
        return -1;
    }


    int result = written;
    if (g_output_cache.enabled) {

        if (stream == stdout) {
            es_output_cache_add("%s", buffer);
        } else if (stream == stderr) {
            es_output_cache_add_error("%s", buffer);
        } else {

            if (fputs(buffer, stream) == EOF) {
                result = -1;
            }
        }
    } else {

        if (stream) {
            if (fputs(buffer, stream) == EOF) {
                result = -1;
            }
        } else {

            if (fputs(buffer, stdout) == EOF) {
                result = -1;
            }
        }
    }

    free(buffer);
    return result;
}

int es_printf(const char* format, ...) {
    if (!format) {
        return -1;
    }

    va_list args;
    va_start(args, format);
    int result = es_format_and_output(stdout, format, args);
    va_end(args);


    if (!g_output_cache.enabled) {
        fflush(stdout);
    }

    return result;
}


int es_print_format(const char* format, ...) {
    if (!format) {
        return -1;
    }

    va_list args;
    va_start(args, format);

    int result;
    if (g_output_cache.enabled) {


        va_list args_copy;
        va_copy(args_copy, args);
        int size = vsnprintf(NULL, 0, format, args_copy) + 1;
        va_end(args_copy);

        if (size <= 0) {
            va_end(args);
            return -1;
        }

        char* buffer = malloc((size_t)size);
        if (!buffer) {
            va_end(args);
            return -1;
        }

        int written = vsnprintf(buffer, (size_t)size, format, args);
        if (written >= 0) {
            result = fputs(buffer, stdout);
            fflush(stdout);
        } else {
            result = -1;
        }

        free(buffer);
    } else {

        result = vfprintf(stdout, format, args);
        fflush(stdout);
    }

    va_end(args);
    return result;
}

int es_fprintf(FILE* stream, const char* format, ...) {
    if (!format) {
        return -1;
    }

    va_list args;
    va_start(args, format);
    int result = es_format_and_output(stream, format, args);
    va_end(args);


    if (!g_output_cache.enabled && stream) {
        fflush(stream);
    }

    return result;
}