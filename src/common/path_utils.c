#include "es_common.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
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

int es_path_get_filename(const char* path, char* filename, size_t size) {
    if (!path || !filename || size == 0) return -1;

    const char* basename = strrchr(path, '/');
    if (!basename) {
        basename = strrchr(path, '\\');
    }

    if (basename) {
        basename++;
    } else {
        basename = path;
    }

    size_t len = strlen(basename);
    if (len >= size) return -1;

    strcpy(filename, basename);
    return 0;
}

int es_path_remove_extension(const char* path, char* result, size_t size) {
    if (!path || !result || size == 0) return -1;

    const char* dot = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');


    const char* last_sep = slash > backslash ? slash : backslash;


    if (dot && (!last_sep || dot > last_sep)) {
        size_t len = dot - path;
        if (len >= size) return -1;
        strncpy(result, path, len);
        result[len] = '\0';
    } else {

        size_t len = strlen(path);
        if (len >= size) return -1;
        strcpy(result, path);
    }

    return 0;
}

int es_path_join(char* result, size_t size, const char* path1, const char* path2) {
    if (!result || !path1 || !path2 || size == 0) return -1;

    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);


    if (len1 + len2 + 2 > size) return -1;

    strcpy(result, path1);


    if (len1 > 0 && path1[len1-1] != '/' && path1[len1-1] != '\\') {
        #ifdef _WIN32
        strcat(result, "\\");
        #else
        strcat(result, "/");
        #endif
    }

    strcat(result, path2);
    return 0;
}

int es_ensure_directory_recursive(const char* path) {
    if (!path || path[0] == '\0') return -1;

    char temp[1024];
    size_t len = strlen(path);
    if (len >= sizeof(temp)) return -1;

    strcpy(temp, path);


    #ifdef _WIN32
    if (len > 2 && temp[1] == ':') {

        char* p = temp + 2;
        if (*p == '/' || *p == '\\') p++;


        char* slash;
        while ((slash = strchr(p, '/')) || (slash = strchr(p, '\\'))) {
            *slash = '\0';
            if (strlen(temp) > 0 && !es_path_exists(temp)) {
                #ifdef _WIN32
    if (mkdir(temp) != 0 && errno != EEXIST) {
    #else
    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
    #endif
                    return -1;
                }
            }
            *slash = '\\';
            p = slash + 1;
        }

        if (strlen(temp) > 0 && !es_path_exists(temp)) {
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

    char* slash;
    while ((slash = strchr(p, '/')) || (slash = strchr(p, '\\'))) {
        *slash = '\0';
        if (strlen(temp) > 0 && !es_path_exists(temp)) {
            #ifdef _WIN32
            if (mkdir(temp) != 0 && errno != EEXIST) {
            #else
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
            #endif
                return -1;
            }
        }
        #ifdef _WIN32
        *slash = '\\';
        #else
        *slash = '/';
        #endif
        p = slash + 1;
    }

    if (strlen(temp) > 0 && !es_path_exists(temp)) {
        #ifdef _WIN32
        return mkdir(temp) == 0 || errno == EEXIST ? 0 : -1;
        #else
        return mkdir(temp, 0755) == 0 || errno == EEXIST ? 0 : -1;
        #endif
    }

    return 0;
}