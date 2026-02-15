#ifndef ES_ARKLINK_INTEGRATION_H
#define ES_ARKLINK_INTEGRATION_H

#include "../../../ArkLink/include/ArkLink/arklink.h"
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EsArkLinkContext EsArkLinkContext;

EsArkLinkContext* es_arklink_context_create(EsConfig* config);
void es_arklink_context_destroy(EsArkLinkContext* ctx);

int es_arklink_add_object_file(EsArkLinkContext* ctx, const char* obj_path);
int es_arklink_add_runtime_objects(EsArkLinkContext* ctx, const char* runtime_dir);
int es_arklink_set_output(EsArkLinkContext* ctx, const char* output_path);
int es_arklink_set_entry_point(EsArkLinkContext* ctx, const char* entry_point);
int es_arklink_link(EsArkLinkContext* ctx);

int es_arklink_link_objects(const char** obj_files, int obj_count, 
                            const char* output_path, EsConfig* config);

#ifdef __cplusplus
}
#endif

#endif
