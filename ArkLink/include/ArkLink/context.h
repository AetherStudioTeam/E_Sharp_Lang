#ifndef ARKLINK_CONTEXT_H
#define ARKLINK_CONTEXT_H

#include "arklink.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ArkArena ArkArena;
typedef struct ArkLinkContext ArkLinkContext;

typedef enum ArkSectionKind {
    ARK_SECTION_CODE,
    ARK_SECTION_DATA,
    ARK_SECTION_RODATA,
    ARK_SECTION_BSS,
    ARK_SECTION_TLS,
} ArkSectionKind;

enum {
    ARK_SECTION_FLAG_EXECUTABLE = 1u << 0,
    ARK_SECTION_FLAG_WRITABLE = 1u << 1,
    ARK_SECTION_FLAG_READONLY = 1u << 2,
};

typedef struct ArkSectionBuffer {
    uint8_t* data;
    size_t size;
    size_t capacity;
    ArkSectionKind kind;
    uint32_t flags;
    uint32_t alignment;
} ArkSectionBuffer;

typedef struct ArkStringView {
    const char* data;
    size_t length;
} ArkStringView;

typedef struct ArkContextConfig {
    size_t arena_chunk_size;
    size_t string_table_buckets;
} ArkContextConfig;





typedef struct ArkParsedImport {
    const char* module;
    const char* symbol;
    uint32_t slot;
} ArkParsedImport;

typedef struct ArkParsedConfig {
    const char* runtime_name;
    const char* entry_point;
    ArkSubsystem subsystem;
    uint64_t image_base;
    uint64_t stack_size;
    ArkParsedImport* imports;
    size_t import_count;
    const char** library_paths;
    size_t library_path_count;
    const char** libraries;
    size_t library_count;
    const char** export_symbols;
    size_t export_symbol_count;
    ArkLinkOutputKind output_kind;
} ArkParsedConfig;





ArkArena* ark_arena_create(size_t chunk_size);
void ark_arena_destroy(ArkArena* arena);
void ark_arena_reset(ArkArena* arena);
void* ark_arena_alloc(ArkArena* arena, size_t size, size_t alignment);





ArkLinkContext* ark_context_create(const ArkLinkJob* job, const ArkContextConfig* cfg);
void ark_context_destroy(ArkLinkContext* ctx);

const ArkLinkJob* ark_context_job(const ArkLinkContext* ctx);
ArkArena* ark_context_arena(ArkLinkContext* ctx);

const char* ark_context_intern(ArkLinkContext* ctx, ArkStringView view);


const ArkParsedConfig* ark_context_config(const ArkLinkContext* ctx);


void ark_context_set_config(ArkLinkContext* ctx, ArkParsedConfig* config);





ArkLinkResult ark_section_buffer_init(ArkSectionBuffer* buffer, ArkSectionKind kind, uint32_t flags, uint32_t alignment);
ArkLinkResult ark_section_buffer_append(ArkSectionBuffer* buffer, const void* data, size_t size);
ArkLinkResult ark_section_buffer_resize(ArkSectionBuffer* buffer, size_t new_size);
void ark_section_buffer_release(ArkSectionBuffer* buffer);





void ark_context_log(const ArkLinkContext* ctx, ArkLogLevel level, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif 
