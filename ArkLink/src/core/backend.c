#include "ArkLink/backend.h"
#include "ArkLink/targets/pe.h"
#include "ArkLink/targets/elf.h"

#ifndef ARK_MAX_BACKENDS
#define ARK_MAX_BACKENDS 8
#endif

static const ArkBackendOps* g_backends[ARK_MAX_BACKENDS];
static size_t g_backend_count = 0;
static int g_defaults_registered = 0;

void ark_backend_register(const ArkBackendOps* ops) {
    if (!ops) {
        return;
    }
    for (size_t i = 0; i < g_backend_count; ++i) {
        if (g_backends[i]->target == ops->target) {
            g_backends[i] = ops;
            return;
        }
    }
    if (g_backend_count < ARK_MAX_BACKENDS) {
        g_backends[g_backend_count++] = ops;
    }
}

const ArkBackendOps* ark_backend_query(ArkLinkTarget target) {
    for (size_t i = 0; i < g_backend_count; ++i) {
        if (g_backends[i]->target == target) {
            return g_backends[i];
        }
    }
    return NULL;
}

void ark_backend_register_all(void) {
    if (g_defaults_registered) {
        return;
    }
    g_defaults_registered = 1;
    ark_backend_register(ark_pe_backend_ops());
    ark_backend_register(ark_elf_backend_ops());
}
