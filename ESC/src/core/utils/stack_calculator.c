#include "stack_calculator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "es_common.h"


static EsStackAnalyzer* g_stack_analyzer = NULL;


EsStackAnalyzer* es_stack_analyzer_init(void) {
    EsStackAnalyzer* analyzer = ES_MALLOC(sizeof(EsStackAnalyzer));
    if (!analyzer) {
        ES_ERROR("Failed to allocate stack analyzer");
        return NULL;
    }

    analyzer->frame_capacity = 16;
    analyzer->frames = ES_MALLOC(sizeof(EsStackFrame) * analyzer->frame_capacity);
    if (!analyzer->frames) {
        ES_ERROR("Failed to allocate stack frames");
        ES_FREE(analyzer);
        return NULL;
    }

    analyzer->frame_count = 0;
    analyzer->current_depth = 0;
    analyzer->max_depth = 0;

    
    return analyzer;
}


void es_stack_analyzer_destroy(EsStackAnalyzer* analyzer) {
    if (!analyzer) return;


    for (size_t i = 0; i < analyzer->frame_count; i++) {
        EsStackFrame* frame = &analyzer->frames[i];
        if (frame->usages) {
            ES_FREE(frame->usages);
        }
    }

    if (analyzer->frames) {
        ES_FREE(analyzer->frames);
    }

    ES_FREE(analyzer);
    
}


EsStackFrame* es_stack_analyzer_begin_function(EsStackAnalyzer* analyzer,
                                             const char* function_name) {
    if (!analyzer || !function_name) {
        ES_ERROR("Invalid parameters for stack analyzer begin function");
        return NULL;
    }


    if (analyzer->frame_count >= analyzer->frame_capacity) {
        size_t new_capacity = analyzer->frame_capacity * 2;
        EsStackFrame* new_frames = ES_REALLOC(analyzer->frames,
                                         sizeof(EsStackFrame) * new_capacity);
        if (!new_frames) {
            ES_ERROR("Failed to expand stack frames array");
            return NULL;
        }
        analyzer->frames = new_frames;
        analyzer->frame_capacity = new_capacity;
    }


    EsStackFrame* frame = &analyzer->frames[analyzer->frame_count++];
    frame->total_size = 0;
    frame->used_size = 0;
    frame->max_usage = 0;
    frame->usage_count = 0;
    frame->usage_capacity = 16;
    frame->usages = ES_MALLOC(sizeof(EsStackUsage) * frame->usage_capacity);

    if (!frame->usages) {
        ES_ERROR("Failed to allocate stack usage array");
        return NULL;
    }

    analyzer->current_depth++;
    if (analyzer->current_depth > analyzer->max_depth) {
        analyzer->max_depth = analyzer->current_depth;
    }
    return frame;
}


void es_stack_analyzer_end_function(EsStackAnalyzer* analyzer) {
    if (!analyzer || analyzer->current_depth == 0) {
        ES_ERROR("Invalid stack analyzer state for end function");
        return;
    }
    analyzer->current_depth--;
}


void es_stack_frame_add_usage(EsStackFrame* frame, size_t size,
                             EsStackUsageType type, const char* description,
                             const char* file, int line) {
    if (!frame || size == 0) {
        ES_ERROR("Invalid parameters for stack frame add usage");
        return;
    }


    if (frame->usage_count >= frame->usage_capacity) {
        size_t new_capacity = frame->usage_capacity * 2;
        EsStackUsage* new_usages = ES_REALLOC(frame->usages,
                                          sizeof(EsStackUsage) * new_capacity);
        if (!new_usages) {
            ES_ERROR("Failed to expand stack usage array");
            return;
        }
        frame->usages = new_usages;
        frame->usage_capacity = new_capacity;
    }


    EsStackUsage* usage = &frame->usages[frame->usage_count++];
    usage->offset = frame->used_size;
    usage->size = size;
    usage->type = type;
    usage->description = description ? description : "unknown";
    usage->file = file ? file : "unknown";
    usage->line = line;


    frame->used_size += size;
    if (frame->used_size > frame->max_usage) {
        frame->max_usage = frame->used_size;
    }

    
}


void es_stack_frame_optimize_layout(EsStackFrame* frame) {
    if (!frame) return;





    size_t aligned_size = frame->used_size;
    if (aligned_size % ES_STACK_ALIGNMENT != 0) {
        aligned_size = ((aligned_size + ES_STACK_ALIGNMENT - 1) / ES_STACK_ALIGNMENT) * ES_STACK_ALIGNMENT;
    }


    if (aligned_size < ES_MIN_STACK_SIZE) {
        aligned_size = ES_MIN_STACK_SIZE;
    }

    frame->total_size = aligned_size;

}


size_t es_stack_frame_get_total_size(const EsStackFrame* frame) {
    if (!frame) return ES_MIN_STACK_SIZE;


    if (frame->total_size == 0) {
        size_t aligned_size = frame->used_size;
        if (aligned_size % ES_STACK_ALIGNMENT != 0) {
            aligned_size = ((aligned_size + ES_STACK_ALIGNMENT - 1) / ES_STACK_ALIGNMENT) * ES_STACK_ALIGNMENT;
        }
        return aligned_size < ES_MIN_STACK_SIZE ? ES_MIN_STACK_SIZE : aligned_size;
    }

    return frame->total_size;
}


void es_stack_frame_generate_report(const EsStackFrame* frame) {
    if (!frame) {
        return;
    }

    
}


int es_stack_frame_check_overflow(const EsStackFrame* frame, size_t stack_limit) {
    if (!frame) return 0;

    size_t required_size = es_stack_frame_get_total_size(frame);

    if (required_size > stack_limit) {
        ES_WARNING("Stack overflow detected: required %zu bytes, limit %zu bytes. Consider reducing recursion depth or using heap allocation for large data structures.",
                   required_size, stack_limit);
        return 1;
    }


    double usage_ratio = (double)frame->max_usage / stack_limit;
    if (usage_ratio > 0.8) {
        ES_WARNING("High stack usage: %.1f%% of limit (%zu/%zu bytes). Consider optimizing local variable usage or using dynamic allocation.",
                   usage_ratio * 100, frame->max_usage, stack_limit);
    }

    return 0;
}


size_t es_calculate_dynamic_stack_size(const char* ir_code) {



    if (!ir_code) {
        return ES_MIN_STACK_SIZE;
    }


    size_t instruction_count = 0;
    const char* ptr = ir_code;
    while (*ptr) {
        if (*ptr == '\n') instruction_count++;
        ptr++;
    }


    size_t base_size = ES_MIN_STACK_SIZE;
    size_t dynamic_size = (instruction_count / 10) * 64;

    size_t total_size = base_size + dynamic_size;


    if (total_size % ES_STACK_ALIGNMENT != 0) {
        total_size = ((total_size + ES_STACK_ALIGNMENT - 1) / ES_STACK_ALIGNMENT) * ES_STACK_ALIGNMENT;
    }

    

    return total_size;
}


size_t es_predict_stack_usage(const char* function_signature,
                             size_t param_count, size_t local_var_count) {


    size_t base_size = 16;
    size_t param_size = param_count > 6 ? (param_count - 6) * 8 : 0;
    size_t local_size = local_var_count * 8;
    size_t register_size = 5 * 8;

    size_t total_size = base_size + param_size + local_size + register_size;


    if (total_size < ES_MIN_STACK_SIZE) {
        total_size = ES_MIN_STACK_SIZE;
    }


    if (total_size % ES_STACK_ALIGNMENT != 0) {
        total_size = ((total_size + ES_STACK_ALIGNMENT - 1) / ES_STACK_ALIGNMENT) * ES_STACK_ALIGNMENT;
    }

    return total_size;
}


EsStackAnalyzer* es_get_global_stack_analyzer(void) {
    if (!g_stack_analyzer) {
        g_stack_analyzer = es_stack_analyzer_init();
    }
    return g_stack_analyzer;
}

void es_cleanup_global_stack_analyzer(void) {
    if (g_stack_analyzer) {
        es_stack_analyzer_destroy(g_stack_analyzer);
        g_stack_analyzer = NULL;
    }
}