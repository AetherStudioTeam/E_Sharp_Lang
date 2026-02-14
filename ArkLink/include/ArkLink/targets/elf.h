#ifndef ARKLINK_TARGETS_ELF_H
#define ARKLINK_TARGETS_ELF_H

#include "ArkLink/backend.h"

#ifdef __cplusplus
extern "C" {
#endif

const ArkBackendOps* ark_elf_backend_ops(void);

#ifdef __cplusplus
}
#endif

#endif 
