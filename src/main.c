#define _GNU_SOURCE
#include "common/es_common.h"
#include "common/output_cache.h"
#include "compiler/compiler.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "project/esproj.h"
#include "compiler/parallel_compiler.h"
#include "type_check/type_check.h"
#include "compiler/semantic_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>


#define ANSI_CURSOR_UP "\033[F"
#define ANSI_CLEAR_LINE "\033[K"
#define ANSI_SAVE_CURSOR "\033[s"
#define ANSI_RESTORE_CURSOR "\033[u"


static int g_current_task_line_active = 0;
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
#endif

typedef enum {
    ES_TARGET_CMD_ASM = 0,
    ES_TARGET_CMD_IR = 3,
    ES_TARGET_CMD_EXE = 4
} EsTargetType;
typedef struct {
    const char* input_file;
    const char* output_file;
    EsTargetType target_type;
    int show_ir;
    int show_help;
    int create_project;
    const char* project_type;
} EsCommandLineOptions;
typedef enum {
    ES_TASK_RESULT_EXECUTED = 0,
    ES_TASK_RESULT_UP_TO_DATE,
    ES_TASK_RESULT_SKIPPED,
    ES_TASK_RESULT_FAILED
} EsTaskResult;
typedef struct {
    int executed;
    int up_to_date;
    int skipped;
    int failed;
} EsTaskStats;
typedef struct {
    EsTaskStats stats;
    double total_duration;
    int failed;
} EsBuildSummary;
static EsBuildSummary g_build_summary = {0};
static int g_color_enabled = 0;
#define ES_COL_RESET "\033[0m"
#define ES_COL_BOLD "\033[1m"
#define ES_COL_GREEN "\033[32m"
#define ES_COL_RED "\033[31m"
#define ES_COL_CYAN "\033[36m"
#define ES_COL_YELLOW "\033[33m"
#define ES_COL_GRAY "\033[90m"
#define ES_COL_MAGENTA "\033[35m"
static const char* es_color(const char* code) {
    return g_color_enabled ? code : "";
}
static int es_console_supports_color(void) {
#ifdef _WIN32
    if (!_isatty(_fileno(stdout))) {
        return 0;
    }
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) {
        return 0;
    }
    if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        if (!SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            return 0;
        }
    }
    return 1;
#else
    return isatty(fileno(stdout));
#endif
}

#ifndef ES_MAX_PATH
#define ES_MAX_PATH 1024
#endif









static char main_path_separator(void) {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

static int main_path_is_separator(char c) {
    return c == '/' || c == '\\';
}

static int main_path_is_absolute(const char* path) {
    if (!path || path[0] == '\0') return 0;
#ifdef _WIN32
    if (main_path_is_separator(path[0])) return 1;
    return strlen(path) >= 2 && path[1] == ':';
#else
    return path[0] == '/';
#endif
}

static void main_path_join(char* buffer, size_t buffer_size, const char* base, const char* part) {
    if (!buffer || buffer_size == 0) return;
    if (!base || base[0] == '\0') {
        snprintf(buffer, buffer_size, "%s", part ? part : "");
        return;
    }
    if (!part || part[0] == '\0') {
        snprintf(buffer, buffer_size, "%s", base);
        return;
    }

    size_t base_len = strlen(base);
    int need_sep = base_len > 0 && !main_path_is_separator(base[base_len - 1]);
    if (need_sep) {
        snprintf(buffer, buffer_size, "%s%c%s", base, main_path_separator(), part);
    } else {
        snprintf(buffer, buffer_size, "%s%s", base, part);
    }
}

static int main_path_is_directory(const char* path) {
    if (!path || path[0] == '\0') return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
#ifdef _WIN32
    return (st.st_mode & S_IFDIR) != 0;
#else
    return S_ISDIR(st.st_mode);
#endif
}

static void es_resolve_project_path(char* buffer, size_t buffer_size, const char* project_root, const char* file_path) {
    if (!buffer || buffer_size == 0) return;
    if (!file_path || file_path[0] == '\0') {
        buffer[0] = '\0';
        return;
    }
    if (main_path_is_absolute(file_path)) {
        snprintf(buffer, buffer_size, "%s", file_path);
    } else if (project_root && project_root[0] != '\0') {
        main_path_join(buffer, buffer_size, project_root, file_path);
    } else {
        snprintf(buffer, buffer_size, "%s", file_path);
    }
}

static void es_flush_output(void) {
    extern void es_output_cache_flush(void);
    es_output_cache_flush();
    fflush(stdout);
    fflush(stderr);
}

static const char* project_name_from_type(const char* project_type) {
    if (!project_type) return "MyProject";
    if (strcmp(project_type, "console") == 0) return "ConsoleApp";
    if (strcmp(project_type, "library") == 0) return "Library";
    if (strcmp(project_type, "web") == 0) return "WebApp";
    if (strcmp(project_type, "system") == 0) return "SystemProject";
    return project_type;
}

static void es_build_summary_reset(void) {
    g_build_summary.stats.executed = 0;
    g_build_summary.stats.up_to_date = 0;
    g_build_summary.stats.skipped = 0;
    g_build_summary.stats.failed = 0;
    g_build_summary.total_duration = 0.0;
    g_build_summary.failed = 0;
}


static void es_show_idle_status(void);
static void es_build_summary_accumulate(const EsTaskStats* stats, double duration, int failed) {
    if (!stats) return;
    g_build_summary.stats.executed += stats->executed;
    g_build_summary.stats.up_to_date += stats->up_to_date;
    g_build_summary.stats.skipped += stats->skipped;
    g_build_summary.stats.failed += stats->failed;
    g_build_summary.total_duration += duration;
    if (failed) {
        g_build_summary.failed = 1;
    }
}
static void es_print_build_summary(void) {
    int total_tasks = g_build_summary.stats.executed + g_build_summary.stats.up_to_date + g_build_summary.stats.skipped;
    if (total_tasks <= 0) {
        return;
    }


    es_show_idle_status();

    extern void es_output_cache_flush(void);
    es_output_cache_flush();

    const char* status_text = g_build_summary.failed ? "FAILED" : "SUCCESSFUL";
    const char* status_color = g_build_summary.failed ? ES_COL_RED : ES_COL_GREEN;


    es_printf("\n%s%sBUILD %s%s in %.1fs%s\n",
           es_color(ES_COL_BOLD),
           es_color(status_color),
           status_text,
           es_color(ES_COL_RESET),
           g_build_summary.total_duration,
           es_color(ES_COL_RESET));


    int executed = g_build_summary.stats.executed;
    int up_to_date = g_build_summary.stats.up_to_date;
    int skipped = g_build_summary.stats.skipped;
    int failed = g_build_summary.stats.failed;

    if (total_tasks > 0) {
        es_printf("%s%d tasks:%s", es_color(ES_COL_GRAY), total_tasks, es_color(ES_COL_RESET));

        if (executed > 0) {
            es_printf(" %s%d executed%s", es_color(ES_COL_GREEN), executed, es_color(ES_COL_RESET));
        }
        if (up_to_date > 0) {
            es_printf(", %s%d up-to-date%s", es_color(ES_COL_YELLOW), up_to_date, es_color(ES_COL_RESET));
        }
        if (skipped > 0) {
            es_printf(", %s%d skipped%s", es_color(ES_COL_GRAY), skipped, es_color(ES_COL_RESET));
        }
        if (failed > 0) {
            es_printf(", %s%d failed%s", es_color(ES_COL_RED), failed, es_color(ES_COL_RESET));
        }
    }
}
static void es_print_cli_header(void) {
    const char* bold = es_color(ES_COL_BOLD);
    const char* reset = es_color(ES_COL_RESET);


    es_printf("%s╔═══════════════════════════════════════════╗%s\n", bold, reset);
    es_printf("%s║        E# Compiler | 构建系统              ║%s\n", bold, reset);
    es_printf("%s╚═══════════════════════════════════════════╝%s\n", bold, reset);
    es_printf("\n");
}
static void es_task_report(const char* stage, const char* file, EsTaskResult result, double duration, EsTaskStats* stats) {
    if (!stage || !stats) return;

    const char* status_suffix = "";
    const char* suffix_color = ES_COL_RESET;
    switch (result) {
        case ES_TASK_RESULT_EXECUTED:
            stats->executed++;
            break;
        case ES_TASK_RESULT_UP_TO_DATE:
            stats->up_to_date++;
            status_suffix = " UP-TO-DATE";
            suffix_color = ES_COL_YELLOW;
            break;
        case ES_TASK_RESULT_SKIPPED:
            stats->skipped++;
            status_suffix = " SKIPPED";
            suffix_color = ES_COL_GRAY;
            break;
        case ES_TASK_RESULT_FAILED:
            stats->executed++;
            stats->failed++;

            break;
    }



    if (g_current_task_line_active) {
        es_print_format(ANSI_CURSOR_UP ANSI_CLEAR_LINE);
    }

    if (result == ES_TASK_RESULT_EXECUTED || result == ES_TASK_RESULT_UP_TO_DATE || result == ES_TASK_RESULT_SKIPPED) {

        es_print_format("%s>%s %s",
               es_color(ES_COL_CYAN),
               es_color(ES_COL_RESET),
               stage);
        if (file && file[0] != '\0') {
            es_print_format(" %s%s%s",
                   es_color(ES_COL_GRAY),
                   file,
                   es_color(ES_COL_RESET));
        }
        if (status_suffix[0] != '\0') {
            es_print_format(" %s%s%s", es_color(suffix_color), status_suffix, es_color(ES_COL_RESET));
        }
        if (duration >= 0.0 && duration > 0.01) {
            es_print_format(" %s%.1fs%s", es_color(ES_COL_GRAY), duration, es_color(ES_COL_RESET));
        }
        es_print_format("\n");
        g_current_task_line_active = 1;
    } else if (result == ES_TASK_RESULT_FAILED) {

        es_print_format("%s>%s %s",
               es_color(ES_COL_CYAN),
               es_color(ES_COL_RESET),
               stage);
        if (file && file[0] != '\0') {
            es_print_format(" %s%s%s",
                   es_color(ES_COL_GRAY),
                   file,
                   es_color(ES_COL_RESET));
        }
        es_print_format(" %sFAILED%s\n", es_color(ES_COL_RED), es_color(ES_COL_RESET));
        g_current_task_line_active = 0;
    }

    if (result == ES_TASK_RESULT_FAILED) {

        es_flush_output();
        g_current_task_line_active = 0;
    }
}


void es_show_idle_status(void) {
    if (g_current_task_line_active) {

        es_print_format(ANSI_CURSOR_UP ANSI_CLEAR_LINE);
        es_print_format("%s>%s %sIDLE%s\n",
               es_color(ES_COL_CYAN),
               es_color(ES_COL_RESET),
               es_color(ES_COL_GRAY),
               es_color(ES_COL_RESET));
        g_current_task_line_active = 0;
    }
}

void es_print_usage(const char* program_name) {
    (void)program_name;
    ES_INFO("E# 编译器 || 使用说明");
    es_printf("  -o, --output <文件名>    指定输出文件名 (默认: output.asm 或 a.exe)\n");
    es_printf("  -t, --target <类型>      指定目标类型: asm, ir, exe (默认: asm)\n");
    es_printf("  -n, --new <类型>         创建新项目 (类型: console, system, library)\n");
    es_printf("  -h, --help               显示此帮助信息\n");

}
EsCommandLineOptions es_parse_command_line(int argc, char* argv[]) {
    EsCommandLineOptions options = {
        .input_file = NULL,
        .output_file = NULL,
        .target_type = ES_TARGET_CMD_ASM,
        .show_ir = 0,
        .show_help = 0,
        .create_project = 0,
        .project_type = NULL
    };
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--new") == 0) {
            if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                ES_ERROR("缺少项目类型参数");
                es_print_usage(argv[0]);
                es_flush_output();
                exit(1);
            }
            options.create_project = 1;
            options.project_type = argv[i + 1];
            if (i + 2 < argc && argv[i + 2][0] != '-') {
                options.input_file = argv[i + 2];
            i += 3;
            } else {
                i += 2;
            }
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            options.show_help = 1;
            i++;
        }
        else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
            options.output_file = argv[i + 1];
            i += 2;
        }
        else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target") == 0) && i + 1 < argc) {
            if (strcmp(argv[i + 1], "asm") == 0) {
                options.target_type = ES_TARGET_CMD_ASM;
            } else if (strcmp(argv[i + 1], "exe") == 0) {
                options.target_type = ES_TARGET_CMD_EXE;
            } else if (strcmp(argv[i + 1], "ir") == 0) {
                options.target_type = ES_TARGET_CMD_IR;
            } else {
                ES_ERROR("未知的目标类型 '%s'", argv[i + 1]);
                ES_INFO("支持的目标类型: asm, exe, ir");
                exit(1);
            }
            i += 2;
        }
        else if (strcmp(argv[i], "-ir") == 0) {
            options.show_ir = 1;
            i++;
        }
        else if (argv[i][0] == '-') {
            ES_ERROR("未知的选项 '%s'", argv[i]);
            es_print_usage(argv[0]);
            exit(1);
        }
        else {
            if (options.input_file == NULL) {
                options.input_file = argv[i];
            } else {
                ES_ERROR("指定了多个输入文件");
                es_print_usage(argv[0]);
                exit(1);
            }
            i++;
        }
    }

    if (options.create_project) {
        if (!options.project_type || options.project_type[0] == '\0') {
            ES_ERROR("项目类型不能为空");
            es_print_usage(argv[0]);
            es_flush_output();
            exit(1);
        }
        if (!options.input_file) {
            options.input_file = project_name_from_type(options.project_type);
            if (!options.input_file) {
                options.input_file = "MyProject";
            }
        }
    }
    return options;
}
int es_compile_file(const char* input_file, const char* output_file, EsTargetType target_type, int show_ir) {
    extern void es_output_cache_init(void);
    extern void es_output_cache_cleanup(void);
    extern void es_output_cache_set_enabled(int enabled);
    es_output_cache_init();
    es_output_cache_set_enabled(1);

    double build_start = es_time_now_seconds();
    EsTaskStats stats = {0};
    int success = 0;
    char* source = NULL;
    Lexer* lexer = NULL;
    Parser* parser = NULL;
    ASTNode* ast = NULL;
    TypeCheckContext* type_context = NULL;
    EsCompiler* compiler = NULL;
    const char* stage_file = input_file;
    if (input_file) {
        const char* slash = strrchr(input_file, '/');
        const char* backslash = strrchr(input_file, '\\');
        if (backslash && (!slash || backslash > slash)) {
            slash = backslash;
        }
        if (slash) {
            stage_file = slash + 1;
        }
    }
    EsTargetPlatform target = ES_TARGET_X86_ASM;
    const char* temp_asm_file = NULL;
    char temp_asm_filename[ES_MAX_PATH];
    char temp_obj_file[ES_MAX_PATH];
    int temp_asm_created = 0;
    int temp_obj_created = 0;
    double stage_start = es_time_now_seconds();
    FILE* fp = fopen(input_file, "r");
    if (!fp) {
        ES_ERROR("无法打开源文件: %s", input_file);
        es_task_report("readSource", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        ES_ERROR("无法读取源文件大小: %s", input_file);
        fclose(fp);
        es_task_report("readSource", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    long file_size = ftell(fp);
    if (file_size < 0) {
        ES_ERROR("无法获取源文件大小: %s", input_file);
        fclose(fp);
        es_task_report("readSource", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    rewind(fp);
    source = (char*)ES_MALLOC((size_t)file_size + 1);
    if (!source) {
        ES_ERROR("内存分配失败，无法读取源文件: %s", input_file);
        fclose(fp);
        es_task_report("readSource", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    size_t read_bytes = fread(source, 1, (size_t)file_size, fp);
    fclose(fp);
    fp = NULL;
    source[read_bytes] = '\0';
    es_task_report("readSource", stage_file, ES_TASK_RESULT_EXECUTED, es_time_now_seconds() - stage_start, &stats);
    stage_start = es_time_now_seconds();
    lexer = lexer_create(source);
    if (!lexer) {
        ES_ERROR("词法分析器创建失败: %s", input_file);
        es_task_report("lex", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    es_task_report("lex", stage_file, ES_TASK_RESULT_EXECUTED, es_time_now_seconds() - stage_start, &stats);
    stage_start = es_time_now_seconds();
    parser = parser_create(lexer);
    ast = parser_parse(parser);
    if (!ast) {
        fflush(stderr);
        fflush(stdout);
        ES_ERROR("语法分析失败: %s", input_file);
        es_task_report("parse", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    es_task_report("parse", stage_file, ES_TASK_RESULT_EXECUTED, es_time_now_seconds() - stage_start, &stats);

    stage_start = es_time_now_seconds();
    SemanticAnalyzer* semantic_analyzer = semantic_analyzer_create();
    if (!semantic_analyzer) {
        ES_ERROR("语义分析器创建失败: %s", input_file);
        es_task_report("semantic", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    SemanticAnalysisResult* semantic_result = semantic_analyzer_analyze(semantic_analyzer, ast);
    if (!semantic_result || !semantic_result->success) {
        ES_ERROR("语义分析失败: %s", input_file);
        es_task_report("semantic", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        if (semantic_result) {
            free(semantic_result);
        }
        semantic_analyzer_destroy(semantic_analyzer);
        goto finalize;
    }
    es_task_report("semantic", stage_file, ES_TASK_RESULT_EXECUTED, es_time_now_seconds() - stage_start, &stats);
    free(semantic_result);
    semantic_analyzer_destroy(semantic_analyzer);

    stage_start = es_time_now_seconds();
    type_context = type_check_context_create();
    if (!type_context) {
        ES_ERROR("类型检查上下文创建失败: %s", input_file);
        es_task_report("typeCheck", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    if (type_check_program(type_context, ast) == 0) {
        ES_ERROR("类型检查失败: %s", input_file);
        es_task_report("typeCheck", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }
    es_task_report("typeCheck", stage_file, ES_TASK_RESULT_EXECUTED, es_time_now_seconds() - stage_start, &stats);
    switch (target_type) {
        case ES_TARGET_CMD_ASM:
            target = ES_TARGET_X86_ASM;
            break;
        case ES_TARGET_CMD_IR:
            target = ES_TARGET_IR_TEXT;
            break;
        case ES_TARGET_CMD_EXE:
            target = ES_TARGET_X86_ASM;

            int asm_filename_len = snprintf(temp_asm_filename, sizeof(temp_asm_filename), "%s.asm", input_file);
            if (asm_filename_len < 0 || (size_t)asm_filename_len >= sizeof(temp_asm_filename)) {
                ES_ERROR("汇编文件名过长: %s.asm", input_file);
                es_task_report("codegen", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
                goto finalize;
            }
            temp_asm_file = temp_asm_filename;
            break;
        default:
            target = ES_TARGET_X86_ASM;
            break;
    }
    stage_start = es_time_now_seconds();
    compiler = es_compiler_create(target_type == ES_TARGET_CMD_EXE ? temp_asm_file : output_file, target);
    if (!compiler) {
        es_task_report("codegen", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
        goto finalize;
    }




    es_compiler_compile(compiler, ast, type_context);



    es_compiler_destroy(compiler);

    compiler = NULL;
    if (target_type == ES_TARGET_CMD_EXE && temp_asm_file) {
        temp_asm_created = 1;
    }
    es_task_report("codegen", stage_file, ES_TASK_RESULT_EXECUTED, es_time_now_seconds() - stage_start, &stats);

    if (target_type == ES_TARGET_CMD_EXE && temp_asm_file) {
        stage_start = es_time_now_seconds();
        int obj_len = snprintf(temp_obj_file, sizeof(temp_obj_file), "%s.obj", input_file);

        if (obj_len < 0 || (size_t)obj_len >= sizeof(temp_obj_file)) {
            ES_ERROR("目标文件名过长: %s.obj", input_file);
            es_task_report("assemble", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
            goto finalize;
        }


        char asm_cmd[1024];
        int asm_cmd_len = snprintf(asm_cmd, sizeof(asm_cmd), "nasm -f win64 %s -o %s", temp_asm_file, temp_obj_file);
        if (asm_cmd_len < 0 || (size_t)asm_cmd_len >= sizeof(asm_cmd)) {
            ES_ERROR("汇编命令字符串过长");
            es_task_report("assemble", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
            goto finalize;
        }
        int asm_result = system(asm_cmd);
        if (asm_result != 0) {
            es_task_report("assemble", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
            goto finalize;
        }
        temp_obj_created = 1;
        es_task_report("assemble", stage_file, ES_TASK_RESULT_EXECUTED, es_time_now_seconds() - stage_start, &stats);

        stage_start = es_time_now_seconds();

        char link_cmd[1024];
        int link_cmd_len = snprintf(link_cmd, sizeof(link_cmd), "gcc %s obj/compiler/runtime.o obj/common/output_cache.o -o %s -mconsole", temp_obj_file, output_file);
        if (link_cmd_len < 0 || (size_t)link_cmd_len >= sizeof(link_cmd)) {
            ES_ERROR("链接命令字符串过长");
            es_task_report("link", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
            goto finalize;
        }
        int link_result = system(link_cmd);
        if (link_result != 0) {
            es_task_report("link", stage_file, ES_TASK_RESULT_FAILED, es_time_now_seconds() - stage_start, &stats);
            goto finalize;
        }
        es_task_report("link", stage_file, ES_TASK_RESULT_EXECUTED, es_time_now_seconds() - stage_start, &stats);
    }
    if (show_ir) {
        es_task_report("showIR", stage_file, ES_TASK_RESULT_EXECUTED, 0.0, &stats);
    }
    success = 1;

finalize:

    if (temp_asm_created && temp_asm_file) {

        remove(temp_asm_file);
    }
    if (temp_obj_created) {

        remove(temp_obj_file);
    }
    if (compiler) {

        es_compiler_destroy(compiler);
    }
    if (type_context) {

        type_check_context_destroy(type_context);

    }
    if (ast) {

            ast_destroy(ast);

    }
    if (parser) {

            parser_destroy(parser);

    }
    if (lexer) {

            lexer_destroy(lexer);

    }
    if (source) {

            ES_FREE(source);

    }

    es_build_summary_accumulate(&stats, es_time_now_seconds() - build_start, success ? 0 : 1);

    es_output_cache_cleanup();

    return success ? 0 : 1;
}
int es_create_project(const char* project_type, const char* project_name) {
    if (!project_type || !project_name) {
        fprintf(stderr, "错误: 项目类型和项目名称不能为空\n");
        es_flush_output();
            return 1;
        }

    if (main_path_is_directory(project_name)) {
        fprintf(stderr, "错误: 项目目录已存在: %s\n", project_name);
        fprintf(stderr, "请使用不同的项目名称，或删除现有目录\n");
        es_flush_output();
        return 1;
    }

    char proj_file[ES_MAX_PATH];
    char proj_file_name[256];

    int proj_file_name_len = snprintf(proj_file_name, sizeof(proj_file_name), "%s.esproj", project_name);
    if (proj_file_name_len < 0 || (size_t)proj_file_name_len >= sizeof(proj_file_name)) {
        fprintf(stderr, "错误: 项目文件名过长: %s.esproj\n", project_name);
        es_flush_output();
        return 1;
    }
    main_path_join(proj_file, sizeof(proj_file), project_name, proj_file_name);
    if (es_path_exists(proj_file)) {
        fprintf(stderr, "错误: 项目文件已存在: %s\n", proj_file);
        fprintf(stderr, "请使用不同的项目名称，或删除现有项目\n");
        es_flush_output();
        return 1;
    }
    EsProject* project = es_proj_create(project_name, ES_PROJ_TYPE_CONSOLE);
    if (strcmp(project_type, "console") == 0) {
        project->type = ES_PROJ_TYPE_CONSOLE;
    } else if (strcmp(project_type, "library") == 0) {
        project->type = ES_PROJ_TYPE_LIBRARY;
    } else if (strcmp(project_type, "web") == 0) {
        project->type = ES_PROJ_TYPE_WEB;
    } else if (strcmp(project_type, "system") == 0) {
        project->type = ES_PROJ_TYPE_SYSTEM;
    } else {
        ES_ERROR("错误: 未知的项目类型: %s", project_type);
        ES_INFO("支持的项目类型: console, library, web, system");
        es_proj_destroy(project);
        es_flush_output();
        return 1;
    }
    project->project_root = strdup(project_name);
    if (es_proj_create_template(project, project_name) == 0) {
        ES_ERROR("错误: 创建项目模板失败");
        es_proj_destroy(project);
        es_flush_output();
        return 1;
    }
    if (es_proj_save(project, proj_file) != 0) {
        ES_ERROR("错误: 保存项目文件失败");
        es_proj_destroy(project);
        es_flush_output();
        return 1;
    }
    int file_count = 0, dep_count = 0;
    EsProjectItem* item = project->items;
    while (item) {
        file_count++;
        item = item->next;
    }
    EsProjectDependency* dep = project->dependencies;
    while (dep) {
        dep_count++;
        dep = dep->next;
    }
    es_proj_destroy(project);
    es_flush_output();
    return 0;
}
int main(int argc, char* argv[]) {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    #endif
    es_build_summary_reset();
    const char* no_color_env = getenv("NO_COLOR");
    if (no_color_env && no_color_env[0] != '\0') {
        g_color_enabled = 0;
    } else {
        g_color_enabled = es_console_supports_color();
    }

    extern void es_output_cache_set_enabled(int enabled);
    es_output_cache_set_enabled(1);

    es_print_cli_header();
#ifdef DEBUG





#endif
    EsCommandLineOptions options = es_parse_command_line(argc, argv);
    if (options.show_help) {
        es_print_usage(argv[0]);
        es_flush_output();
        return 0;
    }
    if (options.create_project) {
        int status = es_create_project(options.project_type, options.input_file);
        es_flush_output();
        return status;
    }
    if (options.input_file == NULL) {
        es_print_usage(argv[0]);
        es_flush_output();
        return 1;
    }
    const char* ext = strrchr(options.input_file, '.');
    if (ext && strcmp(ext, ".esproj") == 0) {
        EsProject* project = es_proj_load(options.input_file);
        if (!project) {
            ES_ERROR("无法加载项目文件: %s", options.input_file);
            ES_INFO("请确保项目文件存在且格式正确");
            es_flush_output();
            return 1;
        }




        if (!project->name || project->name[0] == '\0') {
            ES_ERROR("项目文件无效: 项目名称不能为空");
            es_proj_destroy(project);
            es_flush_output();
            return 1;
        }

        const char* project_root = (project->project_root && project->project_root[0]) ? project->project_root : ".";
        char resolved_output[ES_MAX_PATH] = {0};
        const char* output_path = options.output_file;
        if (!output_path || output_path[0] == '\0') {
            const char* extension = (project->type == ES_PROJ_TYPE_LIBRARY) ? ".lib" : ".exe";
            char bin_dir[ES_MAX_PATH];
            main_path_join(bin_dir, sizeof(bin_dir), project_root, "bin");
            es_ensure_directory_recursive(bin_dir);
            char file_name[ES_MAX_PATH];
            snprintf(file_name, sizeof(file_name), "%s%s", project->name, extension);
            main_path_join(resolved_output, sizeof(resolved_output), bin_dir, file_name);
            output_path = resolved_output;
        }

        EsTargetType target = ES_TARGET_CMD_ASM;


        int has_compile_items = 0;
        int compile_item_count = 0;
        EsProjectItem* item = project->items;
        while (item) {
            if (strcmp(item->item_type, "Compile") == 0) {
                has_compile_items = 1;
                compile_item_count++;
            }
            item = item->next;
        }

        if (!has_compile_items) {
            ES_WARNING("项目 '%s' 中没有找到需要编译的源文件", project->name);
            ES_INFO("项目加载成功，但没有需要编译的文件");
            es_proj_destroy(project);
            es_print_build_summary();
            es_flush_output();
            return 0;
        }


        int max_threads = 4;
        ParallelCompiler* parallel_compiler = parallel_compiler_create(max_threads);
        if (!parallel_compiler) {
            ES_ERROR("无法创建并行编译器");
            es_proj_destroy(project);
            es_print_build_summary();
            es_flush_output();
            return 1;
        }


        item = project->items;
        while (item) {
            if (strcmp(item->item_type, "Compile") == 0) {
                char source_path[ES_MAX_PATH];
                es_resolve_project_path(source_path, sizeof(source_path), project_root, item->file_path);


                if (!es_path_exists(source_path)) {
                    ES_ERROR("源文件不存在: %s", source_path);
                    parallel_compiler_destroy(parallel_compiler);
                    es_proj_destroy(project);
                    es_print_build_summary();
                    es_flush_output();
                    return 1;
                }


                char intermediate_file[ES_MAX_PATH];
                char file_name[ES_MAX_PATH];
                es_path_get_filename(source_path, file_name, sizeof(file_name));
                char file_name_no_ext[ES_MAX_PATH];
                es_path_remove_extension(file_name, file_name_no_ext, sizeof(file_name_no_ext));

                char obj_dir[ES_MAX_PATH];
                es_path_join(obj_dir, sizeof(obj_dir), project_root, "obj");
                es_ensure_directory_recursive(obj_dir);

                snprintf(intermediate_file, sizeof(intermediate_file), "%s%c%s.obj", obj_dir, main_path_separator(), file_name_no_ext);


                if (parallel_compiler_add_file(parallel_compiler, source_path, intermediate_file, target, options.show_ir) != 0) {
                    ES_ERROR("添加编译任务失败: %s", source_path);
                    parallel_compiler_destroy(parallel_compiler);
                    es_proj_destroy(project);
                    es_print_build_summary();
                    es_flush_output();
                    return 1;
                }
            }
            item = item->next;
        }


        ES_INFO("开始并行编译，使用 %d 个线程...", max_threads);
        int compile_result = parallel_compiler_execute(parallel_compiler);


        int total_files = 0, succeeded_files = 0, failed_files = 0;
        parallel_compiler_get_stats(parallel_compiler, &total_files, &succeeded_files, &failed_files);

        if (compile_result != 0 || failed_files > 0) {
            ES_ERROR("并行编译失败：%d 个文件编译失败", failed_files);
            parallel_compiler_destroy(parallel_compiler);
            es_proj_destroy(project);
            es_print_build_summary();
            es_flush_output();
            return 1;
        }


        if (total_files > 0) {
            ES_INFO("开始链接阶段...");


            char link_cmd[1024];
            int link_cmd_len = snprintf(link_cmd, sizeof(link_cmd), "gcc obj/*.obj obj/compiler/runtime.o obj/common/output_cache.o -o %s -mconsole", output_path);
            if (link_cmd_len < 0 || (size_t)link_cmd_len >= sizeof(link_cmd)) {
                ES_ERROR("链接命令字符串过长");
                parallel_compiler_destroy(parallel_compiler);
                es_proj_destroy(project);
                es_print_build_summary();
                es_flush_output();
                return 1;
            }
            int link_result = system(link_cmd);
            if (link_result != 0) {
                ES_ERROR("链接失败");
                parallel_compiler_destroy(parallel_compiler);
                es_proj_destroy(project);
                es_print_build_summary();
                es_flush_output();
                return 1;
            }
            es_task_report("link", "project", ES_TASK_RESULT_EXECUTED, 0.0, &g_build_summary.stats);
        }

        parallel_compiler_destroy(parallel_compiler);
        es_proj_destroy(project);
        es_print_build_summary();
        es_flush_output();
        return 0;
    }
    if (options.output_file == NULL) {
        if (options.target_type == ES_TARGET_CMD_EXE) {
            options.output_file = "a.exe";
        } else if (options.target_type == ES_TARGET_CMD_IR) {
            options.output_file = "output.ir";
        } else {
            options.output_file = "output.asm";
        }
    } else {
        const char* ext = strrchr(options.output_file, '.');
        if (ext) {
            if (strcmp(ext, ".exe") == 0) {
                options.target_type = ES_TARGET_CMD_EXE;
            } else if (strcmp(ext, ".ir") == 0) {
                options.target_type = ES_TARGET_CMD_IR;
            } else if (strcmp(ext, ".asm") == 0) {
                options.target_type = ES_TARGET_CMD_ASM;
            }
        }
    }


    int result = es_compile_file(options.input_file, options.output_file, options.target_type, options.show_ir);


    es_print_build_summary();

    es_flush_output();

    return result == 0 ? 0 : 1;
}