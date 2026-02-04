#ifndef ES_X86_BACKEND_H
#define ES_X86_BACKEND_H

#include <stdio.h>
#include "../../middle/ir/ir.h"






typedef enum {
    TEMP_LOC_NONE,      
    TEMP_LOC_REGISTER,  
    TEMP_LOC_STACK      
} TempLocationType;


typedef struct {
    TempLocationType type;
    union {
        const char* reg;    
        int offset;         
    };
} TempLocation;


typedef struct {
    const char* name;           
    int is_free;                
    const char* content;        
} RegisterState;


typedef struct {
    EsIRFunction* current_func;
    EsIRModule* current_module;

    
    TempLocation temp_locations[256];

    
    RegisterState registers[14];

    
    int stack_size;
    int temp_stack_base;  

    
    struct {
        char name[64];
        int offset;
    } var_offsets[100];
    int var_count;
    int next_var_offset;

    
    int temp_usage[256];
    
    
    void* regalloc;

} CodegenContext;





void es_x86_generate(FILE* output, EsIRModule* module);


void codegen_context_init(CodegenContext* ctx, EsIRFunction* func, EsIRModule* module);
TempLocation* codegen_get_temp_location(CodegenContext* ctx, int temp_idx);
void codegen_set_temp_in_register(CodegenContext* ctx, int temp_idx, const char* reg);
void codegen_set_temp_on_stack(CodegenContext* ctx, int temp_idx, int offset);
const char* codegen_alloc_register(CodegenContext* ctx);
void codegen_free_register(CodegenContext* ctx, const char* reg);
int codegen_get_var_offset(CodegenContext* ctx, const char* var_name);


void codegen_emit_load_temp(FILE* output, CodegenContext* ctx, int temp_idx, const char* target_reg);
void codegen_emit_store_temp(FILE* output, CodegenContext* ctx, int temp_idx, const char* source_reg);
void codegen_emit_load_var(FILE* output, CodegenContext* ctx, const char* var_name, const char* target_reg);
void codegen_emit_store_var(FILE* output, CodegenContext* ctx, const char* var_name, const char* source_reg);

#endif
