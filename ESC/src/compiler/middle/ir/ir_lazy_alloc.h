#ifndef ES_IR_LAZY_ALLOC_H
#define ES_IR_LAZY_ALLOC_H

#include "ir.h"




typedef enum {
    ES_LAZY_OP_ALLOC,       
    ES_LAZY_OP_COPY,        
    ES_LAZY_OP_INIT,        
    ES_LAZY_OP_COUNT
} EsLazyOpType;


typedef struct EsLazyOp {
    EsLazyOpType type;
    void* target;           
    void* source;           
    size_t size;            
    int processed;          
    struct EsLazyOp* next;
} EsLazyOp;


typedef struct {
    EsLazyOp* pending;      
    EsLazyOp* free_list;    
    int pending_count;      
    int processed_count;    
    int coalesced_count;    
    
    
    int batch_size;         
    int enable_coalesce;    
} EsLazyAllocManager;




void es_ir_lazy_init(EsLazyAllocManager* manager);
void es_ir_lazy_destroy(EsLazyAllocManager* manager);


void es_ir_lazy_record_alloc(EsLazyAllocManager* manager, void** target, size_t size);
void es_ir_lazy_record_copy(EsLazyAllocManager* manager, void** target, void* source, size_t size);
void es_ir_lazy_record_init(EsLazyAllocManager* manager, void* target, size_t size);


void es_ir_lazy_flush(EsLazyAllocManager* manager);
void es_ir_lazy_flush_batch(EsLazyAllocManager* manager, int max_ops);


void es_ir_lazy_coalesce_allocs(EsLazyAllocManager* manager);


bool es_ir_lazy_has_pending(EsLazyAllocManager* manager);
int es_ir_lazy_pending_count(EsLazyAllocManager* manager);


void es_ir_lazy_print_stats(EsLazyAllocManager* manager);



















#endif 
