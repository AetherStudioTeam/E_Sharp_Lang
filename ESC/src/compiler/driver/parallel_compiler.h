#ifndef ES_PARALLEL_COMPILER_H
#define ES_PARALLEL_COMPILER_H

#include "../../core/utils/es_common.h"
#include "project.h"
#include "../../core/platform/thread_pool.h"
#include "../frontend/semantic/generics.h"
#include "config_manager.h"
#include <pthread.h>

typedef struct CompileTask {
    char* input_file;
    char* output_file;
    char* obj_file; 
    int target_type;
    int show_ir;
    int result;
    char* error_message;
    double duration;
    void* compiler_context;  
} CompileTask;

typedef struct ParallelCompiler {
    ThreadPool* thread_pool;
    CompileTask** tasks;
    int task_count;
    int max_threads;
    pthread_mutex_t result_mutex;
    pthread_mutex_t registry_mutex;  
    int any_failed;
    EsConfig* config; 
    struct {
        int total_files;
        int succeeded;
        int failed;
        double total_time;
    } stats;
    GenericRegistry* shared_generic_registry;  
} ParallelCompiler;

ParallelCompiler* parallel_compiler_create(int max_threads, EsConfig* config);
void parallel_compiler_destroy(ParallelCompiler* compiler);
int parallel_compiler_add_file(ParallelCompiler* compiler, const char* input_file, const char* output_file, const char* obj_file, int target_type, int show_ir);
int parallel_compiler_execute(ParallelCompiler* compiler);

void parallel_compiler_get_stats(ParallelCompiler* compiler, int* total, int* succeeded, int* failed);


int parallel_compiler_link_results(ParallelCompiler* compiler, const char* final_output);


int parallel_compiler_collect_generic_types(ParallelCompiler* compiler);

void find_runtime_obj(const char* obj_name, char* result, size_t size);

#endif