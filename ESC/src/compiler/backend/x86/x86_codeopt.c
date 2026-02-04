


#include "x86_codeopt.h"
#include "../../../core/utils/es_common.h"
#include "../../middle/ir/ir.h"




X86PeepholeOptimizer* x86_peephole_optimizer_create(void) {
    return NULL;
}

void x86_peephole_optimizer_destroy(X86PeepholeOptimizer* optimizer) {
    (void)optimizer;
}

void x86_optimize_peephole(EsIRModule* module, int optimization_level) {
    (void)module;
    (void)optimization_level;
    
}
