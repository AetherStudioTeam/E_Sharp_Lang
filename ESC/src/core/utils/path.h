#ifndef ES_PATH_H
#define ES_PATH_H

#include <stddef.h>
#include <stdbool.h>

double es_time_now_seconds(void);
int es_path_exists(const char* path);
int es_create_directory(const char* path);
char* es_get_directory(const char* path);
char* es_get_filename(const char* path);
char* es_get_extension(const char* path);
char* es_join_path(const char* base, const char* relative);
char* es_resolve_path(const char* path);
char* es_get_current_directory(void);
char* es_read_file(const char* filename, size_t* length);
int es_write_file(const char* filename, const char* content, size_t length);
int es_get_executable_directory(char* result, size_t size);

#endif 