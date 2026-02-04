#define _POSIX_C_SOURCE 200809L
#include <string.h>
extern char* strcpy(char* dest, const char* src);
extern char* strncpy(char* dest, const char* src, size_t n);
extern char* strrchr(const char* str, int c);
extern int strcmp(const char* s1, const char* s2);
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef S_IFDIR
#define S_IFDIR _S_IFDIR
#endif
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include "core/core.h"
#include "compiler/driver/compiler.h"
#include "compiler/compiler.h"
#include "compiler/driver/config_manager.h"
#include "compiler/platform/platform_abstraction.h"
#include "compiler/build/build_system.h"
#include "compiler/pipeline/compiler_pipeline.h"
#include "compiler/driver/task_manager.h"
#include "compiler/driver/console_utils.h"
#include "version.h"
struct TypeCheckContext;
typedef struct TypeCheckContext TypeCheckContext;
TypeCheckContext* type_check_context_create(void* semantic_analyzer);
void type_check_context_destroy(TypeCheckContext* context);
int type_check_program(TypeCheckContext* context, ASTNode* ast);
#include "compiler/frontend/parser/parser.h"
#include "compiler/frontend/lexer/tokenizer.h"
#include "compiler/driver/project.h"
#include "compiler/driver/parallel_compiler.h"
#include "compiler/frontend/semantic/semantic_analyzer.h"
#include "compiler/driver/preprocessor.h"
#include "compiler/frontend/semantic/generics.h"
#ifndef ES_MAX_PATH
#define ES_MAX_PATH 1024
#endif
extern void es_output_cache_init(void);
extern void es_output_cache_cleanup(void);
extern void es_output_cache_set_enabled(int enabled);
extern void es_output_cache_flush(void);


typedef enum {
    ES_TARGET_CMD_ASM = 0,
    ES_TARGET_CMD_IR = 3,
    ES_TARGET_CMD_EXE = 4,
    ES_TARGET_CMD_VM = 5,
    ES_TARGET_CMD_BUILD = 6,
    ES_TARGET_CMD_CLEAN = 7,
    ES_TARGET_CMD_CHECK = 8,
    ES_TARGET_CMD_VERSION = 9,
    ES_TARGET_CMD_EO = 10    
} EsCommandTargetType;

typedef struct {
    const char* input_file;
    const char* output_file;
    EsCommandTargetType target_type;
    int show_ir;
    int show_help;
    int create_project;
    int keep_temp_files;
    const char* project_type;
    int output_file_set;
    int target_type_set;
} EsCommandLineOptions;

static void es_print_usage(const char* program_name) {
    (void)program_name;
    const char* cyan = es_color(ES_COL_CYAN);
    const char* blue = es_color(ES_COL_BLUE);
    const char* gray = es_color(ES_COL_GRAY);
    const char* reset = es_color(ES_COL_RESET);
    
    ES_INFO("%s||E# 使用说明%s\n", blue, reset);
    es_printf("%sbuild%s:%s构建%s  clean%s:%s清理\n", blue, gray, blue, blue, gray, blue);
    es_printf("%scheck%s:%s检查%s  help%s:%s帮助信息\n", blue, gray, blue, blue, gray, blue);
    es_printf("%s--keep-temp%s:%s保留临时文件 (.asm, .obj)\n", blue, gray, blue);
    es_printf("\n%s=========== %s其他 %s===========\n", gray, gray);
    es_printf("%starget%s:%s输出类型 %s<%sir%s/%sasm%s/%sexe%s/%svm%s/%seo%s>\n", 
              blue, gray, blue, gray, blue, gray, blue, gray, blue, gray, blue, gray, blue, gray);
    es_printf("%snew%s:%s创建项目 %s<%s类型%s> <%s项目名%s>\n", blue, gray, blue, gray, blue, gray, blue, gray);
    es_printf("%sversion%s:%s %s\n", blue, gray, blue, ES_COMPILER_VERSION);
    es_printf("%s", reset);
}



static EsCommandLineOptions es_parse_command_line(int argc, char* argv[]) {
    EsCommandLineOptions options = {
        .input_file = NULL,
        .output_file = NULL,
        .target_type = ES_TARGET_CMD_ASM,
        .show_ir = 0,
        .show_help = 0,
        .create_project = 0,
        .keep_temp_files = 0,
        .project_type = NULL,
        .output_file_set = 0,
        .target_type_set = 0
    };

    if (argc < 2) {
        options.show_help = 1;
        return options;
    }

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        
        if (strcmp(arg, "help") == 0) {
            options.show_help = 1;
        } else if (strcmp(arg, "--keep-temp") == 0) {
            options.keep_temp_files = 1;
        } else if (strcmp(arg, "new") == 0) {
            if (i + 2 >= argc) {
                ES_ERROR("缺少参数: new <类型> <项目名>");
                exit(1);
            }
            options.create_project = 1;
            options.project_type = argv[++i];
            options.input_file = argv[++i];
        } else if (strcmp(arg, "build") == 0) {
            if (i + 1 < argc) {
                options.target_type = ES_TARGET_CMD_BUILD;
                options.input_file = argv[++i];
            } else {
                options.target_type = ES_TARGET_CMD_BUILD;
                
                #ifdef _WIN32
                WIN32_FIND_DATA findData;
                HANDLE hFind = FindFirstFile("*.esproj", &findData);
                if (hFind != INVALID_HANDLE_VALUE) {
                    options.input_file = strdup(findData.cFileName);
                    FindClose(hFind);
                } else {
                    ES_ERROR("未找到 .esproj 文件，请指定输入文件");
                    exit(1);
                }
                #else
                DIR *dir = opendir(".");
                if (dir) {
                    struct dirent *entry;
                    while ((entry = readdir(dir)) != NULL) {
                        const char* ext = strrchr(entry->d_name, '.');
                        if (ext && strcmp(ext, ".esproj") == 0) {
                            options.input_file = strdup(entry->d_name);
                            break;
                        }
                    }
                    closedir(dir);
                }
                if (!options.input_file) {
                    ES_ERROR("未找到 .esproj 文件，请指定输入文件");
                    exit(1);
                }
                #endif
            }
        } else if (strcmp(arg, "clean") == 0) {
            options.target_type = ES_TARGET_CMD_CLEAN;
        } else if (strcmp(arg, "check") == 0) {
            options.target_type = ES_TARGET_CMD_CHECK;
        } else if (strcmp(arg, "target") == 0) {
            if (i + 1 >= argc) {
                ES_ERROR("缺少参数: target <类型>");
                exit(1);
            }
            options.target_type_set = 1;
            const char* target_type = argv[++i];
            if (strcmp(target_type, "asm") == 0) {
                options.target_type = ES_TARGET_CMD_ASM;
            } else if (strcmp(target_type, "exe") == 0) {
                options.target_type = ES_TARGET_CMD_EXE;
            } else if (strcmp(target_type, "ir") == 0) {
                options.target_type = ES_TARGET_CMD_IR;
            } else if (strcmp(target_type, "vm") == 0) {
                options.target_type = ES_TARGET_CMD_VM;
            } else if (strcmp(target_type, "eo") == 0) {
                options.target_type = ES_TARGET_CMD_EO;
            } else {
                ES_ERROR("未知的目标类型 '%s'", target_type);
                exit(1);
            }
        } else if (strcmp(arg, "version") == 0) {
            options.target_type = ES_TARGET_CMD_VERSION;
        } else if (strcmp(arg, "output") == 0) {
            if (i + 1 >= argc) {
                ES_ERROR("缺少参数: output <输出文件>");
                exit(1);
            }
            options.output_file = argv[++i];
            options.output_file_set = 1;
        } else if (strcmp(arg, "--show-ir") == 0 || strcmp(arg, "--ir") == 0) {
            options.show_ir = 1;
         } else if (options.input_file == NULL && options.target_type != ES_TARGET_CMD_CLEAN &&
                    options.target_type != ES_TARGET_CMD_CHECK && options.target_type != ES_TARGET_CMD_VERSION) {
            options.input_file = arg;
        } else {
            ES_ERROR("未知命令或参数: %s", arg);
            options.show_help = 1;
        }
    }

    return options;
}

static int es_compile_single_file(const char* input_file, const char* output_file, EsCommandTargetType target_type, int show_ir, int keep_temp) {
    if (!input_file || !output_file) {
        ES_ERROR("输入文件或输出文件为空");
    }
    EsConfig* config = es_config_create();
    if (!config) {
        ES_ERROR("无法创建配置管理器");
        return 1;
    }
    config->keep_temp_files = keep_temp;
    switch (target_type) {
        case ES_TARGET_CMD_ASM:
            config->target_type = ES_TARGET_ASM;
            break;
        case ES_TARGET_CMD_IR:
            config->target_type = ES_TARGET_IR;
            break;
        case ES_TARGET_CMD_EXE:
            config->target_type = ES_TARGET_EXE;
            break;
        case ES_TARGET_CMD_VM:
            config->target_type = ES_TARGET_VM;
            break;
        case ES_TARGET_CMD_EO:
            config->target_type = ES_TARGET_EO;
            break;
        case ES_TARGET_CMD_BUILD:
            config->target_type = ES_TARGET_EXE;
            break;
        case ES_TARGET_CMD_CLEAN:
            break;
        case ES_TARGET_CMD_CHECK:
            break;
        case ES_TARGET_CMD_VERSION:
            break;
    }
    
    config->show_ir = show_ir;
    
    EsPlatform* platform = es_platform_get_current();
    if (!platform) {
        ES_ERROR("无法创建平台抽象");
        es_config_destroy(config);
        return 1;
    }
    
    EsCompilePipeline* pipeline = es_compile_pipeline_create(config, platform);
    if (!pipeline) {
        ES_ERROR("无法创建编译管道");
        es_config_destroy(config);
        es_platform_destroy(platform);
        return 1;
    }
    
    const char* compile_output = output_file;
    char temp_asm[ES_MAX_PATH];
    if (target_type == ES_TARGET_CMD_EXE || target_type == ES_TARGET_CMD_BUILD) {
        char file_base[ES_MAX_PATH];
        es_path_get_filename(input_file, file_base, sizeof(file_base));
        es_path_remove_extension(file_base, file_base, sizeof(file_base));
        snprintf(temp_asm, sizeof(temp_asm), "%s.asm", file_base);
        compile_output = temp_asm;
    }

    ES_INFO("开始编译: input=%s, output=%s, asm_output=%s", input_file, output_file, compile_output);
    ES_INFO("编译目标类型: %d", target_type);

    int result = es_compile_pipeline_execute(pipeline, input_file, compile_output);
    double total_duration = pipeline->total_duration;

    ES_INFO("编译管道执行结果: %d", result);

    if (result) {
        ES_INFO("检查生成的汇编文件是否存在");
        if (compile_output && es_platform_path_exists(platform, compile_output)) {
            ES_INFO("汇编文件已生成: %s", compile_output);
        } else {
            ES_WARNING("汇编文件未找到: %s", compile_output ? compile_output : "(null)");
        }

        if (target_type == ES_TARGET_CMD_EXE || target_type == ES_TARGET_CMD_BUILD) {
            if (pipeline->compiler) {
                es_compiler_destroy(pipeline->compiler);
                pipeline->compiler = NULL;
            }

            ES_INFO("开始构建步骤: asm=%s, output=%s", temp_asm, output_file);

            EsBuildContext* build_ctx = es_build_context_create(config, platform);
            if (build_ctx) {
                build_ctx->temp_asm_created = 1;
                strncpy(build_ctx->temp_asm_file, temp_asm, sizeof(build_ctx->temp_asm_file));

                ES_INFO("调用 es_build_execute...");
                if (es_build_execute(build_ctx, input_file, output_file)) {
                    ES_INFO("构建成功!");
                    es_task_report("build", input_file, ES_TASK_RESULT_EXECUTED, total_duration, es_get_global_task_stats());
                } else {
                    ES_ERROR("构建失败: %s", es_build_get_error(build_ctx));
                    es_task_report("build", input_file, ES_TASK_RESULT_FAILED, total_duration, es_get_global_task_stats());
                    result = 0;
                }
                es_build_context_destroy(build_ctx);
            } else {
                ES_ERROR("无法创建构建上下文");
                result = 0;
            }
        } else if (target_type == ES_TARGET_CMD_VM) {
            es_task_report("compile", input_file, ES_TASK_RESULT_EXECUTED, total_duration, es_get_global_task_stats());
        } else {
            es_task_report("compile", input_file, ES_TASK_RESULT_EXECUTED, total_duration, es_get_global_task_stats());
        }
    } else {
        ES_ERROR("编译失败: %s", pipeline->error_message);
        es_task_report("compile", input_file, ES_TASK_RESULT_FAILED, total_duration, es_get_global_task_stats());
    }
    
    es_compile_pipeline_destroy(pipeline);
    es_platform_destroy(platform);
    es_config_destroy(config);
    
    return result ? 0 : 1;
}

static int es_build_project(const char* project_file, const char* output_path, int keep_temp) {
    
    EsPlatform* platform = es_platform_get_current();
    if (!platform) {
        ES_ERROR("无法创建平台抽象");
        return 1;
    }
    
    EsProject* project = es_proj_load(project_file);
    if (!project) {
        ES_ERROR("无法加载项目文件: %s", project_file);
        return 1;
    }
    
    int max_threads = 8;
    EsConfig* config = es_config_create();
    if (config) config->keep_temp_files = keep_temp;
    
    ParallelCompiler* parallel_compiler = parallel_compiler_create(max_threads, config);
    if (!parallel_compiler) {
        ES_ERROR("无法创建并行编译器");
        es_proj_destroy(project);
        if (config) es_config_destroy(config);
        return 1;
    }
    
    char project_root[ES_MAX_PATH];
    char* last_sep = strrchr(project_file, '\\');
    if (!last_sep) last_sep = strrchr(project_file, '/');
    if (last_sep) {
        size_t dir_len = last_sep - project_file;
        strncpy(project_root, project_file, dir_len);
        project_root[dir_len] = '\0';
    } else {
        strcpy(project_root, ".");
    }
    
    char obj_dir[ES_MAX_PATH];
    es_platform_path_join(platform, obj_dir, sizeof(obj_dir), project_root, "obj");
    es_ensure_directory_recursive(obj_dir);

    EsProjectItem* item = project->items;
    while (item) {
        if (strcmp(item->item_type, "Compile") == 0) {
            char source_path[ES_MAX_PATH];
            es_platform_path_join(platform, source_path, sizeof(source_path), project_root, item->file_path);
            
            if (!es_platform_path_exists(platform, source_path)) {
                ES_ERROR("源文件不存在: %s", source_path);
                parallel_compiler_destroy(parallel_compiler);
                es_proj_destroy(project);
                es_config_destroy(config);
                return 1;
            }
            
            char file_name[ES_MAX_PATH];
            char* name_start = strrchr(source_path, '\\');
            if (!name_start) name_start = strrchr(source_path, '/');
            if (name_start) {
                strcpy(file_name, name_start + 1);
            } else {
                strcpy(file_name, source_path);
            }
            char file_name_no_ext[ES_MAX_PATH];
            es_path_remove_extension(file_name, file_name_no_ext, sizeof(file_name_no_ext));
            
            char intermediate_asm[ES_MAX_PATH];
            char intermediate_obj[ES_MAX_PATH];
            
            snprintf(intermediate_asm, sizeof(intermediate_asm), "%s%c%s.asm", 
                    obj_dir, es_platform_get_separator(platform), file_name_no_ext);
            snprintf(intermediate_obj, sizeof(intermediate_obj), "%s%c%s.obj", 
                    obj_dir, es_platform_get_separator(platform), file_name_no_ext);
            
            if (parallel_compiler_add_file(parallel_compiler, source_path, intermediate_asm, intermediate_obj, 
                                         ES_TARGET_ASM, 0) != 0) {
                ES_ERROR("添加编译任务失败: %s", source_path);
                parallel_compiler_destroy(parallel_compiler);
                es_proj_destroy(project);
                es_config_destroy(config);
                return 1;
            }
        }
        item = item->next;
    }
    
    if (parallel_compiler_collect_generic_types(parallel_compiler) != 0) {
        ES_WARNING("收集泛型类型信息失败，继续编译...");
    }
    
    int compile_result = parallel_compiler_execute(parallel_compiler);
    
    int total_files = 0, succeeded_files = 0, failed_files = 0;
    parallel_compiler_get_stats(parallel_compiler, &total_files, &succeeded_files, &failed_files);
    
    if (compile_result != 0 || failed_files > 0) {
        ES_ERROR("并行编译失败：%d 个文件编译失败", failed_files);
        parallel_compiler_destroy(parallel_compiler);
        es_proj_destroy(project);
        return 1;
    }
    
    if (total_files > 0) {
        char bin_dir[ES_MAX_PATH];
        char platform_name[32];
        
        strncpy(platform_name, platform->name, sizeof(platform_name));
        for (int i = 0; platform_name[i]; i++) {
            platform_name[i] = tolower(platform_name[i]);
        }
        
        char bin_subdir[ES_MAX_PATH];
        snprintf(bin_subdir, sizeof(bin_subdir), "bin%c%s", 
                es_platform_get_separator(platform), platform_name);
        
        es_platform_path_join(platform, bin_dir, sizeof(bin_dir), project_root, bin_subdir);
        
        es_ensure_directory_recursive(bin_dir);
        
        char project_output_path[ES_MAX_PATH];
        char project_name[ES_MAX_PATH];
        es_path_get_filename(project_file, project_name, sizeof(project_name));
        es_path_remove_extension(project_name, project_name, sizeof(project_name));
        
        char project_filename[ES_MAX_PATH];
        if (platform->type == ES_CONFIG_PLATFORM_WINDOWS) {
            snprintf(project_filename, sizeof(project_filename), "%s.exe", project_name);
        } else {
            strncpy(project_filename, project_name, sizeof(project_filename));
        }
        
        es_platform_path_join(platform, project_output_path, sizeof(project_output_path), bin_dir, project_filename);

        if (parallel_compiler_link_results(parallel_compiler, project_output_path) != 0) {
            ES_ERROR("链接失败");
            parallel_compiler_destroy(parallel_compiler);
            es_proj_destroy(project);
            es_config_destroy(config);
            return 1;
        }
    }
    
    es_platform_destroy(platform);
    parallel_compiler_destroy(parallel_compiler);
    es_proj_destroy(project);
    
    return 0;
}

static int es_run_compiler(const char* input_file, const char* output_file, EsCommandTargetType target_type, int show_ir, int keep_temp) {
    if (!input_file) {
        ES_ERROR("未指定输入文件");
        return 1;
    }
    int is_project = 0;
    const char* ext = strrchr(input_file, '.');
    if (ext && (strcmp(ext, ".esproj") == 0 || strcmp(ext, ".json") == 0)) {
        is_project = 1;
    }
    char default_output[ES_MAX_PATH];
    const char* final_output = output_file ? output_file : default_output;
    

    memset(default_output, 0, sizeof(default_output));
    if (is_project) {
        char project_name[ES_MAX_PATH];
        es_path_get_filename(input_file, project_name, sizeof(project_name));
        es_path_remove_extension(project_name, project_name, sizeof(project_name));
#ifdef _WIN32
        snprintf(default_output, sizeof(default_output), "%s.exe", project_name);
#else
        snprintf(default_output, sizeof(default_output), "%s", project_name);
#endif
    } else {
        switch (target_type) {
            case ES_TARGET_CMD_EXE: {
                char file_base[ES_MAX_PATH];
                es_path_get_filename(input_file, file_base, sizeof(file_base));
                es_path_remove_extension(file_base, file_base, sizeof(file_base));
#ifdef _WIN32
                snprintf(default_output, sizeof(default_output), "%s.exe", file_base);
#else
                snprintf(default_output, sizeof(default_output), "%s", file_base);
#endif
                break;
            }
            case ES_TARGET_CMD_IR:
                strncpy(default_output, "output.ir", sizeof(default_output) - 1);
                break;
            case ES_TARGET_CMD_BUILD: {
                char file_base[ES_MAX_PATH];
                es_path_get_filename(input_file, file_base, sizeof(file_base));
                es_path_remove_extension(file_base, file_base, sizeof(file_base));
#ifdef _WIN32
                snprintf(default_output, sizeof(default_output), "%s.exe", file_base);
#else
                snprintf(default_output, sizeof(default_output), "%s", file_base);
#endif
                break;
            }
            case ES_TARGET_CMD_VM:
                strncpy(default_output, "output.ebc", sizeof(default_output) - 1);
                break;
            default:
                strncpy(default_output, "output.asm", sizeof(default_output) - 1);
                break;
        }
    }
    
    if (is_project) {
        return es_build_project(input_file, final_output, keep_temp);
    } else {
        return es_compile_single_file(input_file, final_output, target_type, show_ir, keep_temp);
    }
}

int main(int argc, char* argv[]) {
    es_console_set_color_enabled(es_console_supports_color());
    es_build_summary_reset();
    
    double total_start = es_get_time();
    
    EsCommandLineOptions options = es_parse_command_line(argc, argv);
    
    if (options.show_help) {
        es_print_usage(argv[0]);
        es_output_cache_cleanup();
        return 0;
    }
    
    if (options.create_project) {
        const char* project_name = project_name_from_type(options.project_type);
        es_create_project(project_name, options.project_type);
        es_build_summary_set_duration(es_get_time() - total_start);
        es_print_build_summary();
        es_output_cache_cleanup();
        return 0;
    }
    
    if (options.target_type == ES_TARGET_CMD_VERSION) {
        const char *blue  = es_color(ES_COL_BLUE);
        const char *gray  = es_color(ES_COL_GRAY);
        const char *reset = es_color(ES_COL_RESET);

        es_printf("%sE# Compiler Version%s: %s %s (%s %s)\n",
                  blue, reset,
                  ES_COMPILER_VERSION,
                  gray, __DATE__, __TIME__);
        es_output_cache_cleanup();
        return 0;
    }


    int result = es_run_compiler(options.input_file, options.output_file, options.target_type, options.show_ir, options.keep_temp_files);

    
    es_build_summary_set_duration(es_get_time() - total_start);
    if (result != 0) {
        es_build_summary_set_failed(1);
    }
    es_print_build_summary();
    
    es_output_cache_cleanup();
    return result;
}