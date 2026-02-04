#ifndef ES_RUNTIME_H
#define ES_RUNTIME_H

#include "../core/platform/platform.h"
#include "../core/utils/es_common.h"
#include <stddef.h>
#include <stdint.h>


#ifdef _WIN32
    #define ES_RUNTIME_EXPORT __declspec(dllexport)
#else
    
    #define ES_RUNTIME_EXPORT 
#endif


typedef struct {
    int x;
    int y;
} ES_Point;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} ES_Rect;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} ES_Color;


typedef struct {
    void* data;
    size_t size;
    size_t capacity;
    size_t element_size;
} ES_Array;


typedef enum {
    ES_HASHMAP_TYPE_INT,
    ES_HASHMAP_TYPE_DOUBLE,
    ES_HASHMAP_TYPE_STRING,
    ES_HASHMAP_TYPE_POINTER
} ES_HashMapType;


typedef struct ES_HashMapItem {
    void* key;
    void* value;
    ES_HashMapType key_type;
    ES_HashMapType value_type;
    struct ES_HashMapItem* next;
} ES_HashMapItem;


typedef struct {
    ES_HashMapItem** buckets;
    size_t capacity;
    size_t size;
} ES_HashMap;


typedef void* ES_Mutex;
typedef void* ES_Window;
typedef void* ES_FILE;
typedef void* ES_Socket;
typedef void* ES_Thread;


typedef enum {
    ES_ERR_NONE = 0,
    ES_ERR_OUT_OF_MEMORY,
    ES_ERR_INVALID_ARGUMENT,
    ES_ERR_FILE_NOT_FOUND,
    ES_ERR_PERMISSION_DENIED,
    ES_ERR_IO_ERROR,
    ES_ERR_NETWORK_ERROR,
    ES_ERR_INDEX_OUT_OF_BOUNDS,
    ES_ERR_DIVISION_BY_ZERO,
    ES_ERR_NULL_POINTER,
    ES_ERR_BUFFER_OVERFLOW,
    ES_ERR_UNKNOWN
} ES_ErrorCode;


ES_RUNTIME_EXPORT void ES_API _print_string(const char* str);
ES_RUNTIME_EXPORT void ES_API _print_number(int num);
ES_RUNTIME_EXPORT void ES_API _print_int(int num);
ES_RUNTIME_EXPORT void ES_API _print_int64(long long num);
ES_RUNTIME_EXPORT void ES_API _print_double(double num);
ES_RUNTIME_EXPORT void ES_API _print_float(double num);
ES_RUNTIME_EXPORT void ES_API _print_char(char c);
ES_RUNTIME_EXPORT void ES_API _print_newline(void);
ES_RUNTIME_EXPORT int ES_API _read_char(void);
ES_RUNTIME_EXPORT int ES_API _pow_int(int base, int exp);


ES_RUNTIME_EXPORT void* ES_API es_malloc(size_t size);
ES_RUNTIME_EXPORT void ES_API es_free(void* ptr);
ES_RUNTIME_EXPORT void* ES_API es_realloc(void* ptr, size_t size);
ES_RUNTIME_EXPORT void* ES_API es_calloc(size_t num, size_t size);


ES_RUNTIME_EXPORT uint64_t ES_API get_total_memory(void);
ES_RUNTIME_EXPORT uint64_t ES_API get_free_memory(void);


ES_RUNTIME_EXPORT size_t ES_API es_strlen(const char* str);
ES_RUNTIME_EXPORT char* ES_API es_strcpy(char* dest, const char* src);
ES_RUNTIME_EXPORT char* ES_API es_strcat(char* dest, const char* src);
ES_RUNTIME_EXPORT char* ES_API es_strdup(const char* src);
ES_RUNTIME_EXPORT int ES_API es_strcmp(const char* str1, const char* str2);
ES_RUNTIME_EXPORT char* ES_API es_strchr(const char* str, int c);
ES_RUNTIME_EXPORT char* ES_API es_strstr(const char* haystack, const char* needle);
ES_RUNTIME_EXPORT char* ES_API es_strtok(char* str, const char* delim);
ES_RUNTIME_EXPORT char* ES_API es_strupr(char* str);
ES_RUNTIME_EXPORT char* ES_API es_strlwr(char* str);
ES_RUNTIME_EXPORT char* ES_API es_strrev(char* str);
ES_RUNTIME_EXPORT char* ES_API es_strtrim(char* str);
ES_RUNTIME_EXPORT char* ES_API es_strltrim(char* str);
ES_RUNTIME_EXPORT char* ES_API es_strrtrim(char* str);
ES_RUNTIME_EXPORT char* ES_API es_strreplace(const char* src, const char* old_sub, const char* new_sub);
ES_RUNTIME_EXPORT int ES_API es_atoi(const char* str);
ES_RUNTIME_EXPORT double ES_API es_atof(const char* str);
ES_RUNTIME_EXPORT char* ES_API _es_int_to_string_value(int64_t num);
ES_RUNTIME_EXPORT char* ES_API es_int_to_string(int64_t num);
ES_RUNTIME_EXPORT char* ES_API _es_double_to_string_value(double num);
ES_RUNTIME_EXPORT char* ES_API _es_strcat(const char* str1, const char* str2);


ES_RUNTIME_EXPORT double ES_API es_sin(double x);
ES_RUNTIME_EXPORT double ES_API es_cos(double x);
ES_RUNTIME_EXPORT double ES_API es_tan(double x);
ES_RUNTIME_EXPORT double ES_API es_sqrt(double x);
ES_RUNTIME_EXPORT double ES_API es_pow(double base, double exp);
ES_RUNTIME_EXPORT double ES_API es_log(double x);
ES_RUNTIME_EXPORT double ES_API es_log10(double x);
ES_RUNTIME_EXPORT double ES_API es_exp(double x);
ES_RUNTIME_EXPORT double ES_API es_asin(double x);
ES_RUNTIME_EXPORT double ES_API es_acos(double x);
ES_RUNTIME_EXPORT double ES_API es_atan(double x);
ES_RUNTIME_EXPORT double ES_API es_sinh(double x);
ES_RUNTIME_EXPORT double ES_API es_cosh(double x);
ES_RUNTIME_EXPORT double ES_API es_tanh(double x);
ES_RUNTIME_EXPORT double ES_API es_fabs(double x);
ES_RUNTIME_EXPORT int ES_API es_abs(int x);


ES_RUNTIME_EXPORT double ES_API es_time(void);
ES_RUNTIME_EXPORT void ES_API es_sleep(int seconds);
ES_RUNTIME_EXPORT void ES_API es_srand(unsigned int seed);
ES_RUNTIME_EXPORT int ES_API es_rand(void);
ES_RUNTIME_EXPORT char* ES_API es_date(void);
ES_RUNTIME_EXPORT char* ES_API es_time_format(const char* format);


ES_RUNTIME_EXPORT double ES_API timer_start(void);
ES_RUNTIME_EXPORT double ES_API timer_elapsed(void);
ES_RUNTIME_EXPORT double ES_API timer_current(void);
ES_RUNTIME_EXPORT long long ES_API timer_start_int(void);
ES_RUNTIME_EXPORT long long ES_API timer_elapsed_int(void);
ES_RUNTIME_EXPORT long long ES_API timer_current_int(void);


ES_RUNTIME_EXPORT void ES_API print_number(double num);
ES_RUNTIME_EXPORT void ES_API print_int(long long num);
ES_RUNTIME_EXPORT void ES_API print_int64(long long num);
ES_RUNTIME_EXPORT void ES_API es_print(const char* str);
ES_RUNTIME_EXPORT void ES_API es_println(const char* str);
ES_RUNTIME_EXPORT void ES_API print_string(const char* str);
ES_RUNTIME_EXPORT void ES_API Console__WriteLine(const char* str);
ES_RUNTIME_EXPORT void ES_API Console__Write(const char* str);
ES_RUNTIME_EXPORT void ES_API Console__WriteLineInt(int num);
ES_RUNTIME_EXPORT void ES_API Console__WriteInt(int num);


ES_RUNTIME_EXPORT ES_FILE ES_API es_fopen(const char* filename, const char* mode);
ES_RUNTIME_EXPORT int ES_API es_fclose(ES_FILE file);
ES_RUNTIME_EXPORT size_t ES_API es_fread(void* buffer, size_t size, size_t count, ES_FILE file);
ES_RUNTIME_EXPORT size_t ES_API es_fwrite(const void* buffer, size_t size, size_t count, ES_FILE file);
ES_RUNTIME_EXPORT int ES_API es_fseek(ES_FILE file, long offset, int origin);
ES_RUNTIME_EXPORT long ES_API es_ftell(ES_FILE file);
ES_RUNTIME_EXPORT int ES_API es_feof(ES_FILE file);
ES_RUNTIME_EXPORT int ES_API es_remove(const char* filename);
ES_RUNTIME_EXPORT int ES_API es_rename(const char* oldname, const char* newname);


ES_RUNTIME_EXPORT void ES_API es_error(ES_ErrorCode code);
ES_RUNTIME_EXPORT const char* ES_API es_strerror(ES_ErrorCode code);


ES_RUNTIME_EXPORT ES_Array* ES_API array_create(size_t element_size, size_t initial_capacity);
ES_RUNTIME_EXPORT void ES_API array_ES_FREE(ES_Array* array);
ES_RUNTIME_EXPORT size_t ES_API array_size(ES_Array* array);
ES_RUNTIME_EXPORT size_t ES_API array_capacity(ES_Array* array);
ES_RUNTIME_EXPORT int ES_API array_resize(ES_Array* array, size_t new_capacity);
ES_RUNTIME_EXPORT int ES_API array_append(ES_Array* array, const void* element);
ES_RUNTIME_EXPORT void* ES_API array_get(ES_Array* array, size_t index);
ES_RUNTIME_EXPORT int ES_API array_set(ES_Array* array, size_t index, const void* element);


ES_RUNTIME_EXPORT ES_HashMap* ES_API hashmap_create(size_t initial_capacity);
ES_RUNTIME_EXPORT void ES_API hashmap_ES_FREE(ES_HashMap* map);
ES_RUNTIME_EXPORT int ES_API hashmap_put(ES_HashMap* map, const void* key, ES_HashMapType key_type, const void* value, ES_HashMapType value_type);
ES_RUNTIME_EXPORT int ES_API hashmap_get(ES_HashMap* map, const void* key, ES_HashMapType key_type, void** value, ES_HashMapType* out_value_type);
ES_RUNTIME_EXPORT int ES_API hashmap_remove(ES_HashMap* map, const void* key, ES_HashMapType key_type);


ES_RUNTIME_EXPORT ES_Socket ES_API es_socket(int domain, int type, int protocol);
ES_RUNTIME_EXPORT int ES_API es_connect(ES_Socket socket, const char* address, int port);
ES_RUNTIME_EXPORT int ES_API es_bind(ES_Socket socket, const char* address, int port);
ES_RUNTIME_EXPORT int ES_API es_listen(ES_Socket socket, int backlog);
ES_RUNTIME_EXPORT ES_Socket ES_API es_accept(ES_Socket socket);
ES_RUNTIME_EXPORT int ES_API es_send(ES_Socket socket, const char* data, size_t length);
ES_RUNTIME_EXPORT int ES_API es_recv(ES_Socket socket, char* buffer, size_t length);
ES_RUNTIME_EXPORT int ES_API es_close_socket(ES_Socket socket);


ES_RUNTIME_EXPORT ES_Thread ES_API es_thread_create_func(void (*func)(void*), void* arg);
ES_RUNTIME_EXPORT int ES_API es_thread_join_func(ES_Thread thread);


ES_RUNTIME_EXPORT ES_Mutex ES_API es_mutex_create_func(void);
ES_RUNTIME_EXPORT int ES_API es_mutex_lock_func(ES_Mutex mutex);
ES_RUNTIME_EXPORT int ES_API es_mutex_unlock_func(ES_Mutex mutex);
ES_RUNTIME_EXPORT void ES_API es_mutex_free_func(ES_Mutex mutex);


ES_RUNTIME_EXPORT ES_Window ES_API es_window_create(int width, int height, const char* title);
ES_RUNTIME_EXPORT void ES_API es_window_show(ES_Window window);
ES_RUNTIME_EXPORT void ES_API es_window_close(ES_Window window);
ES_RUNTIME_EXPORT void ES_API es_window_clear(ES_Window window, ES_Color color);
ES_RUNTIME_EXPORT void ES_API es_window_update(ES_Window window);
ES_RUNTIME_EXPORT int ES_API es_window_is_open(ES_Window window);
ES_RUNTIME_EXPORT void ES_API es_draw_rect(ES_Window window, ES_Rect rect, ES_Color color);
ES_RUNTIME_EXPORT void ES_API es_draw_circle(ES_Window window, ES_Point center, int radius, ES_Color color);
ES_RUNTIME_EXPORT void ES_API es_draw_text(ES_Window window, const char* text, ES_Point position, ES_Color color);


ES_RUNTIME_EXPORT void ES_API es_exit(int code);

#endif 