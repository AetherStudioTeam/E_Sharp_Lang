#ifndef ES_CONFIG_MANAGER_H
#define ES_CONFIG_MANAGER_H

#include "../../core/utils/es_common.h"

#ifndef ES_MAX_PATH
#define ES_MAX_PATH 1024
#endif

typedef enum {
    ES_TARGET_ASM,      
    ES_TARGET_IR,       
    ES_TARGET_EXE,      
    ES_TARGET_VM,       
    ES_TARGET_EO        
} EsTargetType;

typedef enum {
    ES_CONFIG_PLATFORM_WINDOWS,
    ES_CONFIG_PLATFORM_LINUX,
    ES_CONFIG_PLATFORM_UNKNOWN
} EsPlatformType;

typedef struct {
    const char* platform_name;
    const char* const* object_files;
    int object_count;
    const char* const* end_objects;
    int end_object_count;
    const char* const* library_paths;
    int library_path_count;
    const char* const* libraries;
    int library_count;
    const char* entry_point;
    const char* subsystem;
    const char* dynamic_linker;
} EsLinkerConfig;

typedef struct {
    
    EsTargetType target_type;
    EsPlatformType platform;
    const char* input_file;
    const char* output_file;
    
    
    int show_ir;
    int show_help;
    int create_project;
    int keep_temp_files;
    const char* project_type;
    
    
    const EsLinkerConfig* linker_config;
    
    
    int color_enabled;
    char temp_asm_file[ES_MAX_PATH];
    char temp_obj_file[ES_MAX_PATH];
} EsConfig;


EsConfig* es_config_create(void);
void es_config_destroy(EsConfig* config);


EsConfig* es_config_parse_command_line(int argc, char* argv[]);
int es_config_validate(EsConfig* config);


EsPlatformType es_config_detect_platform(void);
const EsLinkerConfig* es_config_get_linker_config(EsPlatformType platform);


const char* es_config_get_default_output(EsConfig* config);
int es_config_needs_linking(EsConfig* config);


const char* project_name_from_type(const char* project_type);
int es_create_project(const char* project_name, const char* project_type);

#endif 