#ifndef ES_VM_CODEGEN_H
#define ES_VM_CODEGEN_H

#include "../../middle/ir/ir.h"
#include "../../../shared/bytecode.h"
#include "../../../shared/bytecode_generator.h"




void es_vm_codegen_generate(EsIRModule* ir_module, EsChunk* chunk);

#endif
