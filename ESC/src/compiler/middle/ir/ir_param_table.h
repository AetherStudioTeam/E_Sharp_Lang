#ifndef ES_IR_PARAM_TABLE_H
#define ES_IR_PARAM_TABLE_H

#include "ir_memory.h"
#include "../../../compiler/frontend/parser/ast.h"


typedef struct EsIRParamNode {
    char* name;                 
    EsTokenType type;           
    int index;                  
    struct EsIRParamNode* next; 
} EsIRParamNode;


typedef struct EsIRParamTable {
    EsIRParamNode** buckets;    
    int bucket_count;           
    int param_count;            
    EsIRMemoryArena* arena;     
} EsIRParamTable;


EsIRParamTable* es_ir_param_table_create(EsIRMemoryArena* arena, int initial_bucket_count);


void es_ir_param_table_destroy(EsIRParamTable* table);


bool es_ir_param_table_add(EsIRParamTable* table, const char* name, EsTokenType type, int index);


EsIRParamNode* es_ir_param_table_find(EsIRParamTable* table, const char* name);


int es_ir_param_table_count(EsIRParamTable* table);


void es_ir_param_table_foreach(EsIRParamTable* table, void (*callback)(EsIRParamNode* node, void* userdata), void* userdata);

#endif 