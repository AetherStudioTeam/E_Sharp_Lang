#include "es_standalone.h"
#include <stddef.h>

int main() {
    
    es_printf("=== E# Standard Library Test ===\n\n");
    
    
    es_printf("[Integer Test]\n");
    es_printf("  12345 = %d\n", 12345);
    es_printf("  -999 = %d\n", -999);
    es_printf("  Hex: 255 = 0x%X\n\n", 255);
    
    
    es_printf("[String Test]\n");
    char buf[64];
    es_strcpy(buf, "Hello");
    es_strcat(buf, " World!");
    es_printf("  Concatenated: %s\n", buf);
    es_printf("  Length: %d\n", es_strlen(buf));
    es_printf("  Compare 'abc' vs 'def': %d\n\n", es_strcmp("abc", "def"));
    
    
    es_printf("[Memory Test]\n");
    char* mem = (char*)es_malloc(32);
    if (mem) {
        es_memset(mem, 'A', 10);
        mem[10] = '\0';
        es_printf("  Memset result: %s\n", mem);
        es_free(mem);
        es_printf("  Memory freed successfully\n\n");
    }
    
    
    es_printf("[Time Test]\n");
    es_time_t t = es_time(NULL);
    es_printf("  Current timestamp: %lld\n\n", t);
    
    
    es_printf("[Format Test]\n");
    es_printf("  String: %s\n", "test");
    es_printf("  Char: %c\n", 'X');
    es_printf("  Pointer: %p\n", (void*)main);
    es_printf("  Float: %f\n\n", 3.14159);
    
    es_printf("=== All Tests Complete ===\n");
    
    return 0;
}
