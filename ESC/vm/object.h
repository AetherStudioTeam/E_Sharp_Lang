#ifndef ES_VM_OBJECT_H
#define ES_VM_OBJECT_H

#include "core/utils/es_common.h"
#include "value.h"




typedef enum {
    OBJ_STRING,
} EsObjType;




struct EsObject {
    EsObjType type;
    bool is_marked;
    struct EsObject* next; 
};




typedef struct {
    struct EsObject obj;
    int length;
    char* chars;
} EsString;


static inline bool is_obj_type(EsValue value, EsObjType type) {
    return IS_OBJ(value) && ((struct EsObject*)AS_OBJ(value))->type == type;
}

#define IS_STRING(value) (is_obj_type(value, OBJ_STRING))
#define IS_STRING_LIT(value) ((value).type == VAL_STRING_LITERAL)
#define IS_STRING_VAL(value) (IS_STRING(value) || IS_STRING_LIT(value))


#define AS_STRING(value) ((EsString*)AS_OBJ(value))
#define AS_STRING_LIT(value) ((value).as.string_literal)
#define AS_CSTRING(value) (IS_STRING_LIT(value) ? AS_STRING_LIT(value) : (IS_STRING(value) ? AS_STRING(value)->chars : ""))
#define AS_STRING_LEN(value) (IS_STRING_LIT(value) ? (int)strlen(AS_STRING_LIT(value)) : (IS_STRING(value) ? AS_STRING(value)->length : 0))

#define OBJ_TYPE(value) (AS_OBJ(value)->type)


EsString* es_object_new_string(void* vm, const char* chars, int length);
EsString* es_object_take_string(void* vm, char* chars, int length);

#endif 
