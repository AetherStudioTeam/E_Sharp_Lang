#include "ArkLink/arklink.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ark_cli_logger(ArkLogLevel level, const char* message, void* user_data) {
    (void)user_data;
    const char* prefix = "INFO";
    if (level == ARK_LOG_ERROR) prefix = "ERROR";
    else if (level == ARK_LOG_WARN) prefix = "WARN";
    else if (level == ARK_LOG_DEBUG) prefix = "DEBUG";
    fprintf(stderr, "[%s] %s\n", prefix, message);
}

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options] input1.eo [input2.eo ...]\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --target pe|elf          Target format (default: pe)\n");
    fprintf(stderr, "  -o output                Output file path (required)\n");
    fprintf(stderr, "  -L path                  Add library search path\n");
    fprintf(stderr, "  -l name                  Link with static library\n");
    fprintf(stderr, "  --import mod:sym         Import symbol from module (e.g., kernel32:ExitProcess)\n");
    fprintf(stderr, "  --runtime NAME           Runtime library name\n");
    fprintf(stderr, "  --import-config path     Import configuration JSON file\n");
    fprintf(stderr, "  --config-json json       Inline JSON configuration\n");
    fprintf(stderr, "  --config-file path       Configuration file path\n");
    fprintf(stderr, "  --verbose                Enable verbose output\n");
    fprintf(stderr, "  --quiet                  Suppress output\n");
    fprintf(stderr, "  -h, --help               Show this help message\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s --target pe -o app.exe main.eo\n", prog);
    fprintf(stderr, "  %s --target pe -o app.exe main.eo --import kernel32:ExitProcess\n", prog);
    fprintf(stderr, "  %s --target elf -o app --config-file config.json main.eo lib.eo\n", prog);
}

static void print_config(const ArkRuntimeConfigParsed* config) {
    if (!config) return;
    
    fprintf(stderr, "[CONFIG] Runtime: %s\n", config->runtime_name ? config->runtime_name : "(default)");
    fprintf(stderr, "[CONFIG] Subsystem: %s\n", 
        config->subsystem == ARK_SUBSYSTEM_WINDOWS ? "windows" : "console");
    fprintf(stderr, "[CONFIG] Image Base: 0x%llX\n", (unsigned long long)config->image_base);
    fprintf(stderr, "[CONFIG] Stack Size: 0x%llX\n", (unsigned long long)config->stack_size);
    fprintf(stderr, "[CONFIG] Entry Point: %s\n", config->entry_point ? config->entry_point : "(default)");
    fprintf(stderr, "[CONFIG] Imports: %zu\n", config->imports_count);
    
    for (size_t i = 0; i < config->imports_count; i++) {
        fprintf(stderr, "[CONFIG]   Import %zu: %s from %s (slot %u)\n",
            i,
            config->imports[i].symbol ? config->imports[i].symbol : "(null)",
            config->imports[i].module ? config->imports[i].module : "(null)",
            config->imports[i].slot);
    }
}

int main(int argc, char** argv) {
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] ArkLink starting, argc=%d\n", argc);
    fflush(stderr);
#endif
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    ArkLinkTarget target = ARK_LINK_TARGET_PE;
    const char* output = NULL;
    const char** inputs = (const char**)calloc((size_t)argc, sizeof(char*));
    size_t input_count = 0;
    const char** lib_paths = (const char**)calloc((size_t)argc, sizeof(char*));
    size_t lib_path_count = 0;
    const char** libs = (const char**)calloc((size_t)argc, sizeof(char*));
    size_t lib_count = 0;
    const char** cli_imports = (const char**)calloc((size_t)argc, sizeof(char*));
    size_t cli_import_count = 0;
    const char* runtime_name = NULL;
    const char* import_config_path = NULL;
    const char* config_json = NULL;
    const char* config_file = NULL;
    ArkLinkLogger logger = ark_cli_logger;
    uint32_t flags = 0;
    int show_help = 0;

    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "pe") == 0) target = ARK_LINK_TARGET_PE;
            else if (strcmp(argv[i], "elf") == 0) target = ARK_LINK_TARGET_ELF;
            else {
                fprintf(stderr, "Unknown target: %s\n", argv[i]);
                free(inputs);
                return 1;
            }
            i++;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
            i++;
        } else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
            lib_paths[lib_path_count++] = argv[++i];
            i++;
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            libs[lib_count++] = argv[++i];
            i++;
        } else if (strcmp(argv[i], "--import") == 0 && i + 1 < argc) {
            cli_imports[cli_import_count++] = argv[++i];
            i++;
        } else if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
            runtime_name = argv[++i];
            i++;
        } else if (strcmp(argv[i], "--import-config") == 0 && i + 1 < argc) {
            import_config_path = argv[++i];
            i++;
        } else if (strcmp(argv[i], "--config-json") == 0 && i + 1 < argc) {
            config_json = argv[++i];
            i++;
        } else if (strcmp(argv[i], "--config-file") == 0 && i + 1 < argc) {
            config_file = argv[++i];
            i++;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            flags |= ARK_LINK_FLAG_VERBOSE;
            flags &= ~ARK_LINK_FLAG_QUIET;
            i++;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            flags |= ARK_LINK_FLAG_QUIET;
            flags &= ~ARK_LINK_FLAG_VERBOSE;
            logger = NULL;
            i++;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help = 1;
            i++;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            free(inputs);
            return 1;
        } else {
            inputs[input_count++] = argv[i];
            i++;
        }
    }
    
    if (show_help) {
        print_usage(argv[0]);
        free(inputs);
        return 0;
    }
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] Parsed args: output=%s, input_count=%zu\n", output ? output : "(null)", input_count);
    fflush(stderr);
#endif
    
    if (!output || input_count == 0) {
        fprintf(stderr, "Error: Output path and at least one input file are required\n\n");
        print_usage(argv[0]);
        free(inputs);
        return 1;
    }

    
    ArkRuntimeConfigParsed* parsed_config = NULL;
    
    if (config_file) {
        parsed_config = ark_config_load_file(config_file);
        if (!parsed_config) {
            fprintf(stderr, "Error: Failed to load configuration file: %s\n", config_file);
            free(inputs);
            return 1;
        }
        if (flags & ARK_LINK_FLAG_VERBOSE) {
            fprintf(stderr, "[INFO] Loaded configuration from: %s\n", config_file);
        }
    } else if (config_json) {
        parsed_config = ark_config_parse_json(config_json);
        if (!parsed_config) {
            fprintf(stderr, "Error: Failed to parse inline JSON configuration\n");
            free(inputs);
            return 1;
        }
        if (flags & ARK_LINK_FLAG_VERBOSE) {
            fprintf(stderr, "[INFO] Using inline JSON configuration\n");
        }
    } else if (import_config_path) {
        parsed_config = ark_config_load_file(import_config_path);
        if (!parsed_config) {
            fprintf(stderr, "Error: Failed to load import configuration: %s\n", import_config_path);
            free(inputs);
            return 1;
        }
    }
    
    
    if (runtime_name && parsed_config) {
        free(parsed_config->runtime_name);
        parsed_config->runtime_name = strdup(runtime_name);
    }
    
    
    if ((flags & ARK_LINK_FLAG_VERBOSE) && parsed_config) {
        print_config(parsed_config);
    }
    
    
    ArkLinkInput* job_inputs = (ArkLinkInput*)calloc(input_count, sizeof(ArkLinkInput));
    for (size_t idx = 0; idx < input_count; ++idx) {
        job_inputs[idx].path = inputs[idx];
    }
    
    
    ArkLinkRuntimeConfig runtime = {
        .runtime_name = parsed_config ? parsed_config->runtime_name : runtime_name,
        .import_config_path = import_config_path,
        .config_json = config_json,
        .entry_point = parsed_config ? parsed_config->entry_point : NULL,
        .subsystem = parsed_config ? (ArkSubsystem)parsed_config->subsystem : ARK_SUBSYSTEM_CONSOLE,
        .image_base = parsed_config ? parsed_config->image_base : 0,
        .stack_size = parsed_config ? parsed_config->stack_size : 0,
        .imports = NULL,
        .import_count = 0,
        .library_paths = lib_paths,
        .library_path_count = lib_path_count,
        .libraries = libs,
        .library_count = lib_count,
        .export_symbols = NULL,
        .export_symbol_count = 0
    };

    
    size_t config_import_count = parsed_config ? parsed_config->imports_count : 0;
    size_t total_import_count = config_import_count + cli_import_count;
    
    
    if (total_import_count > 0) {
        runtime.imports = (ArkLinkImportEntry*)calloc(total_import_count, sizeof(ArkLinkImportEntry));
        runtime.import_count = total_import_count;
        
        
        for (size_t i = 0; i < config_import_count; i++) {
            ((ArkLinkImportEntry*)runtime.imports)[i].module = parsed_config->imports[i].module;
            ((ArkLinkImportEntry*)runtime.imports)[i].symbol = parsed_config->imports[i].symbol;
            ((ArkLinkImportEntry*)runtime.imports)[i].slot = parsed_config->imports[i].slot;
        }
        
        
        for (size_t i = 0; i < cli_import_count; i++) {
            char* import_str = strdup(cli_imports[i]);
            char* sep = strchr(import_str, '=');
            if (!sep) {
                sep = strchr(import_str, ':');
            }
            if (sep) {
                *sep = '\0';
                char* symbol = import_str;
                char* module = sep + 1;
                
                
                size_t module_len = strlen(module);
                char* module_name = malloc(module_len + 5);  
                if (!module_name) {
                    free(import_str);
                    free(inputs);
                    return 1;
                }
                memcpy(module_name, module, module_len + 1);
                
                
                if (module_len < 4 || strcasecmp(module + module_len - 4, ".dll") != 0) {
                    memcpy(module_name + module_len, ".dll", 5);  
                }
                
                ((ArkLinkImportEntry*)runtime.imports)[config_import_count + i].module = module_name;
                ((ArkLinkImportEntry*)runtime.imports)[config_import_count + i].symbol = strdup(symbol);
                ((ArkLinkImportEntry*)runtime.imports)[config_import_count + i].slot = (uint32_t)(config_import_count + i);
                
                if (flags & ARK_LINK_FLAG_VERBOSE) {
                    fprintf(stderr, "[INFO] Adding import: %s from %s\n", symbol, module_name);
                }
            }
            free(import_str);
        }
    }
    
    ArkLinkJob job = {
        .inputs = job_inputs,
        .input_count = input_count,
        .output = output,
        .target = target,
        .output_kind = ARK_LINK_OUTPUT_EXECUTABLE, 
        .runtime = runtime,
        .library_paths = lib_paths,
        .library_path_count = lib_path_count,
        .libraries = libs,
        .library_count = lib_count,
        .logger = logger,
        .logger_user_data = parsed_config, 
        .flags = flags,
    };
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] About to call arklink_link\n");
    fflush(stderr);
#endif
    
    if (flags & ARK_LINK_FLAG_VERBOSE) {
        fprintf(stderr, "[INFO] Linking %zu input(s) to %s\n", input_count, output);
        fprintf(stderr, "[INFO] Target: %s\n", target == ARK_LINK_TARGET_PE ? "PE" : "ELF");
    }
    
    ArkLinkResult result = arklink_link(&job);
    
    if (result != ARK_LINK_OK) {
        fprintf(stderr, "arklink failed: %s\n", arklink_error_string(result));
    } else if (flags & ARK_LINK_FLAG_VERBOSE) {
        fprintf(stderr, "[INFO] Successfully created: %s\n", output);
    }
    
    
    if (runtime.imports) {
        
        for (size_t i = config_import_count; i < total_import_count; i++) {
            free((void*)((ArkLinkImportEntry*)runtime.imports)[i].module);
            free((void*)((ArkLinkImportEntry*)runtime.imports)[i].symbol);
        }
        free((void*)runtime.imports);
    }
    free(job_inputs);
    free(inputs);
    free(lib_paths);
    free(libs);
    free(cli_imports);
    ark_config_free(parsed_config);
    
    return result == ARK_LINK_OK ? 0 : 1;
}
