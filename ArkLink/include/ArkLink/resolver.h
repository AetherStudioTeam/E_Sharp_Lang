#ifndef ARKLINK_RESOLVER_H
#define ARKLINK_RESOLVER_H

#include "context.h"
#include "loader.h"
#include "backend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ArkResolverSymbol {
    const char* name;
    ArkSymbolBinding binding;
    ArkSymbolVisibility visibility;
    ArkSectionBuffer* section;
    uint32_t section_index;
    uint32_t value;
    uint32_t size;
    int32_t import_id;
    int is_export;          
} ArkResolverSymbol;

typedef struct ArkResolverReloc {
    ArkSectionBuffer* section;
    uint32_t section_index;
    uint32_t offset;
    uint32_t type;
    const ArkResolverSymbol* symbol;
    int32_t addend;
} ArkResolverReloc;

typedef struct ArkImportBinding {
    const char* module;
    const char* symbol;
    uint32_t slot;
} ArkImportBinding;

typedef struct ArkExportBinding {
    const char* name;
    uint32_t symbol_index;
    uint32_t ordinal;
} ArkExportBinding;

typedef struct ArkResolverPlan {
    ArkBackendInput* backend_input;
    ArkResolverSymbol* symbols;
    size_t symbol_count;
    ArkResolverReloc* relocs;
    size_t reloc_count;
    ArkImportBinding* imports;
    size_t import_count;
    ArkExportBinding* exports;
    size_t export_count;
    uint32_t entry_symbol;
    uint32_t entry_section;
    uint32_t entry_offset;
} ArkResolverPlan;

ArkLinkResult ark_resolver_resolve(ArkLinkContext* ctx, ArkLinkUnit* const* units, size_t unit_count, ArkResolverPlan* out_plan);
void ark_resolver_plan_destroy(ArkLinkContext* ctx, ArkResolverPlan* plan);

#ifdef __cplusplus
}
#endif

#endif 
