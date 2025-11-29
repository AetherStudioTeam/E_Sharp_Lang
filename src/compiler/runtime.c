#include "runtime.h"
#include "../common/es_common.h"
#include "../common/output_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>


#ifdef _WIN32
__declspec(dllexport)
#endif
void print_number(double num) {
    #ifdef DEBUG

    union {
        double d;
        unsigned long long ull;
    } u;
    u.d = num;
    ES_DEBUG_LOG("RUNTIME", "print_number called with value: %.15f (0x%llx)", num, u.ull);
    fflush(stdout);
    #else
    printf("%.15f\n", num);
    #endif
}


#ifdef _WIN32
void _print_number(double num) {
    print_number(num);
}
#endif


#ifdef _WIN32
__declspec(dllexport)
#endif
void print_int(long long num) {
    printf("%lld\n", num);
}


#ifdef _WIN32
void _print_int(long long num) {
    print_int(num);
}
#endif


void* es_malloc(size_t size) {
    return malloc(size);
}

void es_free(void* ptr) {
    free(ptr);
}


char* es_strdup(const char* str) {
    size_t len = strlen(str) + 1;
    char* new_str = (char*)malloc(len);
    if (new_str) {
        memcpy(new_str, str, len);
    }
    return new_str;
}


double es_sin(double x) { return sin(x); }
double es_cos(double x) { return cos(x); }
double es_tan(double x) { return tan(x); }
double es_sqrt(double x) { return sqrt(x); }


void es_print(const char* str) {
    printf("%s", str);
}

void es_println(const char* str) {
    printf("%s\n", str);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void print_string(const char* str) {
    printf("%s", str);
}

char* es_strcat(const char* str1, const char* str2) {
    if (!str1 || !str2) return NULL;

    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);


    char* result = (char*)malloc(len1 + len2 + 1);
    if (!result) return NULL;


    strcpy(result, str1);

    strcat(result, str2);

    return result;
}

#ifdef _WIN32
char* _es_strcat(const char* str1, const char* str2) {
    return es_strcat(str1, str2);
}
#endif

char* _es_int_to_string_value(double num) {

    char* buffer = (char*)malloc(32);
    if (!buffer) return NULL;

    snprintf(buffer, 32, "%.0f", num);
    return buffer;
}

#ifdef _WIN32
void _print_string(const char* str) {
    print_string(str);
}
#endif


#ifdef _WIN32
__declspec(dllexport)
#endif
void es_runtime_printf(const char* format, double value) {


    (void)format;
    printf("%.15g", value);
}


#ifdef _WIN32
void _es_runtime_printf(const char* format, double value) {
    es_runtime_printf(format, value);
}
#endif


static double timer_start_time = 0.0;
static int timer_initialized = 0;

double timer_start() {
    timer_start_time = (double)clock() / CLOCKS_PER_SEC;
    timer_initialized = 1;
    return timer_start_time;
}

double timer_elapsed() {
    if (!timer_initialized) {
        return -1.0;
    }
    double current_time = (double)clock() / CLOCKS_PER_SEC;
    return current_time - timer_start_time;
}

double timer_current() {
    return (double)clock() / CLOCKS_PER_SEC;
}


long long timer_start_int() {
    timer_start_time = (double)clock() / CLOCKS_PER_SEC;
    timer_initialized = 1;
    return (long long)(timer_start_time * 1000);
}

long long timer_elapsed_int() {
    if (!timer_initialized) {
        return -1;
    }
    double current_time = (double)clock() / CLOCKS_PER_SEC;
    return (long long)((current_time - timer_start_time) * 1000);
}

long long timer_current_int() {
    return (long long)(((double)clock() / CLOCKS_PER_SEC) * 1000);
}