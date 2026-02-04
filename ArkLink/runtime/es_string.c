#include "es_standalone.h"
#include <stddef.h>

int es_strlen(const char* str) {
    if (!str) return 0;
    int len = 0;
    while (str[len]) len++;
    return len;
}

char* es_strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* es_strncpy(char* dest, const char* src, int n) {
    if (!dest || !src || n <= 0) return dest;
    char* d = dest;
    while (n-- && (*d++ = *src++));
    return dest;
}

char* es_strcat(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

int es_strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return s1 == s2 ? 0 : (s1 ? 1 : -1);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int es_strncmp(const char* s1, const char* s2, int n) {
    if (!s1 || !s2 || n <= 0) return 0;
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return n < 0 ? 0 : *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* es_strchr(const char* str, int c) {
    if (!str) return NULL;
    while (*str) {
        if (*str == c) return (char*)str;
        str++;
    }
    return NULL;
}

char* es_strrchr(const char* str, int c) {
    if (!str) return NULL;
    char* last = NULL;
    while (*str) {
        if (*str == c) last = (char*)str;
        str++;
    }
    return last;
}

char* es_strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char*)haystack;
    
    char* h = (char*)haystack;
    while (*h) {
        char* h2 = h;
        char* n = (char*)needle;
        while (*h2 && *n && *h2 == *n) {
            h2++;
            n++;
        }
        if (!*n) return h;
        h++;
    }
    return NULL;
}

void* es_memcpy(void* dest, const void* src, int n) {
    if (!dest || !src || n <= 0) return dest;
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void* es_memmove(void* dest, const void* src, int n) {
    if (!dest || !src || n <= 0) return dest;
    unsigned char* d = dest;
    const unsigned char* s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

void* es_memset(void* s, int c, int n) {
    if (!s || n <= 0) return s;
    unsigned char* p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

int es_memcmp(const void* s1, const void* s2, int n) {
    if (!s1 || !s2 || n <= 0) return 0;
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

int es_atoi(const char* str) {
    if (!str) return 0;
    int sign = 1;
    int result = 0;
    
    while (*str == ' ' || *str == '\t') str++;
    
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

char* es_itoa(int value, char* str, int base) {
    if (!str || base < 2 || base > 36) return str;
    
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    int tmp_value;
    
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);
    
    if (tmp_value < 0 && base == 10) *ptr++ = '-';
    *ptr-- = '\0';
    
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

char* es_ftoa(double f, char* buf, int precision) {
    if (!buf) return buf;
    
    int i = 0;
    if (f < 0) {
        buf[i++] = '-';
        f = -f;
    }
    
    long long int_part = (long long)f;
    double frac_part = f - int_part;
    
    char int_buf[32];
    es_itoa(int_part, int_buf, 10);
    es_strcpy(buf + i, int_buf);
    i += es_strlen(int_buf);
    
    if (precision > 0) {
        buf[i++] = '.';
        
        while (precision-- > 0) {
            frac_part *= 10;
            int digit = (int)frac_part;
            buf[i++] = '0' + digit;
            frac_part -= digit;
        }
    }
    
    buf[i] = '\0';
    return buf;
}
