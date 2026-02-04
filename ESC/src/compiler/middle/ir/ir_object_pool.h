#ifndef ES_IR_OBJECT_POOL_H
#define ES_IR_OBJECT_POOL_H

#include "ir.h"
#include "ir_type.h"
#include "ir_ssa.h"



typedef enum {
    ES_POOL_INST,         
    ES_POOL_BLOCK,        
    ES_POOL_VALUE,        
    ES_POOL_TYPE,         
    ES_POOL_VAR_VERSION,  
    ES_POOL_PHI,          
    ES_POOL_COUNT
} EsPoolObjectType;



typedef struct EsPoolNode {
    struct EsPoolNode* next;
    char data[];  
} EsPoolNode;



typedef struct {
    const char* name;          
    size_t object_size;        
    size_t block_size;         
    
    EsPoolNode* free_list;     
    EsPoolNode* blocks;        
    
    int alloc_count;           
    int free_count;            
    int hit_count;             
    int miss_count;            
} EsIRObjectPool;



typedef struct {
    EsIRObjectPool pools[ES_POOL_COUNT];
    EsIRMemoryArena* arena;    
} EsIRPoolManager;




void es_ir_pool_manager_init(EsIRPoolManager* manager, EsIRMemoryArena* arena);
void es_ir_pool_manager_destroy(EsIRPoolManager* manager);


void* es_ir_pool_alloc(EsIRPoolManager* manager, EsPoolObjectType type);
EsIRInst* es_ir_pool_alloc_inst(EsIRPoolManager* manager);
EsIRBasicBlock* es_ir_pool_alloc_block(EsIRPoolManager* manager);
EsIRType* es_ir_pool_alloc_type(EsIRPoolManager* manager);
EsIRVarVersion* es_ir_pool_alloc_var_version(EsIRPoolManager* manager);
EsIRPhi* es_ir_pool_alloc_phi(EsIRPoolManager* manager);


void es_ir_pool_free(EsIRPoolManager* manager, EsPoolObjectType type, void* obj);
void es_ir_pool_free_inst(EsIRPoolManager* manager, EsIRInst* inst);
void es_ir_pool_free_block(EsIRPoolManager* manager, EsIRBasicBlock* block);
void es_ir_pool_free_type(EsIRPoolManager* manager, EsIRType* type);
void es_ir_pool_free_var_version(EsIRPoolManager* manager, EsIRVarVersion* var);
void es_ir_pool_free_phi(EsIRPoolManager* manager, EsIRPhi* phi);


void es_ir_pool_clear(EsIRPoolManager* manager, EsPoolObjectType type);
void es_ir_pool_clear_all(EsIRPoolManager* manager);


void es_ir_pool_defrag(EsIRPoolManager* manager, EsPoolObjectType type);
void es_ir_pool_defrag_all(EsIRPoolManager* manager);


int es_ir_pool_fragmentation_rate(EsIRPoolManager* manager, EsPoolObjectType type);


void es_ir_pool_print_stats(EsIRPoolManager* manager);
int es_ir_pool_get_hit_rate(EsIRPoolManager* manager, EsPoolObjectType type);

#endif 
