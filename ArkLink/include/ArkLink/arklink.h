#ifndef ARKLINK_ARKLINK_H
#define ARKLINK_ARKLINK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif





enum {
    ARK_LINK_FLAG_VERBOSE = 1u << 0,
    ARK_LINK_FLAG_QUIET = 1u << 1,
    ARK_LINK_FLAG_INCREMENTAL = 1u << 2,
    ARK_LINK_FLAG_LTO = 1u << 3,
    ARK_LINK_FLAG_STRIP = 1u << 4,
};





typedef enum ArkLinkTarget {
    ARK_LINK_TARGET_PE,
    ARK_LINK_TARGET_ELF,
} ArkLinkTarget;

typedef enum ArkLinkOutputKind {
    ARK_LINK_OUTPUT_EXECUTABLE,
    ARK_LINK_OUTPUT_SHARED_LIBRARY,
    ARK_LINK_OUTPUT_STATIC_LIBRARY,
} ArkLinkOutputKind;

typedef enum ArkLinkResult {
    ARK_LINK_OK = 0,
    ARK_LINK_ERR_INVALID_ARGUMENT,
    ARK_LINK_ERR_IO,
    ARK_LINK_ERR_FORMAT,
    ARK_LINK_ERR_UNRESOLVED_SYMBOL,
    ARK_LINK_ERR_BACKEND,
    ARK_LINK_ERR_INTERNAL,
    ARK_LINK_ERR_UNSUPPORTED,
    ARK_LINK_ERR_NOT_FOUND,
    ARK_LINK_ERR_MEMORY,
} ArkLinkResult;

typedef enum ArkLogLevel {
    ARK_LOG_ERROR,
    ARK_LOG_WARN,
    ARK_LOG_INFO,
    ARK_LOG_DEBUG,
} ArkLogLevel;

typedef enum ArkSubsystem {
    ARK_SUBSYSTEM_CONSOLE = 0,
    ARK_SUBSYSTEM_WINDOWS = 1,
} ArkSubsystem;





typedef void (*ArkLinkLogger)(ArkLogLevel level, const char* message, void* user_data);





typedef struct ArkLinkImportEntry {
    const char* module;     
    const char* symbol;     
    uint32_t slot;          
} ArkLinkImportEntry;

typedef struct ArkLinkRuntimeConfig {
    const char* runtime_name;           
    const char* import_config_path;     
    const char* config_json;            
    const char* entry_point;            
    ArkSubsystem subsystem;             
    uint64_t image_base;                
    uint64_t stack_size;                
    const ArkLinkImportEntry* imports;  
    size_t import_count;                
    const char** library_paths;         
    size_t library_path_count;          
    const char** libraries;             
    size_t library_count;               
    const char** export_symbols;        
    size_t export_symbol_count;         
} ArkLinkRuntimeConfig;





typedef struct ArkLinkInput {
    const char* path;       
    const void* data;       
    size_t size;            
} ArkLinkInput;

typedef struct ArkLinkJob {
    const ArkLinkInput* inputs;
    size_t input_count;
    const char* output;
    ArkLinkTarget target;
    ArkLinkOutputKind output_kind;
    ArkLinkRuntimeConfig runtime;
    ArkLinkLogger logger;
    void* logger_user_data;
    uint32_t flags;
    const char** library_paths;
    size_t library_path_count;
    const char** libraries;
    size_t library_count;
} ArkLinkJob;






ArkLinkResult arklink_link(const ArkLinkJob* job);


const char* arklink_error_string(ArkLinkResult code);






typedef struct ArkLinkSession ArkLinkSession;


ArkLinkSession* arklink_session_create(void);


void arklink_session_destroy(ArkLinkSession* session);


ArkLinkResult arklink_session_set_target(ArkLinkSession* session, ArkLinkTarget target);
ArkLinkResult arklink_session_set_output(ArkLinkSession* session, const char* output_path);
ArkLinkResult arklink_session_set_output_kind(ArkLinkSession* session, ArkLinkOutputKind kind);
ArkLinkResult arklink_session_set_entry_point(ArkLinkSession* session, const char* entry_point);
ArkLinkResult arklink_session_set_subsystem(ArkLinkSession* session, ArkSubsystem subsystem);
ArkLinkResult arklink_session_set_image_base(ArkLinkSession* session, uint64_t image_base);
ArkLinkResult arklink_session_set_stack_size(ArkLinkSession* session, uint64_t stack_size);
ArkLinkResult arklink_session_set_logger(ArkLinkSession* session, ArkLinkLogger logger, void* user_data);
ArkLinkResult arklink_session_set_flags(ArkLinkSession* session, uint32_t flags);


ArkLinkResult arklink_session_add_input(ArkLinkSession* session, const char* path);
ArkLinkResult arklink_session_add_input_memory(ArkLinkSession* session, const char* name,
                                                const void* data, size_t size);


ArkLinkResult arklink_session_add_import(ArkLinkSession* session,
                                          const char* module, const char* symbol, uint32_t slot);


ArkLinkResult arklink_session_add_library_path(ArkLinkSession* session, const char* path);
ArkLinkResult arklink_session_add_library(ArkLinkSession* session, const char* name);


ArkLinkResult arklink_session_add_export(ArkLinkSession* session, const char* symbol);


ArkLinkResult arklink_session_load_config(ArkLinkSession* session, const char* config_path);


ArkLinkResult arklink_session_link(ArkLinkSession* session);


const char* arklink_session_get_error(const ArkLinkSession* session);

#ifdef __cplusplus
}
#endif

#endif 
