#ifndef ES_STACK_CALCULATOR_H
#define ES_STACK_CALCULATOR_H

#include <stddef.h>
#include <stdint.h>


#define ES_STACK_ALIGNMENT 16
#define ES_MIN_STACK_SIZE 128
#define ES_MAX_STACK_DEPTH 1000


typedef enum {
    ES_STACK_TYPE_LOCAL_VAR = 0,
    ES_STACK_TYPE_TEMP_VALUE,
    ES_STACK_TYPE_SAVED_REG,
    ES_STACK_TYPE_CALL_FRAME,
    ES_STACK_TYPE_ALIGNMENT_PAD
} EsStackUsageType;


typedef struct {
    size_t offset;
    size_t size;
    EsStackUsageType type;
    const char* description;
    const char* file;
    int line;
} EsStackUsage;


typedef struct {
    size_t total_size;
    size_t used_size;
    size_t max_usage;
    EsStackUsage* usages;
    size_t usage_count;
    size_t usage_capacity;
} EsStackFrame;


typedef struct {
    EsStackFrame* frames;
    size_t frame_count;
    size_t frame_capacity;
    size_t current_depth;
    size_t max_depth;
} EsStackAnalyzer;



EsStackAnalyzer* es_stack_analyzer_init(void);


void es_stack_analyzer_destroy(EsStackAnalyzer* analyzer);


EsStackFrame* es_stack_analyzer_begin_function(EsStackAnalyzer* analyzer,
                                             const char* function_name);


void es_stack_analyzer_end_function(EsStackAnalyzer* analyzer);


void es_stack_frame_add_usage(EsStackFrame* frame, size_t size,
                             EsStackUsageType type, const char* description,
                             const char* file, int line);


void es_stack_frame_optimize_layout(EsStackFrame* frame);


size_t es_stack_frame_get_total_size(const EsStackFrame* frame);


void es_stack_frame_generate_report(const EsStackFrame* frame);


int es_stack_frame_check_overflow(const EsStackFrame* frame, size_t stack_limit);


size_t es_calculate_dynamic_stack_size(const char* ir_code);


size_t es_predict_stack_usage(const char* function_signature,
                             size_t param_count, size_t local_var_count);


EsStackAnalyzer* es_get_global_stack_analyzer(void);


void es_cleanup_global_stack_analyzer(void);

#endif