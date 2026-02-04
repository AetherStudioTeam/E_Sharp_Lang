#include <string.h>


extern void* memset(void* s, int c, size_t n);

#include "compiler_pipeline.h"
#include "../../core/utils/logger.h"
#include "../frontend/semantic/generics.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


struct TypeCheckContext;
typedef struct TypeCheckContext TypeCheckContext;
TypeCheckContext* type_check_context_create(void* semantic_analyzer);
void type_check_context_destroy(TypeCheckContext* context);
int type_check_program(TypeCheckContext* context, ASTNode* ast);


static void load_standard_macros(Preprocessor* preprocessor) {
    if (!preprocessor) return;
    
    preprocessor_add_macro(preprocessor, "println", "Console.WriteLine");
    preprocessor_add_macro(preprocessor, "println_int", "Console.WriteLineInt");
    preprocessor_add_macro(preprocessor, "print", "Console.Write");
    preprocessor_add_macro(preprocessor, "print_int", "Console.WriteInt");
}


static void record_stage_result(EsCompilePipeline* pipeline, EsCompileStage stage, 
                               EsCompileResult result, double duration, const char* file_name) {
    if (pipeline->stage_count >= 8) return;
    
    EsCompileStageResult* stage_result = &pipeline->stage_results[pipeline->stage_count++];
    stage_result->stage = stage;
    stage_result->result = result;
    stage_result->duration = duration;
    stage_result->file_name = file_name;
}

EsCompilePipeline* es_compile_pipeline_create(EsConfig* config, EsPlatform* platform) {
    EsCompilePipeline* pipeline = (EsCompilePipeline*)ES_MALLOC(sizeof(EsCompilePipeline));
    if (!pipeline) return NULL;
    
    memset(pipeline, 0, sizeof(EsCompilePipeline));
    pipeline->config = config;
    pipeline->platform = platform;
    pipeline->success = 1;
    
    return pipeline;
}

void es_compile_pipeline_destroy(EsCompilePipeline* pipeline) {
    if (!pipeline) return;
    
    
    if (pipeline->compiler) {
        es_compiler_destroy(pipeline->compiler);
    }
    if (pipeline->type_context) {
        type_check_context_destroy(pipeline->type_context);
    }
    if (pipeline->semantic_analyzer) {
        semantic_analyzer_destroy(pipeline->semantic_analyzer);
        pipeline->semantic_analyzer = NULL;
    }
    if (pipeline->semantic_result) {
        if (pipeline->semantic_result->error_messages) {
            ES_FREE(pipeline->semantic_result->error_messages);
        }
        ES_FREE(pipeline->semantic_result);
    }
    if (pipeline->ast) {
        ast_destroy(pipeline->ast);
    }
    if (pipeline->parser) {
        parser_destroy(pipeline->parser);
    }
    if (pipeline->lexer) {
        lexer_destroy(pipeline->lexer);
    }
    if (pipeline->processed_source) {
        ES_FREE(pipeline->processed_source);
    }
    if (pipeline->source_code) {
        ES_FREE(pipeline->source_code);
    }
    
    ES_FREE(pipeline);
}

int es_compile_pipeline_read_source(EsCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->input_file) return 0;
    
    clock_t start = clock();
    
    FILE* fp = fopen(pipeline->input_file, "r");
    if (!fp) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "无法打开源文件: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_READ_SOURCE, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "无法读取源文件大小: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_READ_SOURCE, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    long file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "无法获取源文件大小: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_READ_SOURCE, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    rewind(fp);
    pipeline->source_code = (char*)ES_MALLOC((size_t)file_size + 1);
    if (!pipeline->source_code) {
        fclose(fp);
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "内存分配失败，无法读取源文件: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_READ_SOURCE, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    size_t read_bytes = fread(pipeline->source_code, 1, (size_t)file_size, fp);
    fclose(fp);
    pipeline->source_code[read_bytes] = '\0';
    
    record_stage_result(pipeline, ES_STAGE_READ_SOURCE, ES_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int es_compile_pipeline_preprocess(EsCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->source_code) return 0;
    
    clock_t start = clock();
    
    Preprocessor* preprocessor = preprocessor_create();
    if (!preprocessor) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "预处理器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_PREPROCESS, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    load_standard_macros(preprocessor);
    pipeline->processed_source = preprocessor_process(preprocessor, pipeline->source_code);
    preprocessor_destroy(preprocessor);
    
    if (!pipeline->processed_source) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "预处理失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_PREPROCESS, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    record_stage_result(pipeline, ES_STAGE_PREPROCESS, ES_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int es_compile_pipeline_lex(EsCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->processed_source) return 0;
    
    clock_t start = clock();
    
    pipeline->lexer = lexer_create(pipeline->processed_source);
    if (!pipeline->lexer) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "词法分析器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_LEX, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    record_stage_result(pipeline, ES_STAGE_LEX, ES_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int es_compile_pipeline_parse(EsCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->lexer) return 0;
    
    clock_t start = clock();
    
    pipeline->parser = parser_create(pipeline->lexer);
    if (!pipeline->parser) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语法分析器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_PARSE, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    pipeline->ast = parser_parse(pipeline->parser);
    
    if (!pipeline->ast) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语法分析失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_PARSE, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    record_stage_result(pipeline, ES_STAGE_PARSE, ES_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int es_compile_pipeline_semantic(EsCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->ast) return 0;
    
    clock_t start = clock();
    
    pipeline->semantic_analyzer = semantic_analyzer_create();
    if (!pipeline->semantic_analyzer) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语义分析器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_SEMANTIC, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    pipeline->semantic_result = semantic_analyzer_analyze(pipeline->semantic_analyzer, pipeline->ast);
    if (!pipeline->semantic_result || !pipeline->semantic_result->success) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语义分析失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_SEMANTIC, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    record_stage_result(pipeline, ES_STAGE_SEMANTIC, ES_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int es_compile_pipeline_type_check(EsCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->ast) {
        return 0;
    }

    clock_t start = clock();

    pipeline->type_context = type_check_context_create(pipeline->semantic_analyzer);
    if (!pipeline->type_context) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message),
                "类型检查上下文创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_TYPE_CHECK, ES_RESULT_FAILED,
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }

    if (type_check_program(pipeline->type_context, pipeline->ast) == 0) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message),
                "类型检查失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_TYPE_CHECK, ES_RESULT_FAILED,
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }

    record_stage_result(pipeline, ES_STAGE_TYPE_CHECK, ES_RESULT_SUCCESS,
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}



int es_compile_pipeline_codegen(EsCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->ast || !pipeline->type_context) return 0;
    
    clock_t start = clock();
    
    EsTargetPlatform target = ES_TARGET_X86_ASM;
    if (pipeline->config->target_type == ES_TARGET_IR) {
        target = ES_TARGET_IR_TEXT;
    } else if (pipeline->config->target_type == ES_TARGET_VM) {
        target = ES_TARGET_VM_BYTECODE;
    } else if (pipeline->config->target_type == ES_TARGET_EO) {
        target = ES_TARGET_EO_OBJ;
    }
    
    pipeline->compiler = es_compiler_create(pipeline->output_file, target);
    if (!pipeline->compiler) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "编译器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_CODEGEN, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    
    if (!pipeline->semantic_result || !pipeline->semantic_result->success) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语义分析未完成或失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, ES_STAGE_CODEGEN, ES_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    es_compiler_compile(pipeline->compiler, pipeline->ast, pipeline->type_context);
    
    record_stage_result(pipeline, ES_STAGE_CODEGEN, ES_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int es_compile_pipeline_execute(EsCompilePipeline* pipeline, const char* input_file, const char* output_file) {
    if (!pipeline || !input_file || !output_file) return 0;
    
    clock_t total_start = clock();
    
    pipeline->input_file = input_file;
    pipeline->output_file = output_file;
    
    
    if (!es_compile_pipeline_read_source(pipeline)) {
        return 0;
    }
    
    if (!es_compile_pipeline_preprocess(pipeline)) {
        return 0;
    }
    
    if (!es_compile_pipeline_lex(pipeline)) {
        return 0;
    }
    
    if (!es_compile_pipeline_parse(pipeline)) {
        return 0;
    }
    
    if (!es_compile_pipeline_semantic(pipeline)) {
        return 0;
    }
    
    if (!es_compile_pipeline_type_check(pipeline)) {
        return 0;
    }
    
    if (!es_compile_pipeline_codegen(pipeline)) {
        return 0;
    }
    
    pipeline->total_duration = (double)(clock() - total_start) / CLOCKS_PER_SEC;
    record_stage_result(pipeline, ES_STAGE_COMPLETE, ES_RESULT_SUCCESS, pipeline->total_duration, input_file);
    
    return 1;
}

int es_compile_pipeline_get_success(EsCompilePipeline* pipeline) {
    return pipeline ? pipeline->success : 0;
}

const char* es_compile_pipeline_get_error(EsCompilePipeline* pipeline) {
    return pipeline ? pipeline->error_message : "未知错误";
}

EsCompileStageResult* es_compile_pipeline_get_stage_results(EsCompilePipeline* pipeline, int* count) {
    if (!pipeline || !count) return NULL;
    *count = pipeline->stage_count;
    return pipeline->stage_results;
}

double es_compile_pipeline_get_total_duration(EsCompilePipeline* pipeline) {
    return pipeline ? pipeline->total_duration : 0.0;
}