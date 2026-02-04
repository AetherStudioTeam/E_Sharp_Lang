


#ifdef _WIN64



__attribute__((naked)) void ___chkstk_ms(void) {
    __asm__ volatile (
        "movq %rax, %r11\n"
        "leaq 0x18(%rsp), %r10\n"
        "cmpq %r10, %r11\n"
        "jbe 1f\n"
        "subq %rsp, %r11\n"
        "shrq $0xc, %r11\n"
        "2:\n"
        "subq $0x1000, %r10\n"
        "testq %r10, (%r10)\n"
        "decq %r11\n"
        "jne 2b\n"
        "1:\n"
        "ret\n"
    );
}

#endif



void __main(void) {
    
    
}





void* __imp___acrt_iob_func = 0;


void __cxa_atexit(void) { }
void __cxa_finalize(void) { }
