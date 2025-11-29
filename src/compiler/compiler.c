#include "compiler.h"
#include "ir.h"
#include "x86_backend.h"
#include "../common/es_common.h"
#include "../type_check/type_check.h"


EsCompiler* es_compiler_create(const char* output_filename, EsTargetPlatform target) {
    EsCompiler* compiler = (EsCompiler*)ES_MALLOC(sizeof(EsCompiler));
    if (!compiler) return NULL;

    ES_INFO("DEBUG: Opening output file: %s", output_filename);
    compiler->output_file = fopen(output_filename, "w");
    if (!compiler->output_file) {
        ES_ERROR("Failed to open output file: %s", output_filename);
        ES_FREE(compiler);
        return NULL;
    }
    ES_INFO("DEBUG: Successfully opened output file");

    compiler->target = target;

    return compiler;
}


void es_compiler_destroy(EsCompiler* compiler) {
    if (!compiler) return;

    if (compiler->output_file) {
        fclose(compiler->output_file);
    }
    ES_FREE(compiler);
}


void es_compiler_compile(EsCompiler* compiler, ASTNode* ast, TypeCheckContext* type_context) {
#ifdef DEBUG
    ES_COMPILER_DEBUG("=== COMPILER: es_compiler_compile called ===");
#endif

    if (!compiler || !ast) {
#ifdef DEBUG
        ES_COMPILER_DEBUG("compiler or ast is NULL");
#endif
        return;
    }

#ifdef DEBUG
    ES_COMPILER_DEBUG("=== COMPILER: Starting compilation ===");
#endif
#ifdef DEBUG
    ES_COMPILER_DEBUG("Starting compilation with target: %d", compiler->target);
#endif
#ifdef DEBUG
    ES_COMPILER_DEBUG("Output file: %p", compiler->output_file);
#endif

#ifdef DEBUG
    ES_COMPILER_DEBUG("=== COMPILER: Creating IR builder ===");
#endif
    EsIRBuilder* ir_builder = es_ir_builder_create();
    if (!ir_builder) {
#ifdef DEBUG
        ES_COMPILER_DEBUG("Failed to create IR builder");
#endif
        return;
    }
#ifdef DEBUG
    ES_COMPILER_DEBUG("=== COMPILER: IR builder created successfully ===");
#endif

#ifdef DEBUG
    ES_COMPILER_DEBUG("=== COMPILER: Starting IR generation ===");
#endif
    es_ir_generate_from_ast(ir_builder, ast, type_context);
#ifdef DEBUG
    ES_COMPILER_DEBUG("=== COMPILER: IR generation completed ===");
#endif

#ifdef DEBUG
    ES_COMPILER_DEBUG("Generating code for target: %d", compiler->target);
#endif
    switch (compiler->target) {
        case ES_TARGET_IR_TEXT:
#ifdef DEBUG
            ES_COMPILER_DEBUG("Generating IR text");
#endif
            es_ir_print(ir_builder->module, compiler->output_file);
            break;
        case ES_TARGET_X86_ASM:
#ifdef DEBUG
            ES_COMPILER_DEBUG("Generating x86 assembly");
#endif
            es_x86_generate(compiler->output_file, ir_builder->module);
#ifdef DEBUG
            ES_COMPILER_DEBUG("x86 assembly generation completed");
#endif
            break;
        case ES_TARGET_WASM:
            fprintf(compiler->output_file, "; WASM generation not implemented yet\n");
            break;
        default:
#ifdef DEBUG
            ES_COMPILER_DEBUG("Using default IR text generation");
#endif
            es_ir_print(ir_builder->module, compiler->output_file);
            break;
    }

#ifdef DEBUG
    ES_COMPILER_DEBUG("Code generation completed");
#endif

#ifdef DEBUG
    ES_COMPILER_DEBUG("About to destroy IR builder");
#endif
    es_ir_builder_destroy(ir_builder);
#ifdef DEBUG
    ES_COMPILER_DEBUG("IR builder destroyed successfully");
#endif

#ifdef DEBUG
    ES_COMPILER_DEBUG("=== COMPILER: Compilation completed successfully ===");
#endif



#ifdef DEBUG
    ES_COMPILER_DEBUG("=== COMPILER: About to return ===");
#endif
    return;
}