#include "bytecode.h"
#include "core/utils/es_common.h"
#include <stdlib.h>

void es_chunk_init(EsChunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    es_value_array_init(&chunk->constants);
}

void es_chunk_free(EsChunk* chunk) {
    ES_FREE(chunk->code);
    ES_FREE(chunk->lines);
    
    
    for (int i = 0; i < chunk->constants.count; i++) {
        if (IS_STRING_LIT(chunk->constants.values[i])) {
            ES_FREE((void*)AS_STRING_LIT(chunk->constants.values[i]));
        }
    }
    
    es_value_array_free(&chunk->constants);
    es_chunk_init(chunk);
}

void es_chunk_write(EsChunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->code = (uint8_t*)ES_REALLOC(chunk->code, sizeof(uint8_t) * chunk->capacity);
        chunk->lines = (int*)ES_REALLOC(chunk->lines, sizeof(int) * chunk->capacity);
    }
    
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int es_chunk_add_constant(EsChunk* chunk, EsValue value) {
    es_value_array_write(&chunk->constants, value);
    return chunk->constants.count - 1;
}

#include <string.h>

int es_chunk_add_string_constant(EsChunk* chunk, const char* s) {
    
    
    
    size_t len = strlen(s);
    char* copy = (char*)ES_MALLOC(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    EsValue value = STRING_VAL(copy);
    return es_chunk_add_constant(chunk, value);
}
