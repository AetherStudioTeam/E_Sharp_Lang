#ifndef ES_VM_DEBUG_H
#define ES_VM_DEBUG_H

#include "bytecode.h"

void es_disassemble_chunk(EsChunk* chunk, const char* name);
int es_disassemble_instruction(EsChunk* chunk, int offset);

#endif 
