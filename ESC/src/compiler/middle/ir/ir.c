#include "ir.h"
#include "ir_memory.h"
#include "ir_assert.h"
#include "../../../core/utils/es_common.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


extern int strcmp(const char *s1, const char *s2);
extern void *memset(void *s, int c, size_t n);


EsIRValue es_ir_imm(EsIRBuilder* builder, double value) {
    (void)builder;
    EsIRValue result = {0};
    result.type = ES_IR_VALUE_IMM;
    result.data.imm = value;
    return result;
}

EsIRValue es_ir_var(EsIRBuilder* builder, const char* name) {
    ES_IR_ASSERT_VALID_BUILDER(builder);
    ES_IR_ASSERT_NOT_NULL(name);
    
    EsIRValue result = {0};
    result.type = ES_IR_VALUE_VAR;
    result.data.name = es_ir_arena_strdup(builder->arena, name);
    if (!result.data.name) {
        ES_ERROR("Failed to duplicate variable name '%s'", name);
        
        result.type = ES_IR_VALUE_VOID;
    }
    return result;
}

EsIRValue es_ir_temp(EsIRBuilder* builder) {
    EsIRValue result = {0};
    result.type = ES_IR_VALUE_TEMP;
    result.data.index = builder->temp_counter++;
    return result;
}

EsIRValue es_ir_arg(EsIRBuilder* builder, int index) {
    (void)builder;
    EsIRValue result = {0};
    result.type = ES_IR_VALUE_ARG;
    result.data.index = index;
    return result;
}

EsIRValue es_ir_string_const(EsIRBuilder* builder, const char* str) {
    if (!builder || !builder->arena || !str) {
        EsIRValue result = {0};
        return result;
    }

    for (int i = 0; i < builder->module->string_const_count; i++) {
        if (strcmp(builder->module->string_constants[i], str) == 0) {
            EsIRValue result = {0};
            result.type = ES_IR_VALUE_STRING_CONST;
            result.data.string_const_id = i;
            return result;
        }
    }

    if (builder->module->string_const_count >= builder->module->string_const_capacity) {
        builder->module->string_const_capacity *= 2;
        builder->module->string_constants = ES_REALLOC(builder->module->string_constants,
            builder->module->string_const_capacity * sizeof(char*));
    }

    int id = builder->module->string_const_count++;
    builder->module->string_constants[id] = es_ir_arena_strdup(builder->arena, str);

    EsIRValue result = {0};
    result.type = ES_IR_VALUE_STRING_CONST;
    result.data.string_const_id = id;
    return result;
}


EsIRModule* es_ir_module_create(void) {
    EsIRModule* module = ES_CALLOC(1, sizeof(EsIRModule));
    module->string_const_capacity = 16;
    module->string_constants = ES_CALLOC(module->string_const_capacity, sizeof(char*));
    module->global_capacity = 8;
    module->globals = ES_CALLOC(module->global_capacity, sizeof(EsIRGlobal));
    return module;
}

void es_ir_module_destroy(EsIRModule* module) {
    if (!module) return;

    
    

    
    ES_FREE(module->string_constants);

    
    ES_FREE(module->globals);

    
    ES_FREE(module);
}

static EsIRGlobal* es_ir_module_find_global_internal(EsIRModule* module, const char* name) {
    if (!module || !name) return NULL;
    for (int i = 0; i < module->global_count; i++) {
        if (module->globals[i].name && strcmp(module->globals[i].name, name) == 0) {
            return &module->globals[i];
        }
    }
    return NULL;
}

EsIRGlobal* es_ir_module_find_global(EsIRModule* module, const char* name) {
    return es_ir_module_find_global_internal(module, name);
}

EsIRGlobal* es_ir_module_add_global(EsIRBuilder* builder, const char* name, EsTokenType type) {
    if (!builder || !builder->module || !name) return NULL;

    EsIRModule* module = builder->module;
    EsIRGlobal* existing = es_ir_module_find_global_internal(module, name);
    if (existing) {
        return existing;
    }

    if (module->global_count >= module->global_capacity) {
        int old_capacity = module->global_capacity;
        module->global_capacity = old_capacity > 0 ? old_capacity * 2 : 8;
        module->globals = ES_REALLOC(module->globals, module->global_capacity * sizeof(EsIRGlobal));
        memset(module->globals + old_capacity, 0, (module->global_capacity - old_capacity) * sizeof(EsIRGlobal));
    }

    EsIRGlobal* global = &module->globals[module->global_count++];
    global->name = ES_STRDUP(name);
    global->type = type;
    global->has_initializer = 0;
    global->init_number = 0;
    return global;
}

void es_ir_module_set_global_number_initializer(EsIRGlobal* global, double value) {
    if (!global) return;
    global->has_initializer = 1;
    global->init_number = value;
}


EsIRBuilder* es_ir_builder_create(void) {
    EsIRBuilder* builder = ES_CALLOC(1, sizeof(EsIRBuilder));
    if (!builder) {
        ES_IR_HANDLE_ALLOC_FAIL("Failed to allocate EsIRBuilder");
    }

    
    builder->arena = es_ir_arena_create(DEFAULT_POOL_SIZE);
    if (!builder->arena) {
        ES_ERROR("Failed to create IR memory arena");
        ES_FREE(builder);
        return NULL;
    }

    builder->module = es_ir_module_create();
    if (!builder->module) {
        ES_ERROR("Failed to create IR module");
        es_ir_arena_destroy(builder->arena);
        ES_FREE(builder);
        return NULL;
    }

    builder->temp_counter = 0;
    builder->label_counter = 0;
    builder->loop_stack_size = 0;
    builder->loop_stack_capacity = 8;
    builder->loop_continue_blocks = ES_CALLOC(builder->loop_stack_capacity, sizeof(EsIRBasicBlock*));
    if (!builder->loop_continue_blocks) {
        ES_ERROR("Failed to allocate loop continue blocks array");
        goto cleanup;
    }
    builder->loop_break_blocks = ES_CALLOC(builder->loop_stack_capacity, sizeof(EsIRBasicBlock*));
    if (!builder->loop_break_blocks) {
        ES_ERROR("Failed to allocate loop break blocks array");
        goto cleanup;
    }

    builder->class_stack_size = 0;
    builder->class_stack_capacity = 4;
    builder->class_name_stack = ES_CALLOC(builder->class_stack_capacity, sizeof(char*));
    if (!builder->class_name_stack) {
        ES_ERROR("Failed to allocate class name stack");
        goto cleanup;
    }

    builder->layout_capacity = 4;
    builder->layout_count = 0;
    builder->layouts = ES_CALLOC(builder->layout_capacity, sizeof(EsIRClassLayout));
    if (!builder->layouts) {
        ES_ERROR("Failed to allocate layouts array");
        goto cleanup;
    }

    return builder;

cleanup:
    if (builder) {
        if (builder->loop_continue_blocks) ES_FREE(builder->loop_continue_blocks);
        if (builder->loop_break_blocks) ES_FREE(builder->loop_break_blocks);
        if (builder->class_name_stack) ES_FREE(builder->class_name_stack);
        if (builder->layouts) ES_FREE(builder->layouts);
        if (builder->arena) es_ir_arena_destroy(builder->arena);
        ES_FREE(builder);
    }
    return NULL;
}

void es_ir_builder_destroy(EsIRBuilder* builder) {
    if (!builder) return;
    
    
    es_ir_module_destroy(builder->module);
    
    
    if (builder->arena) {
        es_ir_arena_destroy(builder->arena);
    }

    
    if (builder->loop_continue_blocks) {
        ES_FREE(builder->loop_continue_blocks);
    }
    if (builder->loop_break_blocks) {
        ES_FREE(builder->loop_break_blocks);
    }

    if (builder->class_name_stack) {
        for (int i = 0; i < builder->class_stack_size; ++i) {
            ES_FREE(builder->class_name_stack[i]);
        }
        ES_FREE(builder->class_name_stack);
    }
    ES_FREE(builder);
}


EsIRFunction* es_ir_function_create(EsIRBuilder* builder, const char* name, EsIRParam* params, int param_count, EsTokenType return_type) {
    ES_IR_ASSERT_VALID_BUILDER(builder);
    ES_IR_ASSERT_NOT_NULL(name);
    
    ES_IR_ASSERT(param_count >= -1);
    
    
    if (param_count > 0) {
        ES_IR_ASSERT_NOT_NULL(params);
    }

    EsIRFunction* existing = builder->module->functions;
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            if (existing->param_count == -1 && param_count >= 0) {
                existing->param_count = param_count;
                if (param_count > 0) {
                    ES_IR_ALLOC_ARRAY_CHECKED(existing->params, EsIRParam, param_count, builder->arena);
                    
                    
                    if (!existing->param_table) {
                        existing->param_table = es_ir_param_table_create(builder->arena, param_count > 8 ? param_count : 8);
                        if (!existing->param_table) {
                            ES_IR_HANDLE_ALLOC_FAIL("Failed to create param table");
                        }
                    }
                    
                    for (int i = 0; i < param_count; i++) {
                        ES_IR_STRDUP_CHECKED(existing->params[i].name, params[i].name, builder->arena);
                        existing->params[i].type = params[i].type;
                        es_ir_param_table_add(existing->param_table, params[i].name, params[i].type, i);
                    }
                } else {
                    existing->params = NULL;
                }
            }
            return existing;
        }
        existing = existing->next;
    }

    EsIRFunction* func = (EsIRFunction*)es_ir_arena_alloc(builder->arena, sizeof(EsIRFunction));
    if (!func) {
        ES_IR_HANDLE_ALLOC_FAIL("Failed to allocate EsIRFunction");
    }
    memset(func, 0, sizeof(EsIRFunction));
    
    ES_IR_STRDUP_CHECKED(func->name, name, builder->arena);
    func->param_count = param_count;
    func->return_type = return_type;
    func->stack_size = 0;
    func->has_calls = 0;

    
    if (param_count >= 0) {
        func->param_table = es_ir_param_table_create(builder->arena, param_count > 8 ? param_count : 8);
        if (!func->param_table && param_count > 0) {
            ES_IR_HANDLE_ALLOC_FAIL("Failed to create param table");
        }
    }

    if (param_count > 0) {
        ES_IR_ALLOC_ARRAY_CHECKED(func->params, EsIRParam, param_count, builder->arena);
        for (int i = 0; i < param_count; i++) {
            ES_IR_STRDUP_CHECKED(func->params[i].name, params[i].name, builder->arena);
            func->params[i].type = params[i].type;
            
            
            es_ir_param_table_add(func->param_table, params[i].name, params[i].type, i);
        }
    }


    if (!builder->module->functions) {
        builder->module->functions = func;
    } else {
        EsIRFunction* last = builder->module->functions;
        while (last->next) {
            last = last->next;
        }
        last->next = func;
    }

    if (strcmp(name, "main") == 0) {
        builder->module->main_function = func;
    }

    return func;
}

void es_ir_function_set_entry(EsIRBuilder* builder, EsIRFunction* func) {
    builder->current_function = func;


}


static int g_block_id_counter = 0;

EsIRBasicBlock* es_ir_block_create(EsIRBuilder* builder, const char* label) {
    ES_IR_ASSERT_VALID_BUILDER(builder);
    ES_IR_ASSERT_NOT_NULL(label);
    
    EsIRBasicBlock* block = (EsIRBasicBlock*)es_ir_arena_alloc(builder->arena, sizeof(EsIRBasicBlock));
    if (!block) {
        ES_IR_HANDLE_ALLOC_FAIL("Failed to allocate EsIRBasicBlock");
    }
    
    memset(block, 0, sizeof(EsIRBasicBlock));
    ES_IR_STRDUP_CHECKED(block->label, label, builder->arena);
    block->id = g_block_id_counter++;
    
    
    block->inst_capacity = 16;
    ES_IR_ALLOC_ARRAY_CHECKED(block->insts, EsIRInst*, block->inst_capacity, builder->arena);
    block->inst_count = 0;
    
    
    block->pred_capacity = 4;
    ES_IR_ALLOC_ARRAY_CHECKED(block->preds, EsIRBasicBlock*, block->pred_capacity, builder->arena);
    block->pred_count = 0;
    
    block->succ_capacity = 4;
    ES_IR_ALLOC_ARRAY_CHECKED(block->succs, EsIRBasicBlock*, block->succ_capacity, builder->arena);
    block->succ_count = 0;
    
    
    block->cache_count = 0;
    
    return block;
}

void es_ir_block_set_current(EsIRBuilder* builder, EsIRBasicBlock* block) {
    if (!block) return;

    builder->current_block = block;


    if (builder->current_function && !builder->current_function->entry_block) {
        builder->current_function->entry_block = block;
    } else if (builder->current_function && builder->current_function->entry_block) {
        
        EsIRBasicBlock* check = builder->current_function->entry_block;
        while (check) {
            if (check == block) {
                
                return;
            }
            check = check->next;
        }

        EsIRBasicBlock* last = builder->current_function->entry_block;
        int count = 0;
        while (last->next) {
            if (count++ > 10000) {
                
                return;
            }
            last = last->next;
        }
        if (last != block) {
            last->next = block;
        }
    }
}



static inline int get_operand_capacity(EsIROpcode opcode) {
    switch (opcode) {
        
        case ES_IR_NOP:
            return 0;
        
        
        case ES_IR_LOAD:
        case ES_IR_ALLOC:
        case ES_IR_RETURN:
        case ES_IR_JUMP:
        case ES_IR_IMM:
        case ES_IR_INT_TO_STRING:
        case ES_IR_DOUBLE_TO_STRING:
            return 1;
        
        
        case ES_IR_STORE:
        case ES_IR_ADD:
        case ES_IR_SUB:
        case ES_IR_MUL:
        case ES_IR_DIV:
        case ES_IR_MOD:
        case ES_IR_AND:
        case ES_IR_OR:
        case ES_IR_XOR:
        case ES_IR_LSHIFT:
        case ES_IR_RSHIFT:
        case ES_IR_POW:
        case ES_IR_LT:
        case ES_IR_GT:
        case ES_IR_EQ:
        case ES_IR_LE:
        case ES_IR_GE:
        case ES_IR_NE:
        case ES_IR_STRCAT:
        case ES_IR_CAST:
        case ES_IR_COPY:
        case ES_IR_LOADPTR:
            return 2;
        
        
        case ES_IR_STOREPTR:
        case ES_IR_BRANCH:
        case ES_IR_ARRAY_STORE:
            return 3;
        
        
        
        case ES_IR_CALL:
            return 16;
        
        default:
            return 4;
    }
}

static EsIRInst* add_instruction(EsIRBuilder* builder, EsIROpcode opcode) {
    ES_IR_ASSERT_VALID_BUILDER(builder);
    ES_IR_ASSERT(opcode >= ES_IR_LOAD && opcode <= ES_IR_NOP);
    
    if (!builder->current_block) {
        char label[32];
        snprintf(label, sizeof(label), "block_%d", builder->label_counter++);
        EsIRBasicBlock* block = es_ir_block_create(builder, label);
        es_ir_block_set_current(builder, block);
    }
    
    ES_IR_ASSERT_NOT_NULL(builder->current_block);

    EsIRInst* inst = (EsIRInst*)es_ir_arena_alloc(builder->arena, sizeof(EsIRInst));
    if (!inst) {
        ES_IR_HANDLE_ALLOC_FAIL("Failed to allocate EsIRInst");
    }
    memset(inst, 0, sizeof(EsIRInst));
    inst->opcode = opcode;
    
    inst->operand_capacity = get_operand_capacity(opcode);
    if (inst->operand_capacity > 0) {
        inst->operands = (EsIRValue*)es_ir_arena_alloc(builder->arena, inst->operand_capacity * sizeof(EsIRValue));
        if (!inst->operands) {
            ES_IR_HANDLE_ALLOC_FAIL("Failed to allocate operand array");
        }
    } else {
        inst->operands = NULL;
    }

    EsIRBasicBlock* block = builder->current_block;
    
    
    if (block->inst_count >= block->inst_capacity) {
        
        int new_capacity = block->inst_capacity * 2;
        EsIRInst** new_insts = (EsIRInst**)ES_MALLOC(new_capacity * sizeof(EsIRInst*));
        if (!new_insts) return NULL;
        memcpy(new_insts, block->insts, block->inst_count * sizeof(EsIRInst*));
        block->insts = new_insts;
        block->inst_capacity = new_capacity;
    }
    
    block->insts[block->inst_count++] = inst;
    
    
    if (!block->first_inst) {
        block->first_inst = inst;
        block->last_inst = inst;
    } else {
        block->last_inst->next = inst;
        block->last_inst = inst;
    }
    
    
    if (block->cache_count < ES_IR_BLOCK_CACHE_SIZE) {
        
        if (opcode == ES_IR_LOAD || opcode == ES_IR_STORE || 
            opcode == ES_IR_ADD || opcode == ES_IR_SUB ||
            opcode == ES_IR_MUL || opcode == ES_IR_DIV) {
            block->cache[block->cache_count++] = inst;
        }
    }

    return inst;
}




static void add_operand(EsIRInst* inst, EsIRValue operand) {
    ES_IR_ASSERT_VALID_INST(inst);
    ES_IR_ASSERT_VALID_VALUE(operand);
    
    
    if (ES_IR_LIKELY(inst->operand_count < inst->operand_capacity)) {
        inst->operands[inst->operand_count++] = operand;
    } else {
        
        ES_IR_ASSERT_MSG(0, "Operand overflow - increase preallocation");
    }
}


EsIRValue es_ir_load(EsIRBuilder* builder, const char* name) {
    EsIRInst* inst = add_instruction(builder, ES_IR_LOAD);
    add_operand(inst, es_ir_var(builder, name));
    inst->result = es_ir_temp(builder);
    return inst->result;
}

void es_ir_store(EsIRBuilder* builder, const char* name, EsIRValue value) {
    EsIRInst* inst = add_instruction(builder, ES_IR_STORE);
    add_operand(inst, es_ir_var(builder, name));
    add_operand(inst, value);
}

void es_ir_alloc(EsIRBuilder* builder, const char* name) {
    EsIRInst* inst = add_instruction(builder, ES_IR_ALLOC);
    add_operand(inst, es_ir_var(builder, name));
}

EsIRValue es_ir_load_ptr(EsIRBuilder* builder, EsIRValue base, int offset) {
    EsIRInst* inst = add_instruction(builder, ES_IR_LOADPTR);
    add_operand(inst, base);
    add_operand(inst, es_ir_imm(builder, offset));
    inst->result = es_ir_temp(builder);
    return inst->result;
}

void es_ir_store_ptr(EsIRBuilder* builder, EsIRValue base, int offset, EsIRValue value) {
    EsIRInst* inst = add_instruction(builder, ES_IR_STOREPTR);
    add_operand(inst, base);
    add_operand(inst, es_ir_imm(builder, offset));
    add_operand(inst, value);
}

void es_ir_array_store(EsIRBuilder* builder, EsIRValue array, EsIRValue index, EsIRValue value) {
    EsIRInst* inst = add_instruction(builder, ES_IR_ARRAY_STORE);
    add_operand(inst, array);
    add_operand(inst, index);
    add_operand(inst, value);
}

EsIRValue es_ir_add(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_ADD);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_sub(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_SUB);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_mul(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_MUL);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_div(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_DIV);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_mod(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_MOD);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_and(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_AND);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_or(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_OR);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_xor(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_XOR);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_lshift(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_LSHIFT);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_rshift(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, ES_IR_RSHIFT);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_pow(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    EsIRInst* inst = add_instruction(builder, ES_IR_POW);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_strcat(EsIRBuilder* builder, EsIRValue lhs, EsIRValue rhs) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    EsIRInst* inst = add_instruction(builder, ES_IR_STRCAT);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    
    return inst->result;
}

EsIRValue es_ir_int_to_string(EsIRBuilder* builder, EsIRValue value) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    EsIRInst* inst = add_instruction(builder, ES_IR_INT_TO_STRING);
    add_operand(inst, value);
    inst->result = es_ir_temp(builder);
    
    return inst->result;
}

EsIRValue es_ir_double_to_string(EsIRBuilder* builder, EsIRValue value) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    EsIRInst* inst = add_instruction(builder, ES_IR_DOUBLE_TO_STRING);
    add_operand(inst, value);
    inst->result = es_ir_temp(builder);
    
    return inst->result;
}

EsIRValue es_ir_cast(EsIRBuilder* builder, EsIRValue value, EsTokenType target_type) {
    EsIRInst* inst = add_instruction(builder, ES_IR_CAST);
    add_operand(inst, value);


    EsIRValue type_value = {0};
    type_value.type = ES_IR_VALUE_IMM;
    type_value.data.imm = (double)target_type;
    add_operand(inst, type_value);

    inst->result = es_ir_temp(builder);
    return inst->result;
}

EsIRValue es_ir_compare(EsIRBuilder* builder, EsIROpcode op, EsIRValue lhs, EsIRValue rhs) {
    EsIRInst* inst = add_instruction(builder, op);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = es_ir_temp(builder);
    return inst->result;
}

void es_ir_jump(EsIRBuilder* builder, EsIRBasicBlock* target) {
    if (!builder || !target) return;

    EsIRInst* inst = add_instruction(builder, ES_IR_JUMP);
    if (inst) {
        inst->operand_count = 1;
        inst->operands[0].type = ES_IR_VALUE_VAR;
        inst->operands[0].data.name = es_ir_arena_strdup(builder->arena, target->label);
    }
}

void es_ir_branch(EsIRBuilder* builder, EsIRValue cond, EsIRBasicBlock* true_block, EsIRBasicBlock* false_block) {
    EsIRInst* inst = add_instruction(builder, ES_IR_BRANCH);
    add_operand(inst, cond);

    EsIRValue true_val = {0};
    true_val.type = ES_IR_VALUE_VAR;
    true_val.data.name = es_ir_arena_strdup(builder->arena, true_block->label);
    add_operand(inst, true_val);

    EsIRValue false_val = {0};
    false_val.type = ES_IR_VALUE_VAR;
    false_val.data.name = es_ir_arena_strdup(builder->arena, false_block->label);
    add_operand(inst, false_val);
}

EsIRValue es_ir_call(EsIRBuilder* builder, const char* func_name, EsIRValue* args, int arg_count) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    EsIRInst* inst = add_instruction(builder, ES_IR_CALL);

    EsIRValue func_val = {0};
    func_val.type = ES_IR_VALUE_FUNCTION;
    func_val.data.function_name = es_ir_arena_strdup(builder->arena, func_name);
    add_operand(inst, func_val);

    for (int i = 0; i < arg_count; i++) {
        add_operand(inst, args[i]);
    }

    inst->result = es_ir_temp(builder);
    return inst->result;
}

void es_ir_return(EsIRBuilder* builder, EsIRValue value) {
    EsIRInst* inst = add_instruction(builder, ES_IR_RETURN);
    add_operand(inst, value);
}

void es_ir_label(EsIRBuilder* builder, const char* label) {

    EsIRBasicBlock* block = es_ir_block_create(builder, label);


    es_ir_block_set_current(builder, block);
}

void es_ir_nop(EsIRBuilder* builder) {
    add_instruction(builder, ES_IR_NOP);
}

void es_ir_push_loop_context(EsIRBuilder* builder, EsIRBasicBlock* continue_block, EsIRBasicBlock* break_block) {
    if (!builder) return;

    if (builder->loop_stack_size >= builder->loop_stack_capacity) {
        builder->loop_stack_capacity *= 2;
        builder->loop_continue_blocks = ES_REALLOC(builder->loop_continue_blocks,
                                                    builder->loop_stack_capacity * sizeof(EsIRBasicBlock*));
        builder->loop_break_blocks = ES_REALLOC(builder->loop_break_blocks,
                                                 builder->loop_stack_capacity * sizeof(EsIRBasicBlock*));
    }

    builder->loop_continue_blocks[builder->loop_stack_size] = continue_block;
    builder->loop_break_blocks[builder->loop_stack_size] = break_block;
    builder->loop_stack_size++;
}

void es_ir_pop_loop_context(EsIRBuilder* builder) {
    if (!builder || builder->loop_stack_size <= 0) return;

    builder->loop_stack_size--;
    builder->loop_continue_blocks[builder->loop_stack_size] = NULL;
    builder->loop_break_blocks[builder->loop_stack_size] = NULL;
}

EsIRBasicBlock* es_ir_get_current_continue_block(EsIRBuilder* builder) {
    if (!builder || builder->loop_stack_size <= 0) return NULL;
    return builder->loop_continue_blocks[builder->loop_stack_size - 1];
}

EsIRBasicBlock* es_ir_get_current_break_block(EsIRBuilder* builder) {
    if (!builder || builder->loop_stack_size <= 0) return NULL;
    return builder->loop_break_blocks[builder->loop_stack_size - 1];
}



int es_ir_block_get_inst_count(EsIRBasicBlock* block) {
    if (!block) return 0;
    return block->inst_count;
}

EsIRInst* es_ir_block_get_inst(EsIRBasicBlock* block, int index) {
    if (!block || index < 0 || index >= block->inst_count) return NULL;
    return block->insts[index];
}

EsIRInst* es_ir_block_get_first_inst(EsIRBasicBlock* block) {
    if (!block || block->inst_count == 0) return NULL;
    return block->insts[0];
}

EsIRInst* es_ir_block_get_last_inst(EsIRBasicBlock* block) {
    if (!block || block->inst_count == 0) return NULL;
    return block->insts[block->inst_count - 1];
}



void es_ir_block_add_pred(EsIRBuilder* builder, EsIRBasicBlock* block, EsIRBasicBlock* pred) {
    if (!block || !pred || !builder) return;
    
    
    for (int i = 0; i < block->pred_count; i++) {
        if (block->preds[i] == pred) return;
    }
    
    
    if (block->pred_count >= block->pred_capacity) {
        int new_capacity = block->pred_capacity * 2;
        EsIRBasicBlock** new_preds = (EsIRBasicBlock**)ES_MALLOC(new_capacity * sizeof(EsIRBasicBlock*));
        if (!new_preds) return;
        memcpy(new_preds, block->preds, block->pred_count * sizeof(EsIRBasicBlock*));
        block->preds = new_preds;
        block->pred_capacity = new_capacity;
    }
    
    block->preds[block->pred_count++] = pred;
}

void es_ir_block_add_succ(EsIRBuilder* builder, EsIRBasicBlock* block, EsIRBasicBlock* succ) {
    if (!block || !succ || !builder) return;
    
    
    for (int i = 0; i < block->succ_count; i++) {
        if (block->succs[i] == succ) return;
    }
    
    
    if (block->succ_count >= block->succ_capacity) {
        int new_capacity = block->succ_capacity * 2;
        EsIRBasicBlock** new_succs = (EsIRBasicBlock**)ES_MALLOC(new_capacity * sizeof(EsIRBasicBlock*));
        if (!new_succs) return;
        memcpy(new_succs, block->succs, block->succ_count * sizeof(EsIRBasicBlock*));
        block->succs = new_succs;
        block->succ_capacity = new_capacity;
    }
    
    block->succs[block->succ_count++] = succ;
}

int es_ir_block_get_pred_count(EsIRBasicBlock* block) {
    if (!block) return 0;
    return block->pred_count;
}

int es_ir_block_get_succ_count(EsIRBasicBlock* block) {
    if (!block) return 0;
    return block->succ_count;
}

EsIRBasicBlock* es_ir_block_get_pred(EsIRBasicBlock* block, int index) {
    if (!block || index < 0 || index >= block->pred_count) return NULL;
    return block->preds[index];
}

EsIRBasicBlock* es_ir_block_get_succ(EsIRBasicBlock* block, int index) {
    if (!block || index < 0 || index >= block->succ_count) return NULL;
    return block->succs[index];
}



void es_ir_block_invalidate_cache(EsIRBasicBlock* block) {
    if (!block) return;
    block->cache_count = 0;
    memset(block->cache, 0, sizeof(block->cache));
}

EsIRInst* es_ir_block_find_cached_inst(EsIRBasicBlock* block, EsIROpcode opcode) {
    if (!block) return NULL;
    
    for (int i = 0; i < block->cache_count; i++) {
        if (block->cache[i] && block->cache[i]->opcode == opcode) {
            return block->cache[i];
        }
    }
    return NULL;
}


static int find_block_index(EsIRFunction* func, const char* label) {
    if (!func || !label) return -1;

    EsIRBasicBlock* block = func->entry_block;
    int index = 0;
    while (block) {
        if (strcmp(block->label, label) == 0) {
            return index;
        }
        block = block->next;
        index++;
    }
    return -1;
}

static void es_ir_print_instruction(EsIRInst* inst, FILE* output, EsIRFunction* func) {
    if (!inst || !output) return;


    if (inst->result.type != ES_IR_VALUE_VOID) {
        switch (inst->result.type) {
            case ES_IR_VALUE_TEMP:
                fprintf(output, "%%%d = ", inst->result.data.index);
                break;
            case ES_IR_VALUE_VAR:
                fprintf(output, "@%s = ", inst->result.data.name);
                break;
            default:
                break;
        }
    }


    const char* op_name = "unknown";
    switch (inst->opcode) {
        case ES_IR_LOAD: op_name = "load"; break;
        case ES_IR_STORE: op_name = "store"; break;
        case ES_IR_ALLOC: op_name = "alloc"; break;
        case ES_IR_ADD: op_name = "add"; break;
        case ES_IR_SUB: op_name = "sub"; break;
        case ES_IR_MUL: op_name = "mul"; break;
        case ES_IR_DIV: op_name = "div"; break;
        case ES_IR_LT: op_name = "icmp slt"; break;
        case ES_IR_GT: op_name = "icmp sgt"; break;
        case ES_IR_EQ: op_name = "icmp eq"; break;
        case ES_IR_LE: op_name = "icmp sle"; break;
        case ES_IR_GE: op_name = "icmp sge"; break;
        case ES_IR_NE: op_name = "icmp ne"; break;
        case ES_IR_JUMP: op_name = "br"; break;
        case ES_IR_BRANCH: op_name = "br"; break;
        case ES_IR_CALL: op_name = "call"; break;
        case ES_IR_RETURN: op_name = "ret"; break;
        case ES_IR_LABEL: op_name = "label"; break;
        case ES_IR_STRCAT: op_name = "strcat"; break;
        case ES_IR_CAST: op_name = "cast"; break;
        case ES_IR_LOADPTR: op_name = "loadptr"; break;
        case ES_IR_STOREPTR: op_name = "storeptr"; break;
        case ES_IR_ARRAY_STORE: op_name = "array_store"; break;
        case ES_IR_INT_TO_STRING: op_name = "int_to_string"; break;
        case ES_IR_DOUBLE_TO_STRING: op_name = "double_to_string"; break;
        case ES_IR_NOP: op_name = "nop"; break;
    }
    fprintf(output, "%s", op_name);


    if (inst->operand_count > 0) {
        fprintf(output, " ");
        for (int i = 0; i < inst->operand_count; i++) {
            if (i > 0) fprintf(output, ", ");


            if ((inst->opcode == ES_IR_JUMP || inst->opcode == ES_IR_BRANCH) &&
                inst->operands[i].type == ES_IR_VALUE_VAR && func) {

                if (inst->opcode == ES_IR_BRANCH) {
                    if (i == 0) {

                        switch (inst->operands[i].type) {
                            case ES_IR_VALUE_IMM:
                                fprintf(output, "%.0f", inst->operands[i].data.imm);
                                break;
                            case ES_IR_VALUE_VAR:
                                fprintf(output, "@%s", inst->operands[i].data.name);
                                break;
                            case ES_IR_VALUE_TEMP:
                                fprintf(output, "%%%d", inst->operands[i].data.index);
                                break;
                            case ES_IR_VALUE_ARG:
                                fprintf(output, "%%%d", inst->operands[i].data.index);
                                break;
                            default:
                                fprintf(output, "void");
                                break;
                        }
                        continue;
                    } else if (i == 1 || i == 2) {

                        int block_index = find_block_index(func, inst->operands[i].data.name);
                        if (block_index >= 0) {
                            fprintf(output, "%%%d", block_index);
                            continue;
                        }
                    }
                } else {

                    int block_index = find_block_index(func, inst->operands[i].data.name);
                    if (block_index >= 0) {
                        fprintf(output, "%%%d", block_index);
                        continue;
                    }
                }
            }

            switch (inst->operands[i].type) {
                case ES_IR_VALUE_IMM:
                    fprintf(output, "%.0f", inst->operands[i].data.imm);
                    break;
                case ES_IR_VALUE_VAR:
                    fprintf(output, "@%s", inst->operands[i].data.name);
                    break;
                case ES_IR_VALUE_TEMP:
                    fprintf(output, "%%%d", inst->operands[i].data.index);
                    break;
                case ES_IR_VALUE_ARG:
                    fprintf(output, "%%%d", inst->operands[i].data.index);
                    break;
                case ES_IR_VALUE_STRING_CONST:
                    fprintf(output, "str_const[%d]", inst->operands[i].data.string_const_id);
                    break;
                case ES_IR_VALUE_FUNCTION:
                    fprintf(output, "%s", inst->operands[i].data.function_name);
                    break;
                default:
                    fprintf(output, "void");
                    break;
            }
        }
    }
}


static const char* es_token_type_to_string(EsTokenType type) {
    switch (type) {
        case TOKEN_VOID: return "void";
        case TOKEN_INT8: return "i8";
        case TOKEN_INT16: return "i16";
        case TOKEN_INT32: return "i32";
        case TOKEN_INT64: return "i64";
        case TOKEN_UINT8: return "u8";
        case TOKEN_UINT16: return "u16";
        case TOKEN_UINT32: return "u32";
        case TOKEN_UINT64: return "u64";
        case TOKEN_FLOAT32: return "f32";
        case TOKEN_FLOAT64: return "f64";
        case TOKEN_BOOL: return "bool";
        case TOKEN_CHAR: return "char";
        case TOKEN_STRING:
        case TOKEN_TYPE_STRING: return "str";
        default: return "unknown";
    }
}

void es_ir_print(EsIRModule* module, FILE* output) {
    if (!module || !output) return;

    fprintf(output, "; E# IR v1.0\n");
    fprintf(output, "; Generated by E# Compiler\n\n");

    EsIRFunction* func = module->functions;
    while (func) {
        fprintf(output, "define %s @%s(", es_token_type_to_string(func->return_type), func->name);

        for (int i = 0; i < func->param_count; i++) {
            if (i > 0) fprintf(output, ", ");
            fprintf(output, "%s %%%d", es_token_type_to_string(func->params[i].type), i);
        }
        fprintf(output, ") {\n");

        EsIRBasicBlock* block = func->entry_block;
        int block_index = 0;
        while (block) {
            fprintf(output, "  %%%d: ; %s (%d insts)\n", block_index++, block->label, block->inst_count);

            
            for (int i = 0; i < block->inst_count; i++) {
                fprintf(output, "    ");
                es_ir_print_instruction(block->insts[i], output, func);
                fprintf(output, "\n");
            }

            block = block->next;
        }

        fprintf(output, "}\n\n");
        func = func->next;
    }

    fflush(output);
}

static EsIRClassLayout* ensure_class_layout(EsIRBuilder* builder, const char* class_name) {
    if (!builder || !class_name) return NULL;
    for (int i = 0; i < builder->layout_count; i++) {
        if (builder->layouts[i].class_name && strcmp(builder->layouts[i].class_name, class_name) == 0) {
            return &builder->layouts[i];
        }
    }
    if (builder->layout_count >= builder->layout_capacity) {
        int old = builder->layout_capacity;
        builder->layout_capacity = old > 0 ? old * 2 : 4;
        builder->layouts = ES_REALLOC(builder->layouts, builder->layout_capacity * sizeof(EsIRClassLayout));
        memset(builder->layouts + old, 0, (builder->layout_capacity - old) * sizeof(EsIRClassLayout));
    }
    EsIRClassLayout* layout = &builder->layouts[builder->layout_count++];
    layout->class_name = ES_STRDUP(class_name);
    layout->field_capacity = 8;
    layout->field_count = 0;
    layout->fields = ES_CALLOC(layout->field_capacity, sizeof(EsIRFieldOffset));
    return layout;
}

void es_ir_register_class_layout(EsIRBuilder* builder, const char* class_name, ASTNode* class_body) {
    if (!builder || !class_name || !class_body) return;
    EsIRClassLayout* layout = ensure_class_layout(builder, class_name);
    if (!layout) return;
    if (class_body->type == AST_BLOCK) {
        int slot = 0;
        for (int i = 0; i < class_body->data.block.statement_count; i++) {
            ASTNode* member = class_body->data.block.statements[i];
            if (!member) continue;
            if (member->type == AST_ACCESS_MODIFIER) {
                member = member->data.access_modifier.member;
                if (!member) continue;
            }
            if (member->type == AST_VARIABLE_DECLARATION) {
                if (layout->field_count >= layout->field_capacity) {
                    int old = layout->field_capacity;
                    layout->field_capacity = old > 0 ? old * 2 : 8;
                    layout->fields = ES_REALLOC(layout->fields, layout->field_capacity * sizeof(EsIRFieldOffset));
                }
                layout->fields[layout->field_count].name = ES_STRDUP(member->data.variable_decl.name);
                layout->fields[layout->field_count].offset = slot * 8;
                layout->field_count++;
                slot++;
            }
        }
    }
}

int es_ir_layout_get_offset(EsIRBuilder* builder, const char* class_name, const char* field_name) {
    if (!builder || !class_name || !field_name) return -1;
    for (int i = 0; i < builder->layout_count; i++) {
        EsIRClassLayout* layout = &builder->layouts[i];
        if (layout->class_name && strcmp(layout->class_name, class_name) == 0) {
            for (int j = 0; j < layout->field_count; j++) {
                if (layout->fields[j].name && strcmp(layout->fields[j].name, field_name) == 0) {
                    return layout->fields[j].offset;
                }
            }
        }
    }
    return -1;
}

int es_ir_layout_get_size(EsIRBuilder* builder, const char* class_name) {
    if (!builder || !class_name) return 0;
    for (int i = 0; i < builder->layout_count; i++) {
        EsIRClassLayout* layout = &builder->layouts[i];
        if (layout->class_name && strcmp(layout->class_name, class_name) == 0) {
            return layout->field_count * 8;
        }
    }
    return 8 * 8;
}