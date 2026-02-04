


#ifndef X86_CODEOPT_H
#define X86_CODEOPT_H

#include "../../middle/ir/ir.h"

typedef struct X86PeepholeOptimizer X86PeepholeOptimizer;

X86PeepholeOptimizer* x86_peephole_optimizer_create(void);
void x86_peephole_optimizer_destroy(X86PeepholeOptimizer* optimizer);
void x86_optimize_peephole(EsIRModule* module, int optimization_level);

#endif
