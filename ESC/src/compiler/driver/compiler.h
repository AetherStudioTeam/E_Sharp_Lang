#ifndef ES_COMPILER_H
#define ES_COMPILER_H

#include "../../core/utils/es_common.h"
#include <stdio.h>
#include "../frontend/parser/ast.h"
#include "../middle/ir/ir.h"


#define ES_DEBUG_IR(fmt, ...) ES_DEBUG_LOG("IR", fmt, ##__VA_ARGS__)

typedef enum {
    ES_TARGET_X86_ASM,
    ES_TARGET_WASM,
    ES_TARGET_IR_TEXT,
    ES_TARGET_VM_BYTECODE,
    ES_TARGET_EO_OBJ     
} EsTargetPlatform;

#include "../../shared/bytecode.h"

typedef struct {
    FILE* output_file;
    EsTargetPlatform target;
    EsChunk last_chunk;
    char output_filename[256]; 
} EsCompiler;


EsCompiler* es_compiler_create(const char* output_filename, EsTargetPlatform target);
void es_compiler_destroy(EsCompiler* compiler);


void es_compiler_compile(EsCompiler* compiler, ASTNode* ast, struct TypeCheckContext* type_context);

#endif