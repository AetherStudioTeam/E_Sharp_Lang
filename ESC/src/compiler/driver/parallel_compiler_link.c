#include "parallel_compiler.h"
#include "arklink_integration.h"
#include "../../core/utils/es_common.h"
#include "../../core/utils/logger.h"
#include "../../core/utils/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef ES_USE_ARKLINK
#define ES_USE_ARKLINK 1
#endif

extern void find_runtime_obj(const char* obj_name, char* result, size_t size);

#if ES_USE_ARKLINK

int parallel_compiler_link_results(ParallelCompiler* compiler, const char* final_output) {
    if (!compiler || !final_output) return -1;

    if (compiler->any_failed) {
        return -1;
    }

    ES_INFO("Using ArkLink for linking...");
    
    EsArkLinkContext* ark_ctx = es_arklink_context_create(compiler->config);
    if (!ark_ctx) {
        ES_ERROR("Failed to create ArkLink context");
        return -1;
    }
    
    int success_count = 0;
    for (int i = 0; i < compiler->task_count; i++) {
        CompileTask* task = compiler->tasks[i];
        if (task->result == 0) {
            const char* file_to_link = task->obj_file ? task->obj_file : task->output_file;
            if (file_to_link) {
                if (es_arklink_add_object_file(ark_ctx, file_to_link) != 0) {
                    ES_WARNING("Failed to add object to ArkLink: %s", file_to_link);
                } else {
                    success_count++;
                }
            }
        }
    }

    if (success_count == 0) {
        ES_ERROR("No object files to link");
        es_arklink_context_destroy(ark_ctx);
        return -1;
    }
    
    char runtime_obj[ES_MAX_PATH];
    char cache_obj[ES_MAX_PATH];
    char allocator_obj[ES_MAX_PATH];
    char es_string_obj[ES_MAX_PATH];
    
    find_runtime_obj("runtime.o", runtime_obj, sizeof(runtime_obj));
    find_runtime_obj("output_cache.o", cache_obj, sizeof(cache_obj));
    find_runtime_obj("allocator.o", allocator_obj, sizeof(allocator_obj));
    find_runtime_obj("es_string.o", es_string_obj, sizeof(es_string_obj));
    
    es_arklink_add_object_file(ark_ctx, runtime_obj);
    es_arklink_add_object_file(ark_ctx, cache_obj);
    es_arklink_add_object_file(ark_ctx, allocator_obj);
    es_arklink_add_object_file(ark_ctx, es_string_obj);
    
    if (es_arklink_set_output(ark_ctx, final_output) != 0) {
        ES_ERROR("Failed to set output path");
        es_arklink_context_destroy(ark_ctx);
        return -1;
    }
    
    int link_result = es_arklink_link(ark_ctx);
    
    es_arklink_context_destroy(ark_ctx);
    
    if (link_result != 0) {
        ES_ERROR("ArkLink linking failed");
        return -1;
    }
    
    ES_INFO("ArkLink linking successful: %s", final_output);
    return 0;
}

#else

int parallel_compiler_link_results(ParallelCompiler* compiler, const char* final_output) {
    if (!compiler || !final_output) return -1;

    if (compiler->any_failed) {
        return -1;
    }

    size_t total_size = 1; 
    int success_count = 0;
    
    for (int i = 0; i < compiler->task_count; i++) {
        CompileTask* task = compiler->tasks[i];
        if (task->result == 0) {
            const char* file_to_link = task->obj_file ? task->obj_file : task->output_file;
            if (file_to_link) {
                if (success_count > 0) {
                    total_size += 1; 
                }
                total_size += strlen(file_to_link);
                success_count++;
            }
        }
    }

    if (success_count == 0) {
        return -1;
    }

    char* obj_files = (char*)ES_MALLOC(total_size);
    if (!obj_files) {
        ES_ERROR("内存分配失败");
        return -1;
    }
    obj_files[0] = '\0';

    int first_file = 1;
    for (int i = 0; i < compiler->task_count; i++) {
        CompileTask* task = compiler->tasks[i];
        if (task->result == 0) {
            const char* file_to_link = task->obj_file ? task->obj_file : task->output_file;
            if (file_to_link) {
                if (!first_file) {
                    ES_STRCAT_S(obj_files, total_size, " ");
                }
                ES_STRCAT_S(obj_files, total_size, file_to_link);
                first_file = 0;
            }
        }
    }

    char runtime_obj[ES_MAX_PATH];
    char cache_obj[ES_MAX_PATH];
    char allocator_obj[ES_MAX_PATH];
    char es_string_obj[ES_MAX_PATH];
    
    find_runtime_obj("runtime.o", runtime_obj, sizeof(runtime_obj));
    find_runtime_obj("output_cache.o", cache_obj, sizeof(cache_obj));
    find_runtime_obj("allocator.o", allocator_obj, sizeof(allocator_obj));
    find_runtime_obj("es_string.o", es_string_obj, sizeof(es_string_obj));

    char link_cmd[16384];
#ifdef _WIN32
    snprintf(link_cmd, sizeof(link_cmd),
             "gcc -o %s %s %s %s %s %s -lkernel32 -luser32 -lws2_32 -lgdi32 -lm",
             final_output, obj_files, runtime_obj, cache_obj, allocator_obj, es_string_obj);
#else
    snprintf(link_cmd, sizeof(link_cmd),
             "gcc -o %s %s %s %s %s %s -lpthread -lm",
             final_output, obj_files, runtime_obj, cache_obj, allocator_obj, es_string_obj);
#endif

    ES_INFO("正在链接: %s", link_cmd);
    int link_result = system(link_cmd);
    
    ES_FREE(obj_files);
    
    if (link_result != 0) {
        ES_ERROR("链接失败: 命令返回 %d", link_result);
        return -1;
    }

    ES_INFO("链接成功: %s", final_output);
    return 0;
}

#endif
