#include "bytecode_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ES_REALLOC(ptr, size) realloc(ptr, size)

struct EsBytecodeGenerator {
    
};

EsBytecodeGenerator* es_bytecode_generator_create(void) {
    EsBytecodeGenerator* generator = malloc(sizeof(EsBytecodeGenerator));
    return generator;
}

void es_bytecode_generator_destroy(EsBytecodeGenerator* generator) {
    free(generator);
}

void es_bytecode_generator_init_chunk(EsChunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->constants.count = 0;
    chunk->constants.capacity = 0;
    chunk->constants.values = NULL;
}

void es_bytecode_generator_write_byte(EsChunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->code = ES_REALLOC(chunk->code, chunk->capacity);
        chunk->lines = ES_REALLOC(chunk->lines, chunk->capacity * sizeof(int));
    }
    
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void es_bytecode_generator_write_short(EsChunk* chunk, uint16_t value) {
    es_bytecode_generator_write_byte(chunk, (uint8_t)(value & 0xFF), 0);
    es_bytecode_generator_write_byte(chunk, (uint8_t)((value >> 8) & 0xFF), 0);
}

int es_bytecode_generator_add_constant(EsChunk* chunk, EsValue value) {
    printf("es_bytecode_generator_add_constant: adding constant, type=%d, current count=%d\n", value.type, chunk->constants.count);
    if (chunk->constants.capacity < chunk->constants.count + 1) {
        int old_capacity = chunk->constants.capacity;
        chunk->constants.capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->constants.values = ES_REALLOC(chunk->constants.values, chunk->constants.capacity * sizeof(EsValue));
    }
    
    chunk->constants.values[chunk->constants.count] = value;
    printf("es_bytecode_generator_add_constant: constant added, new count=%d\n", chunk->constants.count + 1);
    return chunk->constants.count++;
}

int es_bytecode_generator_add_string_constant(EsChunk* chunk, const char* string) {
    EsValue value;
    value.type = VAL_STRING_LITERAL;
    value.as.string_literal = string;
    return es_bytecode_generator_add_constant(chunk, value);
}

void es_bytecode_generator_free_chunk(EsChunk* chunk) {
    free(chunk->code);
    free(chunk->lines);
    free(chunk->constants.values);
    es_bytecode_generator_init_chunk(chunk);
}

bool es_bytecode_generator_serialize_to_file(EsChunk* chunk, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    uint32_t magic = 0x45534243;
    uint16_t version = 1;
    fwrite(&magic, sizeof(uint32_t), 1, file);
    fwrite(&version, sizeof(uint16_t), 1, file);
    
    uint32_t code_count = chunk->count;
    fwrite(&code_count, sizeof(uint32_t), 1, file);
    if (code_count > 0) {
        fwrite(chunk->code, sizeof(uint8_t), code_count, file);
        fwrite(chunk->lines, sizeof(int), code_count, file);
    }
    
    uint32_t constant_count = chunk->constants.count;
    printf("es_bytecode_generator_serialize_to_file: chunk->constants.count=%d\n", chunk->constants.count);
    printf("es_bytecode_generator_serialize_to_file: chunk->constants.capacity=%d\n", chunk->constants.capacity);
    printf("es_bytecode_generator_serialize_to_file: chunk->constants.values=%p\n", (void*)chunk->constants.values);
    printf("es_bytecode_generator_serialize_to_file: writing constant_count=%d\n", constant_count);
    fwrite(&constant_count, sizeof(uint32_t), 1, file);
    for (int i = 0; i < constant_count; i++) {
        EsValue value = chunk->constants.values[i];
        printf("es_bytecode_generator_serialize_to_file: writing constant %d, type=%d\n", i, value.type);
        fwrite(&value.type, sizeof(EsValueType), 1, file);
        
        switch (value.type) {
            case VAL_BOOL:
                fwrite(&value.as.boolean, sizeof(bool), 1, file);
                break;
            case VAL_NUMBER:
                fwrite(&value.as.number, sizeof(double), 1, file);
                break;
            case VAL_STRING_LITERAL: {
                uint16_t length = strlen(value.as.string_literal);
                fwrite(&length, sizeof(uint16_t), 1, file);
                fwrite(value.as.string_literal, sizeof(char), length, file);
                break;
            }
            case VAL_NULL:
            case VAL_OBJ:
                break;
        }
    }
    
    fclose(file);
    return true;
}