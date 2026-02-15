#include "arklink_integration.h"
#include "../../core/utils/es_common.h"
#include "../../core/utils/logger.h"
#include "../../core/utils/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

struct EsArkLinkContext {
    ArkLinkSession* session;
    EsConfig* config;
    char** obj_files;
    int obj_count;
    int obj_capacity;
};

static void arklink_logger_callback(ArkLogLevel level, const char* message, void* user_data) {
    (void)user_data;
    switch (level) {
        case ARK_LOG_ERROR:
            ES_ERROR("[ArkLink] %s", message);
            break;
        case ARK_LOG_WARN:
            ES_WARNING("[ArkLink] %s", message);
            break;
        case ARK_LOG_INFO:
            ES_INFO("[ArkLink] %s", message);
            break;
        case ARK_LOG_DEBUG:
            ES_DEBUG("[ArkLink] %s", message);
            break;
    }
}

EsArkLinkContext* es_arklink_context_create(EsConfig* config) {
    if (!config) return NULL;
    
    EsArkLinkContext* ctx = (EsArkLinkContext*)ES_MALLOC(sizeof(EsArkLinkContext));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(EsArkLinkContext));
    ctx->config = config;
    ctx->obj_capacity = 64;
    ctx->obj_files = (char**)ES_MALLOC(ctx->obj_capacity * sizeof(char*));
    
    if (!ctx->obj_files) {
        ES_FREE(ctx);
        return NULL;
    }
    
    ctx->session = arklink_session_create();
    if (!ctx->session) {
        ES_FREE(ctx->obj_files);
        ES_FREE(ctx);
        return NULL;
    }
    
    arklink_session_set_logger(ctx->session, arklink_logger_callback, ctx);
    
    ArkLinkTarget target = (config->platform == ES_CONFIG_PLATFORM_WINDOWS) 
                          ? ARK_LINK_TARGET_PE 
                          : ARK_LINK_TARGET_ELF;
    arklink_session_set_target(ctx->session, target);
    
    arklink_session_set_output_kind(ctx->session, ARK_LINK_OUTPUT_EXECUTABLE);
    
    const EsLinkerConfig* linker_config = es_config_get_linker_config(config->platform);
    if (linker_config && linker_config->entry_point) {
        arklink_session_set_entry_point(ctx->session, linker_config->entry_point);
    } else {
        arklink_session_set_entry_point(ctx->session, "main");
    }
    
    if (config->platform == ES_CONFIG_PLATFORM_WINDOWS) {
        arklink_session_set_subsystem(ctx->session, ARK_SUBSYSTEM_CONSOLE);
        arklink_session_set_image_base(ctx->session, 0x140000000ULL);
    } else {
        arklink_session_set_image_base(ctx->session, 0x400000ULL);
    }
    
    arklink_session_set_stack_size(ctx->session, 0x100000);
    
    return ctx;
}

void es_arklink_context_destroy(EsArkLinkContext* ctx) {
    if (!ctx) return;
    
    if (ctx->session) {
        arklink_session_destroy(ctx->session);
    }
    
    if (ctx->obj_files) {
        for (int i = 0; i < ctx->obj_count; i++) {
            ES_FREE(ctx->obj_files[i]);
        }
        ES_FREE(ctx->obj_files);
    }
    
    ES_FREE(ctx);
}

int es_arklink_add_object_file(EsArkLinkContext* ctx, const char* obj_path) {
    if (!ctx || !obj_path) return -1;
    
    if (ctx->obj_count >= ctx->obj_capacity) {
        int new_capacity = ctx->obj_capacity * 2;
        char** new_files = (char**)ES_REALLOC(ctx->obj_files, new_capacity * sizeof(char*));
        if (!new_files) return -1;
        ctx->obj_files = new_files;
        ctx->obj_capacity = new_capacity;
    }
    
    ctx->obj_files[ctx->obj_count] = ES_STRDUP(obj_path);
    if (!ctx->obj_files[ctx->obj_count]) return -1;
    
    ArkLinkResult result = arklink_session_add_input(ctx->session, obj_path);
    if (result != ARK_LINK_OK) {
        ES_ERROR("Failed to add object file: %s", obj_path);
        return -1;
    }
    
    ctx->obj_count++;
    return 0;
}

int es_arklink_add_runtime_objects(EsArkLinkContext* ctx, const char* runtime_dir) {
    if (!ctx || !runtime_dir) return -1;
    
    const char* runtime_files[] = {
        "runtime.o",
        "output_cache.o", 
        "allocator.o",
        "es_string.o",
        NULL
    };
    
    for (int i = 0; runtime_files[i]; i++) {
        char path[ES_MAX_PATH];
        snprintf(path, sizeof(path), "%s%c%s", 
                 runtime_dir, ES_PATH_SEPARATOR, runtime_files[i]);
        
        if (es_path_exists(path)) {
            if (es_arklink_add_object_file(ctx, path) != 0) {
                ES_WARNING("Failed to add runtime object: %s", path);
            }
        } else {
            ES_DEBUG("Runtime object not found: %s", path);
        }
    }
    
    return 0;
}

int es_arklink_set_output(EsArkLinkContext* ctx, const char* output_path) {
    if (!ctx || !output_path) return -1;
    
    ArkLinkResult result = arklink_session_set_output(ctx->session, output_path);
    if (result != ARK_LINK_OK) {
        ES_ERROR("Failed to set output path: %s", output_path);
        return -1;
    }
    
    return 0;
}

int es_arklink_set_entry_point(EsArkLinkContext* ctx, const char* entry_point) {
    if (!ctx || !entry_point) return -1;
    
    ArkLinkResult result = arklink_session_set_entry_point(ctx->session, entry_point);
    if (result != ARK_LINK_OK) {
        ES_ERROR("Failed to set entry point: %s", entry_point);
        return -1;
    }
    
    return 0;
}

int es_arklink_link(EsArkLinkContext* ctx) {
    if (!ctx || !ctx->session) return -1;
    
    ES_INFO("Starting ArkLink...");
    
    ArkLinkResult result = arklink_session_link(ctx->session);
    
    if (result != ARK_LINK_OK) {
        const char* error = arklink_session_get_error(ctx->session);
        ES_ERROR("ArkLink failed: %s", error ? error : "Unknown error");
        return -1;
    }
    
    ES_INFO("ArkLink completed successfully");
    return 0;
}

int es_arklink_link_objects(const char** obj_files, int obj_count, 
                            const char* output_path, EsConfig* config) {
    if (!obj_files || obj_count <= 0 || !output_path || !config) {
        ES_ERROR("Invalid arguments to es_arklink_link_objects");
        return -1;
    }
    
    EsArkLinkContext* ctx = es_arklink_context_create(config);
    if (!ctx) {
        ES_ERROR("Failed to create ArkLink context");
        return -1;
    }
    
    for (int i = 0; i < obj_count; i++) {
        if (es_arklink_add_object_file(ctx, obj_files[i]) != 0) {
            ES_ERROR("Failed to add object file: %s", obj_files[i]);
            es_arklink_context_destroy(ctx);
            return -1;
        }
    }
    
    if (es_arklink_set_output(ctx, output_path) != 0) {
        es_arklink_context_destroy(ctx);
        return -1;
    }
    
    int result = es_arklink_link(ctx);
    
    es_arklink_context_destroy(ctx);
    return result;
}
