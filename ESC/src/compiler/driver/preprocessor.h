#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "../../core/utils/es_common.h"

typedef struct {
    char* name;
    char* replacement;
} Macro;

typedef struct {
    Macro* macros;
    int macro_count;
    int macro_capacity;
} Preprocessor;

Preprocessor* preprocessor_create(void);
void preprocessor_destroy(Preprocessor* preprocessor);
bool preprocessor_add_macro(Preprocessor* preprocessor, const char* name, const char* replacement);
char* preprocessor_process(Preprocessor* preprocessor, const char* source);

#endif