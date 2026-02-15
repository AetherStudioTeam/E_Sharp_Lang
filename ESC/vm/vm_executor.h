#ifndef ES_VM_EXECUTOR_H
#define ES_VM_EXECUTOR_H

#include "vm.h"
#include "bytecode.h"


typedef struct {
    EsVM vm;
    char* bytecode_file_path;
    int verbose;
} VMExecutor;


VMExecutor* vm_executor_create(const char* bytecode_file_path, int verbose);


void vm_executor_destroy(VMExecutor* executor);


EsInterpretResult vm_executor_execute(VMExecutor* executor);


EsChunk* vm_executor_load_bytecode(VMExecutor* executor);


void vm_executor_free_chunk(VMExecutor* executor, EsChunk* chunk);

#endif 