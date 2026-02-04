#include "ir_assert.h"
#include <stdio.h>





bool es_ir_builder_is_valid(EsIRBuilder* builder) {
    if (!builder) return false;
    if (!builder->arena) return false;
    if (!builder->module) return false;
    return true;
}

bool es_ir_function_is_valid(EsIRFunction* func) {
    if (!func) return false;
    if (!func->name) return false;
    if (func->param_count < 0) return false;
    return true;
}

bool es_ir_block_is_valid(EsIRBasicBlock* block) {
    if (!block) return false;
    if (!block->label) return false;
    return true;
}

bool es_ir_inst_is_valid(EsIRInst* inst) {
    if (!inst) return false;
    if (inst->opcode < ES_IR_LOAD || inst->opcode > ES_IR_NOP) return false;
    if (inst->operand_count < 0) return false;
    if (inst->operand_count > inst->operand_capacity) return false;
    return true;
}

bool es_ir_value_is_valid(EsIRValue* value) {
    if (!value) return false;
    if (value->type < ES_IR_VALUE_VOID || value->type > ES_IR_VALUE_FUNCTION) return false;
    return true;
}

bool es_ir_arena_is_valid(EsIRMemoryArena* arena) {
    if (!arena) return false;
    if (!arena->current_pool) return false;
    if (arena->pool_size == 0) return false;
    return true;
}

bool es_ir_operand_index_is_valid(EsIRInst* inst, int index) {
    if (!inst) return false;
    if (index < 0 || index >= inst->operand_count) return false;
    return true;
}

bool es_ir_param_index_is_valid(EsIRFunction* func, int index) {
    if (!func) return false;
    if (index < 0 || index >= func->param_count) return false;
    return true;
}





#if ES_IR_ASSERT_ENABLED

static const char* es_ir_opcode_to_string(EsIROpcode opcode) {
    switch (opcode) {
        case ES_IR_LOAD: return "LOAD";
        case ES_IR_STORE: return "STORE";
        case ES_IR_ALLOC: return "ALLOC";
        case ES_IR_IMM: return "IMM";
        case ES_IR_ADD: return "ADD";
        case ES_IR_SUB: return "SUB";
        case ES_IR_MUL: return "MUL";
        case ES_IR_DIV: return "DIV";
        case ES_IR_MOD: return "MOD";
        case ES_IR_AND: return "AND";
        case ES_IR_OR: return "OR";
        case ES_IR_XOR: return "XOR";
        case ES_IR_LSHIFT: return "LSHIFT";
        case ES_IR_RSHIFT: return "RSHIFT";
        case ES_IR_POW: return "POW";
        case ES_IR_LT: return "LT";
        case ES_IR_GT: return "GT";
        case ES_IR_EQ: return "EQ";
        case ES_IR_LE: return "LE";
        case ES_IR_GE: return "GE";
        case ES_IR_NE: return "NE";
        case ES_IR_JUMP: return "JUMP";
        case ES_IR_BRANCH: return "BRANCH";
        case ES_IR_CALL: return "CALL";
        case ES_IR_RETURN: return "RETURN";
        case ES_IR_LABEL: return "LABEL";
        case ES_IR_STRCAT: return "STRCAT";
        case ES_IR_CAST: return "CAST";
        case ES_IR_LOADPTR: return "LOADPTR";
        case ES_IR_STOREPTR: return "STOREPTR";
        case ES_IR_ARRAY_STORE: return "ARRAY_STORE";
        case ES_IR_INT_TO_STRING: return "INT_TO_STRING";
        case ES_IR_DOUBLE_TO_STRING: return "DOUBLE_TO_STRING";
        case ES_IR_COPY: return "COPY";
        case ES_IR_NOP: return "NOP";
        default: return "UNKNOWN";
    }
}

static const char* es_ir_value_type_to_string(EsIRValueType type) {
    switch (type) {
        case ES_IR_VALUE_VOID: return "VOID";
        case ES_IR_VALUE_IMM: return "IMM";
        case ES_IR_VALUE_VAR: return "VAR";
        case ES_IR_VALUE_TEMP: return "TEMP";
        case ES_IR_VALUE_ARG: return "ARG";
        case ES_IR_VALUE_STRING_CONST: return "STRING_CONST";
        case ES_IR_VALUE_FUNCTION: return "FUNCTION";
        default: return "UNKNOWN";
    }
}

void es_ir_debug_print_builder(EsIRBuilder* builder, const char* label) {
    printf("[IR_DEBUG] %s EsIRBuilder at %p:\n", label ? label : "Builder", (void*)builder);
    if (!builder) {
        printf("  (null)\n");
        return;
    }
    printf("  arena: %p\n", (void*)builder->arena);
    printf("  module: %p\n", (void*)builder->module);
    printf("  current_function: %p\n", (void*)builder->current_function);
    printf("  current_block: %p\n", (void*)builder->current_block);
    printf("  temp_counter: %d\n", builder->temp_counter);
    printf("  label_counter: %d\n", builder->label_counter);
    printf("  loop_stack_size: %d/%d\n", builder->loop_stack_size, builder->loop_stack_capacity);
}

void es_ir_debug_print_function(EsIRFunction* func, const char* label) {
    printf("[IR_DEBUG] %s EsIRFunction at %p:\n", label ? label : "Function", (void*)func);
    if (!func) {
        printf("  (null)\n");
        return;
    }
    printf("  name: %s\n", func->name ? func->name : "(null)");
    printf("  param_count: %d\n", func->param_count);
    printf("  return_type: %d\n", func->return_type);
    printf("  entry_block: %p\n", (void*)func->entry_block);
    printf("  exit_block: %p\n", (void*)func->exit_block);
    printf("  stack_size: %d\n", func->stack_size);
    printf("  has_calls: %d\n", func->has_calls);
}

void es_ir_debug_print_block(EsIRBasicBlock* block, const char* label) {
    printf("[IR_DEBUG] %s EsIRBasicBlock at %p:\n", label ? label : "Block", (void*)block);
    if (!block) {
        printf("  (null)\n");
        return;
    }
    printf("  label: %s\n", block->label ? block->label : "(null)");
    printf("  first_inst: %p\n", (void*)block->first_inst);
    printf("  last_inst: %p\n", (void*)block->last_inst);
    
    int count = 0;
    EsIRInst* inst = block->first_inst;
    while (inst) {
        count++;
        inst = inst->next;
    }
    printf("  instruction_count: %d\n", count);
}

void es_ir_debug_print_inst(EsIRInst* inst, const char* label) {
    printf("[IR_DEBUG] %s EsIRInst at %p:\n", label ? label : "Inst", (void*)inst);
    if (!inst) {
        printf("  (null)\n");
        return;
    }
    printf("  opcode: %s (%d)\n", es_ir_opcode_to_string(inst->opcode), inst->opcode);
    printf("  operand_count: %d/%d\n", inst->operand_count, inst->operand_capacity);
    printf("  is_int_result: %d\n", inst->is_int_result);
    
    if (inst->operand_count > 0 && inst->operands) {
        printf("  operands:\n");
        for (int i = 0; i < inst->operand_count && i < 4; i++) {
            printf("    [%d]: type=%s\n", i, es_ir_value_type_to_string(inst->operands[i].type));
        }
        if (inst->operand_count > 4) {
            printf("    ... (%d more)\n", inst->operand_count - 4);
        }
    }
}

void es_ir_debug_print_value(EsIRValue* value, const char* label) {
    printf("[IR_DEBUG] %s EsIRValue at %p:\n", label ? label : "Value", (void*)value);
    if (!value) {
        printf("  (null)\n");
        return;
    }
    printf("  type: %s (%d)\n", es_ir_value_type_to_string(value->type), value->type);
    switch (value->type) {
        case ES_IR_VALUE_IMM:
            printf("  data.imm: %f\n", value->data.imm);
            break;
        case ES_IR_VALUE_VAR:
        case ES_IR_VALUE_FUNCTION:
            printf("  data.name: %s\n", value->data.name ? value->data.name : "(null)");
            break;
        case ES_IR_VALUE_TEMP:
        case ES_IR_VALUE_ARG:
            printf("  data.index: %d\n", value->data.index);
            break;
        case ES_IR_VALUE_STRING_CONST:
            printf("  data.string_const_id: %d\n", value->data.string_const_id);
            break;
        default:
            break;
    }
}

#endif 
