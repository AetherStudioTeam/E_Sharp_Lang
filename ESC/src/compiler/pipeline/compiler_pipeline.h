#ifndef ES_COMPILER_PIPELINE_H
#define ES_COMPILER_PIPELINE_H

#include "../../core/utils/es_common.h"
#include "../driver/config_manager.h"
#include "../platform/platform_abstraction.h"
#include "../frontend/lexer/tokenizer.h"
#include "../frontend/parser/parser.h"
#include "../frontend/semantic/semantic_analyzer.h"
#include "../driver/preprocessor.h"
#include "../driver/compiler.h"

typedef enum {
    ES_STAGE_READ_SOURCE,
    ES_STAGE_PREPROCESS,
    ES_STAGE_LEX,
    ES_STAGE_PARSE,
    ES_STAGE_SEMANTIC,
    ES_STAGE_TYPE_CHECK,
    ES_STAGE_CODEGEN,
    ES_STAGE_COMPLETE
} EsCompileStage;

typedef enum {
    ES_RESULT_SUCCESS,
    ES_RESULT_FAILED,
    ES_RESULT_UP_TO_DATE
} EsCompileResult;

typedef struct {
    EsCompileStage stage;
    EsCompileResult result;
    double duration;
    const char* file_name;
    char error_message[1024];
} EsCompileStageResult;

typedef struct {
    
    EsConfig* config;
    EsPlatform* platform;
    const char* input_file;
    const char* output_file;
    
    
    EsCompileStage current_stage;
    char* source_code;
    char* processed_source;
    Lexer* lexer;
    Parser* parser;
    ASTNode* ast;
    SemanticAnalyzer* semantic_analyzer;
    SemanticAnalysisResult* semantic_result;
    void* type_context;
    EsCompiler* compiler;
    
    
    EsCompileStageResult stage_results[8];
    int stage_count;
    double total_duration;
    
    
    int success;
    char error_message[1024];
} EsCompilePipeline;


EsCompilePipeline* es_compile_pipeline_create(EsConfig* config, EsPlatform* platform);
void es_compile_pipeline_destroy(EsCompilePipeline* pipeline);


int es_compile_pipeline_execute(EsCompilePipeline* pipeline, const char* input_file, const char* output_file);


int es_compile_pipeline_read_source(EsCompilePipeline* pipeline);
int es_compile_pipeline_preprocess(EsCompilePipeline* pipeline);
int es_compile_pipeline_lex(EsCompilePipeline* pipeline);
int es_compile_pipeline_parse(EsCompilePipeline* pipeline);
int es_compile_pipeline_semantic(EsCompilePipeline* pipeline);
int es_compile_pipeline_type_check(EsCompilePipeline* pipeline);
int es_compile_pipeline_codegen(EsCompilePipeline* pipeline);


int es_compile_pipeline_get_success(EsCompilePipeline* pipeline);
const char* es_compile_pipeline_get_error(EsCompilePipeline* pipeline);
EsCompileStageResult* es_compile_pipeline_get_stage_results(EsCompilePipeline* pipeline, int* count);
double es_compile_pipeline_get_total_duration(EsCompilePipeline* pipeline);



#endif 