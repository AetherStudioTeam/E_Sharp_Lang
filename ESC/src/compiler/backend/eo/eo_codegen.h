#ifndef EO_CODEGEN_H
#define EO_CODEGEN_H

#include "../../middle/ir/ir.h"
#include "../../../tools/eo_writer.h"
#include <stdio.h>




typedef struct {
    EOWriter* writer;
    FILE* output_file;
    int temp_var_count;
    int label_count;
    int32_t* string_const_sym_indices; 
} EOCodegenContext;


void es_eo_generate(FILE* output_file, EsIRModule* module);


static void eo_generate_function(EOCodegenContext* ctx, EsIRFunction* func);


static void eo_generate_block(EOCodegenContext* ctx, EsIRBasicBlock* block);


static void eo_generate_instruction(EOCodegenContext* ctx, EsIRInst* inst);

#endif 
