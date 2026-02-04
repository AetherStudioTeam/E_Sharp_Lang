#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#if !defined(_WIN32) && !defined(__MINGW32__)
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif


extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern int memcmp(const void *s1, const void *s2, size_t n);


#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "../core/platform/platform.h"
#include "runtime.h"
#include "../core/utils/es_string.h"
#include "../core/utils/es_common.h"


#if defined(_WIN32) || defined(__MINGW32__)
    #define ES_RUNTIME_EXPORT __declspec(dllexport)
#else
    
    #define ES_RUNTIME_EXPORT 
#endif


ES_RUNTIME_EXPORT void ES_API _print_string(const char* str) {
    if (!str) return;
    
#ifdef _WIN32
    
    static int console_initialized = 0;
    if (!console_initialized) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        
        
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hStdOut, &mode)) {
            SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        
        console_initialized = 1;
    }
    
    
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    
    
    size_t len = 0;
    while (str[len]) len++;
    
    
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, NULL, 0);
    if (wlen > 0) {
        wchar_t* wstr = (wchar_t*)malloc(wlen * sizeof(wchar_t));
        if (wstr) {
            MultiByteToWideChar(CP_UTF8, 0, str, (int)len, wstr, wlen);
            WriteConsoleW(hStdOut, wstr, wlen, &written, NULL);
            free(wstr);
            return;
        }
    }
    
    
    WriteConsole(hStdOut, str, (DWORD)len, &written, NULL);
#else
    size_t len = 0;
    while (str[len]) len++;
    size_t written;
    es_write_console(ES_STDOUT_HANDLE, str, len, &written);
#endif
}

ES_RUNTIME_EXPORT void ES_API _print_number(int num) {
    char buffer[32];
    int i = 30;
    int negative = 0;
    
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    buffer[31] = '\0';
    
    do {
        buffer[i--] = (num % 10) + '0';
        num /= 10;
    } while (num > 0);
    
    if (negative) {
        buffer[i--] = '-';
    }
    
    _print_string(&buffer[i + 1]);
}

ES_RUNTIME_EXPORT void ES_API _print_int(int num) {
    _print_number(num);
}

ES_RUNTIME_EXPORT void ES_API _print_double(double num) {
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "%.6g", num);
    _print_string(buffer);
}

ES_RUNTIME_EXPORT void ES_API _print_int64(long long num) {
    char buffer[32];
    int i = 30;
    int negative = 0;
    
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    buffer[31] = '\0';
    
    do {
        buffer[i--] = (num % 10) + '0';
        num /= 10;
    } while (num > 0);
    
    if (negative) {
        buffer[i--] = '-';
    }
    
    _print_string(&buffer[i + 1]);
}

ES_RUNTIME_EXPORT void ES_API _print_float(double num) {
    
    if (isnan(num)) {
        _print_string("NaN");
        return;
    }
    
    if (isinf(num)) {
        if (num < 0) {
            _print_string("-Infinity");
        } else {
            _print_string("Infinity");
        }
        return;
    }
    
    
    if (num < 0) {
        _print_string("-");
        num = -num;
    }
    
    
    double int_part_d;
    if (modf(num, &int_part_d) < 1e-10) {
        _print_number((int)int_part_d);
        return;
    }
    
    
    int int_part = (int)num;
    _print_number(int_part);
    
    
    _print_string(".");
    num -= int_part;
    
    
    int precision = 6; 
    
    
    if (num < 0.001 && num > 0) {
        precision = 10;
    }
    
    
    if (int_part >= 1000000) {
        precision = 2;
    } else if (int_part >= 1000) {
        precision = 4;
    }
    
    
    for (int i = 0; i < precision; i++) {
        num *= 10;
        int digit = (int)num;
        _print_number(digit);
        num -= digit;
    }
    
    
    
}

ES_RUNTIME_EXPORT void ES_API _print_char(char c) {
#ifdef _WIN32
    DWORD written;
    es_write_console(ES_STDOUT_HANDLE, &c, 1, &written);
#else
    size_t written;
    es_write_console(ES_STDOUT_HANDLE, &c, 1, &written);
#endif
}

ES_RUNTIME_EXPORT void ES_API _print_newline(void) {
    _print_string("\n");
}

ES_RUNTIME_EXPORT int ES_API _read_char(void) {
#ifdef _WIN32
    char c;
    DWORD read;
    es_read_console(ES_STDIN_HANDLE, &c, 1, &read, NULL);
    return (read > 0) ? c : -1;
#else
    char c;
    ssize_t bytes_read_result;
    es_read_console(ES_STDIN_HANDLE, &c, 1, &bytes_read_result, NULL);
    return (bytes_read_result > 0) ? c : -1;
#endif
}

ES_RUNTIME_EXPORT int ES_API _pow_int(int base, int exp) {
    if (exp < 0) return 0;
    if (exp == 0) return 1;
    int result = 1;
    while (exp > 0) {
        if (exp & 1) result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}


ES_RUNTIME_EXPORT void* ES_API es_malloc(size_t size) {
    if (size == 0) return NULL;
    
#if defined(_WIN32) || defined(__MINGW32__)
    return es_malloc_impl(size);
#else
    
    return malloc(size);
#endif
}

ES_RUNTIME_EXPORT void ES_API es_free(void* ptr) {
    if (!ptr) return;
    
#if defined(_WIN32) || defined(__MINGW32__)
    es_free_impl(ptr);
#else
    
    free(ptr);
#endif
}

ES_RUNTIME_EXPORT void* ES_API es_realloc(void* ptr, size_t size) {
    if (!ptr) return es_malloc(size);
    if (size == 0) {
        es_free(ptr);
        return NULL;
    }
    
#if defined(_WIN32) || defined(__MINGW32__)
    return es_realloc_impl(ptr, size);
#else
    
    return realloc(ptr, size);
#endif
}

ES_RUNTIME_EXPORT void* ES_API es_calloc(size_t num, size_t size) {
    if (num == 0 || size == 0) return NULL;
    
#if defined(_WIN32) || defined(__MINGW32__)
    return es_calloc_impl(num, size);
#else
    
    return calloc(num, size);
#endif
}


ES_RUNTIME_EXPORT size_t ES_API es_strlen(const char* str) {
    if (!str) return 0;
    
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

ES_RUNTIME_EXPORT char* ES_API es_strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    
    
    char* orig_dest = dest;
    size_t copied = 0;
    
    
    while (*src && copied < 65536) { 
        *dest++ = *src++;
        copied++;
    }
    *dest = '\0';
    
    return orig_dest;
}

ES_RUNTIME_EXPORT char* ES_API es_strcat(char* dest, const char* src) {
    if (!dest || !src) return dest;
    
    char* orig_dest = dest;
    
    
    size_t dest_len = 0;
    while (*dest && dest_len < 65536) {
        dest++;
        dest_len++;
    }
    
    
    size_t copied = 0;
    while (*src && (dest_len + copied) < 65536) {
        *dest++ = *src++;
        copied++;
    }
    *dest = '\0';
    
    return orig_dest;
}

ES_RUNTIME_EXPORT char* ES_API es_strdup(const char* src) {
    if (!src) return NULL;
    
    size_t len = es_strlen(src);
    char* dest = (char*)es_malloc(len + 1);
    if (!dest) return NULL;
    
    es_strcpy(dest, src);
    return dest;
}

ES_RUNTIME_EXPORT int ES_API es_strcmp(const char* str1, const char* str2) {
    if (!str1) return !str2 ? 0 : -1;
    if (!str2) return 1;
    
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}


ES_RUNTIME_EXPORT double ES_API es_sin(double x) {
    
    double result = 0.0;
    double term = x;
    int sign = 1;
    
    
    const double PI = 3.14159265358979323846;
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    
    
    for (int i = 1; i <= 10; i++) {
        result += sign * term;
        
        
        term *= x * x / ((2 * i) * (2 * i + 1));
        sign = -sign;
    }
    
    return result;
}

ES_RUNTIME_EXPORT double ES_API es_cos(double x) {
    
    double result = 0.0;
    double term = 1.0;
    int sign = 1;
    
    
    const double PI = 3.14159265358979323846;
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    
    
    for (int i = 0; i < 10; i++) {
        result += sign * term;
        
        
        term *= x * x / ((2 * i + 1) * (2 * i + 2));
        sign = -sign;
    }
    
    return result;
}

ES_RUNTIME_EXPORT double ES_API es_tan(double x) {
    
    double cos_val = es_cos(x);
    if (cos_val == 0.0) {
        
        return (x > 0) ? 1e10 : -1e10;
    }
    return es_sin(x) / cos_val;
}

ES_RUNTIME_EXPORT double ES_API es_sqrt(double x) {
    if (x < 0) return 0.0;
    if (x == 0) return 0.0;
    
    
    double guess = x / 2.0;
    double prev = 0.0;
    
    while (guess != prev) {
        prev = guess;
        guess = (guess + x / guess) / 2.0;
    }
    
    return guess;
}

ES_RUNTIME_EXPORT double ES_API es_pow(double base, double exp) {
    
    if (exp == 0) return 1.0;
    if (base == 0) return 0.0;
    
    double result = 1.0;
    int int_exp = (int)exp;
    
    if (int_exp > 0) {
        for (int i = 0; i < int_exp; i++) {
            result *= base;
        }
    } else {
        for (int i = 0; i < -int_exp; i++) {
            result /= base;
        }
    }
    
    return result;
}


ES_RUNTIME_EXPORT double ES_API es_time(void) {
    return (double)es_time_ms() / 1000.0;
}

ES_RUNTIME_EXPORT void ES_API es_sleep(int seconds) {
    es_sleep_ms(seconds * 1000);
}


ES_RUNTIME_EXPORT void ES_API es_exit(int code) {
#ifdef _WIN32
    ExitProcess(code);
#else
    _exit(code);
#endif
}


static unsigned int es_random_seed = 1;

ES_RUNTIME_EXPORT void ES_API es_srand(unsigned int seed) {
    es_random_seed = seed;
}

ES_RUNTIME_EXPORT int ES_API es_rand(void) {
    
    es_random_seed = es_random_seed * 1103515245 + 12345;
    return (unsigned int)(es_random_seed / 65536) % 32768;
}


ES_RUNTIME_EXPORT int ES_API es_abs(int x) {
    return (x < 0) ? -x : x;
}

ES_RUNTIME_EXPORT double ES_API es_fabs(double x) {
    return (x < 0) ? -x : x;
}


ES_RUNTIME_EXPORT int ES_API es_atoi(const char* str) {
    if (!str) return 0;
    
    int result = 0;
    int sign = 1;
    
    
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

ES_RUNTIME_EXPORT double ES_API es_atof(const char* str) {
    if (!str) return 0.0;
    
    double result = 0.0;
    double fraction = 0.0;
    double divisor = 1.0;
    int sign = 1;
    
    
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10.0 + (*str - '0');
        str++;
    }
    
    
    if (*str == '.') {
        str++;
        while (*str >= '0' && *str <= '9') {
            fraction = fraction * 10.0 + (*str - '0');
            divisor *= 10.0;
            str++;
        }
        result += fraction / divisor;
    }
    
    return sign * result;
}




ES_RUNTIME_EXPORT char* ES_API _es_int_to_string_value(int64_t num) {
    static char buffer[64];
    int i = 62;
    int negative = 0;
    
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    buffer[63] = '\0';
    
    do {
        buffer[i--] = (num % 10) + '0';
        num /= 10;
    } while (num > 0);
    
    if (negative) {
        buffer[i--] = '-';
    }
    
    return es_strdup(&buffer[i + 1]);
}


ES_RUNTIME_EXPORT char* ES_API es_int_to_string(int64_t num) {
    return _es_int_to_string_value(num);
}


ES_RUNTIME_EXPORT char* ES_API _es_double_to_string_value(double num) {
    static char buffer[64];
    
    snprintf(buffer, sizeof(buffer), "%f", num);
    
    char* dot = strchr(buffer, '.');
    if (dot) {
        char* p = buffer + strlen(buffer) - 1;
        while (p > dot && *p == '0') {
            *p-- = '\0';
        }
        if (*p == '.') {
            *p = '\0';
        }
    }
    return es_strdup(buffer);
}


ES_RUNTIME_EXPORT char* ES_API _es_strcat(const char* str1, const char* str2) {
    if (!str1 && !str2) return NULL;
    if (!str1) return es_strdup(str2);
    if (!str2) return es_strdup(str1);
    
    size_t len1 = es_strlen(str1);
    size_t len2 = es_strlen(str2);
    char* result = (char*)es_malloc(len1 + len2 + 1);
    
    if (!result) return NULL;
    
    es_strcpy(result, str1);
    es_strcat(result, str2);
    
    return result;
}



static ULONGLONG g_timer_start_ticks_dbl = 0;
static ULONGLONG g_timer_start_ticks_int = 0;

ES_RUNTIME_EXPORT double ES_API timer_start(void) {
    g_timer_start_ticks_dbl = GetTickCount64();
    return (double)g_timer_start_ticks_dbl / 1000.0;
}

ES_RUNTIME_EXPORT double ES_API timer_elapsed(void) {
    if (g_timer_start_ticks_dbl == 0) {
        return -1.0;
    }
    ULONGLONG current = GetTickCount64();
    ULONGLONG elapsed = (current >= g_timer_start_ticks_dbl) ? (current - g_timer_start_ticks_dbl) : (g_timer_start_ticks_dbl - current);
    return (double)elapsed / 1000.0;
}

ES_RUNTIME_EXPORT double ES_API timer_current(void) {
    return (double)GetTickCount64() / 1000.0;
}

ES_RUNTIME_EXPORT long long ES_API timer_start_int(void) {
    g_timer_start_ticks_int = GetTickCount64();
    return (long long)g_timer_start_ticks_int;
}

ES_RUNTIME_EXPORT long long ES_API timer_elapsed_int(void) {
    if (g_timer_start_ticks_int == 0) {
        return -1;
    }
    ULONGLONG current = GetTickCount64();
    ULONGLONG elapsed = (current >= g_timer_start_ticks_int) ? (current - g_timer_start_ticks_int) : (g_timer_start_ticks_int - current);
    return (long long)elapsed;
}

ES_RUNTIME_EXPORT long long ES_API timer_current_int(void) {
    return (long long)GetTickCount64();
}


ES_RUNTIME_EXPORT void ES_API print_number(double num) {
    _print_number(num);
}

ES_RUNTIME_EXPORT void ES_API print_int(long long num) {
    _print_int64(num);
}

ES_RUNTIME_EXPORT void ES_API print_int64(long long num) {
    _print_int64(num);
}

ES_RUNTIME_EXPORT void ES_API es_print(const char* str) {
    _print_string(str);
}

ES_RUNTIME_EXPORT void ES_API es_println(const char* str) {
    _print_string(str);
    _print_newline();
}

ES_RUNTIME_EXPORT void ES_API print_string(const char* str) {
    _print_string(str);
}

ES_RUNTIME_EXPORT void ES_API _println_string(const char* str) {
    _print_string(str);
    _print_newline();
}


ES_RUNTIME_EXPORT void ES_API Console__WriteLine(const char* str) {
    _print_string(str);
    _print_newline();
}

ES_RUNTIME_EXPORT void ES_API Console__Write(const char* str) {
    _print_string(str);
}

ES_RUNTIME_EXPORT void ES_API Console__WriteLineInt(int num) {
    _print_number(num);
    _print_newline();
}

ES_RUNTIME_EXPORT void ES_API Console__WriteInt(int num) {
    _print_number(num);
}


ES_RUNTIME_EXPORT ES_FILE ES_API es_fopen(const char* filename, const char* mode) {
    if (!filename || !mode) return NULL;
    
#ifdef _WIN32
    FILE* file = NULL;
    errno_t err = fopen_s(&file, filename, mode);
    return (err == 0) ? (ES_FILE)file : NULL;
#else
    return (ES_FILE)fopen(filename, mode);
#endif
}

ES_RUNTIME_EXPORT int ES_API es_fclose(ES_FILE file) {
    if (!file) return EOF;
    
    return fclose((FILE*)file);
}

ES_RUNTIME_EXPORT size_t ES_API es_fread(void* buffer, size_t size, size_t count, ES_FILE file) {
    if (!buffer || !file || size == 0 || count == 0) return 0;
    
    return fread(buffer, size, count, (FILE*)file);
}

ES_RUNTIME_EXPORT size_t ES_API es_fwrite(const void* buffer, size_t size, size_t count, ES_FILE file) {
    if (!buffer || !file || size == 0 || count == 0) return 0;
    
    return fwrite(buffer, size, count, (FILE*)file);
}

ES_RUNTIME_EXPORT int ES_API es_fseek(ES_FILE file, long offset, int origin) {
    if (!file) return -1;
    
    return fseek((FILE*)file, offset, origin);
}

ES_RUNTIME_EXPORT long ES_API es_ftell(ES_FILE file) {
    if (!file) return -1;
    
    return ftell((FILE*)file);
}

ES_RUNTIME_EXPORT int ES_API es_feof(ES_FILE file) {
    if (!file) return 1;
    
    return feof((FILE*)file);
}

ES_RUNTIME_EXPORT int ES_API es_remove(const char* filename) {
    if (!filename) return -1;
    
#ifdef _WIN32
    return _unlink(filename);
#else
    return remove(filename);
#endif
}

ES_RUNTIME_EXPORT int ES_API es_rename(const char* oldname, const char* newname) {
    if (!oldname || !newname) return -1;
    
    return rename(oldname, newname);
}


ES_RUNTIME_EXPORT char* ES_API es_strchr(const char* str, int c) {
    if (!str) return NULL;
    
    while (*str) {
        if (*str == (char)c) {
            return (char*)str;
        }
        str++;
    }
    
    return (c == '\0') ? (char*)str : NULL;
}

ES_RUNTIME_EXPORT char* ES_API es_strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    
    if (*needle == '\0') {
        return (char*)haystack;
    }
    
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        
        if (*n == '\0') {
            return (char*)haystack;
        }
        
        haystack++;
    }
    
    return NULL;
}

static char* es_strtok_last = NULL;

ES_RUNTIME_EXPORT char* ES_API es_strtok(char* str, const char* delim) {
    if (!delim) return NULL;
    
    if (str) {
        es_strtok_last = str;
    } else if (!es_strtok_last) {
        return NULL;
    }
    
    
    while (*es_strtok_last && es_strchr(delim, *es_strtok_last)) {
        es_strtok_last++;
    }
    
    if (*es_strtok_last == '\0') {
        es_strtok_last = NULL;
        return NULL;
    }
    
    char* token = es_strtok_last;
    
    
    while (*es_strtok_last && !es_strchr(delim, *es_strtok_last)) {
        es_strtok_last++;
    }
    
    if (*es_strtok_last) {
        *es_strtok_last = '\0';
        es_strtok_last++;
    } else {
        es_strtok_last = NULL;
    }
    
    return token;
}

ES_RUNTIME_EXPORT char* ES_API es_strupr(char* str) {
    if (!str) return NULL;
    
    char* p = str;
    while (*p) {
        if (*p >= 'a' && *p <= 'z') {
            *p = *p - 'a' + 'A';
        }
        p++;
    }
    
    return str;
}

ES_RUNTIME_EXPORT char* ES_API es_strlwr(char* str) {
    if (!str) return NULL;
    
    char* p = str;
    while (*p) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = *p - 'A' + 'a';
        }
        p++;
    }
    
    return str;
}


ES_RUNTIME_EXPORT double ES_API es_log(double x) {
    if (x <= 0.0) return -1.0;  
    
    
    
    double y = (x - 1.0) / (x + 1.0);
    double result = 0.0;
    double term = y;
    double y_squared = y * y;
    
    
    for (int i = 1; i < 20; i++) {
        result += term / (2 * i - 1);
        term *= y_squared;
    }
    
    return 2.0 * result;
}

ES_RUNTIME_EXPORT double ES_API es_log10(double x) {
    if (x <= 0.0) return -1.0;  
    
    
    const double LOG_10 = 2.30258509299404568402;  
    return es_log(x) / LOG_10;
}

ES_RUNTIME_EXPORT double ES_API es_exp(double x) {
    
    double result = 1.0;
    double term = 1.0;
    
    
    for (int i = 1; i < 30; i++) {
        term *= x / i;
        result += term;
        
        
        if (term < 1e-15) break;
    }
    
    return result;
}

ES_RUNTIME_EXPORT double ES_API es_asin(double x) {
    if (x < -1.0 || x > 1.0) return 0.0;  
    
    
    
    double sqrt_val = es_sqrt(1.0 - x * x);
    if (sqrt_val == 0.0) {
        return (x > 0) ? 1.57079632679489661923 : -1.57079632679489661923;  
    }
    
    return es_atan(x / sqrt_val);
}

ES_RUNTIME_EXPORT double ES_API es_acos(double x) {
    if (x < -1.0 || x > 1.0) return 0.0;  
    
    
    const double PI_2 = 1.57079632679489661923;  
    return PI_2 - es_asin(x);
}

ES_RUNTIME_EXPORT double ES_API es_atan(double x) {
    
    
    
    
    const double PI_2 = 1.57079632679489661923;  
    
    if (x > 1.0) {
        return PI_2 - es_atan(1.0 / x);
    } else if (x < -1.0) {
        return -PI_2 - es_atan(1.0 / x);
    }
    
    
    double result = 0.0;
    double term = x;
    double x_squared = x * x;
    
    for (int i = 1; i < 30; i++) {
        if (i % 2 == 1) {
            result += term / i;
        } else {
            result -= term / i;
        }
        
        term *= x_squared;
        
        
        if (term < 1e-15) break;
    }
    
    return result;
}


ES_RUNTIME_EXPORT ES_Array* ES_API array_create(size_t element_size, size_t initial_capacity) {
    if (element_size == 0) return NULL;
    
    ES_Array* array = (ES_Array*)es_malloc(sizeof(ES_Array));
    if (!array) return NULL;
    
    
    if (initial_capacity == 0) {
        initial_capacity = 8;
    }
    
    array->data = es_malloc(element_size * initial_capacity);
    if (!array->data) {
        ES_FREE(array);
        return NULL;
    }
    
    array->element_size = element_size;
    array->size = 0;
    array->capacity = initial_capacity;
    
    return array;
}

ES_RUNTIME_EXPORT void ES_API array_ES_FREE(ES_Array* array) {
    if (!array) return;
    
    ES_FREE(array->data);
    ES_FREE(array);
}

ES_RUNTIME_EXPORT size_t ES_API array_size(ES_Array* array) {
    if (!array) return 0;
    return array->size;
}

ES_RUNTIME_EXPORT size_t ES_API array_capacity(ES_Array* array) {
    if (!array) return 0;
    return array->capacity;
}

ES_RUNTIME_EXPORT int ES_API array_resize(ES_Array* array, size_t new_capacity) {
    if (!array || new_capacity == 0) return -1;
    
    
    if (new_capacity < array->size) {
        array->size = new_capacity;
    }
    
    void* new_data = es_realloc(array->data, array->element_size * new_capacity);
    if (!new_data) return -1;
    
    array->data = new_data;
    array->capacity = new_capacity;
    
    return 0;
}




static ES_ErrorCode last_error = ES_ERR_NONE;


ES_RUNTIME_EXPORT void ES_API es_error(ES_ErrorCode code) {
    last_error = code;
}


ES_RUNTIME_EXPORT const char* ES_API es_strerror(ES_ErrorCode code) {
    switch (code) {
        case ES_ERR_NONE:
            return "No error";
        case ES_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case ES_ERR_INVALID_ARGUMENT:
            return "Invalid argument";
        case ES_ERR_FILE_NOT_FOUND:
            return "File not found";
        case ES_ERR_PERMISSION_DENIED:
            return "Permission denied";
        case ES_ERR_IO_ERROR:
            return "I/O error";
        case ES_ERR_INDEX_OUT_OF_BOUNDS:
            return "Index out of bounds";
        case ES_ERR_DIVISION_BY_ZERO:
            return "Division by zero";
        case ES_ERR_NULL_POINTER:
            return "Null pointer access";
        case ES_ERR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case ES_ERR_UNKNOWN:
        default:
            return "Unknown error";
    }
}




struct ES_HashMap {
    ES_HashMapItem** buckets;
    size_t capacity;
    size_t size;
};


static unsigned int string_hash(const char* str) {
    unsigned int hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; 
    }
    
    return hash;
}


static unsigned int int_hash(int value) {
    return (unsigned int)value;
}


static unsigned int double_hash(double value) {
    union {
        double d;
        unsigned int u[2];
    } converter;
    
    converter.d = value;
    return converter.u[0] ^ converter.u[1];
}


static unsigned int get_hash_value(const void* key, ES_HashMapType type) {
    switch (type) {
        case ES_HASHMAP_TYPE_INT:
            return int_hash(*(int*)key);
        case ES_HASHMAP_TYPE_DOUBLE:
            return double_hash(*(double*)key);
        case ES_HASHMAP_TYPE_STRING:
            return string_hash((const char*)key);
        default:
            return 0;
    }
}


static int keys_equal(const void* key1, ES_HashMapType type1, 
                     const void* key2, ES_HashMapType type2) {
    if (type1 != type2) {
        return 0;
    }
    
    switch (type1) {
        case ES_HASHMAP_TYPE_INT:
            return *(int*)key1 == *(int*)key2;
        case ES_HASHMAP_TYPE_DOUBLE:
            return *(double*)key1 == *(double*)key2;
        case ES_HASHMAP_TYPE_STRING:
            return es_strcmp((const char*)key1, (const char*)key2) == 0;
        default:
            return 0;
    }
}


static void* clone_key(const void* key, ES_HashMapType type) {
    switch (type) {
        case ES_HASHMAP_TYPE_INT: {
            int* new_key = (int*)es_malloc(sizeof(int));
            if (new_key) *new_key = *(int*)key;
            return new_key;
        }
        case ES_HASHMAP_TYPE_DOUBLE: {
            double* new_key = (double*)es_malloc(sizeof(double));
            if (new_key) *new_key = *(double*)key;
            return new_key;
        }
        case ES_HASHMAP_TYPE_STRING:
            return es_strdup((const char*)key);
        default:
            return NULL;
    }
}


static void* clone_value(const void* value, ES_HashMapType type) {
    switch (type) {
        case ES_HASHMAP_TYPE_INT: {
            int* new_value = (int*)es_malloc(sizeof(int));
            if (new_value) *new_value = *(int*)value;
            return new_value;
        }
        case ES_HASHMAP_TYPE_DOUBLE: {
            double* new_value = (double*)es_malloc(sizeof(double));
            if (new_value) *new_value = *(double*)value;
            return new_value;
        }
        case ES_HASHMAP_TYPE_STRING:
            return es_strdup((const char*)value);
        default:
            return NULL;
    }
}


static void free_key(void* key, ES_HashMapType type) {
    switch (type) {
        case ES_HASHMAP_TYPE_INT:
        case ES_HASHMAP_TYPE_DOUBLE:
            ES_FREE(key);
            break;
        case ES_HASHMAP_TYPE_STRING:
            ES_FREE(key);
            break;
        default:
            break;
    }
}


static void free_value(void* value, ES_HashMapType type) {
    switch (type) {
        case ES_HASHMAP_TYPE_INT:
        case ES_HASHMAP_TYPE_DOUBLE:
            es_free(value);
            break;
        case ES_HASHMAP_TYPE_STRING:
            ES_FREE(value);
            break;
        default:
            break;
    }
}


ES_RUNTIME_EXPORT ES_HashMap* ES_API hashmap_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 16; 
    }
    
    ES_HashMap* map = (ES_HashMap*)es_malloc(sizeof(ES_HashMap));
    if (!map) return NULL;
    
    map->buckets = (ES_HashMapItem**)es_calloc(initial_capacity, sizeof(ES_HashMapItem*));
    if (!map->buckets) {
        ES_FREE(map);
        return NULL;
    }
    
    map->capacity = initial_capacity;
    map->size = 0;
    
    return map;
}


ES_RUNTIME_EXPORT void ES_API hashmap_ES_FREE(ES_HashMap* map) {
    if (!map) return;
    
    
    for (size_t i = 0; i < map->capacity; i++) {
        ES_HashMapItem* item = map->buckets[i];
        while (item) {
            ES_HashMapItem* next = item->next;
            
            
            free_key(item->key, item->key_type);
            free_value(item->value, item->value_type);
            
            
            ES_FREE(item);
            
            item = next;
        }
    }
    
    
    ES_FREE(map->buckets);
    
    
    ES_FREE(map);
}


ES_RUNTIME_EXPORT int ES_API hashmap_put(ES_HashMap* map, const void* key, ES_HashMapType key_type, 
                                         const void* value, ES_HashMapType value_type) {
    if (!map || !key || !value) return -1;
    
    
    unsigned int hash = get_hash_value(key, key_type);
    size_t index = hash % map->capacity;
    
    
    ES_HashMapItem* item = map->buckets[index];
    while (item) {
        if (keys_equal(item->key, item->key_type, key, key_type)) {
            
            free_value(item->value, item->value_type);
            
            item->value = clone_value(value, value_type);
            if (!item->value) return -1;
            
            item->value_type = value_type;
            return 0; 
        }
        item = item->next;
    }
    
    
    ES_HashMapItem* new_item = (ES_HashMapItem*)es_malloc(sizeof(ES_HashMapItem));
    if (!new_item) return -1;
    
    new_item->key = clone_key(key, key_type);
    if (!new_item->key) {
        ES_FREE(new_item);
        return -1;
    }
    
    new_item->value = clone_value(value, value_type);
    if (!new_item->value) {
        free_key(new_item->key, key_type);
        ES_FREE(new_item);
        return -1;
    }
    
    new_item->key_type = key_type;
    new_item->value_type = value_type;
    new_item->next = map->buckets[index];
    
    
    map->buckets[index] = new_item;
    map->size++;
    
    return 0; 
}


ES_RUNTIME_EXPORT int ES_API hashmap_get(ES_HashMap* map, const void* key, ES_HashMapType key_type, 
                                        void** out_value, ES_HashMapType* out_value_type) {
    if (!map || !key || !out_value || !out_value_type) return -1;
    
    
    unsigned int hash = get_hash_value(key, key_type);
    size_t index = hash % map->capacity;
    
    
    ES_HashMapItem* item = map->buckets[index];
    while (item) {
        if (keys_equal(item->key, item->key_type, key, key_type)) {
            
            *out_value = clone_value(item->value, item->value_type);
            if (!*out_value) return -1;
            
            *out_value_type = item->value_type;
            return 0; 
        }
        item = item->next;
    }
    
    
    *out_value = NULL;
    *out_value_type = ES_HASHMAP_TYPE_INT; 
    return -1;
}


ES_RUNTIME_EXPORT int ES_API hashmap_remove(ES_HashMap* map, const void* key, ES_HashMapType key_type) {
    if (!map || !key) return -1;
    
    
    unsigned int hash = get_hash_value(key, key_type);
    size_t index = hash % map->capacity;
    
    
    ES_HashMapItem* item = map->buckets[index];
    ES_HashMapItem* prev = NULL;
    
    while (item) {
        if (keys_equal(item->key, item->key_type, key, key_type)) {
            
            if (prev) {
                prev->next = item->next;
            } else {
                map->buckets[index] = item->next;
            }
            
            
            free_key(item->key, item->key_type);
            free_value(item->value, item->value_type);
            
            
            ES_FREE(item);
            
            map->size--;
            return 0; 
        }
        
        prev = item;
        item = item->next;
    }
    
    
    return -1;
}




ES_RUNTIME_EXPORT char* ES_API es_date() {
    
    time_t current_time;
    time(&current_time);
    
    
    struct tm* local_time = localtime(&current_time);
    if (!local_time) return NULL;
    
    
    char* date_str = (char*)es_malloc(11); 
    if (!date_str) return NULL;
    
    
    ES_SPRINTF_S(date_str, 11, "%04d-%02d-%02d", 
            local_time->tm_year + 1900, 
            local_time->tm_mon + 1, 
            local_time->tm_mday);
    
    return date_str;
}


ES_RUNTIME_EXPORT char* ES_API es_time_format(const char* format) {
    if (!format) return NULL;
    
    
    time_t current_time;
    time(&current_time);
    
    
    struct tm* local_time = localtime(&current_time);
    if (!local_time) return NULL;
    
    
    size_t format_len = es_strlen(format);
    size_t result_len = format_len * 3; 
    char* result = (char*)es_malloc(result_len + 1); 
    if (!result) return NULL;
    
    
    size_t result_index = 0;
    for (size_t i = 0; i < format_len && result_index < result_len; i++) {
        if (format[i] == '%' && i + 1 < format_len) {
            char format_char = format[i + 1];
            switch (format_char) {
                case 'Y': 
                    result_index += ES_SPRINTF_S(result + result_index, result_len - result_index + 1, "%04d", local_time->tm_year + 1900);
                    i++;
                    break;
                case 'y': 
                    result_index += ES_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", (local_time->tm_year + 1900) % 100);
                    i++;
                    break;
                case 'm': 
                    result_index += ES_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time->tm_mon + 1);
                    i++;
                    break;
                case 'd': 
                    result_index += ES_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time->tm_mday);
                    i++;
                    break;
                case 'H': 
                    result_index += ES_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time->tm_hour);
                    i++;
                    break;
                case 'M': 
                    result_index += ES_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time->tm_min);
                    i++;
                    break;
                case 'S': 
                    result_index += ES_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time->tm_sec);
                    i++;
                    break;
                default:
                    
                    result[result_index++] = format[i];
                    break;
            }
        } else {
            
            result[result_index++] = format[i];
        }
    }
    
    
    result[result_index] = '\0';
    
    
    char* final_result = es_realloc(result, result_index + 1);
    if (final_result) {
        return final_result;
    } else {
        
        return result;
    }
}



ES_RUNTIME_EXPORT int ES_API array_append(ES_Array* array, const void* element) {
    if (!array || !element) return -1;
    
    
    if (array->size >= array->capacity) {
        size_t new_capacity = array->capacity * 2;
        if (new_capacity == 0) new_capacity = 8;
        
        if (array_resize(array, new_capacity) != 0) {
            return -1;
        }
    }
    
    
    char* dest = (char*)array->data + array->size * array->element_size;
    for (size_t i = 0; i < array->element_size; i++) {
        dest[i] = ((char*)element)[i];
    }
    
    array->size++;
    return 0;
}

ES_RUNTIME_EXPORT void* ES_API array_get(ES_Array* array, size_t index) {
    if (!array || index >= array->size) return NULL;
    
    return (char*)array->data + index * array->element_size;
}

ES_RUNTIME_EXPORT int ES_API array_set(ES_Array* array, size_t index, const void* element) {
    if (!array || !element || index >= array->size) return -1;
    
    char* dest = (char*)array->data + index * array->element_size;
    for (size_t i = 0; i < array->element_size; i++) {
        dest[i] = ((char*)element)[i];
    }
    
    return 0;
}




ES_RUNTIME_EXPORT char* ES_API es_strrev(char* str) {
    if (!str) return NULL;
    
    size_t len = es_strlen(str);
    if (len <= 1) return str;
    
    char* start = str;
    char* end = str + len - 1;
    
    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
    
    return str;
}


ES_RUNTIME_EXPORT char* ES_API es_strtrim(char* str) {
    if (!str) return NULL;
    
    
    char* end = str + es_strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    
    
    if (start != str) {
        char* p = str;
        while (*start) {
            *p++ = *start++;
        }
        *p = '\0';
    }
    
    return str;
}


ES_RUNTIME_EXPORT char* ES_API es_strltrim(char* str) {
    if (!str) return NULL;
    
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    
    
    if (start != str) {
        char* p = str;
        while (*start) {
            *p++ = *start++;
        }
        *p = '\0';
    }
    
    return str;
}


ES_RUNTIME_EXPORT char* ES_API es_strrtrim(char* str) {
    if (!str) return NULL;
    
    char* end = str + es_strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    return str;
}


ES_RUNTIME_EXPORT char* ES_API es_strreplace(const char* src, const char* old_sub, const char* new_sub) {
    if (!src || !old_sub) return NULL;
    
    
    if (*old_sub == '\0') {
        return es_strdup(src);
    }
    
    
    const char* new = new_sub ? new_sub : "";
    
    
    size_t src_len = es_strlen(src);
    size_t old_len = es_strlen(old_sub);
    size_t new_len = es_strlen(new);
    
    
    size_t max_len = src_len;
    const char* p = src;
    while ((p = es_strstr(p, old_sub)) != NULL) {
        max_len = max_len - old_len + new_len;
        p += old_len;
    }
    
    
    char* result = (char*)es_malloc(max_len + 1);
    if (!result) return NULL;
    
    
    char* dest = result;
    const char* src_pos = src;
    
    while (*src_pos) {
        const char* match = es_strstr(src_pos, old_sub);
        if (match) {
            
            size_t copy_len = match - src_pos;
            for (size_t i = 0; i < copy_len; i++) {
                *dest++ = *src_pos++;
            }
            
            
            src_pos += old_len;
            
            
            for (size_t i = 0; i < new_len; i++) {
                *dest++ = new[i];
            }
        } else {
            
            while (*src_pos) {
                *dest++ = *src_pos++;
            }
            break;
        }
    }
    
    *dest = '\0';
    return result;
}




ES_RUNTIME_EXPORT double ES_API es_sinh(double x) {
    
    double exp_x = es_exp(x);
    double exp_neg_x = es_exp(-x);
    return (exp_x - exp_neg_x) / 2.0;
}


ES_RUNTIME_EXPORT double ES_API es_cosh(double x) {
    
    double exp_x = es_exp(x);
    double exp_neg_x = es_exp(-x);
    return (exp_x + exp_neg_x) / 2.0;
}


ES_RUNTIME_EXPORT double ES_API es_tanh(double x) {
    
    if (x > 20.0) return 1.0;
    if (x < -20.0) return -1.0;
    
    
    double sinh_val = es_sinh(x);
    double cosh_val = es_cosh(x);
    
    
    if (cosh_val == 0.0) {
        return (sinh_val > 0) ? 1.0 : -1.0;
    }
    
    return sinh_val / cosh_val;
}



#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdi32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif


static int network_initialized = 0;
static void init_network() {
    if (!network_initialized) {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return;
        }
#endif
        network_initialized = 1;
    }
}


ES_RUNTIME_EXPORT ES_Socket ES_API es_socket(int domain, int type, int protocol) {
    init_network();
    
    SOCKET sock = socket(domain, type, protocol);
    if (sock == INVALID_SOCKET) {
        return NULL;
    }
    
    
    SOCKET* socket_ptr = (SOCKET*)es_malloc(sizeof(SOCKET));
    if (!socket_ptr) {
        closesocket(sock);
        return NULL;
    }
    
    *socket_ptr = sock;
    return (ES_Socket)socket_ptr;
}


ES_RUNTIME_EXPORT int ES_API es_connect(ES_Socket socket, const char* address, int port) {
    if (!socket || !address) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    
    if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0) {
        
        struct hostent* host = gethostbyname(address);
        if (!host) return -1;
        memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);
    }
    
    return connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0 ? 0 : -1;
}


ES_RUNTIME_EXPORT int ES_API es_bind(ES_Socket socket, const char* address, int port) {
    if (!socket) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    
    if (!address || *address == '\0') {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0) {
            return -1;
        }
    }
    
    return bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0 ? 0 : -1;
}


ES_RUNTIME_EXPORT int ES_API es_listen(ES_Socket socket, int backlog) {
    if (!socket) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    return listen(sock, backlog) == 0 ? 0 : -1;
}


ES_RUNTIME_EXPORT ES_Socket ES_API es_accept(ES_Socket socket) {
    if (!socket) return NULL;
    
    SOCKET sock = *(SOCKET*)socket;
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    SOCKET client_sock = accept(sock, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) {
        return NULL;
    }
    
    
    SOCKET* socket_ptr = (SOCKET*)es_malloc(sizeof(SOCKET));
    if (!socket_ptr) {
        closesocket(client_sock);
        return NULL;
    }
    
    *socket_ptr = client_sock;
    return (ES_Socket)socket_ptr;
}


ES_RUNTIME_EXPORT int ES_API es_send(ES_Socket socket, const char* data, size_t length) {
    if (!socket || !data) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    return send(sock, data, length, 0);
}


ES_RUNTIME_EXPORT int ES_API es_recv(ES_Socket socket, char* buffer, size_t length) {
    if (!socket || !buffer) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    return recv(sock, buffer, length, 0);
}


ES_RUNTIME_EXPORT int ES_API es_close_socket(ES_Socket socket) {
    if (!socket) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    int result = closesocket(sock);
    
    
    ES_FREE(socket);
    
    return result == 0 ? 0 : -1;
}



#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif


#ifdef _WIN32
DWORD WINAPI thread_wrapper(LPVOID arg) {
    void** params = (void**)arg;
    void (*func)(void*) = (void (*)(void*))params[0];
    void* func_arg = params[1];
    ES_FREE(params);
    
    func(func_arg);
    return 0;
}
#else
void* thread_wrapper(void* arg) {
    void** params = (void**)arg;
    void (*func)(void*) = (void (*)(void*))params[0];
    void* func_arg = params[1];
    ES_FREE(params);
    
    func(func_arg);
    return NULL;
}
#endif


ES_RUNTIME_EXPORT ES_Thread ES_API es_thread_create_func(void (*func)(void*), void* arg) {
    if (!func) return NULL;
    
    
    void** params = (void**)es_malloc(2 * sizeof(void*));
    if (!params) return NULL;
    
    params[0] = (void*)func;
    params[1] = arg;
    
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, thread_wrapper, params, 0, NULL);
    if (!thread) {
        ES_FREE(params);
        return NULL;
    }
    
    
    HANDLE* thread_ptr = (HANDLE*)es_malloc(sizeof(HANDLE));
    if (!thread_ptr) {
        CloseHandle(thread);
        ES_FREE(params);
        return NULL;
    }
    
    *thread_ptr = thread;
    return (ES_Thread)thread_ptr;
#else
    pthread_t* thread_ptr = (pthread_t*)es_malloc(sizeof(pthread_t));
    if (!thread_ptr) {
        ES_FREE(params);
        return NULL;
    }
    
    if (pthread_create(thread_ptr, NULL, thread_wrapper, params) != 0) {
        ES_FREE(thread_ptr);
        ES_FREE(params);
        return NULL;
    }
    
    return (ES_Thread)thread_ptr;
#endif
}


ES_RUNTIME_EXPORT int ES_API es_thread_join_func(ES_Thread thread) {
    if (!thread) return -1;
    
#ifdef _WIN32
    HANDLE h = *(HANDLE*)thread;
    DWORD result = WaitForSingleObject(h, INFINITE);
    if (result == WAIT_OBJECT_0) {
        CloseHandle(h);
        ES_FREE(thread);
        return 0;
    }
    return -1;
#else
    pthread_t t = *(pthread_t*)thread;
    int result = pthread_join(t, NULL);
    ES_FREE(thread);
    return result == 0 ? 0 : -1;
#endif
}


ES_RUNTIME_EXPORT void ES_API es_thread_exit_func(void) {
#ifdef _WIN32
    ExitThread(0);
#else
    pthread_exit(NULL);
#endif
}


ES_RUNTIME_EXPORT ES_Mutex ES_API es_mutex_create_func(void) {
#ifdef _WIN32
    HANDLE mutex = CreateMutex(NULL, FALSE, NULL);
    if (!mutex) return NULL;
    
    
    HANDLE* mutex_ptr = (HANDLE*)es_malloc(sizeof(HANDLE));
    if (!mutex_ptr) {
        CloseHandle(mutex);
        return NULL;
    }
    
    *mutex_ptr = mutex;
    return (ES_Mutex)mutex_ptr;
#else
    pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)es_malloc(sizeof(pthread_mutex_t));
    if (!mutex_ptr) return NULL;
    
    if (pthread_mutex_init(mutex_ptr, NULL) != 0) {
        ES_FREE(mutex_ptr);
        return NULL;
    }
    
    return (ES_Mutex)mutex_ptr;
#endif
}


ES_RUNTIME_EXPORT int ES_API es_mutex_lock_func(ES_Mutex mutex) {
    if (!mutex) return -1;
    
#ifdef _WIN32
    HANDLE h = *(HANDLE*)mutex;
    DWORD result = WaitForSingleObject(h, INFINITE);
    return result == WAIT_OBJECT_0 ? 0 : -1;
#else
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    return pthread_mutex_lock(m) == 0 ? 0 : -1;
#endif
}


ES_RUNTIME_EXPORT int ES_API es_mutex_unlock_func(ES_Mutex mutex) {
    if (!mutex) return -1;
    
#ifdef _WIN32
    HANDLE h = *(HANDLE*)mutex;
    return ReleaseMutex(h) ? 0 : -1;
#else
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    return pthread_mutex_unlock(m) == 0 ? 0 : -1;
#endif
}


ES_RUNTIME_EXPORT void ES_API es_mutex_free_func(ES_Mutex mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    HANDLE h = *(HANDLE*)mutex;
    CloseHandle(h);
#else
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    pthread_mutex_destroy(m);
#endif
    
    ES_FREE(mutex);
}



#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#else


#ifdef HAS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#endif


typedef struct {
#ifdef _WIN32
    HWND hwnd;
    HDC hdc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    void* bitmap_data;
#else
#ifdef HAS_X11
    Display* display;
    Window window;
    GC gc;
    XImage* image;
#endif
    void* bitmap_data;
#endif
    int width;
    int height;
    int is_open;
    char* title;
} ES_WindowImpl;


ES_RUNTIME_EXPORT ES_Window ES_API es_window_create(int width, int height, const char* title) {
    if (width <= 0 || height <= 0) return NULL;
    
    ES_WindowImpl* window = (ES_WindowImpl*)es_malloc(sizeof(ES_WindowImpl));
    if (!window) return NULL;
    
    memset(window, 0, sizeof(ES_WindowImpl));
    window->width = width;
    window->height = height;
    window->is_open = 0;
    
    if (title) {
        window->title = es_strdup(title);
    } else {
        window->title = es_strdup("E# Window");
    }
    
#ifdef _WIN32
    
    static int class_registered = 0;
    if (!class_registered) {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "ESWindow";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        
        if (!RegisterClass(&wc)) {
            ES_FREE(window->title);
            ES_FREE(window);
            return NULL;
        }
        class_registered = 1;
    }
    
    
    window->hwnd = CreateWindow(
        "ESWindow",
        window->title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (!window->hwnd) {
        ES_FREE(window->title);
        ES_FREE(window);
        return NULL;
    }
    
    
    window->hdc = CreateCompatibleDC(NULL);
    window->bitmap = CreateCompatibleBitmap(window->hdc, width, height);
    window->old_bitmap = SelectObject(window->hdc, window->bitmap);
    
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; 
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    window->bitmap_data = es_malloc(width * height * 4);
    if (!window->bitmap_data) {
        SelectObject(window->hdc, window->old_bitmap);
        DeleteObject(window->bitmap);
        DeleteDC(window->hdc);
        DestroyWindow(window->hwnd);
        ES_FREE(window->title);
        ES_FREE(window);
        return NULL;
    }
    
    
    SetDIBits(window->hdc, window->bitmap, 0, height, window->bitmap_data, &bmi, DIB_RGB_COLORS);
#else
#ifdef HAS_X11
    
    window->display = XOpenDisplay(NULL);
    if (!window->display) {
        ES_FREE(window->title);
        ES_FREE(window);
        return NULL;
    }
    
    int screen = DefaultScreen(window->display);
    window->window = XCreateSimpleWindow(
        window->display, 
        RootWindow(window->display, screen),
        0, 0, width, height, 1,
        BlackPixel(window->display, screen),
        WhitePixel(window->display, screen)
    );
    
    XStoreName(window->display, window->window, window->title);
    window->gc = XCreateGC(window->display, window->window, 0, NULL);
    
    
    window->bitmap_data = es_malloc(width * height * 4);
    if (!window->bitmap_data) {
        XFreeGC(window->display, window->gc);
        XDestroyWindow(window->display, window->window);
        XCloseDisplay(window->display);
        ES_FREE(window->title);
        ES_FREE(window);
        return NULL;
    }
    
    
    Visual* visual = DefaultVisual(window->display, screen);
    window->image = XCreateImage(
        window->display, visual, 24, ZPixmap, 0,
        (char*)window->bitmap_data, width, height, 32, 0
    );
#else
    
    ES_FREE(window->title);
    ES_FREE(window);
    return NULL;
#endif
#endif
    
    return (ES_Window)window;
}


ES_RUNTIME_EXPORT void ES_API es_window_show(ES_Window window) {
    if (!window) return;
    
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    win->is_open = 1;
    
#ifdef _WIN32
    ShowWindow(win->hwnd, SW_SHOW);
    UpdateWindow(win->hwnd);
#else
#ifdef HAS_X11
    XMapWindow(win->display, win->window);
    XFlush(win->display);
#endif
#endif
}


ES_RUNTIME_EXPORT void ES_API es_window_close(ES_Window window) {
    if (!window) return;
    
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    win->is_open = 0;
    
#ifdef _WIN32
    DestroyWindow(win->hwnd);
#else
#ifdef HAS_X11
    XDestroyWindow(win->display, win->window);
    XCloseDisplay(win->display);
#endif
#endif
}


ES_RUNTIME_EXPORT void ES_API es_window_clear(ES_Window window, ES_Color color) {
    if (!window) return;
    
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    
    
    unsigned int pixel_color = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    unsigned int* pixels = (unsigned int*)win->bitmap_data;
    
    for (int i = 0; i < win->width * win->height; i++) {
        pixels[i] = pixel_color;
    }
}


ES_RUNTIME_EXPORT void ES_API es_window_update(ES_Window window) {
    if (!window) return;
    
#ifdef _WIN32
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = win->width;
    bmi.bmiHeader.biHeight = -win->height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    SetDIBits(win->hdc, win->bitmap, 0, win->height, win->bitmap_data, &bmi, DIB_RGB_COLORS);
    
    
    HDC hdc = GetDC(win->hwnd);
    BitBlt(hdc, 0, 0, win->width, win->height, win->hdc, 0, 0, SRCCOPY);
    ReleaseDC(win->hwnd, hdc);
#else
#ifdef HAS_X11
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    
    XPutImage(win->display, win->window, win->gc, win->image, 0, 0, 0, 0, win->width, win->height);
    XFlush(win->display);
#endif
#endif
}


ES_RUNTIME_EXPORT int ES_API es_window_is_open(ES_Window window) {
    if (!window) return 0;
    
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    return win->is_open;
}


ES_RUNTIME_EXPORT void ES_API es_draw_rect(ES_Window window, ES_Rect rect, ES_Color color) {
    if (!window) return;
    
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    unsigned int pixel_color = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    unsigned int* pixels = (unsigned int*)win->bitmap_data;
    
    
    int left = rect.x < 0 ? 0 : rect.x;
    int top = rect.y < 0 ? 0 : rect.y;
    int right = (rect.x + rect.width) > win->width ? win->width : rect.x + rect.width;
    int bottom = (rect.y + rect.height) > win->height ? win->height : rect.y + rect.height;
    
    
    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            pixels[y * win->width + x] = pixel_color;
        }
    }
}


ES_RUNTIME_EXPORT void ES_API es_draw_circle(ES_Window window, ES_Point center, int radius, ES_Color color) {
    if (!window || radius <= 0) return;
    
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    unsigned int pixel_color = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    unsigned int* pixels = (unsigned int*)win->bitmap_data;
    
    
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    
    while (x <= y) {
        
        for (int i = -x; i <= x; i++) {
            
            if (center.y + y >= 0 && center.y + y < win->height && 
                center.x + i >= 0 && center.x + i < win->width) {
                pixels[(center.y + y) * win->width + (center.x + i)] = pixel_color;
            }
            if (center.y - y >= 0 && center.y - y < win->height && 
                center.x + i >= 0 && center.x + i < win->width) {
                pixels[(center.y - y) * win->width + (center.x + i)] = pixel_color;
            }
        }
        
        for (int i = -y; i <= y; i++) {
            
            if (center.y + x >= 0 && center.y + x < win->height && 
                center.x + i >= 0 && center.x + i < win->width) {
                pixels[(center.y + x) * win->width + (center.x + i)] = pixel_color;
            }
            if (center.y - x >= 0 && center.y - x < win->height && 
                center.x + i >= 0 && center.x + i < win->width) {
                pixels[(center.y - x) * win->width + (center.x + i)] = pixel_color;
            }
        }
        
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}


ES_RUNTIME_EXPORT void ES_API es_draw_text(ES_Window window, const char* text, ES_Point position, ES_Color color) {
    if (!window || !text) return;
    
    ES_WindowImpl* win = (ES_WindowImpl*)window;
    unsigned int pixel_color = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    unsigned int* pixels = (unsigned int*)win->bitmap_data;
    
    
    int char_width = 8;
    int char_height = 16;
    
    
    
    static const unsigned char font_data[128][16] = {
        
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        
        {0x00, 0x00, 0x18, 0x3C, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00},
        
    };
    
    
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 126) continue; 
        
        
        int char_x = position.x + i * char_width;
        int char_y = position.y;
        
        
        for (int row = 0; row < char_height; row++) {
            unsigned char font_row = font_data[c][row];
            for (int col = 0; col < char_width; col++) {
                if (font_row & (0x80 >> col)) {
                    int pixel_x = char_x + col;
                    int pixel_y = char_y + row;
                    
                    
                    if (pixel_x >= 0 && pixel_x < win->width && 
                        pixel_y >= 0 && pixel_y < win->height) {
                        pixels[pixel_y * win->width + pixel_x] = pixel_color;
                    }
                }
            }
        }
    }
}


uint64_t get_total_memory(void) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullTotalPhys;
    }
    return 0;
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (uint64_t)info.totalram * info.mem_unit;
    }
    return 0;
#endif
}

uint64_t get_free_memory(void) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullAvailPhys;
    }
    return 0;
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (uint64_t)info.freeram * info.mem_unit;
    }
    return 0;
#endif
}