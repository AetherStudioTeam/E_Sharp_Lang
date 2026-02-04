#include "parallel_compiler.h"
#include "../../core/utils/es_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../core/utils/es_string.h"
#include "../../core/utils/path.h"


extern void *memset(void *s, int c, size_t n);
extern size_t strlen(const char *s);
extern char *strstr(const char *haystack, const char *needle);
extern char *strncpy(char *dest, const char *src, size_t n);

#include "../pipeline/compiler_pipeline.h"


static void collect_compile_error(CompileTask* task, const char* error_msg) {
    if (!task || !error_msg) return;


    if (task->error_message) {
        ES_FREE(task->error_message);
    }


    task->error_message = ES_STRDUP(error_msg);
}

static void compile_task_worker(void* arg) {
    CompileTask* task = (CompileTask*)arg;
    double start_time = es_time_now_seconds();

    
    ParallelCompiler* compiler = (ParallelCompiler*)task->compiler_context;
    int result;
    
    
    EsCompilePipeline* pipeline = es_compile_pipeline_create(compiler->config, NULL);
    if (!pipeline) {
        result = -1;
    } else {
        
        result = es_compile_pipeline_execute(pipeline, task->input_file, task->output_file);
        
        
        es_compile_pipeline_destroy(pipeline);
        
        
        if (result == 1 && task->obj_file) {
            char assemble_cmd[1024];
            const char* format = NULL;
            EsPlatformType platform = compiler->config ? compiler->config->platform : ES_CONFIG_PLATFORM_WINDOWS;
            
            if (platform == ES_CONFIG_PLATFORM_WINDOWS) {
                format = "nasm -f win64 %s -o %s";
            } else {
                format = "nasm -f elf64 %s -o %s";
            }
            
            snprintf(assemble_cmd, sizeof(assemble_cmd), format, task->output_file, task->obj_file);
            int assemble_res = system(assemble_cmd);
            if (assemble_res != 0) {
                result = -1;
                char error_msg[512];
                snprintf(error_msg, sizeof(error_msg), "汇编失败: %s -> %s", task->output_file, task->obj_file);
                collect_compile_error(task, error_msg);
            } else {
                
                
            }
        }
        
    }
    
    task->duration = es_time_now_seconds() - start_time;


    if (result != 1) { 
        if (!task->error_message) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "编译失败: %s (耗时: %.2fs)",
                     task->input_file, task->duration);
            collect_compile_error(task, error_msg);
        }
        task->result = -1;
    } else {
        task->result = 0;
    }
}

ParallelCompiler* parallel_compiler_create(int max_threads, EsConfig* config) {
    if (max_threads <= 0) {
        max_threads = 4;
    }

    ParallelCompiler* compiler = (ParallelCompiler*)ES_MALLOC(sizeof(ParallelCompiler));
    if (!compiler) return NULL;

    compiler->thread_pool = thread_pool_create(max_threads);
    if (!compiler->thread_pool) {
        ES_FREE(compiler);
        return NULL;
    }

    compiler->tasks = NULL;
    compiler->task_count = 0;
    compiler->max_threads = max_threads;
    compiler->any_failed = 0;
    compiler->config = config;
    memset(&compiler->stats, 0, sizeof(compiler->stats));

    
    compiler->shared_generic_registry = generics_create_registry();
    if (!compiler->shared_generic_registry) {
        thread_pool_destroy(compiler->thread_pool);
        ES_FREE(compiler);
        return NULL;
    }

    if (pthread_mutex_init(&compiler->result_mutex, NULL) != 0) {
        thread_pool_destroy(compiler->thread_pool);
        generics_destroy_registry(compiler->shared_generic_registry);
        ES_FREE(compiler);
        return NULL;
    }

    if (pthread_mutex_init(&compiler->registry_mutex, NULL) != 0) {
        pthread_mutex_destroy(&compiler->result_mutex);
        thread_pool_destroy(compiler->thread_pool);
        generics_destroy_registry(compiler->shared_generic_registry);
        ES_FREE(compiler);
        return NULL;
    }

    return compiler;
}

void parallel_compiler_destroy(ParallelCompiler* compiler) {
    if (!compiler) return;


    for (int i = 0; i < compiler->task_count; i++) {
        if (compiler->tasks[i]) {
            ES_FREE(compiler->tasks[i]->input_file);
            ES_FREE(compiler->tasks[i]->output_file);
            if (compiler->tasks[i]->obj_file) ES_FREE(compiler->tasks[i]->obj_file);
            ES_FREE(compiler->tasks[i]->error_message);
            ES_FREE(compiler->tasks[i]);
        }
    }
    ES_FREE(compiler->tasks);

    pthread_mutex_destroy(&compiler->result_mutex);
    pthread_mutex_destroy(&compiler->registry_mutex);
    thread_pool_destroy(compiler->thread_pool);
    
    
    if (compiler->shared_generic_registry) {
        generics_destroy_registry(compiler->shared_generic_registry);
    }
    
    ES_FREE(compiler);
}

int parallel_compiler_add_file(ParallelCompiler* compiler, const char* input_file, const char* output_file, const char* obj_file, int target_type, int show_ir) {
    if (!compiler || !input_file || !output_file) return -1;

    CompileTask* task = (CompileTask*)ES_MALLOC(sizeof(CompileTask));
    if (!task) return -1;

    task->input_file = ES_STRDUP(input_file);
    task->output_file = ES_STRDUP(output_file);
    task->obj_file = obj_file ? ES_STRDUP(obj_file) : NULL;
    task->target_type = target_type;
    task->show_ir = show_ir;
    task->result = -1;
    task->error_message = NULL;
    task->duration = 0.0;
    task->compiler_context = compiler;  


    compiler->tasks = (CompileTask**)ES_REALLOC(compiler->tasks, (compiler->task_count + 1) * sizeof(CompileTask*));
    if (!compiler->tasks) {
        ES_FREE(task->input_file);
        ES_FREE(task->output_file);
        ES_FREE(task);
        return -1;
    }

    compiler->tasks[compiler->task_count] = task;
    compiler->task_count++;

    return 0;
}

int parallel_compiler_execute(ParallelCompiler* compiler) {
    if (!compiler || compiler->task_count == 0) return 0;

    ES_INFO("开始并行编译，任务数量: %d", compiler->task_count);

    compiler->stats.total_files = compiler->task_count;
    compiler->stats.succeeded = 0;
    compiler->stats.failed = 0;
    compiler->any_failed = 0;

    double total_start_time = es_time_now_seconds();

    for (int i = 0; i < compiler->task_count; i++) {
        thread_pool_submit(compiler->thread_pool, compile_task_worker, compiler->tasks[i]);
    }

    ES_INFO("等待所有任务完成...");
    thread_pool_wait(compiler->thread_pool);
    ES_INFO("所有任务已完成。");

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
                ES_ERROR("错误: %s", task->error_message);
            }
        }
    }
    pthread_mutex_unlock(&compiler->result_mutex);

    return compiler->any_failed ? -1 : 0;
}

static void find_runtime_obj(const char* obj_name, char* result, size_t size) {
    char* current_dir = es_get_current_directory();

    
    if (es_path_exists(obj_name)) {
        ES_STRCPY_S(result, size, obj_name);
        if (current_dir) ES_FREE(current_dir);
        return;
    }

    
    const char* subdirs[] = {
        "obj" ES_PATH_SEPARATOR_STR "runtime",
        "obj" ES_PATH_SEPARATOR_STR "core" ES_PATH_SEPARATOR_STR "utils",
        "obj" ES_PATH_SEPARATOR_STR "core" ES_PATH_SEPARATOR_STR "memory",
        "obj" ES_PATH_SEPARATOR_STR "compiler",
        "obj" ES_PATH_SEPARATOR_STR "common",
        "build"
    };
    for (size_t i = 0; i < sizeof(subdirs)/sizeof(subdirs[0]); i++) {
        char temp[ES_MAX_PATH];
        es_path_join(temp, sizeof(temp), subdirs[i], obj_name);
        if (es_path_exists(temp)) {
            ES_STRCPY_S(result, size, temp);
            if (current_dir) ES_FREE(current_dir);
            return;
        }
    }

    
    if (current_dir) {
        char parent_obj[ES_MAX_PATH];
        es_path_join(parent_obj, sizeof(parent_obj), current_dir, "..");

        for (size_t i = 0; i < sizeof(subdirs)/sizeof(subdirs[0]); i++) {
            char full_path[ES_MAX_PATH];
            char temp_path[ES_MAX_PATH];
            es_path_join(full_path, sizeof(full_path), parent_obj, subdirs[i]);
            es_path_join(temp_path, sizeof(temp_path), full_path, obj_name);
            if (es_path_exists(temp_path)) {
                ES_STRCPY_S(result, size, temp_path);
                ES_FREE(current_dir);
                return;
            }
        }
        ES_FREE(current_dir);
    }

    
    char exe_dir[ES_MAX_PATH];
    if (es_get_executable_directory(exe_dir, sizeof(exe_dir)) == 0) {
        
        char temp[ES_MAX_PATH];
        es_path_join(temp, sizeof(temp), exe_dir, obj_name);
        if (es_path_exists(temp)) {
            ES_STRCPY_S(result, size, temp);
            return;
        }

        
        const char* exe_rel_base[] = {
            ".." ES_PATH_SEPARATOR_STR ".." ES_PATH_SEPARATOR_STR "obj" ES_PATH_SEPARATOR_STR "runtime",
            ".." ES_PATH_SEPARATOR_STR ".." ES_PATH_SEPARATOR_STR "obj" ES_PATH_SEPARATOR_STR "core" ES_PATH_SEPARATOR_STR "utils",
            ".." ES_PATH_SEPARATOR_STR ".." ES_PATH_SEPARATOR_STR "obj" ES_PATH_SEPARATOR_STR "core" ES_PATH_SEPARATOR_STR "memory",
            ".." ES_PATH_SEPARATOR_STR ".." ES_PATH_SEPARATOR_STR "obj" ES_PATH_SEPARATOR_STR "compiler",
            ".." ES_PATH_SEPARATOR_STR ".." ES_PATH_SEPARATOR_STR "obj" ES_PATH_SEPARATOR_STR "common",
            ".." ES_PATH_SEPARATOR_STR ".." ES_PATH_SEPARATOR_STR "build"
        };
        for (size_t i = 0; i < sizeof(exe_rel_base)/sizeof(exe_rel_base[0]); i++) {
            char rel_path[ES_MAX_PATH];
            char temp2[ES_MAX_PATH];
            es_path_join(rel_path, sizeof(rel_path), exe_dir, exe_rel_base[i]);
            es_path_join(temp2, sizeof(temp2), rel_path, obj_name);
            if (es_path_exists(temp2)) {
                ES_STRCPY_S(result, size, temp2);
                return;
            }
        }
    }

    
    ES_STRCPY_S(result, size, obj_name);
}

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

void parallel_compiler_get_stats(ParallelCompiler* compiler, int* total, int* succeeded, int* failed) {
    if (!compiler) return;

    if (total) *total = compiler->stats.total_files;
    if (succeeded) *succeeded = compiler->stats.succeeded;
    if (failed) *failed = compiler->stats.failed;
}


int parallel_compiler_collect_generic_types(ParallelCompiler* compiler) {
    if (!compiler || !compiler->shared_generic_registry) return -1;
    
    for (int i = 0; i < compiler->task_count; i++) {
        CompileTask* task = compiler->tasks[i];
        
        
        FILE* fp = fopen(task->input_file, "r");
        if (!fp) {
            continue;
        }
        
        if (fseek(fp, 0, SEEK_END) != 0) {
            fclose(fp);
            continue;
        }
        
        long file_size = ftell(fp);
        if (file_size <= 0) {
            fclose(fp);
            continue;
        }
        
        rewind(fp);
        char* source = (char*)ES_MALLOC(file_size + 1);
        if (!source) {
            ES_ERROR("内存分配失败: %s", task->input_file);
            fclose(fp);
            continue;
        }
        
        size_t read_bytes = fread(source, 1, file_size, fp);
        fclose(fp);
        source[read_bytes] = '\0';
        
        char* pos = source;
        while ((pos = strstr(pos, "template")) != NULL) {
            
            
            char* search_pos = pos + 8; 
            char* class_pos = NULL;
            
            
            while ((class_pos = strstr(search_pos, "class")) != NULL) {
                
                char* where_pos = strstr(search_pos, "where");
                char* brace_pos = strstr(search_pos, "{");
                
                
                if (where_pos && where_pos < class_pos && brace_pos && class_pos < brace_pos) {
                    search_pos = class_pos + 5;
                    continue; 
                }
                
                
                break;
            }
            
            if (!class_pos) {
                pos++;
                continue;
            }
            
            
            char* name_start = class_pos + 5;
            while (*name_start && (*name_start == ' ' || *name_start == '\t')) {
                name_start++;
            }
            
            
            
            char* name_end = name_start;
            while (*name_end && 
                   (*name_end != '{' && *name_end != '<' && *name_end != ' ' && 
                    *name_end != '\n' && *name_end != '\t' && *name_end != ',')) {
                name_end++;
            }
            
            
            
            if (name_end > name_start) {
                char* test = name_start;
                bool valid_name = true;
                
                
                if (!((*test >= 'a' && *test <= 'z') || (*test >= 'A' && *test <= 'Z') || *test == '_')) {
                    valid_name = false;
                } else {
                    
                    test++;
                    while (test < name_end) {
                        if (!((*test >= 'a' && *test <= 'z') || (*test >= 'A' && *test <= 'Z') || 
                              (*test >= '0' && *test <= '9') || *test == '_')) {
                            valid_name = false;
                            break;
                        }
                        test++;
                    }
                }
                
                if (!valid_name) {
                    pos = name_end;
                    continue;
                }
            }
            
            if (name_end > name_start) {
                int name_len = name_end - name_start;
                char* type_name = (char*)ES_MALLOC(name_len + 1);
                strncpy(type_name, name_start, name_len);
                type_name[name_len] = '\0';
                
                
                int param_count = 1; 
                char* template_decl_start = strstr(pos, "<");
                if (template_decl_start && template_decl_start < class_pos) {
                    char* template_decl_end = strstr(template_decl_start, ">");
                    if (template_decl_end && template_decl_end < class_pos) {
                        
                        param_count = 1;
                        char* p = template_decl_start + 1;
                        while (p < template_decl_end) {
                            if (*p == ',') param_count++;
                            p++;
                        }
                    }
                }
                
                
                pthread_mutex_lock(&compiler->registry_mutex);
                if (!generics_lookup_type(compiler->shared_generic_registry, type_name)) {
                    
                    generics_register_type(compiler->shared_generic_registry, type_name, NULL, param_count, NULL);
                }
                pthread_mutex_unlock(&compiler->registry_mutex);
                
                ES_FREE(type_name);
            }
            
            pos = name_end; 
        }
        
        ES_FREE(source);
    }
    
    return 0;
}