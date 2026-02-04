#include <string.h>


extern void* memset(void* s, int c, size_t n);
extern int strcmp(const char* s1, const char* s2);

#include "config_manager.h"
#include "../../core/utils/logger.h"
#include "../../core/utils/path.h"
#include "compiler/platform/platform_abstraction.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define ES_PLATFORM_DEFAULT ES_CONFIG_PLATFORM_WINDOWS
#else
#include <unistd.h>
#define ES_PLATFORM_DEFAULT ES_CONFIG_PLATFORM_LINUX
#endif


static const char* g_win_object_files[] = {
    NULL
};

static const char* g_win_end_objects[] = {
    NULL
};

static const char* g_win_library_paths[] = {
    "C:/msys64/mingw64/lib",
    "C:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/15.2.0"
};

static const char* g_win_libraries[] = {
    ":libgcc.a", ":libgcc_eh.a", "moldname", "mingw32", "mingwex", "msvcrt", 
    "kernel32", "user32", "gdi32", "ws2_32", "advapi32", "shell32", "ole32", 
    "uuid", "oleaut32", "comdlg32", "comctl32", "psapi", "version", "winmm"
};


static const char* g_linux_object_files[] = {
    NULL
};

static const char* g_linux_libraries[] = {
    "pthread", "m"
};


static const EsLinkerConfig g_linker_configs[] = {
    {
        "Windows",
        g_win_object_files, sizeof(g_win_object_files) / sizeof(char*),
        g_win_end_objects, sizeof(g_win_end_objects) / sizeof(char*),
        g_win_library_paths, sizeof(g_win_library_paths) / sizeof(char*),
        g_win_libraries, sizeof(g_win_libraries) / sizeof(char*),
        "mainCRTStartup", "console", NULL
    },
    {
        "Linux",
        NULL, 0,
        NULL, 0,
        NULL, 0,
        g_linux_libraries, sizeof(g_linux_libraries) / sizeof(char*),
        "main", NULL, NULL
    }
};

EsConfig* es_config_create(void) {
    EsConfig* config = (EsConfig*)ES_MALLOC(sizeof(EsConfig));
    if (!config) return NULL;
    
    memset(config, 0, sizeof(EsConfig));
    config->target_type = ES_TARGET_ASM;
    config->platform = es_config_detect_platform();
    config->linker_config = es_config_get_linker_config(config->platform);
    config->keep_temp_files = 1;
    
    return config;
}

void es_config_destroy(EsConfig* config) {
    if (!config) return;
    ES_FREE(config);
}

EsPlatformType es_config_detect_platform(void) {
#ifdef _WIN32
    return ES_CONFIG_PLATFORM_WINDOWS;
#else
    return ES_CONFIG_PLATFORM_LINUX;
#endif
}

const EsLinkerConfig* es_config_get_linker_config(EsPlatformType platform) {
    switch (platform) {
        case ES_CONFIG_PLATFORM_WINDOWS:
            return &g_linker_configs[0];
        case ES_CONFIG_PLATFORM_LINUX:
            return &g_linker_configs[1];
        default:
            return &g_linker_configs[1]; 
    }
}

const char* project_name_from_type(const char* project_type) {
    if (!project_type) return "MyProject";
    if (strcmp(project_type, "console") == 0) return "ConsoleApp";
    if (strcmp(project_type, "library") == 0) return "Library";
    if (strcmp(project_type, "web") == 0) return "WebApp";
    if (strcmp(project_type, "system") == 0) return "SystemProject";
    return project_type;
}

const char* es_config_get_default_output(EsConfig* config) {
    if (!config) return "output";
    
    switch (config->target_type) {
        case ES_TARGET_EXE:
            return (config->platform == ES_CONFIG_PLATFORM_WINDOWS) ? "a.exe" : "a";
        case ES_TARGET_IR:
            return "output.ir";
        case ES_TARGET_ASM:
        default:
            return "output.asm";
    }
}

int es_config_needs_linking(EsConfig* config) {
    return config && config->target_type == ES_TARGET_EXE;
}

int es_config_validate(EsConfig* config) {
    if (!config) return 0;
    
    if (!config->create_project && !config->input_file) {
        ES_ERROR("未指定输入文件");
        return 0;
    }
    
    return 1;
}


int es_create_project(const char* project_name, const char* project_type) {
    EsPlatform* platform = es_platform_get_current();
    if (!platform) {
        ES_ERROR("无法创建平台抽象");
        return 1;
    }

    if (es_ensure_directory_recursive(project_name) != 0) {
        es_platform_destroy(platform);
        return 1;
    }

    char project_file[ES_MAX_PATH];
    snprintf(project_file, sizeof(project_file), "%s/project.esproj", project_name);

    FILE* fp = fopen(project_file, "w");
    if (!fp) {
        ES_ERROR("无法创建项目文件: %s", project_file);
        es_platform_destroy(platform);
        return 1;
    }

    fprintf(fp, "<Project>\n");
    fprintf(fp, "  <PropertyGroup>\n");
    fprintf(fp, "    <OutputType>%s</OutputType>\n", project_type);
    fprintf(fp, "    <TargetFramework>es1.0.0</TargetFramework>\n");
    fprintf(fp, "  </PropertyGroup>\n");
    fprintf(fp, "  <ItemGroup>\n");
    fprintf(fp, "    <Compile Include=\"main.es\" />\n");
    fprintf(fp, "  </ItemGroup>\n");
    fprintf(fp, "</Project>\n");
    fclose(fp);

    char main_file[ES_MAX_PATH];
    snprintf(main_file, sizeof(main_file), "%s/main.es", project_name);

    fp = fopen(main_file, "w");
    if (!fp) {
        ES_ERROR("无法创建主文件: %s", main_file);
        es_platform_destroy(platform);
        return 1;
    }

    if (strcmp(project_type, "console") == 0) {
        fprintf(fp, "function int main() {\n");
        fprintf(fp, "    println(\"Hello, E# World!\");\n");
        fprintf(fp, "    return 0;\n");
        fprintf(fp, "}\n");
    } else if (strcmp(project_type, "system") == 0) {
        fprintf(fp, "function int main() {\n");
        fprintf(fp, "    // System programming example\n");
        fprintf(fp, "    return 0;\n");
        fprintf(fp, "}\n");
    } else {
        fprintf(fp, "// Library code\n");
    }

    fclose(fp);
    es_platform_destroy(platform);

    return 0;
}