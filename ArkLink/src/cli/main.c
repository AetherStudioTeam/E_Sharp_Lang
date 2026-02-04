


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ArkLink/linker.h"

#define VERSION "1.0.0"

static void print_version(void) {
    printf("ArkLink version %s\n", VERSION);
    printf("E# Language Linker - https://github.com/AetherStudioTeam/E_Sharp_Lang/\n");
}

static void print_usage(const char* program) {
    printf("Usage: %s [options] <input.eo>... -o <output>\n", program);
    printf("\nOptions:\n");
    printf("  -o <file>       Output filename (default: a.exe)\n");
    printf("  -target <type>  Target platform (pe/elf, default: pe)\n");
    printf("  -subsystem <n>  Subsystem (console/gui)\n");
    printf("  -base <addr>    Preferred base address (default: 0x140000000)\n");
    printf("  -L <path>       Library search path\n");
    printf("  -l <lib>        Link with library\n");
    printf("  -nostdlib       Do not link default runtime library\n");
    printf("  -runtime <path> Use custom runtime library\n");
    printf("  -v, --version   Show version\n");
    printf("  -h, --help      Show this help\n");
    printf("\nExamples:\n");
    printf("  %s program.eo -o program.exe\n", program);
    printf("  %s main.eo lib1.eo lib2.eo -o app.exe -subsystem gui\n", program);
    printf("  %s program.eo -o program -target elf\n", program);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    ArkLinkerConfig config = {0};
    config.output_file = "a.exe";
    config.target = ArkLink_TARGET_PE;     
    config.subsystem = 3;  
    config.preferred_base = 0x140000000ULL;
    config.major_os_version = 6;
    config.minor_os_version = 0;
    config.no_default_runtime = false;
    config.code_align_log2 = 4;           
    config.data_align_log2 = 3;           
    config.section_alignment = 0x1000;    
    config.file_alignment = 0x200;        
    
    char** input_files = malloc(argc * sizeof(char*));
    int input_count = 0;
    
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            free(input_files);
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            free(input_files);
            return 0;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            config.output_file = argv[++i];
        }
        else if (strcmp(argv[i], "-target") == 0 && i + 1 < argc) {
            const char* target = argv[++i];
            if (strcmp(target, "pe") == 0) {
                config.target = ArkLink_TARGET_PE;
            } else if (strcmp(target, "elf") == 0) {
                config.target = ArkLink_TARGET_ELF;
                
                if (config.preferred_base == 0x140000000ULL) {
                    config.preferred_base = 0x400000ULL;
                }
            } else {
                fprintf(stderr, "Error: Unknown target '%s'. Use 'pe' or 'elf'.\n", target);
                free(input_files);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-subsystem") == 0 && i + 1 < argc) {
            const char* sub = argv[++i];
            if (strcmp(sub, "console") == 0) {
                config.subsystem = 3;
            } else if (strcmp(sub, "gui") == 0) {
                config.subsystem = 2;
                config.is_gui = true;
            } else {
                config.subsystem = atoi(sub);
            }
        }
        else if (strcmp(argv[i], "-base") == 0 && i + 1 < argc) {
            config.preferred_base = strtoull(argv[++i], NULL, 0);
        }
        else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
            
            i++;
        }
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            
            i++;
        }
        else if (strcmp(argv[i], "-nostdlib") == 0) {
            config.no_default_runtime = true;
        }
        else if (strcmp(argv[i], "-runtime") == 0 && i + 1 < argc) {
            config.runtime_lib = argv[++i];
        }
        else if (argv[i][0] != '-') {
            input_files[input_count++] = argv[i];
        }
        else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            free(input_files);
            return 1;
        }
    }
    
    if (input_count == 0) {
        fprintf(stderr, "Error: No input files\n");
        free(input_files);
        return 1;
    }
    
    print_version();
    printf("\n");
    
    
    ArkLinker* linker = es_linker_create(&config);
    if (!linker) {
        fprintf(stderr, "Error: Failed to create linker\n");
        free(input_files);
        return 1;
    }
    
    
    for (int i = 0; i < input_count; i++) {
        printf("[%d/%d] Adding: %s\n", i + 1, input_count, input_files[i]);
        if (!es_linker_add_file(linker, input_files[i])) {
            fprintf(stderr, "Error: %s\n", es_linker_get_error(linker));
            es_linker_destroy(linker);
            free(input_files);
            return 1;
        }
    }
    
    
    if (!config.no_default_runtime) {
        const char* runtime_path = config.runtime_lib ? config.runtime_lib : "runtime/es_runtime.eo";
        
        
        bool runtime_already_added = false;
        for (int i = 0; i < input_count; i++) {
            if (strstr(input_files[i], "es_runtime") != NULL) {
                runtime_already_added = true;
                break;
            }
        }
        
        if (!runtime_already_added) {
            printf("[%d/%d] Adding runtime: %s\n", input_count + 1, input_count + 1, runtime_path);
            if (!es_linker_add_file(linker, runtime_path)) {
                fprintf(stderr, "Warning: Failed to load runtime library: %s\n", 
                        es_linker_get_error(linker));
                fprintf(stderr, "  Continuing without runtime...\n");
            }
        }
    }
    
    
    if (linker->import_count == 0) {
        printf("Adding kernel32 imports...\n");
        if (!es_linker_add_kernel32_imports(linker)) {
            fprintf(stderr, "Warning: Failed to add kernel32 imports\n");
        }
    }
    
    printf("\nLinking...\n");
    printf("  Code:    %u bytes\n", linker->sections[EO_SEC_CODE].file_size);
    printf("  Data:    %u bytes\n", linker->sections[EO_SEC_DATA].file_size);
    printf("  Rodata:  %u bytes\n", linker->sections[EO_SEC_RODATA].file_size);
    printf("  BSS:     %u bytes\n", linker->sections[EO_SEC_BSS].mem_size);
    printf("  Symbols: %d\n", linker->symbol_count);
    printf("  Imports: %d DLLs\n", linker->import_count);
    
    
    if (!es_linker_link(linker)) {
        fprintf(stderr, "Error: Link failed - %s\n", es_linker_get_error(linker));
        es_linker_destroy(linker);
        free(input_files);
        return 1;
    }
    
    printf("\nOutput: %s\n", config.output_file);
    printf("Success!\n");
    
    es_linker_destroy(linker);
    free(input_files);
    
    return 0;
}
