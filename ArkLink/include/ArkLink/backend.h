#ifndef ARKLINK_BACKEND_H
#define ARKLINK_BACKEND_H

#include "ArkLink/context.h"
#include "ArkLink/loader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ArkBackendInputSection {
    const char* name;
    const ArkSectionBuffer* buffer;
    uint32_t id;
} ArkBackendInputSection;

typedef struct ArkBackendInputSymbol {
    const char* name;
    uint32_t section_id;
    uint32_t value;
    uint32_t size;
    ArkSymbolBinding binding;
    ArkSymbolVisibility visibility;
    int32_t import_id;
    int is_export;          
} ArkBackendInputSymbol;

typedef struct ArkBackendInputReloc {
    uint32_t section_id;
    uint32_t offset;
    uint32_t type;
    uint32_t symbol_index;
    int32_t addend;
} ArkBackendInputReloc;

typedef struct ArkBackendInputImport {
    const char* module;
    const char* symbol;
    uint32_t slot;
} ArkBackendInputImport;

typedef struct ArkBackendInputExport {
    const char* name;
    uint32_t symbol_index;
    uint32_t ordinal;
} ArkBackendInputExport;

typedef struct ArkBackendInput {
    ArkBackendInputSection* sections;
    size_t section_count;
    ArkBackendInputSymbol* symbols;
    size_t symbol_count;
    ArkBackendInputReloc* relocs;
    size_t reloc_count;
    ArkBackendInputImport* imports;
    size_t import_count;
    ArkBackendInputExport* exports;
    size_t export_count;
    uint32_t entry_section;
    uint32_t entry_offset;
    ArkLinkOutputKind output_kind;

    
    ArkSubsystem subsystem;
    uint64_t image_base;
    uint64_t stack_size;
    const char* entry_point_name;
} ArkBackendInput;

typedef struct ArkBackendState ArkBackendState;

typedef struct ArkBackendOps {
    ArkLinkTarget target;
    ArkLinkResult (*prepare)(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendState** out_state);
    ArkLinkResult (*write_output)(ArkBackendState* state, const char* output_path);
    void (*destroy_state)(ArkBackendState* state);
} ArkBackendOps;

const ArkBackendOps* ark_backend_query(ArkLinkTarget target);
void ark_backend_register(const ArkBackendOps* ops);

#ifdef __cplusplus
}
#endif

#endif 
