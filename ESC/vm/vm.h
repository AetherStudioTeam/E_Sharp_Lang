#ifndef ES_VM_H
#define ES_VM_H

#include "bytecode.h"
#include "value.h"

#define STACK_MAX 256
#define FRAMES_MAX 64




typedef struct {
    uint8_t* ip;          
    EsValue* slots;       
} EsCallFrame;




typedef struct {
    EsChunk* chunk;
    uint8_t* ip;          
    
    EsValue stack[STACK_MAX];
    EsValue* stack_top;   

    EsCallFrame frames[FRAMES_MAX];
    int frame_count;

    size_t bytes_allocated;
    size_t next_gc;
    struct EsObject* objects; 
} EsVM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} EsInterpretResult;

void es_vm_init(EsVM* vm);
void es_vm_free(EsVM* vm);
EsInterpretResult es_vm_interpret(EsVM* vm, EsChunk* chunk);


void es_vm_push(EsVM* vm, EsValue value);
EsValue es_vm_pop(EsVM* vm);


void* es_vm_reallocate(EsVM* vm, void* pointer, size_t old_size, size_t new_size);
void es_vm_collect_garbage(EsVM* vm);

#endif 
