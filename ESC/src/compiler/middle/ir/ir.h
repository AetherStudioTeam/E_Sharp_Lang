#ifndef ES_IR_H
#define ES_IR_H
#include "../../../core/utils/es_common.h"
#include "../../../compiler/frontend/parser/ast.h"
#include "ir_memory.h"
#include "ir_param_table.h"


#ifndef ES_IR_INLINE
    #ifdef _MSC_VER
        #define ES_IR_INLINE __forceinline
    #else
        #define ES_IR_INLINE static inline __attribute__((always_inline))
    #endif
#endif


#ifndef ES_IR_LIKELY
    #ifdef __GNUC__
        #define ES_IR_LIKELY(x) __builtin_expect(!!(x), 1)
        #define ES_IR_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #else
        #define ES_IR_LIKELY(x) (x)
        #define ES_IR_UNLIKELY(x) (x)
    #endif
#endif
typedef struct EsIRModule EsIRModule;
typedef struct EsIRFunction EsIRFunction;
typedef struct EsIRBasicBlock EsIRBasicBlock;
typedef struct EsIRInst EsIRInst;
typedef struct EsIRBuilder EsIRBuilder;

typedef enum {
    ES_IR_VALUE_VOID,
    ES_IR_VALUE_IMM,
    ES_IR_VALUE_VAR,
    ES_IR_VALUE_TEMP,
    ES_IR_VALUE_ARG,
    ES_IR_VALUE_STRING_CONST,
    ES_IR_VALUE_FUNCTION,
} EsIRValueType;
#define ES_IR_TYPE_I64 ES_IR_VALUE_IMM

typedef struct {
    EsIRValueType type;
    union {
        double imm;
        char* name;
        int index;
        int string_const_id;
        char* function_name;
    } data;
} EsIRValue;

typedef enum {
    ES_IR_LOAD,
    ES_IR_STORE,
    ES_IR_ALLOC,
    ES_IR_IMM,    
    ES_IR_ADD,
    ES_IR_SUB,
    ES_IR_MUL,
    ES_IR_DIV,
    ES_IR_MOD,
    ES_IR_AND,
    ES_IR_OR,
    ES_IR_XOR,
    ES_IR_LSHIFT,
    ES_IR_RSHIFT,
    ES_IR_POW,
    ES_IR_LT,
    ES_IR_GT,
    ES_IR_EQ,
    ES_IR_LE,
    ES_IR_GE,
    ES_IR_NE,
    ES_IR_JUMP,
    ES_IR_BRANCH,
    ES_IR_CALL,
    ES_IR_RETURN,
    ES_IR_LABEL,
    ES_IR_STRCAT,
    ES_IR_CAST,
    ES_IR_LOADPTR,
    ES_IR_STOREPTR,
    ES_IR_ARRAY_STORE,
    ES_IR_INT_TO_STRING,
    ES_IR_DOUBLE_TO_STRING,
    ES_IR_COPY,   
    ES_IR_NOP,
} EsIROpcode;


#define ES_IR_CACHE_LINE_SIZE 64



typedef struct EsIRInst {
    
    EsIROpcode opcode;           
    int operand_count;           
    int operand_capacity;        
    bool is_int_result;          
    char _padding[3];            
    
    EsIRValue* operands;         
    EsIRValue result;            
    
    
    EsIRInst* next;              
} EsIRInst;


#define ES_IR_BLOCK_CACHE_SIZE 4

typedef struct EsIRBasicBlock {
    
    char* label;                 
    int id;                      
    int inst_count;              
    int inst_capacity;           
    int cache_count;             
    
    
    EsIRInst** insts;            
    EsIRInst* cache[ES_IR_BLOCK_CACHE_SIZE];  
    
    
    EsIRInst* first_inst;        
    EsIRInst* last_inst;         
    
    
    struct EsIRBasicBlock** preds;  
    int pred_count;
    int pred_capacity;
    struct EsIRBasicBlock** succs;  
    int succ_count;
    int succ_capacity;
    
    struct EsIRBasicBlock* next;  
} EsIRBasicBlock;

typedef struct {
    char* name;
    EsTokenType type;
} EsIRParam;

typedef struct {
    char* name;
    EsTokenType type;
    int has_initializer;
    double init_number;
} EsIRGlobal;

typedef struct EsIRFunction {
    char* name;
    EsIRParam* params;              
    EsIRParamTable* param_table;    
    int param_count;
    EsTokenType return_type;
    EsIRBasicBlock* entry_block;
    EsIRBasicBlock* exit_block;
    int stack_size;
    int has_calls;
    struct EsIRFunction* next;
} EsIRFunction;

typedef struct EsIRModule {
    EsIRFunction* functions;
    EsIRFunction* main_function;
    char** string_constants;
    int string_const_count;
    int string_const_capacity;
    EsIRGlobal* globals;
    int global_count;
    int global_capacity;
} EsIRModule;

struct EsIRBuilder {
    EsIRModule* module;
    EsIRFunction* current_function;
    EsIRBasicBlock* current_block;
    int temp_counter;
    int label_counter;
    EsIRBasicBlock** loop_continue_blocks;
    EsIRBasicBlock** loop_break_blocks;
    int loop_stack_size;
    int loop_stack_capacity;
    char** class_name_stack;
    int class_stack_size;
    int class_stack_capacity;
    struct EsIRClassLayout* layouts;
    int layout_count;
    int layout_capacity;
    struct TypeCheckContext* type_context;
    EsIRMemoryArena* arena;  
};
EsIRBuilder* es_ir_builder_create(void);
void es_ir_builder_destroy(EsIRBuilder* builder);
EsIRModule* es_ir_module_create(void);
void es_ir_module_destroy(EsIRModule* module);
EsIRFunction* es_ir_function_create(EsIRBuilder* builder, const char* name, EsIRParam* params, int param_count, EsTokenType return_type);
void es_ir_function_set_entry(EsIRBuilder* builder, EsIRFunction* func);
EsIRBasicBlock* es_ir_block_create(EsIRBuilder* builder, const char* label);
void es_ir_block_set_current(EsIRBuilder* builder, EsIRBasicBlock* block);


int es_ir_block_get_inst_count(EsIRBasicBlock* block);
EsIRInst* es_ir_block_get_inst(EsIRBasicBlock* block, int index);
EsIRInst* es_ir_block_get_first_inst(EsIRBasicBlock* block);
EsIRInst* es_ir_block_get_last_inst(EsIRBasicBlock* block);


void es_ir_block_add_pred(EsIRBuilder* builder, EsIRBasicBlock* block, EsIRBasicBlock* pred);
void es_ir_block_add_succ(EsIRBuilder* builder, EsIRBasicBlock* block, EsIRBasicBlock* succ);
int es_ir_block_get_pred_count(EsIRBasicBlock* block);
int es_ir_block_get_succ_count(EsIRBasicBlock* block);
EsIRBasicBlock* es_ir_block_get_pred(EsIRBasicBlock* block, int index);
EsIRBasicBlock* es_ir_block_get_succ(EsIRBasicBlock* block, int index);


void es_ir_block_invalidate_cache(EsIRBasicBlock* block);
EsIRInst* es_ir_block_find_cached_inst(EsIRBasicBlock* block, EsIROpcode opcode);
EsIRValue es_ir_load(EsIRBuilder* builder, const char* name);
void es_ir_store(EsIRBuilder* builder, const char* name, EsIRValue value);
void es_ir_alloc(EsIRBuilder* builder, const char* name);
EsIRValue es_ir_load_ptr(EsIRBuilder* builder, EsIRValue base, int offset);
void es_ir_store_ptr(EsIRBuilder* builder, EsIRValue base, int offset, EsIRValue value);
void es_ir_array_store(EsIRBuilder* builder, EsIRValue array, EsIRValue index, EsIRValue value);
EsIRValue es_ir_add(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_sub(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_mul(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_div(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_mod(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_and(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_or(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_xor(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_lshift(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_rshift(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_pow(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_strcat(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs);
EsIRValue es_ir_int_to_string(EsIRBuilder* builder, EsIRValue value);
EsIRValue es_ir_double_to_string(EsIRBuilder* builder, EsIRValue value);
EsIRValue es_ir_cast(EsIRBuilder* builder, EsIRValue value, EsTokenType target_type);
EsIRValue es_ir_compare(EsIRBuilder* builder, EsIROpcode op, EsIRValue lhs, EsIRValue rhs);
void es_ir_jump(EsIRBuilder* builder, EsIRBasicBlock* target);
void es_ir_branch(EsIRBuilder* builder, EsIRValue cond, EsIRBasicBlock* true_block, EsIRBasicBlock* false_block);
EsIRValue es_ir_call(EsIRBuilder* builder, const char* func_name, EsIRValue* args, int arg_count);
void es_ir_return(EsIRBuilder* builder, EsIRValue value);
void es_ir_label(EsIRBuilder* builder, const char* label);
void es_ir_nop(EsIRBuilder* builder);
void es_ir_push_loop_context(EsIRBuilder* builder, EsIRBasicBlock* continue_block, EsIRBasicBlock* break_block);
void es_ir_pop_loop_context(EsIRBuilder* builder);
EsIRBasicBlock* es_ir_get_current_continue_block(EsIRBuilder* builder);
EsIRBasicBlock* es_ir_get_current_break_block(EsIRBuilder* builder);
EsIRValue es_ir_imm(EsIRBuilder* builder, double value);
EsIRValue es_ir_var(EsIRBuilder* builder, const char* name);
EsIRValue es_ir_temp(EsIRBuilder* builder);
EsIRValue es_ir_arg(EsIRBuilder* builder, int index);
EsIRValue es_ir_string_const(EsIRBuilder* builder, const char* str);
EsIRGlobal* es_ir_module_add_global(EsIRBuilder* builder, const char* name, EsTokenType type);
EsIRGlobal* es_ir_module_find_global(EsIRModule* module, const char* name);
void es_ir_module_set_global_number_initializer(EsIRGlobal* global, double value);
void es_ir_generate_from_ast(EsIRBuilder* builder, ASTNode* ast, struct TypeCheckContext* type_context);
void es_ir_print(EsIRModule* module, FILE* output);


typedef struct EsIRFieldOffset {
    char* name;
    int offset;
} EsIRFieldOffset;

typedef struct EsIRClassLayout {
    char* class_name;
    EsIRFieldOffset* fields;
    int field_count;
    int field_capacity;
} EsIRClassLayout;
void es_ir_register_class_layout(EsIRBuilder* builder, const char* class_name, ASTNode* class_body);
int es_ir_layout_get_offset(EsIRBuilder* builder, const char* class_name, const char* field_name);
int es_ir_layout_get_size(EsIRBuilder* builder, const char* class_name);


EsIRParamNode* es_ir_function_find_param(EsIRFunction* func, const char* name);
int es_ir_function_get_param_index(EsIRFunction* func, const char* name);





ES_IR_INLINE EsIRValue es_ir_imm_fast(double value) {
    EsIRValue result = {0};
    result.type = ES_IR_VALUE_IMM;
    result.data.imm = value;
    return result;
}


ES_IR_INLINE EsIRValue es_ir_temp_fast(int* counter) {
    EsIRValue result = {0};
    result.type = ES_IR_VALUE_TEMP;
    result.data.index = (*counter)++;
    return result;
}


ES_IR_INLINE EsIRValue es_ir_arg_fast(int index) {
    EsIRValue result = {0};
    result.type = ES_IR_VALUE_ARG;
    result.data.index = index;
    return result;
}


ES_IR_INLINE int es_ir_block_inst_count_fast(EsIRBasicBlock* block) {
    return ES_IR_LIKELY(block != NULL) ? block->inst_count : 0;
}

ES_IR_INLINE EsIRInst* es_ir_block_inst_fast(EsIRBasicBlock* block, int index) {
    if (ES_IR_UNLIKELY(block == NULL || index < 0 || index >= block->inst_count)) {
        return NULL;
    }
    return block->insts[index];
}


ES_IR_INLINE EsIROpcode es_ir_inst_opcode_fast(EsIRInst* inst) {
    return ES_IR_LIKELY(inst != NULL) ? inst->opcode : ES_IR_NOP;
}

ES_IR_INLINE int es_ir_inst_operand_count_fast(EsIRInst* inst) {
    return ES_IR_LIKELY(inst != NULL) ? inst->operand_count : 0;
}

ES_IR_INLINE EsIRValue* es_ir_inst_operands_fast(EsIRInst* inst) {
    return ES_IR_LIKELY(inst != NULL) ? inst->operands : NULL;
}

#endif  