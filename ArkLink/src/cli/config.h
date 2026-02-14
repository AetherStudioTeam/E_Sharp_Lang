#ifndef ARKLINK_CLI_CONFIG_H
#define ARKLINK_CLI_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include "ArkLink/arklink.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* module;
    char* symbol;
    uint32_t slot;
} ArkImportConfig;

typedef struct {
    char* runtime_name;
    ArkSubsystem subsystem;
    char* entry_point;
    uint64_t image_base;
    uint64_t stack_size;
    
    ArkImportConfig* imports;
    size_t imports_count;
    size_t imports_capacity;
} ArkRuntimeConfigParsed;


ArkRuntimeConfigParsed* ark_config_parse_json(const char* json_str);


ArkRuntimeConfigParsed* ark_config_load_file(const char* path);


void ark_config_free(ArkRuntimeConfigParsed* config);

#ifdef __cplusplus
}
#endif

#endif 
