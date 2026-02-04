#ifndef ES_IR_ASSERT_H
#define ES_IR_ASSERT_H

#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif






#ifndef ES_IR_ASSERT_ENABLED
    #ifdef DEBUG
        #define ES_IR_ASSERT_ENABLED 1
    #else
        #define ES_IR_ASSERT_ENABLED 0
    #endif
#endif


#ifndef ES_IR_BOUNDS_CHECK_ENABLED
    #ifdef DEBUG
        #define ES_IR_BOUNDS_CHECK_ENABLED 1
    #else
        #define ES_IR_BOUNDS_CHECK_ENABLED 0
    #endif
#endif





#if ES_IR_ASSERT_ENABLED
    #define ES_IR_ASSERT(cond) do { \
        if (!(cond)) { \
            ES_ERROR("IR_ASSERT failed: %s at %s:%d", #cond, __FILE__, __LINE__); \
            assert(cond); \
        } \
    } while(0)
    
    #define ES_IR_ASSERT_MSG(cond, msg) do { \
        if (!(cond)) { \
            ES_ERROR("IR_ASSERT failed: %s - %s at %s:%d", #cond, msg, __FILE__, __LINE__); \
            assert(cond); \
        } \
    } while(0)
    
    #define ES_IR_ASSERT_NOT_NULL(ptr) ES_IR_ASSERT_MSG((ptr) != NULL, #ptr " must not be NULL")
    
    #define ES_IR_ASSERT_VALID_BUILDER(builder) do { \
        ES_IR_ASSERT_NOT_NULL(builder); \
        ES_IR_ASSERT_NOT_NULL((builder)->arena); \
        ES_IR_ASSERT_NOT_NULL((builder)->module); \
    } while(0)
    
    #define ES_IR_ASSERT_VALID_FUNCTION(func) do { \
        ES_IR_ASSERT_NOT_NULL(func); \
        ES_IR_ASSERT_NOT_NULL((func)->name); \
        ES_IR_ASSERT((func)->param_count >= 0); \
    } while(0)
    
    #define ES_IR_ASSERT_VALID_BLOCK(block) do { \
        ES_IR_ASSERT_NOT_NULL(block); \
        ES_IR_ASSERT_NOT_NULL((block)->label); \
    } while(0)
    
    #define ES_IR_ASSERT_VALID_INST(inst) do { \
        ES_IR_ASSERT_NOT_NULL(inst); \
        ES_IR_ASSERT((inst)->opcode >= ES_IR_LOAD && (inst)->opcode <= ES_IR_NOP); \
        ES_IR_ASSERT((inst)->operand_count >= 0); \
        ES_IR_ASSERT((inst)->operand_count <= (inst)->operand_capacity); \
    } while(0)
    
    #define ES_IR_ASSERT_VALID_VALUE(val) do { \
        ES_IR_ASSERT((val).type >= ES_IR_VALUE_VOID && (val).type <= ES_IR_VALUE_FUNCTION); \
    } while(0)
    
    #define ES_IR_ASSERT_VALID_ARENA(arena) do { \
        ES_IR_ASSERT_NOT_NULL(arena); \
        ES_IR_ASSERT_NOT_NULL((arena)->current_pool); \
        ES_IR_ASSERT((arena)->pool_size > 0); \
    } while(0)
#else
    #define ES_IR_ASSERT(cond) ((void)0)
    #define ES_IR_ASSERT_MSG(cond, msg) ((void)0)
    #define ES_IR_ASSERT_NOT_NULL(ptr) ((void)0)
    #define ES_IR_ASSERT_VALID_BUILDER(builder) ((void)0)
    #define ES_IR_ASSERT_VALID_FUNCTION(func) ((void)0)
    #define ES_IR_ASSERT_VALID_BLOCK(block) ((void)0)
    #define ES_IR_ASSERT_VALID_INST(inst) ((void)0)
    #define ES_IR_ASSERT_VALID_VALUE(val) ((void)0)
    #define ES_IR_ASSERT_VALID_ARENA(arena) ((void)0)
#endif





#if ES_IR_BOUNDS_CHECK_ENABLED
    #define ES_IR_BOUNDS_CHECK(index, count) do { \
        if ((index) < 0 || (index) >= (count)) { \
            ES_ERROR("IR_BOUNDS_CHECK failed: index=%d, count=%d at %s:%d", \
                     (int)(index), (int)(count), __FILE__, __LINE__); \
            assert((index) >= 0 && (index) < (count)); \
        } \
    } while(0)
    
    #define ES_IR_BOUNDS_CHECK_ARRAY(ptr, index, count) do { \
        ES_IR_ASSERT_NOT_NULL(ptr); \
        ES_IR_BOUNDS_CHECK(index, count); \
    } while(0)
    
    #define ES_IR_CAPACITY_CHECK(current, capacity) do { \
        if ((current) >= (capacity)) { \
            ES_ERROR("IR_CAPACITY_CHECK failed: current=%d, capacity=%d at %s:%d", \
                     (int)(current), (int)(capacity), __FILE__, __LINE__); \
            assert((current) < (capacity)); \
        } \
    } while(0)
#else
    #define ES_IR_BOUNDS_CHECK(index, count) ((void)0)
    #define ES_IR_BOUNDS_CHECK_ARRAY(ptr, index, count) ((void)0)
    #define ES_IR_CAPACITY_CHECK(current, capacity) ((void)0)
#endif






#define ES_IR_ALLOC_FAIL_ABORT 0
#define ES_IR_ALLOC_FAIL_RETURN_NULL 1
#define ES_IR_ALLOC_FAIL_LOG_AND_RETURN 2

#ifndef ES_IR_ALLOC_FAIL_POLICY
    #define ES_IR_ALLOC_FAIL_POLICY ES_IR_ALLOC_FAIL_LOG_AND_RETURN
#endif

#if ES_IR_ALLOC_FAIL_POLICY == ES_IR_ALLOC_FAIL_ABORT
    #define ES_IR_HANDLE_ALLOC_FAIL(msg) do { \
        ES_ERROR("IR_ALLOC_FAIL: %s at %s:%d", msg, __FILE__, __LINE__); \
        abort(); \
    } while(0)
#elif ES_IR_ALLOC_FAIL_POLICY == ES_IR_ALLOC_FAIL_RETURN_NULL
    #define ES_IR_HANDLE_ALLOC_FAIL(msg) do { \
        return NULL; \
    } while(0)
#else 
    #define ES_IR_HANDLE_ALLOC_FAIL(msg) do { \
        ES_ERROR("IR_ALLOC_FAIL: %s at %s:%d", msg, __FILE__, __LINE__); \
        return NULL; \
    } while(0)
#endif


#define ES_IR_ALLOC_CHECKED(ptr, type, arena) do { \
    (ptr) = (type*)es_ir_arena_alloc((arena), sizeof(type)); \
    if (ES_IR_UNLIKELY(!(ptr))) { \
        ES_IR_HANDLE_ALLOC_FAIL("Failed to allocate " #type); \
    } \
} while(0)

#define ES_IR_ALLOC_ARRAY_CHECKED(ptr, type, count, arena) do { \
    (ptr) = (type*)es_ir_arena_alloc((arena), sizeof(type) * (count)); \
    if (ES_IR_UNLIKELY(!(ptr))) { \
        ES_IR_HANDLE_ALLOC_FAIL("Failed to allocate array of " #type); \
    } \
} while(0)

#define ES_IR_STRDUP_CHECKED(ptr, str, arena) do { \
    (ptr) = es_ir_arena_strdup((arena), (str)); \
    if (ES_IR_UNLIKELY(!(ptr))) { \
        ES_IR_HANDLE_ALLOC_FAIL("Failed to duplicate string"); \
    } \
} while(0)






bool es_ir_builder_is_valid(EsIRBuilder* builder);


bool es_ir_function_is_valid(EsIRFunction* func);


bool es_ir_block_is_valid(EsIRBasicBlock* block);


bool es_ir_inst_is_valid(EsIRInst* inst);


bool es_ir_value_is_valid(EsIRValue* value);


bool es_ir_arena_is_valid(EsIRMemoryArena* arena);


bool es_ir_operand_index_is_valid(EsIRInst* inst, int index);


bool es_ir_param_index_is_valid(EsIRFunction* func, int index);





#if ES_IR_ASSERT_ENABLED
    
    void es_ir_debug_print_builder(EsIRBuilder* builder, const char* label);
    
    
    void es_ir_debug_print_function(EsIRFunction* func, const char* label);
    
    
    void es_ir_debug_print_block(EsIRBasicBlock* block, const char* label);
    
    
    void es_ir_debug_print_inst(EsIRInst* inst, const char* label);
    
    
    void es_ir_debug_print_value(EsIRValue* value, const char* label);
#else
    #define es_ir_debug_print_builder(builder, label) ((void)0)
    #define es_ir_debug_print_function(func, label) ((void)0)
    #define es_ir_debug_print_block(block, label) ((void)0)
    #define es_ir_debug_print_inst(inst, label) ((void)0)
    #define es_ir_debug_print_value(value, label) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif 
