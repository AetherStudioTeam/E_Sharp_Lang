#include "ArkLink/resolver.h"
#include "ArkLink/backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct ArkSymbolEntry {
    struct ArkSymbolEntry* next;
    const char* name;
    ArkResolverSymbol* symbol;
    size_t symbol_index;
} ArkSymbolEntry;


typedef struct {
    ArkSymbolEntry** buckets;
    size_t bucket_count;
} ArkSymbolTable;

typedef struct ArkResolverState {
    ArkLinkContext* ctx;
    ArkLinkUnit** units;
    size_t unit_count;
    size_t unit_capacity;
    ArkResolverPlan* plan;
    ArkSymbolTable symtab;
} ArkResolverState;


static size_t ark_hash_string(const char* str) {
    size_t hash = 1469598103934665603ull;
    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 1099511628211ull;
    }
    return hash;
}


static ArkLinkResult ark_symtab_init(ArkSymbolTable* table, size_t bucket_count) {
    table->buckets = (ArkSymbolEntry**)calloc(bucket_count, sizeof(ArkSymbolEntry*));
    if (!table->buckets) {
        return ARK_LINK_ERR_MEMORY;
    }
    table->bucket_count = bucket_count;
    return ARK_LINK_OK;
}


static void ark_symtab_free(ArkSymbolTable* table) {
    if (!table || !table->buckets) {
        return;
    }
    for (size_t i = 0; i < table->bucket_count; i++) {
        ArkSymbolEntry* entry = table->buckets[i];
        while (entry) {
            ArkSymbolEntry* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    table->buckets = NULL;
    table->bucket_count = 0;
}


static ArkSymbolEntry* ark_symtab_lookup(ArkSymbolTable* table, const char* name) {
    size_t idx = ark_hash_string(name) % table->bucket_count;
    ArkSymbolEntry* entry = table->buckets[idx];
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}


static ArkLinkResult ark_symtab_insert(ArkSymbolTable* table, const char* name, 
                              ArkResolverSymbol* symbol, size_t symbol_index) {
    size_t idx = ark_hash_string(name) % table->bucket_count;
    
    
    ArkSymbolEntry* existing = table->buckets[idx];
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            return ARK_LINK_ERR_FORMAT;  
        }
        existing = existing->next;
    }
    
    ArkSymbolEntry* entry = (ArkSymbolEntry*)malloc(sizeof(ArkSymbolEntry));
    if (!entry) {
        return ARK_LINK_ERR_MEMORY;
    }
    
    entry->name = name;
    entry->symbol = symbol;
    entry->symbol_index = symbol_index;
    entry->next = table->buckets[idx];
    table->buckets[idx] = entry;
    
    return ARK_LINK_OK;
}


static ArkLinkResult ark_resolver_collect_symbols(ArkResolverState* state) {
    
    size_t total = 0;
    for (size_t i = 0; i < state->unit_count; ++i) {
        total += state->units[i]->symbol_count;
    }
    
    if (total == 0) {
        state->plan->symbols = NULL;
        state->plan->symbol_count = 0;
        return ARK_LINK_OK;
    }
    
    
    size_t bucket_count = total * 2;
    if (bucket_count < 16) bucket_count = 16;
    ArkLinkResult result = ark_symtab_init(&state->symtab, bucket_count);
    if (result != ARK_LINK_OK) {
        return result;
    }
    
    
    state->plan->symbols = (ArkResolverSymbol*)calloc(total, sizeof(ArkResolverSymbol));
    if (!state->plan->symbols) {
        ark_symtab_free(&state->symtab);
        return ARK_LINK_ERR_INTERNAL;
    }
    
    
    size_t cursor = 0;
    for (size_t u = 0; u < state->unit_count; ++u) {
        ArkLinkUnit* unit = state->units[u];
        for (size_t s = 0; s < unit->symbol_count; ++s) {
            ArkSymbolDesc* src = &unit->symbols[s];
            
            
            ArkSymbolEntry* existing = ark_symtab_lookup(&state->symtab, src->name);
            
            if (existing) {
                
                ArkResolverSymbol* existing_sym = existing->symbol;
                
                if (src->binding == ARK_BIND_GLOBAL && 
                    existing_sym->binding == ARK_BIND_GLOBAL) {
                    
                    ark_context_log(state->ctx, ARK_LOG_ERROR, 
                        "Multiple definitions of symbol: %s", src->name);
                    return ARK_LINK_ERR_FORMAT;
                }
                
                if (src->binding == ARK_BIND_GLOBAL ||
                    (src->binding == ARK_BIND_LOCAL && existing_sym->binding == ARK_BIND_WEAK)) {
                    
                    existing_sym->binding = src->binding;
                    existing_sym->visibility = src->visibility;
                    existing_sym->section_index = src->section_index;
                    existing_sym->value = src->value;
                    existing_sym->size = src->size;
                    
                    if (src->section_index > 0 && src->section_index <= unit->section_count) {
                        existing_sym->section = unit->sections[src->section_index - 1].buffer;
                    } else {
                        existing_sym->section = NULL;
                    }
                }
                
            } else {
                
                ArkResolverSymbol* dst = &state->plan->symbols[cursor];
                dst->name = src->name;
                dst->binding = src->binding;
                dst->visibility = src->visibility;
                dst->section_index = src->section_index;
                dst->value = src->value;
                dst->size = src->size;
                dst->import_id = -1;  
                
                if (src->section_index > 0 && src->section_index <= unit->section_count) {
                    dst->section = unit->sections[src->section_index - 1].buffer;
                } else {
                    dst->section = NULL;
                }
                
                result = ark_symtab_insert(&state->symtab, src->name, dst, cursor);
                if (result != ARK_LINK_OK) {
                    ark_symtab_free(&state->symtab);
                    return result;
                }
                cursor++;
            }
        }
    }
    
    state->plan->symbol_count = cursor;
    return ARK_LINK_OK;
}


static ArkLinkResult ark_resolver_check_undefined(ArkResolverState* state) {
    int has_undefined = 0;
    const ArkParsedConfig* config = ark_context_config(state->ctx);
    const ArkLinkJob* job = ark_context_job(state->ctx);

    
    int progress = 1;
    while (progress) {
        progress = 0;
        size_t undef_count = 0;
        const char** undef_names = NULL;

        
        for (size_t i = 0; i < state->plan->symbol_count; i++) {
            ArkResolverSymbol* sym = &state->plan->symbols[i];
            if (sym->section == NULL && sym->binding == ARK_BIND_GLOBAL) {
                
                int is_import = 0;
                if (config && config->imports) {
                    for (size_t k = 0; k < config->import_count; k++) {
                        if (strcmp(config->imports[k].symbol, sym->name) == 0) {
                            is_import = 1;
                            break;
                        }
                    }
                }
                if (is_import) continue;

                
                const char** new_undef_names = (const char**)realloc(undef_names, (undef_count + 1) * sizeof(char*));
                if (!new_undef_names) {
                    free(undef_names);
                    return ARK_LINK_ERR_MEMORY;
                }
                undef_names = new_undef_names;
                undef_names[undef_count++] = sym->name;
            }
        }

        if (undef_count == 0) break;

        
        
        for (size_t i = 0; i < job->library_count; i++) {
            char lib_path[512];
            if (ark_loader_find_library(job->libraries[i], job->library_paths, job->library_path_count, lib_path, sizeof(lib_path)) == ARK_LINK_OK) {
                ArkArchive* archive = NULL;
                if (ark_loader_load_archive(state->ctx, lib_path, &archive, NULL) == ARK_LINK_OK) {
                    ArkLinkUnit** extracted_units = NULL;
                    size_t extracted_count = 0;
                    if (ark_archive_extract_needed(state->ctx, archive, undef_names, undef_count, &extracted_units, &extracted_count) == ARK_LINK_OK && extracted_count > 0) {
                        
                        ArkLinkUnit** new_units = (ArkLinkUnit**)realloc(state->units, (state->unit_count + extracted_count) * sizeof(ArkLinkUnit*));
                        if (!new_units) {
                            free(extracted_units);
                            free(undef_names);
                            return ARK_LINK_ERR_MEMORY;
                        }
                        state->units = new_units;
                        for (size_t j = 0; j < extracted_count; j++) {
                            state->units[state->unit_count++] = extracted_units[j];
                        }
                        free(extracted_units);
                        progress = 1;
                        
                        
                        ark_symtab_free(&state->symtab);
                        free(state->plan->symbols);
                        state->plan->symbols = NULL;
                        state->plan->symbol_count = 0;
                        ark_resolver_collect_symbols(state);
                    }
                    ark_archive_destroy(state->ctx, archive);
                }
            }
            if (progress) break;
        }
        free(undef_names);
    }

    for (size_t i = 0; i < state->plan->symbol_count; i++) {
        ArkResolverSymbol* sym = &state->plan->symbols[i];

        
        if (sym->section == NULL && sym->binding == ARK_BIND_GLOBAL) {
             if (config && config->imports) {
                for (size_t k = 0; k < config->import_count; k++) {
                    if (strcmp(config->imports[k].symbol, sym->name) == 0) {
                        sym->binding = ARK_BIND_WEAK;  
                        sym->import_id = (int32_t)k; 
                        break;
                    }
                }
            }
        }

        if (sym->binding == ARK_BIND_WEAK ||
            (sym->section == NULL && sym->binding != ARK_BIND_LOCAL)) {
            
            ArkSymbolEntry* entry = ark_symtab_lookup(&state->symtab, sym->name);
            if (entry && entry->symbol != sym) {
                ArkResolverSymbol* def = entry->symbol;
                if (def->binding != ARK_BIND_WEAK && def->section != NULL) {
                    
                    sym->binding = def->binding;
                    sym->section = def->section;
                    sym->section_index = def->section_index;
                    sym->value = def->value;
                    sym->size = def->size;
                    continue;
                }
            }
            
            
            int is_import = 0;
            if (config && config->imports) {
                for (size_t k = 0; k < config->import_count; k++) {
                    if (strcmp(config->imports[k].symbol, sym->name) == 0) {
                        is_import = 1;
                        break;
                    }
                }
            }

            
            if (sym->binding == ARK_BIND_WEAK || is_import) {
                sym->binding = ARK_BIND_WEAK; 
                continue;
            }
            
            ark_context_log(state->ctx, ARK_LOG_ERROR,
                "Undefined symbol: %s", sym->name);
            has_undefined = 1;
        }
    }
    
    if (has_undefined) {
        return ARK_LINK_ERR_UNRESOLVED_SYMBOL;
    }
    
    return ARK_LINK_OK;
}


static ArkLinkResult ark_resolver_build_import_plan(ArkResolverState* state) {
    const ArkParsedConfig* config = ark_context_config(state->ctx);

    
    size_t import_count = 0;
    for (size_t i = 0; i < state->plan->symbol_count; i++) {
        ArkResolverSymbol* sym = &state->plan->symbols[i];
        if (sym->binding == ARK_BIND_WEAK) {
            import_count++;
        }
    }
    
    if (import_count == 0) {
        state->plan->imports = NULL;
        state->plan->import_count = 0;
        return ARK_LINK_OK;
    }
    
    
    state->plan->imports = (ArkImportBinding*)calloc(import_count, sizeof(ArkImportBinding));
    if (!state->plan->imports) {
        return ARK_LINK_ERR_INTERNAL;
    }
    
    
    size_t slot = 0;
    for (size_t i = 0; i < state->plan->symbol_count; i++) {
        ArkResolverSymbol* sym = &state->plan->symbols[i];
        if (sym->binding == ARK_BIND_WEAK) {
            ArkImportBinding* imp = &state->plan->imports[slot];
            
            
            const char* module_name = "unknown";
            if (config && config->imports) {
                for (size_t k = 0; k < config->import_count; k++) {
                    if (strcmp(config->imports[k].symbol, sym->name) == 0) {
                        module_name = config->imports[k].module;
                        
                        sym->import_id = (int32_t)slot;
                        break;
                    }
                }
            }

            imp->module = module_name;
            imp->symbol = sym->name;
            imp->slot = (uint32_t)slot;
            slot++;
        }
    }
    
    state->plan->import_count = slot;
    return ARK_LINK_OK;
}


static ArkLinkResult ark_resolver_collect_exports(ArkResolverState* state) {
    const ArkParsedConfig* config = ark_context_config(state->ctx);
    if (!config || config->export_symbol_count == 0) {
        state->plan->exports = NULL;
        state->plan->export_count = 0;
        return ARK_LINK_OK;
    }

    state->plan->exports = (ArkExportBinding*)calloc(config->export_symbol_count, sizeof(ArkExportBinding));
    if (!state->plan->exports) {
        return ARK_LINK_ERR_INTERNAL;
    }

    size_t cursor = 0;
    for (size_t i = 0; i < config->export_symbol_count; i++) {
        const char* export_name = config->export_symbols[i];
        
        
        ArkSymbolEntry* entry = ark_symtab_lookup(&state->symtab, export_name);
        if (!entry) {
            ark_context_log(state->ctx, ARK_LOG_WARN,
                "Exported symbol not found: %s", export_name);
            continue;
        }

        entry->symbol->is_export = 1;

        ArkExportBinding* exp = &state->plan->exports[cursor];
        exp->name = export_name;
        exp->symbol_index = (uint32_t)entry->symbol_index; 
        







        exp->symbol_index = (uint32_t)entry->symbol_index;
        exp->ordinal = (uint32_t)(cursor + 1); 
        cursor++;
    }

    state->plan->export_count = cursor;
    return ARK_LINK_OK;
}


static ArkLinkResult ark_resolver_collect_relocs(ArkResolverState* state) {
    size_t total = 0;
    for (size_t i = 0; i < state->unit_count; ++i) {
        total += state->units[i]->relocations.count;
    }
    
    if (total == 0) {
        state->plan->relocs = NULL;
        state->plan->reloc_count = 0;
        return ARK_LINK_OK;
    }
    
    state->plan->relocs = (ArkResolverReloc*)calloc(total, sizeof(ArkResolverReloc));
    if (!state->plan->relocs) {
        return ARK_LINK_ERR_INTERNAL;
    }
    
    size_t cursor = 0;
    size_t symbol_offset = 0;

    for (size_t u = 0; u < state->unit_count; ++u) {
        ArkLinkUnit* unit = state->units[u];

        for (size_t r = 0; r < unit->relocations.count; ++r) {
            ArkRelocationDesc* src = &unit->relocations.relocs[r];
            ArkResolverReloc* dst = &state->plan->relocs[cursor++];

            

            dst->section_index = src->section_index;
            dst->offset = (uint32_t)src->offset;
            dst->type = src->type;
            dst->addend = src->addend;
#ifdef ARKLINK_DEBUG
            fprintf(stderr, "[DEBUG] ark_resolver_collect_relocs: src->addend=%d, dst->addend=%d\n",
                    src->addend, dst->addend);
#endif

            
            if (src->section_index > 0 && src->section_index <= unit->section_count) {
                dst->section = unit->sections[src->section_index - 1].buffer;
            } else {
                dst->section = NULL;
            }

            
            size_t symbol_index = symbol_offset + src->sym_idx;
            if (symbol_index >= state->plan->symbol_count) {
                return ARK_LINK_ERR_FORMAT;
            }

            
            if (src->sym_idx < unit->symbol_count) {
                ArkSymbolDesc* target_desc = &unit->symbols[src->sym_idx];
                ArkSymbolEntry* entry = ark_symtab_lookup(&state->symtab, target_desc->name);

                if (entry) {
                    dst->symbol = entry->symbol;
                } else {
                    
                    dst->symbol = &state->plan->symbols[symbol_index];
                }
            } else {
                
                dst->symbol = &state->plan->symbols[symbol_index];
            }
        }

        symbol_offset += unit->symbol_count;
    }
    
    state->plan->reloc_count = cursor;
    return ARK_LINK_OK;
}


static void ark_resolver_find_entry(ArkResolverState* state) {
    state->plan->entry_symbol = 0;
    state->plan->entry_section = 0;
    state->plan->entry_offset = 0;
    
    
    const char* entry_names[] = {"main", "_start", "WinMain", "wmain"};
    size_t entry_name_lens[] = {4, 6, 7, 5};
    size_t num_entries = sizeof(entry_names) / sizeof(entry_names[0]);
    
    for (size_t i = 0; i < state->plan->symbol_count; i++) {
        ArkResolverSymbol* sym = &state->plan->symbols[i];
#ifdef ARKLINK_DEBUG
        fprintf(stderr, "[DEBUG] Symbol %zu: %s, section_index=%u, value=0x%llX\n",
                i, sym->name, sym->section_index, (unsigned long long)sym->value);
#endif
        for (size_t e = 0; e < num_entries; e++) {
            if (strncmp(sym->name, entry_names[e], entry_name_lens[e]) == 0 &&
                sym->name[entry_name_lens[e]] == '\0') {
#ifdef ARKLINK_DEBUG
                fprintf(stderr, "[DEBUG] Found entry symbol: %s, section_index=%u, value=0x%llX\n",
                        sym->name, sym->section_index, (unsigned long long)sym->value);
#endif
                state->plan->entry_symbol = (uint32_t)i;
                state->plan->entry_section = sym->section_index;
                state->plan->entry_offset = sym->value;
                return;
            }
        }
    }
    
    
    if (state->unit_count > 0 && state->units[0]->entry_point != 0) {
        
        for (size_t i = 0; i < state->plan->symbol_count; i++) {
            ArkResolverSymbol* sym = &state->plan->symbols[i];
            if (sym->value == state->units[0]->entry_point && sym->section != NULL) {
                state->plan->entry_symbol = (uint32_t)i;
                state->plan->entry_section = sym->section_index;
                state->plan->entry_offset = sym->value;
                return;
            }
        }
    }
}


static ArkLinkResult ark_resolver_build_backend_input(ArkResolverState* state) {
    ArkBackendInput* input = (ArkBackendInput*)calloc(1, sizeof(ArkBackendInput));
    if (!input) {
        return ARK_LINK_ERR_INTERNAL;
    }
    state->plan->backend_input = input;
    
    
    size_t section_total = 0;
    for (size_t i = 0; i < state->unit_count; ++i) {
        section_total += state->units[i]->section_count;
    }
    
    if (section_total > 0) {
        input->sections = (ArkBackendInputSection*)calloc(section_total, sizeof(ArkBackendInputSection));
        if (!input->sections) {
            return ARK_LINK_ERR_INTERNAL;
        }
        input->section_count = section_total;
        
        size_t cursor = 0;
        for (size_t u = 0; u < state->unit_count; ++u) {
            ArkLinkUnit* unit = state->units[u];
            for (size_t s = 0; s < unit->section_count; ++s) {
                ArkLinkSection* src = &unit->sections[s];
                ArkBackendInputSection* dst = &input->sections[cursor];
                dst->name = src->name;
                dst->buffer = src->buffer;
                dst->id = (uint32_t)cursor;
                ++cursor;
            }
        }
    }
    
    
    if (state->plan->symbol_count > 0) {
        input->symbols = (ArkBackendInputSymbol*)calloc(state->plan->symbol_count, sizeof(ArkBackendInputSymbol));
        if (!input->symbols) {
            return ARK_LINK_ERR_INTERNAL;
        }
        input->symbol_count = state->plan->symbol_count;
        
        for (size_t i = 0; i < state->plan->symbol_count; ++i) {
            ArkResolverSymbol* src = &state->plan->symbols[i];
            ArkBackendInputSymbol* dst = &input->symbols[i];
            dst->name = src->name;
            
            dst->section_id = (src->section_index == 0) ? 0xFFFFFFFF : (src->section_index - 1);
            dst->value = src->value;
            dst->size = src->size;
            dst->binding = src->binding;
            dst->visibility = src->visibility;
            dst->import_id = src->import_id;
        }
    }
    
    
    if (state->plan->reloc_count > 0) {
        input->relocs = (ArkBackendInputReloc*)calloc(state->plan->reloc_count, sizeof(ArkBackendInputReloc));
        if (!input->relocs) {
            return ARK_LINK_ERR_INTERNAL;
        }
        input->reloc_count = state->plan->reloc_count;
        
        for (size_t i = 0; i < state->plan->reloc_count; ++i) {
            ArkResolverReloc* src = &state->plan->relocs[i];
            ArkBackendInputReloc* dst = &input->relocs[i];
            dst->section_id = src->section_index;
            dst->offset = src->offset;
            dst->type = src->type;
            dst->addend = src->addend;
            
            
            if (src->symbol && state->plan->symbols) {
                dst->symbol_index = (uint32_t)(src->symbol - state->plan->symbols);
            } else {
                dst->symbol_index = 0;
            }
        }
    }
    
    
    if (state->plan->import_count > 0) {
        input->imports = (ArkBackendInputImport*)calloc(state->plan->import_count, sizeof(ArkBackendInputImport));
        if (!input->imports) {
            return ARK_LINK_ERR_INTERNAL;
        }
        input->import_count = state->plan->import_count;
        
        for (size_t i = 0; i < state->plan->import_count; ++i) {
            ArkImportBinding* src = &state->plan->imports[i];
            ArkBackendInputImport* dst = &input->imports[i];
            dst->module = src->module;
            dst->symbol = src->symbol;
            dst->slot = src->slot;
        }
    }

    
    if (state->plan->export_count > 0) {
        input->exports = (ArkBackendInputExport*)calloc(state->plan->export_count, sizeof(ArkBackendInputExport));
        if (!input->exports) {
            return ARK_LINK_ERR_INTERNAL;
        }
        input->export_count = state->plan->export_count;

        for (size_t i = 0; i < state->plan->export_count; ++i) {
            ArkExportBinding* src = &state->plan->exports[i];
            ArkBackendInputExport* dst = &input->exports[i];
            dst->name = src->name;
            dst->symbol_index = src->symbol_index;
            dst->ordinal = src->ordinal;
        }
    }
    
    
    input->entry_section = state->plan->entry_section;
    input->entry_offset = state->plan->entry_offset;
    
    return ARK_LINK_OK;
}

ArkLinkResult ark_resolver_resolve(ArkLinkContext* ctx, ArkLinkUnit* const* units, size_t unit_count, ArkResolverPlan* out_plan) {
    if (!ctx || !units || !out_plan) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    memset(out_plan, 0, sizeof(*out_plan));
    
    ArkResolverState state = {
        .ctx = ctx,
        .units = (ArkLinkUnit**)malloc(unit_count * sizeof(ArkLinkUnit*)),
        .unit_count = unit_count,
        .unit_capacity = unit_count,
        .plan = out_plan,
        .symtab = {NULL, 0},
    };
    memcpy(state.units, units, unit_count * sizeof(ArkLinkUnit*));
    
    
    ArkLinkResult res = ark_resolver_collect_symbols(&state);
    if (res != ARK_LINK_OK) {
        ark_symtab_free(&state.symtab);
        return res;
    }
    
    
    res = ark_resolver_check_undefined(&state);
    if (res != ARK_LINK_OK) {
        ark_symtab_free(&state.symtab);
        return res;
    }
    
    
    res = ark_resolver_build_import_plan(&state);
    if (res != ARK_LINK_OK) {
        ark_symtab_free(&state.symtab);
        return res;
    }

    
    res = ark_resolver_collect_exports(&state);
    if (res != ARK_LINK_OK) {
        ark_symtab_free(&state.symtab);
        return res;
    }
    
    
    res = ark_resolver_collect_relocs(&state);
    if (res != ARK_LINK_OK) {
        ark_symtab_free(&state.symtab);
        return res;
    }
    
    
    ark_resolver_find_entry(&state);
    
    
    res = ark_resolver_build_backend_input(&state);
    
    ark_symtab_free(&state.symtab);
    free(state.units);
    return res;
}

void ark_resolver_plan_destroy(ArkLinkContext* ctx, ArkResolverPlan* plan) {
    (void)ctx;
    if (!plan) {
        return;
    }
    if (plan->backend_input) {
        free(plan->backend_input->sections);
        free(plan->backend_input->symbols);
        free(plan->backend_input->relocs);
        free(plan->backend_input->imports);
        free(plan->backend_input);
    }
    free(plan->symbols);
    free(plan->relocs);
    free(plan->imports);
    memset(plan, 0, sizeof(*plan));
}
