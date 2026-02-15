#include "compiler.h"
#include "../middle/ir/ir.h"
#include "../middle/ir/ir_optimizer.h"
#include "../backend/x86/x86_codegen.h"
#include "../backend/vm/vm_codegen.h"
#include "../backend/eo/eo_codegen.h"
#include "../../core/utils/es_common.h"
#include "../../shared/bytecode_generator.h"
#include "../middle/codegen/type_checker.h"


EsCompiler* es_compiler_create(const char* output_filename, EsTargetPlatform target) {
    EsCompiler* compiler = (EsCompiler*)ES_MALLOC(sizeof(EsCompiler));
    if (!compiler) return NULL;

    ES_STRNCPY_SAFE(compiler->output_filename, output_filename);

    if (target == ES_TARGET_VM_BYTECODE || target == ES_TARGET_EO_OBJ) {
        compiler->output_file = fopen(output_filename, "wb");
    } else {
        compiler->output_file = fopen(output_filename, "w");
    }
    if (!compiler->output_file) {
        ES_ERROR("Failed to open output file: %s", output_filename);
        ES_FREE(compiler);
        return NULL;
    }

    compiler->target = target;
    es_bytecode_generator_init_chunk(&compiler->last_chunk);

    return compiler;
}


void es_compiler_destroy(EsCompiler* compiler) {
    if (!compiler) return;

    if (compiler->output_file) {
        fclose(compiler->output_file);
    }
    es_bytecode_generator_free_chunk(&compiler->last_chunk);
    ES_FREE(compiler);
}


void es_compiler_compile(EsCompiler* compiler, ASTNode* ast, TypeCheckContext* type_context) {
    if (!compiler || !ast) {
        return;
    }

    EsIRBuilder* ir_builder = es_ir_builder_create();
    if (!ir_builder) {
        return;
    }

    es_ir_generate_from_ast(ir_builder, ast, type_context);

    
    IROptimizer* optimizer = ir_optimizer_create();
    if (optimizer) {
        OptimizationFlags opt_flags = OPT_CONSTANT_FOLDING | OPT_DEAD_CODE_ELIMINATION |
                                     OPT_STRENGTH_REDUCTION | OPT_LOOP_INVARIANT_CODE_MOTION;
        ir_optimize_module(optimizer, ir_builder->module, opt_flags);
        ir_optimizer_destroy(optimizer);
    }

    switch (compiler->target) {
        case ES_TARGET_IR_TEXT:
            es_ir_print(ir_builder->module, compiler->output_file);
            break;
        case ES_TARGET_X86_ASM:
            es_x86_generate(compiler->output_file, ir_builder->module);
            break;
        case ES_TARGET_WASM:
            fprintf(compiler->output_file, "; WASAS生成尚未实现\n");
            break;
        case ES_TARGET_VM_BYTECODE: {
            es_vm_codegen_generate(ir_builder->module, &compiler->last_chunk);

            fclose(compiler->output_file);
            compiler->output_file = NULL;

            char output_filename[256];
            ES_STRNCPY(output_filename, compiler->output_filename, sizeof(output_filename));
            char* dot = strrchr(output_filename, '.');
            if (dot) {
                strcpy(dot, ".ebc");
            } else {
                strcat(output_filename, ".ebc");
            }

            es_bytecode_generator_serialize_to_file(&compiler->last_chunk, output_filename);
            break;
        }
        case ES_TARGET_EO_OBJ: {
            
            es_eo_generate(compiler->output_file, ir_builder->module);
            break;
        }
        default:
            es_ir_print(ir_builder->module, compiler->output_file);
            break;
    }


    es_ir_builder_destroy(ir_builder);




    return;
}