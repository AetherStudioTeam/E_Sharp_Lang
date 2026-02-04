#include "es_standalone.h"

#ifdef _WIN32
#include <windows.h>
#endif

void es_putchar(char c) {
#ifdef _WIN32
    DWORD written;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), &c, 1, &written, NULL);
#else
    putchar(c);
#endif
}

void es_puts(const char* str) {
    if (!str) return;
#ifdef _WIN32
    DWORD written;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str, es_strlen(str), &written, NULL);
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), "\n", 1, &written, NULL);
#else
    puts(str);
#endif
}

void es_print(const char* str) {
    if (!str) return;
#ifdef _WIN32
    DWORD written;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str, es_strlen(str), &written, NULL);
#else
    printf("%s", str);
#endif
}

void es_println(const char* str) {
    es_print(str);
    es_putchar('\n');
}


void Console__WriteLine(const char* str) {
    es_println(str);
}

void Console__WriteLineInt(int n) {
    es_println_int(n);
}

void es_print_int(int n) {
    char buf[32];
    es_itoa(n, buf, 10);
    es_print(buf);
}

void es_println_int(int n) {
    es_print_int(n);
    es_putchar('\n');
}

void es_print_float(double f, int precision) {
    char buf[64];
    es_ftoa(f, buf, precision);
    es_print(buf);
}

void es_println_float(double f, int precision) {
    es_print_float(f, precision);
    es_putchar('\n');
}
