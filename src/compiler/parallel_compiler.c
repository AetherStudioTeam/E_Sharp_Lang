#include "parallel_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


extern int es_compile_file(const char* input_file, const char* output_file, int target_type, int show_ir);


static void collect_compile_error(CompileTask* task, const char* error_msg) {
    if (!task || !error_msg) return;


    if (task->error_message) {
        free(task->error_message);
    }


    task->error_message = strdup(error_msg);
}

static void compile_task_worker(void* arg) {
    CompileTask* task = (CompileTask*)arg;
    double start_time = es_time_now_seconds();


    fprintf(stdout, "[线程 %lu] 开始编译: %s\n", (unsigned long)pthread_self(), task->input_file);
    fflush(stdout);


    int result = es_compile_file(task->input_file, task->output_file, task->target_type, task->show_ir);
    task->duration = es_time_now_seconds() - start_time;


    if (result != 0) {

        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "编译失败: %s (返回码: %d, 耗时: %.2fs)",
                 task->input_file, result, task->duration);
        collect_compile_error(task, error_msg);
        task->result = -1;

        fprintf(stdout, "[线程 %lu] 编译失败: %s (耗时: %.2fs)\n",
               (unsigned long)pthread_self(), task->input_file, task->duration);
    } else {
        task->result = 0;
        fprintf(stdout, "[线程 %lu] 编译成功: %s (耗时: %.2fs)\n",
               (unsigned long)pthread_self(), task->input_file, task->duration);
    }
    fflush(stdout);
}

ParallelCompiler* parallel_compiler_create(int max_threads) {
    if (max_threads <= 0) {
        max_threads = 4;
    }

    ParallelCompiler* compiler = (ParallelCompiler*)malloc(sizeof(ParallelCompiler));
    if (!compiler) return NULL;

    compiler->thread_pool = thread_pool_create(max_threads);
    if (!compiler->thread_pool) {
        free(compiler);
        return NULL;
    }

    compiler->tasks = NULL;
    compiler->task_count = 0;
    compiler->max_threads = max_threads;
    compiler->any_failed = 0;
    memset(&compiler->stats, 0, sizeof(compiler->stats));

    if (pthread_mutex_init(&compiler->result_mutex, NULL) != 0) {
        thread_pool_destroy(compiler->thread_pool);
        free(compiler);
        return NULL;
    }

    return compiler;
}

void parallel_compiler_destroy(ParallelCompiler* compiler) {
    if (!compiler) return;


    for (int i = 0; i < compiler->task_count; i++) {
        if (compiler->tasks[i]) {
            free(compiler->tasks[i]->input_file);
            free(compiler->tasks[i]->output_file);
            free(compiler->tasks[i]->error_message);
            free(compiler->tasks[i]);
        }
    }
    free(compiler->tasks);

    pthread_mutex_destroy(&compiler->result_mutex);
    thread_pool_destroy(compiler->thread_pool);
    free(compiler);
}

int parallel_compiler_add_file(ParallelCompiler* compiler, const char* input_file, const char* output_file, int target_type, int show_ir) {
    if (!compiler || !input_file || !output_file) return -1;

    CompileTask* task = (CompileTask*)malloc(sizeof(CompileTask));
    if (!task) return -1;

    task->input_file = strdup(input_file);
    task->output_file = strdup(output_file);
    task->target_type = target_type;
    task->show_ir = show_ir;
    task->result = -1;
    task->error_message = NULL;
    task->duration = 0.0;


    compiler->tasks = (CompileTask**)realloc(compiler->tasks, (compiler->task_count + 1) * sizeof(CompileTask*));
    if (!compiler->tasks) {
        free(task->input_file);
        free(task->output_file);
        free(task);
        return -1;
    }

    compiler->tasks[compiler->task_count] = task;
    compiler->task_count++;

    return 0;
}

int parallel_compiler_execute(ParallelCompiler* compiler) {
    if (!compiler || compiler->task_count == 0) return 0;


    compiler->stats.total_files = compiler->task_count;
    compiler->stats.succeeded = 0;
    compiler->stats.failed = 0;
    compiler->any_failed = 0;

    fprintf(stdout, "\n=== 并行编译开始 ===\n");
    fprintf(stdout, "总文件数: %d, 线程数: %d\n", compiler->task_count, compiler->max_threads);
    fflush(stdout);

    double total_start_time = es_time_now_seconds();


    for (int i = 0; i < compiler->task_count; i++) {
        thread_pool_submit(compiler->thread_pool, compile_task_worker, compiler->tasks[i]);
    }


    thread_pool_wait(compiler->thread_pool);

    double total_duration = es_time_now_seconds() - total_start_time;


    pthread_mutex_lock(&compiler->result_mutex);
    for (int i = 0; i < compiler->task_count; i++) {
        CompileTask* task = compiler->tasks[i];
        if (task->result == 0) {
            compiler->stats.succeeded++;
        } else {
            compiler->stats.failed++;
            compiler->any_failed = 1;


            if (task->error_message) {
                fprintf(stderr, "错误: %s\n", task->error_message);
            }
        }
    }
    pthread_mutex_unlock(&compiler->result_mutex);

    fprintf(stdout, "\n=== 并行编译完成 ===\n");
    fprintf(stdout, "总耗时: %.2fs\n", total_duration);
    fprintf(stdout, "成功: %d, 失败: %d\n", compiler->stats.succeeded, compiler->stats.failed);
    if (compiler->stats.succeeded > 0 && total_duration > 0) {
        fprintf(stdout, "平均速度: %.2f 文件/秒\n", compiler->stats.succeeded / total_duration);
    }
    fflush(stdout);

    return compiler->any_failed ? -1 : 0;
}

int parallel_compiler_link_results(ParallelCompiler* compiler, const char* final_output) {
    if (!compiler || !final_output) return -1;

    if (compiler->any_failed) {
        fprintf(stdout, "跳过链接阶段：存在编译失败的文件\n");
        return -1;
    }

    fprintf(stdout, "\n=== 开始链接阶段 ===\n");
    fprintf(stdout, "目标文件: %s\n", final_output);
    fflush(stdout);


    char obj_files[8192] = "";
    int first_file = 1;
    int success_count = 0;

    for (int i = 0; i < compiler->task_count; i++) {
        CompileTask* task = compiler->tasks[i];
        if (task->result == 0) {

            if (!first_file) {
                strcat(obj_files, " ");
            }
            strcat(obj_files, task->output_file);
            first_file = 0;
            success_count++;
        }
    }

    if (strlen(obj_files) == 0) {
        fprintf(stdout, "没有成功的文件可以链接\n");
        return -1;
    }

    fprintf(stdout, "链接 %d 个对象文件\n", success_count);


    char link_cmd[16384];
    snprintf(link_cmd, sizeof(link_cmd),
             "gcc -o %s %s runtime.o output_cache.o -mconsole",
             final_output, obj_files);

    fprintf(stdout, "执行链接命令: %s\n", link_cmd);
    fflush(stdout);


    int link_result = system(link_cmd);
    if (link_result != 0) {
        fprintf(stderr, "链接失败: 命令返回 %d\n", link_result);
        return -1;
    }

    fprintf(stdout, "链接成功完成\n");
    fflush(stdout);

    return 0;
}

void parallel_compiler_get_stats(ParallelCompiler* compiler, int* total, int* succeeded, int* failed) {
    if (!compiler) return;

    if (total) *total = compiler->stats.total_files;
    if (succeeded) *succeeded = compiler->stats.succeeded;
    if (failed) *failed = compiler->stats.failed;
}