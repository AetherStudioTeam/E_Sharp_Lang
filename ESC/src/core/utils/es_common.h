#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef ES_COMMON_H
#define ES_COMMON_H

#define ES_VERSION_MAJOR 1
#define ES_VERSION_MINOR 0
#define ES_VERSION_PATCH 0
#define ES_VERSION_STRING "1.0.0"


#ifdef DEBUG
    #define ES_DEBUG_ENABLED 1
    #define ES_DEBUG_PRINT(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
    #define ES_DEBUG_LOG(category, fmt, ...) printf("[%s] " fmt "\n", category, ##__VA_ARGS__)
    #define ES_TRACE(fmt, ...) printf("[TRACE] " fmt "\n", ##__VA_ARGS__)
    #define ES_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
    #define ES_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
    #define ES_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#else
    #define ES_DEBUG_ENABLED 0
    #define ES_DEBUG_PRINT(fmt, ...) ((void)0)
    #define ES_DEBUG_LOG(category, fmt, ...) ((void)0)
    #define ES_TRACE(fmt, ...) ((void)0)
    #define ES_DEBUG(fmt, ...) ((void)0)
    #define ES_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
    #define ES_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#endif
    #define ES_ERROR(fmt, ...) do { \
        extern int es_output_cache_is_enabled(void); \
        extern void es_output_cache_add_error(const char* format, ...); \
        if (es_output_cache_is_enabled()) { \
            es_output_cache_add_error("[ES_ERROR] " fmt "\n", ##__VA_ARGS__); \
        } else { \
            fprintf(stderr, "[ES_ERROR] " fmt "\n", ##__VA_ARGS__); \
            fflush(stderr); \
        } \
    } while(0)
    #define ES_LEXER_DEBUG(fmt, ...) ((void)0)
    #define ES_PARSER_DEBUG(fmt, ...) ((void)0)
    #define ES_TYPE_DEBUG(fmt, ...) ((void)0)
    #define ES_COMPILER_DEBUG(fmt, ...) ((void)0)
    #define ES_IR_DEBUG(fmt, ...) ((void)0)

#ifndef ES_PARSER_ERROR
#define ES_PARSER_ERROR(fmt, ...) do { \
    extern int es_output_cache_is_enabled(void); \
    extern void es_output_cache_add_error(const char* format, ...); \
    if (es_output_cache_is_enabled()) { \
        es_output_cache_add_error("[PARSER_ERROR] " fmt "\n", ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[PARSER_ERROR] " fmt "\n", ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while(0)
#endif


#define ES_ERROR_LOC(file, line, fmt, ...) do { \
    extern int es_output_cache_is_enabled(void); \
    extern void es_output_cache_add_error(const char* format, ...); \
    if (es_output_cache_is_enabled()) { \
        es_output_cache_add_error("[ES_ERROR] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[ES_ERROR] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while(0)
#define ES_WARNING(fmt, ...) do { \
    extern int es_output_cache_is_enabled(void); \
    extern void es_output_cache_add_error(const char* format, ...); \
    if (es_output_cache_is_enabled()) { \
        es_output_cache_add_error("[ES_WARNING] " fmt "\n", ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[ES_WARNING] " fmt "\n", ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while(0)
#define ES_WARNING_LOC(file, line, fmt, ...) do { \
    extern int es_output_cache_is_enabled(void); \
    extern void es_output_cache_add_error(const char* format, ...); \
    if (es_output_cache_is_enabled()) { \
        es_output_cache_add_error("[ES_WARNING] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[ES_WARNING] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while(0)


#define ES_COMPILE_ERROR(fmt, ...) do { \
    fprintf(stderr, "[ES_COMPILE_ERROR] " fmt "\n", ##__VA_ARGS__); \
    fflush(stderr); \
    exit(1); \
} while(0)

#define ES_COMPILE_ERROR_LOC(file, line, fmt, ...) do { \
    fprintf(stderr, "[ES_COMPILE_ERROR] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
    fflush(stderr); \
    exit(1); \
} while(0)


#include "../memory/allocator.h"
#include "../memory/smart_ptr.h"

#define ES_MALLOC(size) es_safe_malloc(size, __FILE__, __LINE__)
#define ES_FREE(ptr) es_safe_free(ptr, __FILE__, __LINE__)
#define ES_CALLOC(count, size) es_safe_calloc(count, size, __FILE__, __LINE__)
#define ES_REALLOC(ptr, size) es_safe_realloc(ptr, size, __FILE__, __LINE__)
#define ES_STRDUP(str) es_safe_strdup(str, __FILE__, __LINE__)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include "es_string.h"
#include <time.h>
#include <stdarg.h>


double es_get_time(void);

void* es_malloc_checked(size_t size, const char* file, int line);
void* es_calloc_checked(size_t count, size_t size, const char* file, int line);
void* es_realloc_checked(void* ptr, size_t size, const char* file, int line);
char* es_strdup_checked(const char* str, const char* file, int line);
void es_free_checked(void* ptr, const char* file, int line);


#define ES_STRLEN(str) strlen(str)
#define ES_STRCPY(dest, src) do { \
    size_t src_len = strlen(src); \
    if (src_len >= sizeof(dest)) { \
        fprintf(stderr, "[ES_ERROR] Buffer overflow in ES_STRCPY at %s:%d\n", __FILE__, __LINE__); \
        fflush(stderr); \
        exit(1); \
    } \
    ES_STRCPY_S(dest, sizeof(dest), src); \
} while(0)
#define ES_STRNCPY(dest, src, n) strncpy(dest, src, n)


    #define ES_ASSERT(condition) ((void)0)


#ifdef _WIN32
    #define ES_API_EXPORT __declspec(dllexport)
    #define ES_API_IMPORT __declspec(dllimport)
#else
    
    #define ES_API_EXPORT 
    #define ES_API_IMPORT 
#endif


#ifdef ES_COMPILING_DLL
    #define ES_API_EXPORT_IMPORT ES_API_EXPORT
#else
    #define ES_API_EXPORT_IMPORT ES_API_IMPORT
#endif


#if defined(_WIN32) || defined(_WIN64)
    #define ES_PLATFORM_WINDOWS 1
    #define ES_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
    #define ES_PLATFORM_MACOS 1
    #define ES_PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define ES_PLATFORM_LINUX 1
    #define ES_PLATFORM_NAME "Linux"
#else
    #define ES_PLATFORM_UNKNOWN 1
    #define ES_PLATFORM_NAME "Unknown"
#endif


#if defined(__x86_64__) || defined(_M_X64)
    #define ES_ARCH_X64 1
    #define ES_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
    #define ES_ARCH_X86 1
    #define ES_ARCH_NAME "x86"
#elif defined(__arm__) || defined(_M_ARM)
    #define ES_ARCH_ARM 1
    #define ES_ARCH_NAME "ARM"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ES_ARCH_ARM64 1
    #define ES_ARCH_NAME "ARM64"
#else
    #define ES_ARCH_UNKNOWN 1
    #define ES_ARCH_NAME "Unknown"
#endif


    #define ES_LEAK_DETECTION_ENABLED 0


typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    const char* function;
    uint64_t timestamp;
    const char* type;
} MemoryAllocationInfo;

typedef struct {
    MemoryAllocationInfo* allocations;
    size_t capacity;
    size_t count;
    size_t total_allocated;
    size_t total_freed;
    uint64_t start_time;
    int enabled;
} MemoryLeakDetector;


extern MemoryLeakDetector g_memory_leak_detector;

void es_memory_leak_detector_init(void);
void es_memory_leak_detector_cleanup(void);
void es_record_allocation(void* ptr, size_t size, const char* file, int line,
                         const char* function, const char* type);
void es_record_deallocation(void* ptr, const char* file, int line, const char* function);
void es_report_memory_leaks(void);
void es_get_memory_stats(size_t* current_usage, size_t* peak_usage,
                        size_t* total_allocations, size_t* total_leaks);


#ifdef ES_LEAK_DETECTION_ENABLED
    #define ES_MALLOC_LEAK(size) es_malloc_leak_detected(size, __FILE__, __LINE__)
    #define ES_CALLOC_LEAK(count, size) es_calloc_leak_detected(count, size, __FILE__, __LINE__)
    #define ES_REALLOC_LEAK(ptr, size) es_realloc_leak_detected(ptr, size, __FILE__, __LINE__)
    #define ES_STRDUP_LEAK(str) es_strdup_leak_detected(str, __FILE__, __LINE__)
    #define ES_FREE_LEAK(ptr) es_free_leak_detected(ptr, __FILE__, __LINE__)
#else
    #define ES_MALLOC_LEAK(size) ES_MALLOC(size)
    #define ES_CALLOC_LEAK(count, size) ES_CALLOC(count, size)
    #define ES_REALLOC_LEAK(ptr, size) ES_REALLOC(ptr, size)
    #define ES_STRDUP_LEAK(str) ES_STRDUP(str)
    #define ES_FREE_LEAK(ptr) ES_FREE(ptr)
#endif


void* es_malloc_leak_detected(size_t size, const char* file, int line);
void* es_calloc_leak_detected(size_t count, size_t size, const char* file, int line);
void* es_realloc_leak_detected(void* ptr, size_t size, const char* file, int line);
char* es_strdup_leak_detected(const char* str, const char* file, int line);
void es_free_leak_detected(void* ptr, const char* file, int line);


double es_time_now_seconds(void);
int es_path_exists(const char* path);
int es_path_get_filename(const char* path, char* filename, size_t size);
int es_path_remove_extension(const char* path, char* result, size_t size);
int es_path_join(char* result, size_t size, const char* path1, const char* path2);
int es_ensure_directory_recursive(const char* path);

#define ES_COMPILER_INFO (ES_PLATFORM_NAME "-" ES_ARCH_NAME " " ES_VERSION_STRING)

#define printf es_printf

#endif