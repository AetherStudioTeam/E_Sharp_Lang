#include "value.h"
#include "object.h"
#include <stdio.h>

void es_value_array_init(EsValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void es_value_array_write(EsValueArray* array, EsValue value) {
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        array->values = (EsValue*)ES_REALLOC(array->values, sizeof(EsValue) * array->capacity);
    }
    
    array->values[array->count] = value;
    array->count++;
}

void es_value_array_free(EsValueArray* array) {
    ES_FREE(array->values);
    es_value_array_init(array);
}

void es_value_print(EsValue value) {
    switch (value.type) {
        case VAL_BOOL:   printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NULL:   printf("null"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_STRING_LITERAL: printf("%s", AS_STRING_LIT(value)); break;
        case VAL_OBJ: {
            if (IS_STRING_VAL(value)) {
                printf("%s", AS_CSTRING(value));
            } else {
                printf("<obj at %p>", AS_OBJ(value));
            }
            break;
        }
    }
}
