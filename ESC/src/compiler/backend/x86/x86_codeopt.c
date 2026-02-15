#include "x86_codeopt.h"
#include "../../../core/utils/es_common.h"
#include "../../middle/ir/ir.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    char* pattern;
    char* replacement;
    int size;
} PeepholePattern;

typedef struct X86PeepholeOptimizer {
    PeepholePattern* patterns;
    int pattern_count;
    int pattern_capacity;
    int optimization_count;
} X86PeepholeOptimizer;

X86PeepholeOptimizer* x86_peephole_optimizer_create(void) {
    X86PeepholeOptimizer* optimizer = (X86PeepholeOptimizer*)calloc(1, sizeof(X86PeepholeOptimizer));
    if (!optimizer) return NULL;
    
    optimizer->pattern_capacity = 32;
    optimizer->patterns = (PeepholePattern*)calloc(optimizer->pattern_capacity, sizeof(PeepholePattern));
    if (!optimizer->patterns) {
        free(optimizer);
        return NULL;
    }
    
    return optimizer;
}

void x86_peephole_optimizer_destroy(X86PeepholeOptimizer* optimizer) {
    if (!optimizer) return;
    
    for (int i = 0; i < optimizer->pattern_count; i++) {
        free(optimizer->patterns[i].pattern);
        free(optimizer->patterns[i].replacement);
    }
    free(optimizer->patterns);
    free(optimizer);
}

typedef struct {
    EsIROpcode opcode;
    EsIRValue operands[3];
    int operand_count;
    EsIRValue result;
} InstructionPattern;

static bool match_mov_mov(EsIRInst* inst1, EsIRInst* inst2) {
    if (!inst1 || !inst2) return false;
    
    if (inst1->opcode == ES_IR_COPY && inst2->opcode == ES_IR_COPY) {
        if (inst1->operand_count == 1 && inst2->operand_count == 1) {
            if (inst1->result.type == ES_IR_VALUE_TEMP &&
                inst2->operands[0].type == ES_IR_VALUE_TEMP &&
                inst1->result.data.index == inst2->operands[0].data.index) {
                return true;
            }
        }
    }
    return false;
}

static bool match_redundant_load_store(EsIRInst* inst1, EsIRInst* inst2) {
    if (!inst1 || !inst2) return false;
    
    if (inst1->opcode == ES_IR_STORE && inst2->opcode == ES_IR_LOAD) {
        if (inst1->operand_count >= 2 && inst2->operand_count >= 1) {
            if (inst1->operands[0].type == ES_IR_VALUE_VAR &&
                inst2->operands[0].type == ES_IR_VALUE_VAR &&
                strcmp(inst1->operands[0].data.name, inst2->operands[0].data.name) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool match_algebraic_identity(EsIRInst* inst) {
    if (!inst) return false;
    
    switch (inst->opcode) {
        case ES_IR_ADD:
            if (inst->operand_count == 2 &&
                inst->operands[1].type == ES_IR_VALUE_IMM &&
                inst->operands[1].data.imm == 0) {
                return true;
            }
            break;
            
        case ES_IR_MUL:
            if (inst->operand_count == 2 &&
                inst->operands[1].type == ES_IR_VALUE_IMM &&
                inst->operands[1].data.imm == 1) {
                return true;
            }
            break;
            
        case ES_IR_SUB:
            if (inst->operand_count == 2 &&
                inst->operands[1].type == ES_IR_VALUE_IMM &&
                inst->operands[1].data.imm == 0) {
                return true;
            }
            break;
            
        case ES_IR_DIV:
            if (inst->operand_count == 2 &&
                inst->operands[1].type == ES_IR_VALUE_IMM &&
                inst->operands[1].data.imm == 1) {
                return true;
            }
            break;
            
        default:
            break;
    }
    
    return false;
}

static bool match_strength_reduction_candidate(EsIRInst* inst) {
    if (!inst) return false;
    
    if (inst->opcode == ES_IR_MUL && inst->operand_count == 2) {
        if (inst->operands[1].type == ES_IR_VALUE_IMM) {
            double val = inst->operands[1].data.imm;
            if (val == 2 || val == 4 || val == 8 || val == 16) {
                return true;
            }
        }
    }
    
    return false;
}

static bool apply_mov_mov_elimination(EsIRBasicBlock* block, EsIRInst* inst1, EsIRInst* inst2) {
    if (!match_mov_mov(inst1, inst2)) return false;
    
    inst2->operands[0] = inst1->operands[0];
    return true;
}

static bool apply_redundant_load_store_elimination(EsIRBasicBlock* block, EsIRInst* inst1, EsIRInst* inst2) {
    if (!match_redundant_load_store(inst1, inst2)) return false;
    
    inst2->opcode = ES_IR_COPY;
    inst2->operands[0] = inst1->operands[1];
    inst2->operand_count = 1;
    return true;
}

static bool apply_algebraic_identity(EsIRInst* inst) {
    if (!match_algebraic_identity(inst)) return false;
    
    switch (inst->opcode) {
        case ES_IR_ADD:
        case ES_IR_SUB:
        case ES_IR_MUL:
        case ES_IR_DIV:
            inst->opcode = ES_IR_COPY;
            inst->operand_count = 1;
            return true;
            
        default:
            return false;
    }
}

static bool apply_strength_reduction(EsIRInst* inst) {
    if (!match_strength_reduction_candidate(inst)) return false;
    
    double val = inst->operands[1].data.imm;
    int shift_amount = 0;
    
    switch ((int)val) {
        case 2: shift_amount = 1; break;
        case 4: shift_amount = 2; break;
        case 8: shift_amount = 3; break;
        case 16: shift_amount = 4; break;
        default: return false;
    }
    
    inst->opcode = ES_IR_LSHIFT;
    inst->operands[1].data.imm = shift_amount;
    return true;
}

static bool optimize_basic_block(X86PeepholeOptimizer* optimizer, EsIRBasicBlock* block) {
    if (!block || !block->first_inst) return false;
    
    bool changed = false;
    EsIRInst* inst = block->first_inst;
    
    while (inst && inst->next) {
        EsIRInst* next = inst->next;
        
        if (apply_mov_mov_elimination(block, inst, next)) {
            optimizer->optimization_count++;
            changed = true;
        }
        else if (apply_redundant_load_store_elimination(block, inst, next)) {
            optimizer->optimization_count++;
            changed = true;
        }
        
        inst = next;
    }
    
    inst = block->first_inst;
    while (inst) {
        if (apply_algebraic_identity(inst)) {
            optimizer->optimization_count++;
            changed = true;
        }
        else if (apply_strength_reduction(inst)) {
            optimizer->optimization_count++;
            changed = true;
        }
        
        inst = inst->next;
    }
    
    return changed;
}

void x86_optimize_peephole(EsIRModule* module, int optimization_level) {
    if (!module || optimization_level <= 0) return;
    
    X86PeepholeOptimizer* optimizer = x86_peephole_optimizer_create();
    if (!optimizer) return;
    
    int max_passes = (optimization_level >= 2) ? 5 : 3;
    int pass = 0;
    bool changed = true;
    
    while (changed && pass < max_passes) {
        changed = false;
        pass++;
        
        EsIRFunction* func = module->functions;
        while (func) {
            EsIRBasicBlock* block = func->entry_block;
            while (block) {
                changed |= optimize_basic_block(optimizer, block);
                block = block->next;
            }
            func = func->next;
        }
    }
    
    x86_peephole_optimizer_destroy(optimizer);
}

int x86_peephole_get_optimization_count(X86PeepholeOptimizer* optimizer) {
    return optimizer ? optimizer->optimization_count : 0;
}
