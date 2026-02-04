#ifndef ES_IR_SSA_H
#define ES_IR_SSA_H

#include "ir.h"
#include "ir_type.h"




typedef struct EsIRVarVersion {
    char* name;                    
    int version;                   
    EsIRType* type;                
    EsIRBasicBlock* block;         
    EsIRInst* def;                 
    struct EsIRVarVersion* next;   
} EsIRVarVersion;


typedef struct {
    EsIRVarVersion** buckets;      
    int bucket_count;              
    int var_count;                 
} EsIRVarTable;




typedef struct EsIRPhi {
    char* var_name;                
    int version;                   
    EsIRType* type;                
    
    
    EsIRBasicBlock** blocks;       
    EsIRVarVersion** versions;     
    int pred_count;                
    
    
    EsIRBasicBlock* parent_block;
    
    
    struct EsIRPhi* next;
} EsIRPhi;



typedef struct {
    EsIRBuilder* builder;          
    EsIRVarTable* var_table;       
    EsIRMemoryArena* arena;        
    
    
    int* version_counters;         
    int var_capacity;              
} EsIRSSABuilder;




EsIRVarTable* es_ir_var_table_create(EsIRMemoryArena* arena, int bucket_count);
void es_ir_var_table_destroy(EsIRVarTable* table);


EsIRVarVersion* es_ir_var_get_version(EsIRVarTable* table, const char* name);
EsIRVarVersion* es_ir_var_new_version(EsIRVarTable* table, const char* name, EsIRType* type, 
                                       EsIRBasicBlock* block, EsIRInst* def);


EsIRVarVersion* es_ir_var_find_version(EsIRVarTable* table, const char* name, EsIRBasicBlock* block);


char* es_ir_var_versioned_name(EsIRMemoryArena* arena, const char* name, int version);




EsIRPhi* es_ir_phi_create(EsIRMemoryArena* arena, const char* var_name, EsIRType* type, 
                           int pred_count, EsIRBasicBlock* parent);


void es_ir_phi_add_operand(EsIRPhi* phi, EsIRBasicBlock* block, EsIRVarVersion* version, int index);


EsIRPhi* es_ir_phi_find(EsIRBasicBlock* block, const char* var_name);


void es_ir_block_add_phi(EsIRBasicBlock* block, EsIRPhi* phi);




EsIRSSABuilder* es_ir_ssa_builder_create(EsIRBuilder* builder);
void es_ir_ssa_builder_destroy(EsIRSSABuilder* ssa_builder);


void es_ir_ssa_construct(EsIRSSABuilder* ssa_builder, EsIRFunction* func);


void es_ir_ssa_insert_phis(EsIRSSABuilder* ssa_builder, EsIRFunction* func);


void es_ir_ssa_rename_vars(EsIRSSABuilder* ssa_builder, EsIRFunction* func);




void es_ir_ssa_compute_dominance_frontier(EsIRFunction* func);


void es_ir_ssa_compute_dominator_tree(EsIRFunction* func);


bool es_ir_ssa_dominates(EsIRBasicBlock* dom, EsIRBasicBlock* block);


bool es_ir_ssa_verify(EsIRFunction* func);




void es_ir_ssa_insert_phis_for_var(EsIRSSABuilder* ssa_builder, EsIRFunction* func, const char* var_name);


char** es_ir_ssa_collect_vars(EsIRMemoryArena* arena, EsIRFunction* func, int* count);

#endif 
