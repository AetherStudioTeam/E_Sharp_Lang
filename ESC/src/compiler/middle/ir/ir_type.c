#include "ir_type.h"
#include <string.h>
#include <stdio.h>



void es_ir_type_pool_init(EsIRTypePool* pool) {
    if (!pool) return;
    pool->types = NULL;
    pool->count = 0;
}

void es_ir_type_pool_destroy(EsIRTypePool* pool) {
    if (!pool) return;
    
    
    EsIRType* type = pool->types;
    while (type) {
        EsIRType* next = type->next;
        
        
        if (type->kind == ES_IR_TYPE_FUNCTION) {
            if (type->data.function.params) {
                ES_FREE(type->data.function.params);
            }
        } else if (type->kind == ES_IR_TYPE_STRUCT || type->kind == ES_IR_TYPE_CLASS) {
            if (type->data.compound.name) {
                ES_FREE(type->data.compound.name);
            }
            if (type->data.compound.fields) {
                ES_FREE(type->data.compound.fields);
            }
            if (type->data.compound.field_names) {
                for (int i = 0; i < type->data.compound.field_count; i++) {
                    ES_FREE(type->data.compound.field_names[i]);
                }
                ES_FREE(type->data.compound.field_names);
            }
        }
        
        ES_FREE(type);
        type = next;
    }
    
    pool->types = NULL;
    pool->count = 0;
}


static EsIRType* create_type(EsIRTypePool* pool, EsIRTypeKind kind, int size, int align) {
    if (!pool) return NULL;
    
    EsIRType* type = (EsIRType*)ES_CALLOC(1, sizeof(EsIRType));
    if (!type) return NULL;
    
    type->kind = kind;
    type->modifiers = ES_IR_TYPE_MOD_NONE;
    type->size = size;
    type->align = align;
    
    
    type->next = pool->types;
    pool->types = type;
    pool->count++;
    
    return type;
}



EsIRType* es_ir_type_void(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_VOID, 0, 1);
}

EsIRType* es_ir_type_int8(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_INT8, 1, 1);
}

EsIRType* es_ir_type_int16(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_INT16, 2, 2);
}

EsIRType* es_ir_type_int32(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_INT32, 4, 4);
}

EsIRType* es_ir_type_int64(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_INT64, 8, 8);
}

EsIRType* es_ir_type_uint8(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_UINT8, 1, 1);
}

EsIRType* es_ir_type_uint16(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_UINT16, 2, 2);
}

EsIRType* es_ir_type_uint32(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_UINT32, 4, 4);
}

EsIRType* es_ir_type_uint64(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_UINT64, 8, 8);
}

EsIRType* es_ir_type_float32(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_FLOAT32, 4, 4);
}

EsIRType* es_ir_type_float64(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_FLOAT64, 8, 8);
}

EsIRType* es_ir_type_bool(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_BOOL, 1, 1);
}

EsIRType* es_ir_type_char(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_CHAR, 1, 1);
}

EsIRType* es_ir_type_string(EsIRTypePool* pool) {
    
    return create_type(pool, ES_IR_TYPE_STRING, 8, 8);
}

EsIRType* es_ir_type_any(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_ANY, 8, 8);
}

EsIRType* es_ir_type_unknown(EsIRTypePool* pool) {
    return create_type(pool, ES_IR_TYPE_UNKNOWN, 0, 1);
}



EsIRType* es_ir_type_pointer(EsIRTypePool* pool, EsIRType* pointee) {
    if (!pool || !pointee) return NULL;
    
    EsIRType* type = create_type(pool, ES_IR_TYPE_POINTER, 8, 8);
    if (!type) return NULL;
    
    type->data.pointer.pointee = pointee;
    return type;
}

EsIRType* es_ir_type_array(EsIRTypePool* pool, EsIRType* element, int size) {
    if (!pool || !element) return NULL;
    
    EsIRType* type = create_type(pool, ES_IR_TYPE_ARRAY, element->size * size, element->align);
    if (!type) return NULL;
    
    type->data.array.element = element;
    type->data.array.size = size;
    return type;
}

EsIRType* es_ir_type_function(EsIRTypePool* pool, EsIRType** params, int param_count, EsIRType* ret) {
    if (!pool) return NULL;
    
    EsIRType* type = create_type(pool, ES_IR_TYPE_FUNCTION, 8, 8);
    if (!type) return NULL;
    
    type->data.function.ret = ret ? ret : es_ir_type_void(pool);
    type->data.function.param_count = param_count;
    
    if (param_count > 0 && params) {
        type->data.function.params = (EsIRType**)ES_MALLOC(param_count * sizeof(EsIRType*));
        if (type->data.function.params) {
            memcpy(type->data.function.params, params, param_count * sizeof(EsIRType*));
        }
    } else {
        type->data.function.params = NULL;
    }
    
    return type;
}

EsIRType* es_ir_type_struct(EsIRTypePool* pool, const char* name) {
    if (!pool) return NULL;
    
    EsIRType* type = create_type(pool, ES_IR_TYPE_STRUCT, 0, 1);
    if (!type) return NULL;
    
    if (name) {
        type->data.compound.name = ES_STRDUP(name);
    } else {
        type->data.compound.name = NULL;
    }
    type->data.compound.fields = NULL;
    type->data.compound.field_names = NULL;
    type->data.compound.field_count = 0;
    
    return type;
}



EsIRType* es_ir_type_from_token(EsIRTypePool* pool, EsTokenType token_type) {
    if (!pool) return NULL;
    
    switch (token_type) {
        case TOKEN_VOID:      return es_ir_type_void(pool);
        case TOKEN_INT8:      return es_ir_type_int8(pool);
        case TOKEN_INT16:     return es_ir_type_int16(pool);
        case TOKEN_INT32:     return es_ir_type_int32(pool);
        case TOKEN_INT64:     return es_ir_type_int64(pool);
        case TOKEN_UINT8:     return es_ir_type_uint8(pool);
        case TOKEN_UINT16:    return es_ir_type_uint16(pool);
        case TOKEN_UINT32:    return es_ir_type_uint32(pool);
        case TOKEN_UINT64:    return es_ir_type_uint64(pool);
        case TOKEN_FLOAT32:   return es_ir_type_float32(pool);
        case TOKEN_FLOAT64:   return es_ir_type_float64(pool);
        case TOKEN_BOOL:      return es_ir_type_bool(pool);
        case TOKEN_CHAR:      return es_ir_type_char(pool);
        case TOKEN_STRING:
        case TOKEN_TYPE_STRING: return es_ir_type_string(pool);
        default:              return es_ir_type_unknown(pool);
    }
}



int es_ir_type_size(EsIRType* type) {
    if (!type) return 0;
    return type->size;
}

int es_ir_type_align(EsIRType* type) {
    if (!type) return 1;
    return type->align;
}

const char* es_ir_type_to_string(EsIRType* type) {
    if (!type) return "null";
    
    switch (type->kind) {
        case ES_IR_TYPE_VOID:     return "void";
        case ES_IR_TYPE_INT8:     return "int8";
        case ES_IR_TYPE_INT16:    return "int16";
        case ES_IR_TYPE_INT32:    return "int32";
        case ES_IR_TYPE_INT64:    return "int64";
        case ES_IR_TYPE_UINT8:    return "uint8";
        case ES_IR_TYPE_UINT16:   return "uint16";
        case ES_IR_TYPE_UINT32:   return "uint32";
        case ES_IR_TYPE_UINT64:   return "uint64";
        case ES_IR_TYPE_FLOAT32:  return "float32";
        case ES_IR_TYPE_FLOAT64:  return "float64";
        case ES_IR_TYPE_BOOL:     return "bool";
        case ES_IR_TYPE_CHAR:     return "char";
        case ES_IR_TYPE_STRING:   return "string";
        case ES_IR_TYPE_POINTER:  return "pointer";
        case ES_IR_TYPE_ARRAY:    return "array";
        case ES_IR_TYPE_FUNCTION: return "function";
        case ES_IR_TYPE_STRUCT:   return type->data.compound.name ? type->data.compound.name : "struct";
        case ES_IR_TYPE_CLASS:    return type->data.compound.name ? type->data.compound.name : "class";
        case ES_IR_TYPE_ANY:      return "any";
        case ES_IR_TYPE_UNKNOWN:  return "unknown";
        default:                  return "invalid";
    }
}



bool es_ir_type_equal(EsIRType* a, EsIRType* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    
    
    if (a->kind == ES_IR_TYPE_POINTER) {
        return es_ir_type_equal(a->data.pointer.pointee, b->data.pointer.pointee);
    }
    if (a->kind == ES_IR_TYPE_ARRAY) {
        return es_ir_type_equal(a->data.array.element, b->data.array.element) &&
               a->data.array.size == b->data.array.size;
    }
    
    return true;
}

bool es_ir_type_compatible(EsIRType* src, EsIRType* dst) {
    if (!src || !dst) return false;
    if (es_ir_type_equal(src, dst)) return true;
    
    
    if (dst->kind == ES_IR_TYPE_ANY) return true;
    
    
    if (es_ir_type_is_numeric(src) && es_ir_type_is_numeric(dst)) {
        return true;
    }
    
    
    if (es_ir_type_is_pointer(src) && es_ir_type_is_pointer(dst)) {
        return true;  
    }
    
    return false;
}

bool es_ir_type_is_integer(EsIRType* type) {
    if (!type) return false;
    return type->kind >= ES_IR_TYPE_INT8 && type->kind <= ES_IR_TYPE_UINT64;
}

bool es_ir_type_is_unsigned(EsIRType* type) {
    if (!type) return false;
    return type->kind >= ES_IR_TYPE_UINT8 && type->kind <= ES_IR_TYPE_UINT64;
}

bool es_ir_type_is_float(EsIRType* type) {
    if (!type) return false;
    return type->kind == ES_IR_TYPE_FLOAT32 || type->kind == ES_IR_TYPE_FLOAT64;
}

bool es_ir_type_is_numeric(EsIRType* type) {
    return es_ir_type_is_integer(type) || es_ir_type_is_float(type);
}

bool es_ir_type_is_pointer(EsIRType* type) {
    if (!type) return false;
    return type->kind == ES_IR_TYPE_POINTER || type->kind == ES_IR_TYPE_STRING;
}

EsIRType* es_ir_type_pointee(EsIRType* type) {
    if (!es_ir_type_is_pointer(type)) return NULL;
    if (type->kind == ES_IR_TYPE_POINTER) {
        return type->data.pointer.pointee;
    }
    return NULL;  
}



EsIRType* es_ir_type_binary_result(EsIRTypePool* pool, EsIRType* lhs, EsIRType* rhs, EsIROpcode op) {
    if (!pool || !lhs || !rhs) return es_ir_type_unknown(pool);
    
    
    if (op >= ES_IR_LT && op <= ES_IR_NE) {
        return es_ir_type_bool(pool);
    }
    
    
    if (op == ES_IR_AND || op == ES_IR_OR) {
        return es_ir_type_bool(pool);
    }
    
    
    if (!es_ir_type_is_numeric(lhs) || !es_ir_type_is_numeric(rhs)) {
        return es_ir_type_unknown(pool);
    }
    
    
    if (lhs->kind == ES_IR_TYPE_FLOAT64 || rhs->kind == ES_IR_TYPE_FLOAT64) {
        return es_ir_type_float64(pool);
    }
    
    
    if (lhs->kind == ES_IR_TYPE_FLOAT32 || rhs->kind == ES_IR_TYPE_FLOAT32) {
        return es_ir_type_float32(pool);
    }
    
    
    int lhs_bits = lhs->size * 8;
    int rhs_bits = rhs->size * 8;
    
    if (lhs_bits >= rhs_bits) {
        return lhs;
    } else {
        return rhs;
    }
}

EsIRType* es_ir_type_compare_result(EsIRTypePool* pool) {
    return pool ? es_ir_type_bool(pool) : NULL;
}

EsIRType* es_ir_type_promote(EsIRTypePool* pool, EsIRType* type) {
    if (!pool || !type) return NULL;
    
    
    if (es_ir_type_is_integer(type) && type->size < 4) {
        if (es_ir_type_is_unsigned(type)) {
            return es_ir_type_uint32(pool);
        } else {
            return es_ir_type_int32(pool);
        }
    }
    
    return type;
}

EsIRType* es_ir_type_common(EsIRTypePool* pool, EsIRType* a, EsIRType* b) {
    if (!pool) return NULL;
    if (!a) return b;
    if (!b) return a;
    
    if (es_ir_type_equal(a, b)) return a;
    
    
    if (es_ir_type_is_numeric(a) && es_ir_type_is_numeric(b)) {
        
        if (es_ir_type_is_float(a) || es_ir_type_is_float(b)) {
            if (a->kind == ES_IR_TYPE_FLOAT64 || b->kind == ES_IR_TYPE_FLOAT64) {
                return es_ir_type_float64(pool);
            }
            return es_ir_type_float32(pool);
        }
        
        
        if (a->size >= b->size) return a;
        return b;
    }
    
    
    return es_ir_type_any(pool);
}



bool es_ir_type_can_assign(EsIRType* src, EsIRType* dst) {
    return es_ir_type_compatible(src, dst);
}

bool es_ir_type_can_cast(EsIRType* src, EsIRType* dst) {
    if (!src || !dst) return false;
    
    
    if (es_ir_type_equal(src, dst)) return true;
    
    
    if (es_ir_type_is_numeric(src) && es_ir_type_is_numeric(dst)) return true;
    
    
    if (es_ir_type_is_pointer(src) && es_ir_type_is_integer(dst)) return true;
    if (es_ir_type_is_integer(src) && es_ir_type_is_pointer(dst)) return true;
    
    
    if (es_ir_type_is_pointer(src) && es_ir_type_is_pointer(dst)) return true;
    
    return false;
}

bool es_ir_type_supports_op(EsIRType* type, EsIROpcode op) {
    if (!type) return false;
    
    switch (op) {
        case ES_IR_ADD:
        case ES_IR_SUB:
        case ES_IR_MUL:
        case ES_IR_DIV:
        case ES_IR_MOD:
            return es_ir_type_is_numeric(type);
            
        case ES_IR_AND:
        case ES_IR_OR:
        case ES_IR_XOR:
        case ES_IR_LSHIFT:
        case ES_IR_RSHIFT:
            return es_ir_type_is_integer(type);
            
        case ES_IR_LT:
        case ES_IR_GT:
        case ES_IR_LE:
        case ES_IR_GE:
            return es_ir_type_is_numeric(type);
            
        case ES_IR_EQ:
        case ES_IR_NE:
            return true;  
            
        default:
            return true;
    }
}

EsIRValue es_ir_type_default_value(EsIRType* type) {
    EsIRValue value = {0};
    value.type = ES_IR_VALUE_IMM;
    value.data.imm = 0;
    
    if (!type) return value;
    
    switch (type->kind) {
        case ES_IR_TYPE_FLOAT32:
        case ES_IR_TYPE_FLOAT64:
            value.data.imm = 0.0;
            break;
        case ES_IR_TYPE_BOOL:
            value.data.imm = 0;  
            break;
        case ES_IR_TYPE_POINTER:
        case ES_IR_TYPE_STRING:
            
            value.data.imm = 0;
            break;
        default:
            value.data.imm = 0;
            break;
    }
    
    return value;
}
