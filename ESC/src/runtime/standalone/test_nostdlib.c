


#include "es_standalone.h"


int main(int argc, char** argv) {
    es_printf("========================================\n");
    es_printf("E# Standalone Program Test\n");
    es_printf("========================================\n\n");
    
    
    es_printf("Command line arguments:\n");
    es_printf("  argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        es_printf("  argv[%d] = %s\n", i, argv[i]);
    }
    
    
    es_printf("\nMemory test:\n");
    char* buf = (char*)es_malloc(100);
    if (buf) {
        es_strcpy(buf, "Hello from es_malloc!");
        es_printf("  Allocated: %s\n", buf);
        es_free(buf);
        es_printf("  Free successful\n");
    }
    
    
    es_printf("\nString test:\n");
    char str1[100] = "Hello";
    char str2[100] = "World";
    es_strcat(str1, " ");
    es_strcat(str1, str2);
    es_printf("  Concatenated: %s\n", str1);
    es_printf("  Length: %d\n", (int)es_strlen(str1));
    es_printf("  Compare: %d\n", es_strcmp("abc", "def"));
    
    
    es_printf("\nPrintf test:\n");
    es_printf("  Integer: %d\n", 42);
    es_printf("  Negative: %d\n", -17);
    es_printf("  Hex: %x\n", 255);
    es_printf("  String: %s\n", "test");
    es_printf("  Char: %c\n", 'X');
    es_printf("  Width: [%10d]\n", 42);
    es_printf("  Left: [%-10d]\n", 42);
    
    
    es_printf("\nEnvironment test:\n");
    char* path = es_getenv("PATH");
    if (path) {
        es_printf("  PATH is set (length: %d)\n", (int)es_strlen(path));
    } else {
        es_printf("  PATH not found\n");
    }
    
    es_printf("\n========================================\n");
    es_printf("All tests completed successfully!\n");
    es_printf("========================================\n");
    
    return 0;
}
