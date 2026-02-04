#ifndef ES_STANDALONE_H
#define ES_STANDALONE_H


typedef long long es_time_t;
typedef unsigned int es_size_t;


typedef char* va_list;
#define va_start(ap, last) ap = (char*)&last + sizeof(last)
#define va_arg(ap, type) (*(type*)((ap += sizeof(type)) - sizeof(type)))
#define va_end(ap) ap = (char*)0


int es_strlen(const char* str);
char* es_strcpy(char* dest, const char* src);
char* es_strncpy(char* dest, const char* src, int n);
char* es_strcat(char* dest, const char* src);
int es_strcmp(const char* s1, const char* s2);
int es_strncmp(const char* s1, const char* s2, int n);
char* es_strchr(const char* str, int c);
char* es_strrchr(const char* str, int c);
char* es_strstr(const char* haystack, const char* needle);


void* es_memcpy(void* dest, const void* src, int n);
void* es_memmove(void* dest, const void* src, int n);
void* es_memset(void* s, int c, int n);
int es_memcmp(const void* s1, const void* s2, int n);


void* es_malloc(int size);
void* es_calloc(int num, int size);
void* es_realloc(void* ptr, int new_size);
void es_free(void* ptr);


void es_putchar(char c);
void es_puts(const char* str);
void es_print(const char* str);
void es_println(const char* str);
void es_print_int(int n);
void es_println_int(int n);
void es_print_float(double f, int precision);
void es_println_float(double f, int precision);


void es_printf(const char* format, ...);
void es_vprintf(const char* format, va_list args);


int es_atoi(const char* str);
char* es_itoa(int value, char* str, int base);
char* es_ftoa(double f, char* buf, int precision);


es_time_t es_time(es_time_t* tloc);
void es_sleep_ms(int ms);

#endif 
