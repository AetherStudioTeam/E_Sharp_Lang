


#include "es_standalone.h"


static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    es_printf("Testing " #name "... "); \
    test_##name(); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        es_printf("FAILED: %s:%d\n", __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define PASS() do { \
    es_printf("PASSED\n"); \
    tests_passed++; \
} while(0)





TEST(strlen) {
    ASSERT(es_strlen("") == 0);
    ASSERT(es_strlen("a") == 1);
    ASSERT(es_strlen("hello") == 5);
    ASSERT(es_strlen("hello world") == 11);
    PASS();
}

TEST(strcpy) {
    char buf[100];
    es_strcpy(buf, "hello");
    ASSERT(es_strcmp(buf, "hello") == 0);
    
    es_strcpy(buf, "");
    ASSERT(es_strcmp(buf, "") == 0);
    PASS();
}

TEST(strncpy) {
    char buf[100];
    es_strncpy(buf, "hello", 3);
    buf[3] = '\0';
    ASSERT(es_strcmp(buf, "hel") == 0);
    PASS();
}

TEST(strcmp) {
    ASSERT(es_strcmp("a", "b") < 0);
    ASSERT(es_strcmp("b", "a") > 0);
    ASSERT(es_strcmp("abc", "abc") == 0);
    ASSERT(es_strcmp("abc", "abd") < 0);
    PASS();
}

TEST(strncmp) {
    ASSERT(es_strncmp("abc", "abc", 3) == 0);
    ASSERT(es_strncmp("abc", "abd", 2) == 0);
    ASSERT(es_strncmp("abc", "abd", 3) < 0);
    PASS();
}

TEST(strcat) {
    char buf[100] = "hello";
    es_strcat(buf, " world");
    ASSERT(es_strcmp(buf, "hello world") == 0);
    PASS();
}

TEST(strchr) {
    char* p = es_strchr("hello", 'l');
    ASSERT(p != NULL);
    ASSERT(*p == 'l');
    ASSERT(es_strchr("hello", 'x') == NULL);
    PASS();
}

TEST(strstr) {
    char* p = es_strstr("hello world", "world");
    ASSERT(p != NULL);
    ASSERT(es_strcmp(p, "world") == 0);
    ASSERT(es_strstr("hello", "xyz") == NULL);
    PASS();
}

TEST(memcpy) {
    char src[] = "hello";
    char dst[10];
    es_memcpy(dst, src, 6);
    ASSERT(es_strcmp(dst, "hello") == 0);
    PASS();
}

TEST(memset) {
    char buf[10];
    es_memset(buf, 'A', 5);
    buf[5] = '\0';
    ASSERT(es_strcmp(buf, "AAAAA") == 0);
    PASS();
}

TEST(memcmp) {
    ASSERT(es_memcmp("abc", "abc", 3) == 0);
    ASSERT(es_memcmp("abc", "abd", 3) < 0);
    ASSERT(es_memcmp("abd", "abc", 3) > 0);
    PASS();
}





TEST(malloc_free) {
    void* p = es_malloc(100);
    ASSERT(p != NULL);
    es_free(p);
    PASS();
}

TEST(calloc) {
    int* p = (int*)es_calloc(10, sizeof(int));
    ASSERT(p != NULL);
    for (int i = 0; i < 10; i++) {
        ASSERT(p[i] == 0);
    }
    es_free(p);
    PASS();
}

TEST(realloc) {
    char* p = (char*)es_malloc(10);
    ASSERT(p != NULL);
    es_strcpy(p, "hello");
    
    p = (char*)es_realloc(p, 100);
    ASSERT(p != NULL);
    ASSERT(es_strcmp(p, "hello") == 0);
    
    es_free(p);
    PASS();
}

TEST(mempool) {
    es_mempool_t* pool = es_mempool_create(32, 10);
    ASSERT(pool != NULL);
    
    void* p1 = es_mempool_alloc(pool);
    void* p2 = es_mempool_alloc(pool);
    ASSERT(p1 != NULL);
    ASSERT(p2 != NULL);
    ASSERT(p1 != p2);
    
    es_mempool_free(pool, p1);
    es_mempool_free(pool, p2);
    es_mempool_destroy(pool);
    PASS();
}





TEST(sprintf) {
    char buf[100];
    
    es_sprintf(buf, "hello");
    ASSERT(es_strcmp(buf, "hello") == 0);
    
    es_sprintf(buf, "%d", 42);
    ASSERT(es_strcmp(buf, "42") == 0);
    
    es_sprintf(buf, "%s", "world");
    ASSERT(es_strcmp(buf, "world") == 0);
    
    es_sprintf(buf, "%c", 'X');
    ASSERT(es_strcmp(buf, "X") == 0);
    
    PASS();
}

TEST(sprintf_integers) {
    char buf[100];
    
    es_sprintf(buf, "%d", -42);
    ASSERT(es_strcmp(buf, "-42") == 0);
    
    es_sprintf(buf, "%x", 255);
    ASSERT(es_strcmp(buf, "ff") == 0);
    
    es_sprintf(buf, "%X", 255);
    ASSERT(es_strcmp(buf, "FF") == 0);
    
    PASS();
}

TEST(sprintf_width) {
    char buf[100];
    
    es_sprintf(buf, "%5d", 42);
    ASSERT(es_strcmp(buf, "   42") == 0);
    
    es_sprintf(buf, "%-5d", 42);
    ASSERT(es_strcmp(buf, "42   ") == 0);
    
    PASS();
}





TEST(atoi) {
    ASSERT(es_atoi("42") == 42);
    ASSERT(es_atoi("-42") == -42);
    ASSERT(es_atoi("0") == 0);
    PASS();
}

TEST(strtol) {
    ASSERT(es_strtol("42", NULL, 10) == 42);
    ASSERT(es_strtol("0xFF", NULL, 0) == 255);
    ASSERT(es_strtol("077", NULL, 0) == 63);  
    ASSERT(es_strtol("FF", NULL, 16) == 255);
    PASS();
}





static int compare_int(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

TEST(qsort) {
    int arr[] = {5, 2, 8, 1, 9, 3};
    es_qsort(arr, 6, sizeof(int), compare_int);
    
    ASSERT(arr[0] == 1);
    ASSERT(arr[1] == 2);
    ASSERT(arr[2] == 3);
    ASSERT(arr[3] == 5);
    ASSERT(arr[4] == 8);
    ASSERT(arr[5] == 9);
    PASS();
}

TEST(bsearch) {
    int arr[] = {1, 2, 3, 5, 8, 9};
    int key = 5;
    int* result = (int*)es_bsearch(&key, arr, 6, sizeof(int), compare_int);
    
    ASSERT(result != NULL);
    ASSERT(*result == 5);
    
    key = 7;
    result = (int*)es_bsearch(&key, arr, 6, sizeof(int), compare_int);
    ASSERT(result == NULL);
    PASS();
}





TEST(rand) {
    es_srand(12345);
    int r1 = es_rand();
    int r2 = es_rand();
    
    
    ASSERT(r1 != r2);
    
    
    ASSERT(r1 >= 0 && r1 < 32768);
    ASSERT(r2 >= 0 && r2 < 32768);
    
    PASS();
}





int main() {
    es_printf("========================================\n");
    es_printf("E# Standalone Runtime Library Test\n");
    es_printf("========================================\n\n");
    
    
    es_printf("--- String Tests ---\n");
    RUN_TEST(strlen);
    RUN_TEST(strcpy);
    RUN_TEST(strncpy);
    RUN_TEST(strcmp);
    RUN_TEST(strncmp);
    RUN_TEST(strcat);
    RUN_TEST(strchr);
    RUN_TEST(strstr);
    RUN_TEST(memcpy);
    RUN_TEST(memset);
    RUN_TEST(memcmp);
    
    
    es_printf("\n--- Memory Tests ---\n");
    RUN_TEST(malloc_free);
    RUN_TEST(calloc);
    RUN_TEST(realloc);
    RUN_TEST(mempool);
    
    
    es_printf("\n--- Printf Tests ---\n");
    RUN_TEST(sprintf);
    RUN_TEST(sprintf_integers);
    RUN_TEST(sprintf_width);
    
    
    es_printf("\n--- Conversion Tests ---\n");
    RUN_TEST(atoi);
    RUN_TEST(strtol);
    
    
    es_printf("\n--- Sort/Search Tests ---\n");
    RUN_TEST(qsort);
    RUN_TEST(bsearch);
    
    
    es_printf("\n--- Random Tests ---\n");
    RUN_TEST(rand);
    
    
    es_printf("\n========================================\n");
    es_printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    es_printf("========================================\n");
    
    
    es_printf("\n--- Printf Output Test ---\n");
    es_printf("String: %s\n", "Hello, World!");
    es_printf("Integer: %d\n", 42);
    es_printf("Negative: %d\n", -17);
    es_printf("Hex: %x\n", 255);
    es_printf("Pointer: %p\n", (void*)main);
    es_printf("Char: %c\n", 'A');
    es_printf("Width: [%5d]\n", 42);
    es_printf("Left: [%-5d]\n", 42);
    
    return tests_failed > 0 ? 1 : 0;
}
