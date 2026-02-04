#include "es_standalone.h"




void es_assert(int condition, const char* message) {
    if (!condition) {
        es_print("Assertion failed: ");
        es_println(message ? message : "unknown");
        
    }
}


int es_exit_code = 0;

void es_set_exit_code(int code) {
    es_exit_code = code;
}

int es_get_exit_code(void) {
    return es_exit_code;
}
