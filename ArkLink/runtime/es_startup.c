



extern int main(void);


#ifdef _WIN32
#include <windows.h>

void mainCRTStartup(void) {
    int ret = main();
    ExitProcess(ret);
}


void _start(void) {
    mainCRTStartup();
}
#else

void _start(void) {
    int ret = main();
    
    __asm__ volatile (
        "mov %0, %%edi\n"
        "mov $60, %%eax\n"
        "syscall\n"
        :: "r"(ret)
    );
}
#endif


#define MAX_ATEXIT 32
static void (*atexit_funcs[MAX_ATEXIT])(void);
static int atexit_count = 0;

int atexit(void (*func)(void)) {
    if (atexit_count >= MAX_ATEXIT) return 1;
    atexit_funcs[atexit_count++] = func;
    return 0;
}

void es_call_atexit(void) {
    while (atexit_count > 0) {
        atexit_funcs[--atexit_count]();
    }
}
