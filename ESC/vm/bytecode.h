#ifndef ES_BYTECODE_H
#define ES_BYTECODE_H

#include <stdint.h>
#include <stddef.h>
#include "value.h"





typedef enum {
    OP_CONSTANT,      
    OP_NULL,          
    OP_TRUE,          
    OP_FALSE,         
    
    OP_POP,           
    
    OP_GET_LOCAL,     
    OP_SET_LOCAL,     
    OP_GET_GLOBAL,    
    OP_DEFINE_GLOBAL, 
    OP_SET_GLOBAL,    
    
    OP_EQUAL,         
    OP_GREATER,       
    OP_LESS,          
    
    OP_ADD,           
    OP_SUB,           
    OP_MUL,           
    OP_DIV,           
    OP_NOT,           
    OP_NEGATE,        
    
    OP_PRINT,         
    
    OP_JUMP,          
    OP_JUMP_IF_FALSE, 
    OP_LOOP,          
    
    OP_CALL,          
    OP_RETURN,        
    
    OP_STK_ADJ,       
    
    OP_INT_TO_STRING, 
    
    OP_HALT           
} EsOpCode;





typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;       
    EsValueArray constants; 
} EsChunk;

void es_chunk_init(EsChunk* chunk);
void es_chunk_write(EsChunk* chunk, uint8_t byte, int line);
void es_chunk_free(EsChunk* chunk);
int es_chunk_add_constant(EsChunk* chunk, EsValue value);
int es_chunk_add_string_constant(EsChunk* chunk, const char* s);

#endif 
