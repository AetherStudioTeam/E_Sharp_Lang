#ifndef ES_X86_BACKEND_H
#define ES_X86_BACKEND_H

#include <stdio.h>
#include "ir.h"


void es_x86_generate(FILE* output, EsIRModule* module);

#endif