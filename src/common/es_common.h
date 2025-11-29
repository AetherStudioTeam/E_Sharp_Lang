#ifndef ES_COMMON_H
#define ES_COMMON_H




#define ES_VERSION_MAJOR 1
#define ES_VERSION_MINOR 0
#define ES_VERSION_PATCH 0
#define ES_VERSION_STRING "1.0.0"


#ifdef DEBUG
    #define ES_DEBUG_ENABLED 1
    #define _POSIX_C_SOURCE 200809L

    #include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>

    #ifndef CLOCK_REALTIME
        #define CLOCK_REALTIME 0
        static inline int clock_gettime(int clk_id, struct timespec *tp) {
            (void)clk_id;
            FILETIME ft;
            ULARGE_INTEGER ull;
            GetSystemTimeAsFileTime(&ft);
            ull.LowPart = ft.dwLowDateTime;
            ull.HighPart = ft.dwHighDateTime;
            ull.QuadPart -= 116444736000000000ULL;
            tp->tv_sec = ull.QuadPart / 10000000ULL;
            tp->tv_nsec = ull.QuadPart % 10000000ULL * 100;
            return 0;
        }
    #endif


    static inline void es_set_console_color(int color) {

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            SetConsoleTextAttribute(hConsole, color);
        }
    }


    #define ES_COLOR_RESET   "\033[0m"
    #define ES_COLOR_RED     "\033[31m"
    #define ES_COLOR_GREEN   "\033[32m"
    #define ES_COLOR_YELLOW  "\033[33m"
    #define ES_COLOR_BLUE    "\033[34m"
    #define ES_COLOR_MAGENTA "\033[35m"
    #define ES_COLOR_CYAN    "\033[36m"
    #define ES_COLOR_WHITE   "\033[37m"
#else
    #define _POSIX_C_SOURCE 200809L
    #include <unistd.h>
    #include <time.h>
    #define ES_COLOR_RESET   "\033[0m"
    #define ES_COLOR_RED     "\033[31m"
    #define ES_COLOR_GREEN   "\033[32m"
    #define ES_COLOR_YELLOW  "\033[33m"
    #define ES_COLOR_BLUE    "\033[34m"
    #define ES_COLOR_MAGENTA "\033[35m"
    #define ES_COLOR_CYAN    "\033[36m"
    #define ES_COLOR_WHITE   "\033[37m"
#endif


    typedef enum {
        ES_DEBUG_LEVEL_TRACE = 0,
        ES_DEBUG_LEVEL_DEBUG = 1,
        ES_DEBUG_LEVEL_INFO  = 2,
        ES_DEBUG_LEVEL_WARN  = 3,
        ES_DEBUG_LEVEL_ERROR = 4
    } EsDebugLevel;


    int es_fprintf(FILE* stream, const char* format, ...);

    #define ES_CURRENT_DEBUG_LEVEL ES_DEBUG_LEVEL_TRACE


    static inline void es_debug_log_internal(EsDebugLevel level, const char* category,
                                           const char* file, int line, const char* func,
                                           const char* fmt, ...) {
        if (level < ES_CURRENT_DEBUG_LEVEL) return;

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        struct tm* tm_info = localtime(&ts.tv_sec);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
        
        (void)ts;

        const char* color = ES_COLOR_WHITE;
        const char* level_str = "TRACE";
        int win_color = 7;

        switch (level) {
            case ES_DEBUG_LEVEL_TRACE:
                color = ES_COLOR_CYAN; level_str = "TRACE"; win_color = 11; break;
            case ES_DEBUG_LEVEL_DEBUG:
                color = ES_COLOR_BLUE; level_str = "DEBUG"; win_color = 9; break;
            case ES_DEBUG_LEVEL_INFO:
                color = ES_COLOR_GREEN; level_str = "INFO"; win_color = 10; break;
            case ES_DEBUG_LEVEL_WARN:
                color = ES_COLOR_YELLOW; level_str = "WARN"; win_color = 14; break;
            case ES_DEBUG_LEVEL_ERROR:
                color = ES_COLOR_RED; level_str = "ERROR"; win_color = 12; break;
        }

        const char* filename = strrchr(file, '/');
        if (!filename) filename = strrchr(file, '\\');
        filename = filename ? filename + 1 : file;

        const char* func_name = func;
        const char* paren = strchr(func, '(');
        if (paren) {
            static char simplified_func[128];
            size_t len = paren - func;
            if (len < sizeof(simplified_func) - 1) {
                strncpy(simplified_func, func, len);
                simplified_func[len] = '\0';
                func_name = simplified_func;
            }
        }

        char message[1024];
        if (fmt && fmt[0] != '\0') {
            va_list args;
            va_start(args, fmt);
            int msg_len = vsnprintf(message, sizeof(message), fmt, args);
            va_end(args);

            if (msg_len >= (int)sizeof(message)) {
                size_t truncate_pos = sizeof(message) - 4;
                if (truncate_pos > 0) {
                    message[truncate_pos] = '.';
                    message[truncate_pos + 1] = '.';
                    message[truncate_pos + 2] = '.';
                    message[truncate_pos + 3] = '\0';
                }
            }
        } else {
            strcpy(message, "(no message)");
        }

        extern int es_output_cache_is_enabled(void);
        extern void es_output_cache_add(const char* format, ...);

        if (es_output_cache_is_enabled()) {

            es_fprintf(stdout, "[%s] %-5s %-8s %s:%d %s() - %s\n",
                      timestamp, level_str, category, filename, line, func_name, message);
        } else {
#ifdef _WIN32

            es_set_console_color(7);
            printf("[%s] ", timestamp);
            es_set_console_color(win_color);
            printf("%-5s ", level_str);
            es_set_console_color(13);
            printf("%-8s ", category);
            es_set_console_color(7);
            printf("%s:%d ", filename, line);
            es_set_console_color(11);
            printf("%s()", func_name);
            es_set_console_color(7);
            printf(" - ");
            es_set_console_color(win_color);
            printf("%s", message);
            es_set_console_color(7);
            printf("\n");
            fflush(stdout);
#else

            printf("%s[%s] %s%-5s%s %s%-8s%s %s%s:%d%s %s%s()%s - %s%s\n",
                   ES_COLOR_WHITE, timestamp,
                   color, level_str, ES_COLOR_WHITE,
                   ES_COLOR_MAGENTA, category, ES_COLOR_WHITE,
                   ES_COLOR_CYAN, filename, line, ES_COLOR_WHITE,
                   ES_COLOR_CYAN, func_name, ES_COLOR_WHITE,
                   color, message, ES_COLOR_RESET);
            fflush(stdout);
#endif
        }
    }


    #define ES_DEBUG_PRINT(...) \
        es_debug_log_internal(ES_DEBUG_LEVEL_DEBUG, "GENERAL", __FILE__, __LINE__, __func__, __VA_ARGS__)

    #define ES_DEBUG_LOG(category, ...) \
        es_debug_log_internal(ES_DEBUG_LEVEL_DEBUG, category, __FILE__, __LINE__, __func__, __VA_ARGS__)


    #define ES_TRACE(...) \
        es_debug_log_internal(ES_DEBUG_LEVEL_TRACE, "TRACE", __FILE__, __LINE__, __func__, __VA_ARGS__)

    #define ES_DEBUG(...) \
        es_debug_log_internal(ES_DEBUG_LEVEL_DEBUG, "DEBUG", __FILE__, __LINE__, __func__, __VA_ARGS__)

    #define ES_INFO(...) \
        es_debug_log_internal(ES_DEBUG_LEVEL_INFO, "INFO", __FILE__, __LINE__, __func__, __VA_ARGS__)

    #define ES_WARN(...) \
        es_debug_log_internal(ES_DEBUG_LEVEL_WARN, "WARN", __FILE__, __LINE__, __func__, __VA_ARGS__)

    #define ES_ERROR(...) \
        es_debug_log_internal(ES_DEBUG_LEVEL_ERROR, "ERROR", __FILE__, __LINE__, __func__, __VA_ARGS__)


    #define ES_LEXER_DEBUG(fmt, ...) ES_DEBUG_LOG("LEXER", fmt, ##__VA_ARGS__)
    #define ES_PARSER_DEBUG(fmt, ...) ES_DEBUG_LOG("PARSER", fmt, ##__VA_ARGS__)
    #define ES_ERROR_LOG(category, ...) es_debug_log_internal(ES_DEBUG_LEVEL_ERROR, category, __FILE__, __LINE__, __func__, __VA_ARGS__)
    #define ES_PARSER_ERROR(fmt, ...) ES_ERROR_LOG("PARSER", fmt, ##__VA_ARGS__)
    #define ES_TYPE_DEBUG(fmt, ...) ES_DEBUG_LOG("TYPE", fmt, ##__VA_ARGS__)
    #define ES_COMPILER_DEBUG(fmt, ...) ES_DEBUG_LOG("COMPILER", fmt, ##__VA_ARGS__)
    #define ES_IR_DEBUG(fmt, ...) ES_DEBUG_LOG("IR", fmt, ##__VA_ARGS__)

#else
    #define ES_DEBUG_ENABLED 0
    #define ES_DEBUG_PRINT(fmt, ...) ((void)0)
    #define ES_DEBUG_LOG(category, fmt, ...) ((void)0)
    #define ES_TRACE(fmt, ...) ((void)0)
    #define ES_DEBUG(fmt, ...) ((void)0)
    #define ES_INFO(fmt, ...) ((void)0)
    #define ES_WARN(fmt, ...) ((void)0)
    #define ES_ERROR(fmt, ...) do { \
        extern int es_output_cache_is_enabled(void); \
        extern void es_output_cache_add_error(const char* format, ...); \
        if (es_output_cache_is_enabled()) { \
            es_output_cache_add_error("[ES_ERROR] " fmt "\n", ##__VA_ARGS__); \
        } else { \
            fprintf(stderr, "[ES_ERROR] " fmt "\n", ##__VA_ARGS__); \
            fflush(stderr); \
        } \
    } while(0)
    #define ES_LEXER_DEBUG(fmt, ...) ((void)0)
    #define ES_PARSER_DEBUG(fmt, ...) ((void)0)
    #define ES_TYPE_DEBUG(fmt, ...) ((void)0)
    #define ES_COMPILER_DEBUG(fmt, ...) ((void)0)
    #define ES_IR_DEBUG(fmt, ...) ((void)0)
#endif

#ifndef ES_PARSER_ERROR
#define ES_PARSER_ERROR(fmt, ...) do { \
    extern int es_output_cache_is_enabled(void); \
    extern void es_output_cache_add_error(const char* format, ...); \
    if (es_output_cache_is_enabled()) { \
        es_output_cache_add_error("[PARSER_ERROR] " fmt "\n", ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[PARSER_ERROR] " fmt "\n", ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while(0)
#endif


#define ES_ERROR_LOC(file, line, fmt, ...) do { \
    extern int es_output_cache_is_enabled(void); \
    extern void es_output_cache_add_error(const char* format, ...); \
    if (es_output_cache_is_enabled()) { \
        es_output_cache_add_error("[ES_ERROR] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[ES_ERROR] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while(0)
#define ES_WARNING(fmt, ...) do { \
    extern int es_output_cache_is_enabled(void); \
    extern void es_output_cache_add_error(const char* format, ...); \
    if (es_output_cache_is_enabled()) { \
        es_output_cache_add_error("[ES_WARNING] " fmt "\n", ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[ES_WARNING] " fmt "\n", ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while(0)
#define ES_WARNING_LOC(file, line, fmt, ...) do { \
    extern int es_output_cache_is_enabled(void); \
    extern void es_output_cache_add_error(const char* format, ...); \
    if (es_output_cache_is_enabled()) { \
        es_output_cache_add_error("[ES_WARNING] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, "[ES_WARNING] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while(0)


#define ES_COMPILE_ERROR(fmt, ...) do { \
    fprintf(stderr, "[ES_COMPILE_ERROR] " fmt "\n", ##__VA_ARGS__); \
    fflush(stderr); \
    exit(1); \
} while(0)

#define ES_COMPILE_ERROR_LOC(file, line, fmt, ...) do { \
    fprintf(stderr, "[ES_COMPILE_ERROR] %s:%d: " fmt "\n", file, line, ##__VA_ARGS__); \
    fflush(stderr); \
    exit(1); \
} while(0)


#define ES_MALLOC(size) es_malloc_checked(size, __FILE__, __LINE__)
#define ES_FREE(ptr) es_free_checked(ptr, __FILE__, __LINE__)
#define ES_CALLOC(count, size) es_calloc_checked(count, size, __FILE__, __LINE__)
#define ES_REALLOC(ptr, size) es_realloc_checked(ptr, size, __FILE__, __LINE__)
#define ES_STRDUP(str) es_strdup_checked(str, __FILE__, __LINE__)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include <time.h>
#include <stdarg.h>

void* es_malloc_checked(size_t size, const char* file, int line);
void* es_calloc_checked(size_t count, size_t size, const char* file, int line);
void* es_realloc_checked(void* ptr, size_t size, const char* file, int line);
char* es_strdup_checked(const char* str, const char* file, int line);
void es_free_checked(void* ptr, const char* file, int line);


#define ES_STRLEN(str) strlen(str)
#define ES_STRCPY(dest, src) do { \
    size_t src_len = strlen(src); \
    if (src_len >= sizeof(dest)) { \
        fprintf(stderr, "[ES_ERROR] Buffer overflow in ES_STRCPY at %s:%d\n", __FILE__, __LINE__); \
        fflush(stderr); \
        exit(1); \
    } \
    strcpy(dest, src); \
} while(0)
#define ES_STRNCPY(dest, src, n) strncpy(dest, src, n)


#ifdef DEBUG
    #define ES_ASSERT(condition) \
        do { \
            if (!(condition)) { \
                fprintf(stderr, "[ES_ASSERT] Assertion failed: %s at %s:%d\n", \
                        #condition, __FILE__, __LINE__); \
                fflush(stderr); \
                exit(1); \
            } \
        } while(0)
#else
    #define ES_ASSERT(condition) ((void)0)
#endif


#ifdef _WIN32
    #define ES_API_EXPORT __declspec(dllexport)
    #define ES_API_IMPORT __declspec(dllimport)
#else
    #define ES_API_EXPORT __attribute__((visibility("default")))
    #define ES_API_IMPORT
#endif


#ifdef ES_COMPILING_DLL
    #define ES_API ES_API_EXPORT
#else
    #define ES_API ES_API_IMPORT
#endif


#if defined(_WIN32) || defined(_WIN64)
    #define ES_PLATFORM_WINDOWS 1
    #define ES_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
    #define ES_PLATFORM_MACOS 1
    #define ES_PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define ES_PLATFORM_LINUX 1
    #define ES_PLATFORM_NAME "Linux"
#else
    #define ES_PLATFORM_UNKNOWN 1
    #define ES_PLATFORM_NAME "Unknown"
#endif


#if defined(__x86_64__) || defined(_M_X64)
    #define ES_ARCH_X64 1
    #define ES_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
    #define ES_ARCH_X86 1
    #define ES_ARCH_NAME "x86"
#elif defined(__arm__) || defined(_M_ARM)
    #define ES_ARCH_ARM 1
    #define ES_ARCH_NAME "ARM"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ES_ARCH_ARM64 1
    #define ES_ARCH_NAME "ARM64"
#else
    #define ES_ARCH_UNKNOWN 1
    #define ES_ARCH_NAME "Unknown"
#endif


#ifdef DEBUG
    #define ES_LEAK_DETECTION_ENABLED 1
#else
    #define ES_LEAK_DETECTION_ENABLED 0
#endif


typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    const char* function;
    uint64_t timestamp;
    const char* type;
} MemoryAllocationInfo;

typedef struct {
    MemoryAllocationInfo* allocations;
    size_t capacity;
    size_t count;
    size_t total_allocated;
    size_t total_freed;
    uint64_t start_time;
    int enabled;
} MemoryLeakDetector;


extern MemoryLeakDetector g_memory_leak_detector;

void es_memory_leak_detector_init(void);
void es_memory_leak_detector_cleanup(void);
void es_record_allocation(void* ptr, size_t size, const char* file, int line,
                         const char* function, const char* type);
void es_record_deallocation(void* ptr, const char* file, int line, const char* function);
void es_report_memory_leaks(void);
void es_get_memory_stats(size_t* current_usage, size_t* peak_usage,
                        size_t* total_allocations, size_t* total_leaks);


#ifdef ES_LEAK_DETECTION_ENABLED
    #define ES_MALLOC_LEAK(size) es_malloc_leak_detected(size, __FILE__, __LINE__)
    #define ES_CALLOC_LEAK(count, size) es_calloc_leak_detected(count, size, __FILE__, __LINE__)
    #define ES_REALLOC_LEAK(ptr, size) es_realloc_leak_detected(ptr, size, __FILE__, __LINE__)
    #define ES_STRDUP_LEAK(str) es_strdup_leak_detected(str, __FILE__, __LINE__)
    #define ES_FREE_LEAK(ptr) es_free_leak_detected(ptr, __FILE__, __LINE__)
#else
    #define ES_MALLOC_LEAK(size) malloc(size)
    #define ES_CALLOC_LEAK(count, size) calloc(count, size)
    #define ES_REALLOC_LEAK(ptr, size) realloc(ptr, size)
    #define ES_STRDUP_LEAK(str) strdup(str)
    #define ES_FREE_LEAK(ptr) free(ptr)
#endif


void* es_malloc_leak_detected(size_t size, const char* file, int line);
void* es_calloc_leak_detected(size_t count, size_t size, const char* file, int line);
void* es_realloc_leak_detected(void* ptr, size_t size, const char* file, int line);
char* es_strdup_leak_detected(const char* str, const char* file, int line);
void es_free_leak_detected(void* ptr, const char* file, int line);


double es_time_now_seconds(void);
int es_path_exists(const char* path);
int es_path_get_filename(const char* path, char* filename, size_t size);
int es_path_remove_extension(const char* path, char* result, size_t size);
int es_path_join(char* result, size_t size, const char* path1, const char* path2);
int es_ensure_directory_recursive(const char* path);

#define ES_COMPILER_INFO (ES_PLATFORM_NAME "-" ES_ARCH_NAME " " ES_VERSION_STRING)

#define printf es_printf

#endif