#ifndef STRING_OPTIMIZER_H
#define STRING_OPTIMIZER_H

#include <stddef.h>


const char* es_get_string_constant(const char* str);


char* es_strcat_optimized(const char* str1, const char* str2);
char* es_strcat_multiple(const char** parts, int count);


char* es_int_to_string_optimized(int num);
char* es_double_to_string_optimized(double num);


int es_string_fast_compare(const char* str1, const char* str2);


void es_process_strings_batch(const char** strings, int count, void (*processor)(const char*));


char* es_pool_alloc_string(size_t size);

#endif 