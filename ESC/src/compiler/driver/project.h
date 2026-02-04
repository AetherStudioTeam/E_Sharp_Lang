#ifndef ES_PROJ_H
#define ES_PROJ_H

#include "../../core/utils/es_common.h"




typedef enum {
    ES_PROJ_TYPE_CONSOLE,
    ES_PROJ_TYPE_LIBRARY,
    ES_PROJ_TYPE_WEB,
    ES_PROJ_TYPE_SYSTEM
} EsProjectType;

typedef enum {
    ES_PROJ_CONFIG_DEBUG,
    ES_PROJ_CONFIG_RELEASE,
    ES_PROJ_CONFIG_CUSTOM
} EsProjectConfig;


typedef struct EsProjectDependency {
    char* name;
    char* version;
    char* path;
    struct EsProjectDependency* next;
} EsProjectDependency;


typedef struct EsProjectItem {
    char* file_path;
    char* item_type;
    struct EsProjectItem* next;
} EsProjectItem;


typedef struct EsProjectPropertyGroup {
    EsProjectConfig config;
    char* output_path;
    char* intermediate_path;
    char* target_name;
    char* defines;
    char* include_paths;
    int optimize;
    int debug_symbols;
    struct EsProjectPropertyGroup* next;
} EsProjectPropertyGroup;


typedef struct EsProject {
    char* name;
    char* version;
    EsProjectType type;
    char* output_type;
    char* root_namespace;
    char* description;


    EsProjectItem* items;
    EsProjectDependency* dependencies;
    EsProjectPropertyGroup* property_groups;


    char* sdk_version;
    char* created_date;
    char* modified_date;


    char* project_root;
} EsProject;


EsProject* es_proj_create(const char* name, EsProjectType type);
void es_proj_destroy(EsProject* project);


EsProject* es_proj_load(const char* proj_file);
int es_proj_save(EsProject* project, const char* proj_file);


void es_proj_add_file(EsProject* project, const char* file_path, const char* item_type);
void es_proj_remove_file(EsProject* project, const char* file_path);
void es_proj_add_dependency(EsProject* project, const char* name, const char* version, const char* path);


char** es_proj_get_source_files(EsProject* project, int* count);
char* es_proj_get_output_path(EsProject* project, EsProjectConfig config);
char* es_proj_get_intermediate_path(EsProject* project, EsProjectConfig config);


int es_proj_create_template(EsProject* project, const char* output_dir);
char** es_proj_get_templates(int* count);

#endif