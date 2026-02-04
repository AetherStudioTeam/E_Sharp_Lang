#include "ir_lazy_alloc.h"
#include <string.h>
#include <stdio.h>


static EsLazyOp* create_op(EsLazyAllocManager* manager) {
    if (!manager) return NULL;
    
    
    if (manager->free_list) {
        EsLazyOp* op = manager->free_list;
        manager->free_list = op->next;
        memset(op, 0, sizeof(EsLazyOp));
        return op;
    }
    
    
    return (EsLazyOp*)ES_CALLOC(1, sizeof(EsLazyOp));
}


static void recycle_op(EsLazyAllocManager* manager, EsLazyOp* op) {
    if (!manager || !op) return;
    
    op->next = manager->free_list;
    manager->free_list = op;
}

void es_ir_lazy_init(EsLazyAllocManager* manager) {
    if (!manager) return;
    
    memset(manager, 0, sizeof(EsLazyAllocManager));
    manager->batch_size = 32;      
    manager->enable_coalesce = 1;  
}

void es_ir_lazy_destroy(EsLazyAllocManager* manager) {
    if (!manager) return;
    
    
    es_ir_lazy_flush(manager);
    
    
    EsLazyOp* op = manager->pending;
    while (op) {
        EsLazyOp* next = op->next;
        ES_FREE(op);
        op = next;
    }
    
    op = manager->free_list;
    while (op) {
        EsLazyOp* next = op->next;
        ES_FREE(op);
        op = next;
    }
    
    memset(manager, 0, sizeof(EsLazyAllocManager));
}

void es_ir_lazy_record_alloc(EsLazyAllocManager* manager, void** target, size_t size) {
    if (!manager || !target) return;
    
    EsLazyOp* op = create_op(manager);
    if (!op) return;
    
    op->type = ES_LAZY_OP_ALLOC;
    op->target = target;
    op->size = size;
    op->processed = 0;
    
    
    op->next = manager->pending;
    manager->pending = op;
    manager->pending_count++;
}

void es_ir_lazy_record_copy(EsLazyAllocManager* manager, void** target, void* source, size_t size) {
    if (!manager || !target) return;
    
    EsLazyOp* op = create_op(manager);
    if (!op) return;
    
    op->type = ES_LAZY_OP_COPY;
    op->target = target;
    op->source = source;
    op->size = size;
    op->processed = 0;
    
    op->next = manager->pending;
    manager->pending = op;
    manager->pending_count++;
}

void es_ir_lazy_record_init(EsLazyAllocManager* manager, void* target, size_t size) {
    if (!manager || !target) return;
    
    EsLazyOp* op = create_op(manager);
    if (!op) return;
    
    op->type = ES_LAZY_OP_INIT;
    op->target = target;
    op->size = size;
    op->processed = 0;
    
    op->next = manager->pending;
    manager->pending = op;
    manager->pending_count++;
}


static void execute_op(EsLazyOp* op) {
    if (!op || op->processed) return;
    
    switch (op->type) {
        case ES_LAZY_OP_ALLOC: {
            void** target = (void**)op->target;
            if (target && *target == NULL) {
                *target = ES_MALLOC(op->size);
                if (*target) {
                    memset(*target, 0, op->size);
                }
            }
            break;
        }
        
        case ES_LAZY_OP_COPY: {
            void** target = (void**)op->target;
            if (target && op->source && op->size > 0) {
                if (*target == NULL) {
                    *target = ES_MALLOC(op->size);
                }
                if (*target) {
                    memcpy(*target, op->source, op->size);
                }
            }
            break;
        }
        
        case ES_LAZY_OP_INIT: {
            void* target = op->target;
            if (target && op->size > 0) {
                memset(target, 0, op->size);
            }
            break;
        }
        
        default:
            break;
    }
    
    op->processed = 1;
}

void es_ir_lazy_flush(EsLazyAllocManager* manager) {
    if (!manager) return;
    
    
    if (manager->enable_coalesce) {
        es_ir_lazy_coalesce_allocs(manager);
    }
    
    
    EsLazyOp* op = manager->pending;
    EsLazyOp* processed_tail = NULL;
    
    while (op) {
        EsLazyOp* next = op->next;
        
        if (!op->processed) {
            execute_op(op);
            manager->processed_count++;
        }
        
        
        if (op->processed) {
            if (processed_tail) {
                processed_tail->next = op;
            }
            processed_tail = op;
            op->next = NULL;
        }
        
        op = next;
    }
    
    
    op = processed_tail;
    while (op) {
        EsLazyOp* next = op->next;
        recycle_op(manager, op);
        manager->pending_count--;
        op = next;
    }
    
    manager->pending = NULL;
}

void es_ir_lazy_flush_batch(EsLazyAllocManager* manager, int max_ops) {
    if (!manager || max_ops <= 0) return;
    
    int count = 0;
    EsLazyOp* op = manager->pending;
    
    while (op && count < max_ops) {
        if (!op->processed) {
            execute_op(op);
            manager->processed_count++;
            count++;
        }
        op = op->next;
    }
}

void es_ir_lazy_coalesce_allocs(EsLazyAllocManager* manager) {
    if (!manager || !manager->enable_coalesce) return;
    
    
    
    
    
    size_t size_histogram[8] = {0};  
    
    EsLazyOp* op = manager->pending;
    while (op) {
        if (op->type == ES_LAZY_OP_ALLOC && !op->processed) {
            
            int bucket = 0;
            size_t size = op->size;
            while (size > 8 && bucket < 7) {
                size >>= 1;
                bucket++;
            }
            size_histogram[bucket]++;
        }
        op = op->next;
    }
    
    
    int coalesced = 0;
    for (int i = 0; i < 8; i++) {
        if (size_histogram[i] > 4) {
            
            coalesced++;
        }
    }
    
    manager->coalesced_count += coalesced;
}

bool es_ir_lazy_has_pending(EsLazyAllocManager* manager) {
    if (!manager) return false;
    
    EsLazyOp* op = manager->pending;
    while (op) {
        if (!op->processed) return true;
        op = op->next;
    }
    return false;
}

int es_ir_lazy_pending_count(EsLazyAllocManager* manager) {
    if (!manager) return 0;
    return manager->pending_count;
}

void es_ir_lazy_print_stats(EsLazyAllocManager* manager) {
    if (!manager) return;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║         Lazy Allocation Statistics               ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Pending Operations:     %10d               ║\n", manager->pending_count);
    printf("║ Processed Operations:   %10d               ║\n", manager->processed_count);
    printf("║ Coalesced Groups:       %10d               ║\n", manager->coalesced_count);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Batch Size:             %10d               ║\n", manager->batch_size);
    printf("║ Coalesce Enabled:       %10s               ║\n", 
           manager->enable_coalesce ? "Yes" : "No");
    printf("╚══════════════════════════════════════════════════╝\n");
}
