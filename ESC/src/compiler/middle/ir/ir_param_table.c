#include "ir_param_table.h"
#include <string.h>


static size_t hash_string(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}


EsIRParamTable* es_ir_param_table_create(EsIRMemoryArena* arena, int initial_bucket_count) {
    if (!arena || initial_bucket_count <= 0) return NULL;
    
    
    int bucket_count = 1;
    while (bucket_count < initial_bucket_count) {
        bucket_count <<= 1;
    }
    
    EsIRParamTable* table = (EsIRParamTable*)es_ir_arena_alloc(arena, sizeof(EsIRParamTable));
    if (!table) return NULL;
    
    table->buckets = (EsIRParamNode**)es_ir_arena_alloc(arena, bucket_count * sizeof(EsIRParamNode*));
    if (!table->buckets) return NULL;
    
    
    memset(table->buckets, 0, bucket_count * sizeof(EsIRParamNode*));
    
    table->bucket_count = bucket_count;
    table->param_count = 0;
    table->arena = arena;
    
    return table;
}


bool es_ir_param_table_add(EsIRParamTable* table, const char* name, EsTokenType type, int index) {
    if (!table || !name) return false;
    
    
    if (es_ir_param_table_find(table, name)) {
        return false; 
    }
    
    size_t hash = hash_string(name);
    int bucket_index = hash & (table->bucket_count - 1);
    
    
    EsIRParamNode* node = (EsIRParamNode*)es_ir_arena_alloc(table->arena, sizeof(EsIRParamNode));
    if (!node) return false;
    
    node->name = es_ir_arena_strdup(table->arena, name);
    if (!node->name) return false;
    
    node->type = type;
    node->index = index;
    node->next = table->buckets[bucket_index];
    
    table->buckets[bucket_index] = node;
    table->param_count++;
    
    return true;
}


EsIRParamNode* es_ir_param_table_find(EsIRParamTable* table, const char* name) {
    if (!table || !name) return NULL;
    
    size_t hash = hash_string(name);
    int bucket_index = hash & (table->bucket_count - 1);
    
    EsIRParamNode* node = table->buckets[bucket_index];
    while (node) {
        if (strcmp(node->name, name) == 0) {
            return node;
        }
        node = node->next;
    }
    
    return NULL;
}


int es_ir_param_table_count(EsIRParamTable* table) {
    return table ? table->param_count : 0;
}


void es_ir_param_table_foreach(EsIRParamTable* table, void (*callback)(EsIRParamNode* node, void* userdata), void* userdata) {
    if (!table || !callback) return;
    
    for (int i = 0; i < table->bucket_count; i++) {
        EsIRParamNode* node = table->buckets[i];
        while (node) {
            callback(node, userdata);
            node = node->next;
        }
    }
}