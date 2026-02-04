#include "logger.h"
#include "output_cache.h"
#include "es_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

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

    char* buffer = ES_MALLOC((size_t)size);
    if (!buffer) {
        if (!es_output_cache_is_enabled() && stream) {
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
        ES_FREE(buffer);
        return -1;
    }

    int result = written;
    if (es_output_cache_is_enabled()) {
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

    ES_FREE(buffer);
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

    if (!es_output_cache_is_enabled()) {
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
    if (es_output_cache_is_enabled()) {
        va_list args_copy;
        va_copy(args_copy, args);
        int size = vsnprintf(NULL, 0, format, args_copy) + 1;
        va_end(args_copy);

        if (size <= 0) {
            va_end(args);
            return -1;
        }

        char* buffer = ES_MALLOC((size_t)size);
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

        ES_FREE(buffer);
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

    if (!es_output_cache_is_enabled() && stream) {
        fflush(stream);
    }

    return result;
}