#ifndef ES_STANDALONE_H
#define ES_STANDALONE_H

#include <stddef.h>
#include <stdint.h>

void* es_memcpy(void* dest, const void* src, size_t n);
void* es_memset(void* s, int c, size_t n);
void* es_memmove(void* dest, const void* src, size_t n);
int es_memcmp(const void* s1, const void* s2, size_t n);
void* es_memchr(const void* s, int c, size_t n);


size_t es_strlen(const char* s);
char* es_strcpy(char* dest, const char* src);
char* es_strncpy(char* dest, const char* src, size_t n);
int es_strcmp(const char* s1, const char* s2);
int es_strncmp(const char* s1, const char* s2, size_t n);
char* es_strcat(char* dest, const char* src);
char* es_strncat(char* dest, const char* src, size_t n);
char* es_strchr(const char* s, int c);
char* es_strrchr(const char* s, int c);
char* es_strstr(const char* haystack, const char* needle);






void* es_malloc(size_t size);
void* es_calloc(size_t nmemb, size_t size);
void* es_realloc(void* ptr, size_t size);
void es_free(void* ptr);


typedef struct es_mempool es_mempool_t;
es_mempool_t* es_mempool_create(size_t block_size, size_t block_count);
void es_mempool_destroy(es_mempool_t* pool);
void* es_mempool_alloc(es_mempool_t* pool);
void es_mempool_free(es_mempool_t* pool, void* ptr);






int es_printf(const char* format, ...);
int es_sprintf(char* str, const char* format, ...);
int es_snprintf(char* str, size_t size, const char* format, ...);
int es_vprintf(const char* format, va_list ap);
int es_vsprintf(char* str, const char* format, va_list ap);
int es_vsnprintf(char* str, size_t size, const char* format, va_list ap);


int es_putchar(int c);
int es_puts(const char* s);





int es_atoi(const char* nptr);
long es_atol(const char* nptr);
long long es_atoll(const char* nptr);
long es_strtol(const char* nptr, char** endptr, int base);
unsigned long es_strtoul(const char* nptr, char** endptr, int base);





int es_rand(void);
void es_srand(unsigned int seed);





void es_qsort(void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*));
void* es_bsearch(const void* key, const void* base, size_t nmemb, size_t size,
                 int (*compar)(const void*, const void*));





typedef long es_clock_t;
typedef long long es_time_t;

es_clock_t es_clock(void);
es_time_t es_time(es_time_t* tloc);






void es_exit(int status);
void es_abort(void);


int es_atexit(void (*func)(void));
void es_call_atexit_handlers(void);


int es_getargc(void);
char** es_getargv(void);


char* es_getenv(const char* name);


#ifdef _WIN32
void __cdecl mainCRTStartup(void);
void __cdecl WinMainCRTStartup(void);
#else
void _start(void);
#endif





#ifdef _WIN32

void* es_win_heap_create(void);
void es_win_heap_destroy(void* heap);
void* es_win_heap_alloc(void* heap, size_t size);
void es_win_heap_free(void* heap, void* ptr);
void* es_win_heap_realloc(void* heap, void* ptr, size_t size);


void* es_win_get_stdout(void);
void* es_win_get_stderr(void);
int es_win_write_console(void* handle, const void* buffer, size_t size);
int es_win_write_file(void* handle, const void* buffer, size_t size);
#endif



#endif 
