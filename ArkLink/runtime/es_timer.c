#include "es_standalone.h"

#ifdef _WIN32
#include <windows.h>

es_time_t es_time(es_time_t* tloc) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    es_time_t result = (ull.QuadPart - 116444736000000000ULL) / 10000000ULL;
    if (tloc) *tloc = result;
    return result;
}

void es_sleep_ms(int ms) {
    Sleep(ms);
}
#else
#include <time.h>
#include <unistd.h>

es_time_t es_time(es_time_t* tloc) {
    es_time_t result = time(NULL);
    if (tloc) *tloc = result;
    return result;
}

void es_sleep_ms(int ms) {
    usleep(ms * 1000);
}
#endif
