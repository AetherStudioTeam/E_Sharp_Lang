#include "ir.h"
#include "ir_param_table.h"


EsIRParamNode* es_ir_function_find_param(EsIRFunction* func, const char* name) {
    if (!func || !name) return NULL;
    
    
    if (func->param_table) {
        return es_ir_param_table_find(func->param_table, name);
    }
    
    
    for (int i = 0; i < func->param_count; i++) {
        if (func->params[i].name && strcmp(func->params[i].name, name) == 0) {
            
            static EsIRParamNode temp_node;
            temp_node.name = func->params[i].name;
            temp_node.type = func->params[i].type;
            temp_node.index = i;
            temp_node.next = NULL;
            return &temp_node;
        }
    }
    
    return NULL;
}


int es_ir_function_get_param_index(EsIRFunction* func, const char* name) {
    EsIRParamNode* param = es_ir_function_find_param(func, name);
    return param ? param->index : -1;
}