#include "ArkLink/context.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

struct ArkArenaChunk {
    struct ArkArenaChunk* next;
    size_t capacity;
    size_t offset;
    uint8_t data[1];
};

struct ArkArena {
    struct ArkArenaChunk* head;
    size_t chunk_size;
};

struct ArkStringNode {
    struct ArkStringNode* next;
    size_t length;
    char data[1];
};

struct ArkLinkContext {
    ArkArena* arena;
    ArkLinkJob job;
    ArkContextConfig config;
    ArkLinkLogger logger;
    void* logger_user_data;
    struct ArkStringNode** buckets;
    size_t bucket_count;
    ArkParsedConfig* parsed_config;
};

static struct ArkArenaChunk* arena_chunk_create(size_t chunk_size) {
    struct ArkArenaChunk* chunk = (struct ArkArenaChunk*)malloc(sizeof(struct ArkArenaChunk) + chunk_size - 1);
    if (!chunk) {
        return NULL;
    }
    chunk->next = NULL;
    chunk->capacity = chunk_size;
    chunk->offset = 0;
    return chunk;
}

ArkArena* ark_arena_create(size_t chunk_size) {
    if (chunk_size == 0) {
        chunk_size = 64 * 1024;
    }
    ArkArena* arena = (ArkArena*)calloc(1, sizeof(ArkArena));
    if (!arena) {
        return NULL;
    }
    arena->chunk_size = chunk_size;
    return arena;
}

void ark_arena_destroy(ArkArena* arena) {
    if (!arena) {
        return;
    }
    ark_arena_reset(arena);
    free(arena);
}

void ark_arena_reset(ArkArena* arena) {
    struct ArkArenaChunk* chunk = arena->head;
    while (chunk) {
        struct ArkArenaChunk* next = chunk->next;
        free(chunk);
        chunk = next;
    }
    arena->head = NULL;
}

void* ark_arena_alloc(ArkArena* arena, size_t size, size_t alignment) {
    if (alignment == 0) {
        alignment = sizeof(void*);
    }
    struct ArkArenaChunk* chunk = arena->head;
    if (!chunk || chunk->capacity - chunk->offset < size + alignment) {
        struct ArkArenaChunk* new_chunk = arena_chunk_create(arena->chunk_size > size ? arena->chunk_size : size + alignment);
        if (!new_chunk) {
            return NULL;
        }
        new_chunk->next = chunk;
        arena->head = new_chunk;
        chunk = new_chunk;
    }
    size_t aligned_offset = (chunk->offset + (alignment - 1)) & ~(alignment - 1);
    if (aligned_offset + size > chunk->capacity) {
        return NULL;
    }
    void* ptr = chunk->data + aligned_offset;
    chunk->offset = aligned_offset + size;
    return ptr;
}

static size_t hash_bytes(const char* data, size_t length) {
    size_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned char)data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static const char* ark_context_intern_impl(ArkLinkContext* ctx, const char* data, size_t length) {
    size_t idx = hash_bytes(data, length) % ctx->bucket_count;
    struct ArkStringNode* node = ctx->buckets[idx];
    while (node) {
        if (node->length == length && memcmp(node->data, data, length) == 0) {
            return node->data;
        }
        node = node->next;
    }
    node = (struct ArkStringNode*)ark_arena_alloc(ctx->arena, sizeof(struct ArkStringNode) + length, alignof(struct ArkStringNode));
    if (!node) {
        return NULL;
    }
    node->length = length;
    memcpy(node->data, data, length);
    node->data[length] = '\0';
    node->next = ctx->buckets[idx];
    ctx->buckets[idx] = node;
    return node->data;
}

ArkLinkContext* ark_context_create(const ArkLinkJob* job, const ArkContextConfig* cfg) {
    ArkContextConfig defaults = {
        .arena_chunk_size = 64 * 1024,
        .string_table_buckets = 1024,
    };
    ArkContextConfig applied = defaults;
    if (cfg) {
        if (cfg->arena_chunk_size) {
            applied.arena_chunk_size = cfg->arena_chunk_size;
        }
        if (cfg->string_table_buckets) {
            applied.string_table_buckets = cfg->string_table_buckets;
        }
    }

    ArkArena* arena = ark_arena_create(applied.arena_chunk_size);
    if (!arena) {
        return NULL;
    }
    ArkLinkContext* ctx = (ArkLinkContext*)calloc(1, sizeof(ArkLinkContext));
    if (!ctx) {
        ark_arena_destroy(arena);
        return NULL;
    }
    ctx->arena = arena;
    ctx->config = applied;
    ctx->logger = job ? job->logger : NULL;
    ctx->logger_user_data = job ? job->logger_user_data : NULL;
    ctx->parsed_config = NULL;
    if (job) {
        ctx->job = *job;
    }
    ctx->bucket_count = applied.string_table_buckets;
    ctx->buckets = (struct ArkStringNode**)calloc(ctx->bucket_count, sizeof(struct ArkStringNode*));
    if (!ctx->buckets) {
        ark_context_destroy(ctx);
        return NULL;
    }
    return ctx;
}

static void ark_parsed_config_free(ArkParsedConfig* config) {
    if (!config) {
        return;
    }
    
    free(config->imports);
    free(config->library_paths);
    free(config->libraries);
    free(config->export_symbols);
    free(config);
}

void ark_context_destroy(ArkLinkContext* ctx) {
    if (!ctx) {
        return;
    }
    ark_parsed_config_free(ctx->parsed_config);
    ark_arena_destroy(ctx->arena);
    free(ctx->buckets);
    free(ctx);
}

const ArkLinkJob* ark_context_job(const ArkLinkContext* ctx) {
    return ctx ? &ctx->job : NULL;
}

ArkArena* ark_context_arena(ArkLinkContext* ctx) {
    return ctx ? ctx->arena : NULL;
}

const char* ark_context_intern(ArkLinkContext* ctx, ArkStringView view) {
    if (!ctx || !view.data) {
        return NULL;
    }
    return ark_context_intern_impl(ctx, view.data, view.length);
}

const ArkParsedConfig* ark_context_config(const ArkLinkContext* ctx) {
    return ctx ? ctx->parsed_config : NULL;
}

void ark_context_set_config(ArkLinkContext* ctx, ArkParsedConfig* config) {
    if (!ctx) {
        return;
    }
    if (ctx->parsed_config) {
        ark_parsed_config_free(ctx->parsed_config);
    }
    ctx->parsed_config = config;
}

ArkLinkResult ark_section_buffer_init(ArkSectionBuffer* buffer, ArkSectionKind kind, uint32_t flags, uint32_t alignment) {
    if (!buffer) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
    buffer->kind = kind;
    buffer->flags = flags;
    buffer->alignment = alignment ? alignment : 1;
    return ARK_LINK_OK;
}

static uint8_t* ark_section_buffer_grow(ArkSectionBuffer* buffer, size_t new_capacity) {
    uint8_t* new_data = (uint8_t*)realloc(buffer->data, new_capacity);
    if (!new_data) {
        return NULL;
    }
    buffer->data = new_data;
    buffer->capacity = new_capacity;
    return new_data;
}

ArkLinkResult ark_section_buffer_append(ArkSectionBuffer* buffer, const void* data, size_t size) {
    if (!buffer || (!data && size > 0)) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    size_t required = buffer->size + size;
    if (required > buffer->capacity) {
        size_t new_capacity = buffer->capacity ? buffer->capacity * 2 : 256;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        if (!ark_section_buffer_grow(buffer, new_capacity)) {
            return ARK_LINK_ERR_IO;
        }
    }
    if (size > 0) {
        memcpy(buffer->data + buffer->size, data, size);
    }
    buffer->size += size;
    return ARK_LINK_OK;
}

ArkLinkResult ark_section_buffer_resize(ArkSectionBuffer* buffer, size_t new_size) {
    if (!buffer) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    if (new_size > buffer->capacity) {
        if (!ark_section_buffer_grow(buffer, new_size)) {
            return ARK_LINK_ERR_IO;
        }
    }
    if (new_size > buffer->size) {
        memset(buffer->data + buffer->size, 0, new_size - buffer->size);
    }
    buffer->size = new_size;
    return ARK_LINK_OK;
}

void ark_section_buffer_release(ArkSectionBuffer* buffer) {
    if (!buffer) {
        return;
    }
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}

void ark_context_log(const ArkLinkContext* ctx, ArkLogLevel level, const char* fmt, ...) {
    if (!ctx || !ctx->logger || !fmt) {
        return;
    }
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    ctx->logger(level, buffer, ctx->logger_user_data);
}
