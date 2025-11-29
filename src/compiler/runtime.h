#ifndef RUNTIME_H
#define RUNTIME_H

#include <stddef.h>


#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT void print_number(double num);
EXPORT void print_int(long long num);
void* es_malloc(size_t size);
void es_free(void* ptr);
char* es_strdup(const char* str);

double es_sin(double x);
double es_cos(double x);
double es_tan(double x);
double es_sqrt(double x);

void es_print(const char* str);
void es_println(const char* str);

void print_string(const char* str);


#ifdef _WIN32
#define timer_start _timer_start
#define timer_elapsed _timer_elapsed
#define timer_current _timer_current
#define timer_start_int _timer_start_int
#define timer_elapsed_int _timer_elapsed_int
#define timer_current_int _timer_current_int
#endif
double timer_start();
double timer_elapsed();
double timer_current();


long long timer_start_int();
long long timer_elapsed_int();
long long timer_current_int();

#endif