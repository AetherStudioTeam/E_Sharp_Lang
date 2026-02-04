#ifndef ES_PLATFORM_ABSTRACTION_H
#define ES_PLATFORM_ABSTRACTION_H

#include "../../core/utils/es_common.h"
#include "../driver/config_manager.h"

typedef struct EsPlatform {
    EsPlatformType type;
    const char* name;
    
    
    char path_separator;
    int (*is_absolute_path)(const char* path);
    int (*path_exists)(const char* path);
    int (*is_directory)(const char* path);
    
    
    int (*supports_color)(void);
    void (*set_utf8)(void);
    
    
    int (*execute_command)(const char* command);
} EsPlatform;


EsPlatform* es_platform_create(EsPlatformType type);
void es_platform_destroy(EsPlatform* platform);


char es_platform_get_separator(EsPlatform* platform);
int es_platform_path_is_absolute(EsPlatform* platform, const char* path);
int es_platform_path_exists(EsPlatform* platform, const char* path);
int es_platform_path_is_directory(EsPlatform* platform, const char* path);
void es_platform_path_join(EsPlatform* platform, char* buffer, size_t buffer_size, const char* base, const char* part);


int es_platform_console_supports_color(EsPlatform* platform);
void es_platform_console_set_utf8(EsPlatform* platform);


int es_platform_execute_command(EsPlatform* platform, const char* command);


EsPlatform* es_platform_get_current(void);

#endif 