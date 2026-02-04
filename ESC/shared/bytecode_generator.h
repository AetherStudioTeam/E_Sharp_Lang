#ifndef ES_BYTECODE_GENERATOR_H
#define ES_BYTECODE_GENERATOR_H
#include "bytecode.h"
#include <stdbool.h>
typedef struct EsBytecodeGenerator EsBytecodeGenerator;
EsBytecodeGenerator* es_bytecode_generator_create(void);
void es_bytecode_generator_destroy(EsBytecodeGenerator* generator);
void es_bytecode_generator_init_chunk(EsChunk* chunk);
void es_bytecode_generator_write_byte(EsChunk* chunk, uint8_t byte, int line);
void es_bytecode_generator_write_short(EsChunk* chunk, uint16_t value);
int es_bytecode_generator_add_constant(EsChunk* chunk, EsValue value);
int es_bytecode_generator_add_string_constant(EsChunk* chunk, const char* string);
void es_bytecode_generator_free_chunk(EsChunk* chunk);
bool es_bytecode_generator_serialize_to_file(EsChunk* chunk, const char* filename);
#endif 