#ifndef ES_IR_TYPE_H
#define ES_IR_TYPE_H

#include "ir.h"


typedef enum {
    ES_IR_TYPE_VOID,      
    ES_IR_TYPE_INT8,      
    ES_IR_TYPE_INT16,     
    ES_IR_TYPE_INT32,     
    ES_IR_TYPE_INT64,     
    ES_IR_TYPE_UINT8,     
    ES_IR_TYPE_UINT16,    
    ES_IR_TYPE_UINT32,    
    ES_IR_TYPE_UINT64,    
    ES_IR_TYPE_FLOAT32,   
    ES_IR_TYPE_FLOAT64,   
    ES_IR_TYPE_BOOL,      
    ES_IR_TYPE_CHAR,      
    ES_IR_TYPE_STRING,    
    ES_IR_TYPE_POINTER,   
    ES_IR_TYPE_ARRAY,     
    ES_IR_TYPE_FUNCTION,  
    ES_IR_TYPE_STRUCT,    
    ES_IR_TYPE_CLASS,     
    ES_IR_TYPE_ANY,       
    ES_IR_TYPE_UNKNOWN,   
} EsIRTypeKind;


typedef enum {
    ES_IR_TYPE_MOD_NONE = 0,
    ES_IR_TYPE_MOD_CONST = 1 << 0,      
    ES_IR_TYPE_MOD_VOLATILE = 1 << 1,   
    ES_IR_TYPE_MOD_REFERENCE = 1 << 2,  
} EsIRTypeModifier;


struct EsIRType;
typedef struct EsIRType EsIRType;


struct EsIRType {
    EsIRTypeKind kind;           
    int modifiers;               
    int size;                    
    int align;                   
    
    
    union {
        
        struct {
            EsIRType* pointee;   
        } pointer;
        
        
        struct {
            EsIRType* element;   
            int size;            
        } array;
        
        
        struct {
            EsIRType** params;   
            int param_count;     
            EsIRType* ret;       
        } function;
        
        
        struct {
            char* name;          
            EsIRType** fields;   
            char** field_names;  
            int field_count;     
        } compound;
    } data;
    
    
    EsIRType* next;
};


typedef struct {
    EsIRType* types;             
    int count;                   
} EsIRTypePool;




void es_ir_type_pool_init(EsIRTypePool* pool);
void es_ir_type_pool_destroy(EsIRTypePool* pool);


EsIRType* es_ir_type_void(EsIRTypePool* pool);
EsIRType* es_ir_type_int8(EsIRTypePool* pool);
EsIRType* es_ir_type_int16(EsIRTypePool* pool);
EsIRType* es_ir_type_int32(EsIRTypePool* pool);
EsIRType* es_ir_type_int64(EsIRTypePool* pool);
EsIRType* es_ir_type_uint8(EsIRTypePool* pool);
EsIRType* es_ir_type_uint16(EsIRTypePool* pool);
EsIRType* es_ir_type_uint32(EsIRTypePool* pool);
EsIRType* es_ir_type_uint64(EsIRTypePool* pool);
EsIRType* es_ir_type_float32(EsIRTypePool* pool);
EsIRType* es_ir_type_float64(EsIRTypePool* pool);
EsIRType* es_ir_type_bool(EsIRTypePool* pool);
EsIRType* es_ir_type_char(EsIRTypePool* pool);
EsIRType* es_ir_type_string(EsIRTypePool* pool);
EsIRType* es_ir_type_any(EsIRTypePool* pool);
EsIRType* es_ir_type_unknown(EsIRTypePool* pool);


EsIRType* es_ir_type_pointer(EsIRTypePool* pool, EsIRType* pointee);
EsIRType* es_ir_type_array(EsIRTypePool* pool, EsIRType* element, int size);
EsIRType* es_ir_type_function(EsIRTypePool* pool, EsIRType** params, int param_count, EsIRType* ret);


EsIRType* es_ir_type_struct(EsIRTypePool* pool, const char* name);




EsIRType* es_ir_type_from_token(EsIRTypePool* pool, EsTokenType token_type);


int es_ir_type_size(EsIRType* type);


int es_ir_type_align(EsIRType* type);


const char* es_ir_type_to_string(EsIRType* type);


bool es_ir_type_equal(EsIRType* a, EsIRType* b);


bool es_ir_type_compatible(EsIRType* src, EsIRType* dst);


bool es_ir_type_is_integer(EsIRType* type);


bool es_ir_type_is_unsigned(EsIRType* type);


bool es_ir_type_is_float(EsIRType* type);


bool es_ir_type_is_numeric(EsIRType* type);


bool es_ir_type_is_pointer(EsIRType* type);


EsIRType* es_ir_type_pointee(EsIRType* type);





EsIRType* es_ir_type_binary_result(EsIRTypePool* pool, EsIRType* lhs, EsIRType* rhs, EsIROpcode op);


EsIRType* es_ir_type_compare_result(EsIRTypePool* pool);



EsIRType* es_ir_type_promote(EsIRTypePool* pool, EsIRType* type);



EsIRType* es_ir_type_common(EsIRTypePool* pool, EsIRType* a, EsIRType* b);




bool es_ir_type_can_assign(EsIRType* src, EsIRType* dst);


bool es_ir_type_can_cast(EsIRType* src, EsIRType* dst);


bool es_ir_type_supports_op(EsIRType* type, EsIROpcode op);


EsIRValue es_ir_type_default_value(EsIRType* type);

#endif 
