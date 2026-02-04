#include "ir_ssa.h"
#include <string.h>
#include <stdio.h>


static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}



EsIRVarTable* es_ir_var_table_create(EsIRMemoryArena* arena, int bucket_count) {
    if (!arena) return NULL;
    
    EsIRVarTable* table = (EsIRVarTable*)es_ir_arena_alloc(arena, sizeof(EsIRVarTable));
    if (!table) return NULL;
    
    table->buckets = (EsIRVarVersion**)es_ir_arena_alloc(arena, bucket_count * sizeof(EsIRVarVersion*));
    if (!table->buckets) return NULL;
    
    memset(table->buckets, 0, bucket_count * sizeof(EsIRVarVersion*));
    table->bucket_count = bucket_count;
    table->var_count = 0;
    
    return table;
}

void es_ir_var_table_destroy(EsIRVarTable* table) {
    
    (void)table;
}


static EsIRVarVersion* var_table_find(EsIRVarTable* table, const char* name) {
    if (!table || !name) return NULL;
    
    unsigned int hash = hash_string(name);
    int index = hash % table->bucket_count;
    
    EsIRVarVersion* var = table->buckets[index];
    while (var) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }
    
    return NULL;
}


static void var_table_add(EsIRVarTable* table, EsIRVarVersion* var) {
    if (!table || !var || !var->name) return;
    
    unsigned int hash = hash_string(var->name);
    int index = hash % table->bucket_count;
    
    var->next = table->buckets[index];
    table->buckets[index] = var;
    table->var_count++;
}

EsIRVarVersion* es_ir_var_get_version(EsIRVarTable* table, const char* name) {
    return var_table_find(table, name);
}

EsIRVarVersion* es_ir_var_new_version(EsIRVarTable* table, const char* name, EsIRType* type,
                                       EsIRBasicBlock* block, EsIRInst* def) {
    if (!table || !name) return NULL;
    
    
    EsIRVarVersion* existing = var_table_find(table, name);
    int new_version = 0;
    if (existing) {
        new_version = existing->version + 1;
    }
    
    
    
    EsIRVarVersion* var = (EsIRVarVersion*)ES_CALLOC(1, sizeof(EsIRVarVersion));
    if (!var) return NULL;
    
    var->name = ES_STRDUP(name);
    var->version = new_version;
    var->type = type;
    var->block = block;
    var->def = def;
    
    var_table_add(table, var);
    
    return var;
}

EsIRVarVersion* es_ir_var_find_version(EsIRVarTable* table, const char* name, EsIRBasicBlock* block) {
    
    
    (void)block;
    return var_table_find(table, name);
}

char* es_ir_var_versioned_name(EsIRMemoryArena* arena, const char* name, int version) {
    if (!arena || !name) return NULL;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s_%d", name, version);
    return es_ir_arena_strdup(arena, buffer);
}



EsIRPhi* es_ir_phi_create(EsIRMemoryArena* arena, const char* var_name, EsIRType* type,
                           int pred_count, EsIRBasicBlock* parent) {
    if (!arena || !var_name) return NULL;
    
    EsIRPhi* phi = (EsIRPhi*)es_ir_arena_alloc(arena, sizeof(EsIRPhi));
    if (!phi) return NULL;
    
    phi->var_name = es_ir_arena_strdup(arena, var_name);
    phi->type = type;
    phi->pred_count = pred_count;
    phi->parent_block = parent;
    phi->next = NULL;
    
    
    if (pred_count > 0) {
        phi->blocks = (EsIRBasicBlock**)es_ir_arena_alloc(arena, pred_count * sizeof(EsIRBasicBlock*));
        phi->versions = (EsIRVarVersion**)es_ir_arena_alloc(arena, pred_count * sizeof(EsIRVarVersion*));
        
        memset(phi->blocks, 0, pred_count * sizeof(EsIRBasicBlock*));
        memset(phi->versions, 0, pred_count * sizeof(EsIRVarVersion*));
    } else {
        phi->blocks = NULL;
        phi->versions = NULL;
    }
    
    return phi;
}

void es_ir_phi_add_operand(EsIRPhi* phi, EsIRBasicBlock* block, EsIRVarVersion* version, int index) {
    if (!phi || index < 0 || index >= phi->pred_count) return;
    
    phi->blocks[index] = block;
    phi->versions[index] = version;
}

EsIRPhi* es_ir_phi_find(EsIRBasicBlock* block, const char* var_name) {
    if (!block || !var_name) return NULL;
    
    
    
    (void)block;
    (void)var_name;
    return NULL;
}

void es_ir_block_add_phi(EsIRBasicBlock* block, EsIRPhi* phi) {
    if (!block || !phi) return;
    
    
    
    phi->next = NULL;  
    (void)block;
}



EsIRSSABuilder* es_ir_ssa_builder_create(EsIRBuilder* builder) {
    if (!builder) return NULL;
    
    EsIRSSABuilder* ssa = (EsIRSSABuilder*)ES_CALLOC(1, sizeof(EsIRSSABuilder));
    if (!ssa) return NULL;
    
    ssa->builder = builder;
    ssa->arena = builder->arena;
    ssa->var_table = es_ir_var_table_create(ssa->arena, 64);
    ssa->version_counters = NULL;
    ssa->var_capacity = 0;
    
    return ssa;
}

void es_ir_ssa_builder_destroy(EsIRSSABuilder* ssa_builder) {
    if (!ssa_builder) return;
    
    if (ssa_builder->var_table) {
        es_ir_var_table_destroy(ssa_builder->var_table);
    }
    
    ES_FREE(ssa_builder);
}


typedef struct {
    char** names;
    int count;
    int capacity;
} VarNameList;

static void add_var_name(VarNameList* list, const char* name) {
    if (!list || !name) return;
    
    
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->names[i], name) == 0) {
            return;
        }
    }
    
    
    if (list->count >= list->capacity) {
        list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
        char** new_names = (char**)ES_REALLOC(list->names, list->capacity * sizeof(char*));
        if (!new_names) return;
        list->names = new_names;
    }
    
    list->names[list->count++] = ES_STRDUP(name);
}

char** es_ir_ssa_collect_vars(EsIRMemoryArena* arena, EsIRFunction* func, int* count) {
    if (!func || !count) return NULL;
    
    VarNameList list = {0};
    
    
    EsIRBasicBlock* block = func->entry_block;
    while (block) {
        for (int i = 0; i < block->inst_count; i++) {
            EsIRInst* inst = block->insts[i];
            if (!inst) continue;
            
            
            if ((inst->opcode == ES_IR_LOAD || inst->opcode == ES_IR_STORE) &&
                inst->operand_count > 0) {
                EsIRValue* val = &inst->operands[0];
                if (val->type == ES_IR_VALUE_VAR && val->data.name) {
                    add_var_name(&list, val->data.name);
                }
            }
        }
        block = block->next;
    }
    
    *count = list.count;
    return list.names;
}


void es_ir_ssa_construct(EsIRSSABuilder* ssa_builder, EsIRFunction* func) {
    if (!ssa_builder || !func) return;
    
    
    es_ir_ssa_insert_phis(ssa_builder, func);
    
    
    es_ir_ssa_rename_vars(ssa_builder, func);
}

void es_ir_ssa_insert_phis(EsIRSSABuilder* ssa_builder, EsIRFunction* func) {
    if (!ssa_builder || !func) return;
    
    
    int var_count = 0;
    char** vars = es_ir_ssa_collect_vars(ssa_builder->arena, func, &var_count);
    
    
    for (int i = 0; i < var_count; i++) {
        es_ir_ssa_insert_phis_for_var(ssa_builder, func, vars[i]);
        ES_FREE(vars[i]);
    }
    
    ES_FREE(vars);
}

void es_ir_ssa_insert_phis_for_var(EsIRSSABuilder* ssa_builder, EsIRFunction* func, const char* var_name) {
    if (!ssa_builder || !func || !var_name) return;
    
    
    EsIRBasicBlock* block = func->entry_block;
    while (block) {
        if (block->pred_count > 1) {
            
            
            
            
        }
        block = block->next;
    }
}

void es_ir_ssa_rename_vars(EsIRSSABuilder* ssa_builder, EsIRFunction* func) {
    if (!ssa_builder || !func) return;
    
    
    EsIRBasicBlock* block = func->entry_block;
    while (block) {
        for (int i = 0; i < block->inst_count; i++) {
            EsIRInst* inst = block->insts[i];
            if (!inst) continue;
            
            
            if (inst->opcode == ES_IR_STORE && inst->operand_count > 0) {
                EsIRValue* val = &inst->operands[0];
                if (val->type == ES_IR_VALUE_VAR && val->data.name) {
                    
                    EsIRVarVersion* version = es_ir_var_new_version(
                        ssa_builder->var_table,
                        val->data.name,
                        NULL,  
                        block,
                        inst
                    );
                    (void)version;
                }
            }
        }
        block = block->next;
    }
}



void es_ir_ssa_compute_dominance_frontier(EsIRFunction* func) {
    
    (void)func;
}

void es_ir_ssa_compute_dominator_tree(EsIRFunction* func) {
    
    (void)func;
}

bool es_ir_ssa_dominates(EsIRBasicBlock* dom, EsIRBasicBlock* block) {
    
    if (!dom || !block) return false;
    return dom == block;
}

bool es_ir_ssa_verify(EsIRFunction* func) {
    if (!func) return false;
    
    
    
    
    EsIRBasicBlock* block = func->entry_block;
    while (block) {
        for (int i = 0; i < block->inst_count; i++) {
            EsIRInst* inst = block->insts[i];
            if (!inst) continue;
            
            
            
        }
        block = block->next;
    }
    
    return true;
}
