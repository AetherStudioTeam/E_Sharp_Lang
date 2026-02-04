#include "ir_object_pool.h"
#include <string.h>
#include <stdio.h>



static const size_t g_object_sizes[ES_POOL_COUNT] = {
    sizeof(EsIRInst),         
    sizeof(EsIRBasicBlock),   
    sizeof(EsIRValue),        
    sizeof(EsIRType),         
    sizeof(EsIRVarVersion),   
    sizeof(EsIRPhi),          
};

static const char* g_pool_names[ES_POOL_COUNT] = {
    "Instruction ",
    "BasicBlock  ",
    "Value       ",
    "Type        ",
    "VarVersion  ",
    "Phi         ",
};



static void pool_init(EsIRObjectPool* pool, const char* name, size_t object_size, size_t block_size) {
    pool->name = name;
    pool->object_size = object_size;
    pool->block_size = block_size;
    pool->free_list = NULL;
    pool->blocks = NULL;
    pool->alloc_count = 0;
    pool->free_count = 0;
    pool->hit_count = 0;
    pool->miss_count = 0;
}


static void pool_grow(EsIRObjectPool* pool, EsIRMemoryArena* arena) {
    
    size_t block_mem_size = pool->block_size * (sizeof(EsPoolNode) + pool->object_size);
    char* block_mem = (char*)es_ir_arena_alloc(arena, block_mem_size);
    if (!block_mem) return;
    
    
    for (size_t i = 0; i < pool->block_size; i++) {
        EsPoolNode* node = (EsPoolNode*)(block_mem + i * (sizeof(EsPoolNode) + pool->object_size));
        node->next = pool->free_list;
        pool->free_list = node;
    }
    
    
    EsPoolNode* block_header = (EsPoolNode*)es_ir_arena_alloc(arena, sizeof(EsPoolNode));
    if (block_header) {
        block_header->next = pool->blocks;
        pool->blocks = block_header;
    }
}



void es_ir_pool_manager_init(EsIRPoolManager* manager, EsIRMemoryArena* arena) {
    if (!manager) return;
    
    memset(manager, 0, sizeof(EsIRPoolManager));
    manager->arena = arena;
    
    
    for (int i = 0; i < ES_POOL_COUNT; i++) {
        pool_init(&manager->pools[i], g_pool_names[i], g_object_sizes[i], 64);
    }
}

void es_ir_pool_manager_destroy(EsIRPoolManager* manager) {
    if (!manager) return;
    
    
    
    for (int i = 0; i < ES_POOL_COUNT; i++) {
        manager->pools[i].free_list = NULL;
        manager->pools[i].blocks = NULL;
    }
}



void* es_ir_pool_alloc(EsIRPoolManager* manager, EsPoolObjectType type) {
    if (!manager || type < 0 || type >= ES_POOL_COUNT) return NULL;
    
    EsIRObjectPool* pool = &manager->pools[type];
    pool->alloc_count++;
    
    
    if (pool->free_list) {
        EsPoolNode* node = pool->free_list;
        pool->free_list = node->next;
        pool->hit_count++;
        
        
        memset(node->data, 0, pool->object_size);
        return node->data;
    }
    
    
    pool->miss_count++;
    pool_grow(pool, manager->arena);
    
    
    if (pool->free_list) {
        EsPoolNode* node = pool->free_list;
        pool->free_list = node->next;
        memset(node->data, 0, pool->object_size);
        return node->data;
    }
    
    return NULL;
}

EsIRInst* es_ir_pool_alloc_inst(EsIRPoolManager* manager) {
    return (EsIRInst*)es_ir_pool_alloc(manager, ES_POOL_INST);
}

EsIRBasicBlock* es_ir_pool_alloc_block(EsIRPoolManager* manager) {
    return (EsIRBasicBlock*)es_ir_pool_alloc(manager, ES_POOL_BLOCK);
}

EsIRType* es_ir_pool_alloc_type(EsIRPoolManager* manager) {
    return (EsIRType*)es_ir_pool_alloc(manager, ES_POOL_TYPE);
}

EsIRVarVersion* es_ir_pool_alloc_var_version(EsIRPoolManager* manager) {
    return (EsIRVarVersion*)es_ir_pool_alloc(manager, ES_POOL_VAR_VERSION);
}

EsIRPhi* es_ir_pool_alloc_phi(EsIRPoolManager* manager) {
    return (EsIRPhi*)es_ir_pool_alloc(manager, ES_POOL_PHI);
}



void es_ir_pool_free(EsIRPoolManager* manager, EsPoolObjectType type, void* obj) {
    if (!manager || !obj || type < 0 || type >= ES_POOL_COUNT) return;
    
    EsIRObjectPool* pool = &manager->pools[type];
    pool->free_count++;
    
    
    EsPoolNode* node = (EsPoolNode*)((char*)obj - sizeof(EsPoolNode));
    
    
    node->next = pool->free_list;
    pool->free_list = node;
}

void es_ir_pool_free_inst(EsIRPoolManager* manager, EsIRInst* inst) {
    es_ir_pool_free(manager, ES_POOL_INST, inst);
}

void es_ir_pool_free_block(EsIRPoolManager* manager, EsIRBasicBlock* block) {
    es_ir_pool_free(manager, ES_POOL_BLOCK, block);
}

void es_ir_pool_free_type(EsIRPoolManager* manager, EsIRType* type) {
    es_ir_pool_free(manager, ES_POOL_TYPE, type);
}

void es_ir_pool_free_var_version(EsIRPoolManager* manager, EsIRVarVersion* var) {
    es_ir_pool_free(manager, ES_POOL_VAR_VERSION, var);
}

void es_ir_pool_free_phi(EsIRPoolManager* manager, EsIRPhi* phi) {
    es_ir_pool_free(manager, ES_POOL_PHI, phi);
}



void es_ir_pool_clear(EsIRPoolManager* manager, EsPoolObjectType type) {
    if (!manager || type < 0 || type >= ES_POOL_COUNT) return;
    
    EsIRObjectPool* pool = &manager->pools[type];
    
    
    pool->free_list = NULL;
    
    
    
}

void es_ir_pool_clear_all(EsIRPoolManager* manager) {
    if (!manager) return;
    
    for (int i = 0; i < ES_POOL_COUNT; i++) {
        es_ir_pool_clear(manager, (EsPoolObjectType)i);
    }
}




static int count_pool_objects(EsIRObjectPool* pool) {
    int count = 0;
    EsPoolNode* node = pool->free_list;
    while (node) {
        count++;
        node = node->next;
    }
    return count;
}

void es_ir_pool_defrag(EsIRPoolManager* manager, EsPoolObjectType type) {
    if (!manager || type < 0 || type >= ES_POOL_COUNT) return;
    
    
    
    
    EsIRObjectPool* pool = &manager->pools[type];
    
    
    int free_count = count_pool_objects(pool);
    if (free_count < 2) return;  
    
    
    
    (void)free_count;
}

void es_ir_pool_defrag_all(EsIRPoolManager* manager) {
    if (!manager) return;
    
    for (int i = 0; i < ES_POOL_COUNT; i++) {
        es_ir_pool_defrag(manager, (EsPoolObjectType)i);
    }
}




int es_ir_pool_fragmentation_rate(EsIRPoolManager* manager, EsPoolObjectType type) {
    if (!manager || type < 0 || type >= ES_POOL_COUNT) return 0;
    
    EsIRObjectPool* pool = &manager->pools[type];
    
    int free_count = count_pool_objects(pool);
    int total_allocated = pool->alloc_count;
    
    if (total_allocated == 0) return 0;
    
    
    return (free_count * 100) / total_allocated;
}



int es_ir_pool_get_hit_rate(EsIRPoolManager* manager, EsPoolObjectType type) {
    if (!manager || type < 0 || type >= ES_POOL_COUNT) return 0;
    
    EsIRObjectPool* pool = &manager->pools[type];
    int total = pool->hit_count + pool->miss_count;
    
    if (total == 0) return 0;
    return (pool->hit_count * 100) / total;
}

void es_ir_pool_print_stats(EsIRPoolManager* manager) {
    if (!manager) return;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              Object Pool Statistics                      ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║ Type         Allocs   Frees   Hit%%   Frag%%   Hit/Miss   ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    
    int total_allocs = 0;
    int total_frees = 0;
    int total_hits = 0;
    int total_misses = 0;
    
    for (int i = 0; i < ES_POOL_COUNT; i++) {
        EsIRObjectPool* pool = &manager->pools[i];
        if (pool->alloc_count > 0) {
            int hit_rate = es_ir_pool_get_hit_rate(manager, (EsPoolObjectType)i);
            int frag_rate = es_ir_pool_fragmentation_rate(manager, (EsPoolObjectType)i);
            printf("║ %s  %6d  %6d   %3d%%   %3d%%  %5d/%5d  ║\n",
                   pool->name,
                   pool->alloc_count,
                   pool->free_count,
                   hit_rate,
                   frag_rate,
                   pool->hit_count,
                   pool->miss_count);
            
            total_allocs += pool->alloc_count;
            total_frees += pool->free_count;
            total_hits += pool->hit_count;
            total_misses += pool->miss_count;
        }
    }
    
    printf("╠══════════════════════════════════════════════════════════╣\n");
    int total_rate = (total_hits + total_misses > 0) 
        ? (total_hits * 100) / (total_hits + total_misses) 
        : 0;
    printf("║ TOTAL        %6d  %6d   %3d%%                      ║\n",
           total_allocs, total_frees, total_rate);
    printf("╚══════════════════════════════════════════════════════════╝\n");
}
