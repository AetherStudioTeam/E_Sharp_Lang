#ifndef ES_MEMORY_LEAK_DETECTOR_H
#define ES_MEMORY_LEAK_DETECTOR_H

#include "../utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

    #define ES_LEAK_DETECTION_ENABLED 0


void es_memory_leak_detector_init(void);


void es_memory_leak_detector_cleanup(void);


void es_record_allocation(void* ptr, size_t size, const char* file, int line,
                         const char* function, const char* type);


void es_record_deallocation(void* ptr, const char* file, int line,
                           const char* function);


void es_report_memory_leaks(void);


void es_get_memory_stats(size_t* current_usage, size_t* peak_usage,
                        size_t* total_allocations, size_t* total_leaks);


void* es_malloc_leak_detected(size_t size, const char* file, int line);
void* es_calloc_leak_detected(size_t count, size_t size, const char* file, int line);
void* es_realloc_leak_detected(void* ptr, size_t size, const char* file, int line);
char* es_strdup_leak_detected(const char* str, const char* file, int line);
void es_free_leak_detected(void* ptr, const char* file, int line);


#if ES_LEAK_DETECTION_ENABLED

    #define ES_LEAK_DETECTOR_INIT() es_memory_leak_detector_init()
    #define ES_LEAK_DETECTOR_CLEANUP() es_memory_leak_detector_cleanup()
    #define ES_REPORT_LEAKS() es_report_memory_leaks()


    #define ES_GET_MEMORY_STATS(current, peak, allocations, leaks) \
        es_get_memory_stats(current, peak, allocations, leaks)
#else
    #define ES_LEAK_DETECTOR_INIT() ((void)0)
    #define ES_LEAK_DETECTOR_CLEANUP() ((void)0)
    #define ES_REPORT_LEAKS() ((void)0)
    #define ES_GET_MEMORY_STATS(current, peak, allocations, leaks) \
        do { if (current) *current = 0; if (peak) *peak = 0; \
             if (allocations) *allocations = 0; if (leaks) *leaks = 0; } while(0)
#endif

#endif