#include "es_common.h"
#include "es_string.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir _mkdir
#else
#include <time.h>
#endif

double es_time_now_seconds(void) {
    #ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    return ull.QuadPart / 10000000.0;
    #else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    (void)ts;
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
    #endif
}

int es_path_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int es_create_directory(const char* path) {
    if (!path || path[0] == '\0') return -1;
    
#ifdef _WIN32
    return _mkdir(path) == 0 ? 0 : -1;
#else
    return mkdir(path, 0755) == 0 ? 0 : -1;
#endif
}

int es_path_get_filename(const char* path, char* filename, size_t size) {
    if (!path || !filename || size == 0) return -1;

    const char* basename = NULL;
    
    for (const char* p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            basename = p;
        }
    }

    if (basename) {
        basename++;
    } else {
        basename = path;
    }

    size_t len = 0;
    for (const char* p = basename; *p; p++) {
        len++;
    }
    if (len >= size) return -1;

    ES_STRCPY_S(filename, size, basename);
    return 0;
}

int es_path_remove_extension(const char* path, char* result, size_t size) {
    if (!path || !result || size == 0) return -1;

    const char* dot = NULL;
    
    for (const char* p = path; *p; p++) {
        if (*p == '.') {
            dot = p;
        }
    }
    const char* slash = NULL;
    const char* backslash = NULL;
    
    for (const char* p = path; *p; p++) {
        if (*p == '/') {
            slash = p;
        } else if (*p == '\\') {
            backslash = p;
        }
    }
    const char* last_sep = (slash > backslash) ? slash : backslash;


    if (dot && (!last_sep || dot > last_sep)) {
        size_t len = dot - path;
        if (len >= size) return -1;
        
        for (size_t i = 0; i < len && path[i]; i++) {
            result[i] = path[i];
        }
        result[len] = '\0';
        result[len] = '\0';
    } else {

        size_t len = 0;
        for (const char* p = path; *p; p++) {
            len++;
        }
        if (len >= size) return -1;
        ES_STRCPY_S(result, size, path);
    }

    return 0;
}

int es_path_join(char* result, size_t size, const char* path1, const char* path2) {
    if (!result || !path1 || !path2 || size == 0) return -1;

    size_t len1 = 0;
    for (const char* p = path1; *p; p++) {
        len1++;
    }
    size_t len2 = 0;
    for (const char* p = path2; *p; p++) {
        len2++;
    }


    if (len1 + len2 + 2 > size) return -1;

    ES_STRCPY_S(result, size, path1);


    if (len1 > 0 && path1[len1-1] != '/' && path1[len1-1] != '\\') {
        ES_STRCAT_S(result, size, "/");
    }

    ES_STRCAT_S(result, size, path2);
    return 0;
}

int es_ensure_directory_recursive(const char* path) {
    if (!path || path[0] == '\0') return -1;

    char temp[1024];
    size_t len = 0;
    for (const char* p = path; *p; p++) {
        len++;
    }
    if (len >= sizeof(temp)) return -1;

    ES_STRCPY_S(temp, sizeof(temp), path);

    #ifdef _WIN32
    
    if (len > 2 && temp[1] == ':') {
        char* p = temp + 2;
        if (*p == '/' || *p == '\\') p++;

        while (*p) {
            char* slash = NULL;
            for (const char* q = p; *q; q++) {
                if (*q == '/' || *q == '\\') {
                    slash = (char*)q;
                    break;
                }
            }
            if (!slash) break;

            *slash = '\0';
            
            size_t temp_len = 0;
            for (const char* t = temp; *t; t++) {
                temp_len++;
            }
            if (temp_len > 0 && !es_path_exists(temp)) {
                #ifdef _WIN32
                if (mkdir(temp) != 0 && errno != EEXIST) {
                    return -1;
                }
                #else
                if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                    return -1;
                }
                #endif
            }
            *slash = '/';
            p = slash + 1;
        }

        
        size_t temp_len = 0;
        for (const char* t = temp; *t; t++) {
            temp_len++;
        }
        if (temp_len > 0 && !es_path_exists(temp)) {
            #ifdef _WIN32
            return mkdir(temp) == 0 || errno == EEXIST ? 0 : -1;
            #else
            return mkdir(temp, 0755) == 0 || errno == EEXIST ? 0 : -1;
            #endif
        }
        return 0;
    }
    #endif

    
    char* p = temp;
    if (*p == '/' || *p == '\\') p++;

    while (*p) {
        char* slash = NULL;
        for (const char* q = p; *q; q++) {
            if (*q == '/' || *q == '\\') {
                slash = (char*)q;
                break;
            }
        }
        if (!slash) break;

        *slash = '\0';
        
        size_t temp_len = 0;
        for (const char* t = temp; *t; t++) {
            temp_len++;
        }
        if (temp_len > 0 && !es_path_exists(temp)) {
            #ifdef _WIN32
            if (mkdir(temp) != 0 && errno != EEXIST) {
                return -1;
            }
            #else
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            #endif
        }
        *slash = '/';
        p = slash + 1;
    }

    
    size_t temp_len = 0;
    for (const char* t = temp; *t; t++) {
        temp_len++;
    }
    if (temp_len > 0 && !es_path_exists(temp)) {
        #ifdef _WIN32
        return mkdir(temp) == 0 || errno == EEXIST ? 0 : -1;
        #else
        return mkdir(temp, 0755) == 0 || errno == EEXIST ? 0 : -1;
        #endif
    }

    return 0;
}