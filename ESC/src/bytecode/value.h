#ifndef ES_VM_VALUE_H
#define ES_VM_VALUE_H

#include "core/utils/es_common.h"

typedef enum {
    VAL_BOOL,
    VAL_NULL,
    VAL_NUMBER,
    VAL_OBJ,
    VAL_STRING_LITERAL, 
} EsValueType;

typedef struct EsValue {
    EsValueType type;
    union {
        bool boolean;
        double number;
        void* obj;
        const char* string_literal;
    } as;
} EsValue;

#define BOOL_VAL(value)   ((EsValue){VAL_BOOL, {.boolean = value}})
#define NULL_VAL          ((EsValue){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value) ((EsValue){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((EsValue){VAL_OBJ, {.obj = (void*)object}})
#define STRING_VAL(chars) ((EsValue){VAL_STRING_LITERAL, {.string_literal = chars}})

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NULL(value)    ((value).type == VAL_NULL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)
#define IS_STRING_LIT(value) ((value).type == VAL_STRING_LITERAL)

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)     ((value).as.obj)
#define AS_STRING_LIT(value) ((value).as.string_literal)

typedef struct {
    int capacity;
    int count;
    EsValue* values;
} EsValueArray;

void es_value_array_init(EsValueArray* array);
void es_value_array_write(EsValueArray* array, EsValue value);
void es_value_array_free(EsValueArray* array);
void es_value_print(EsValue value);

#endif
