




#include "ArkLink/loader.h"
#include "ArkLink/context.h"
#include "archive.h"
#include "coff_obj.h"

#include <stdlib.h>
#include <string.h>


static uint32_t coff_reloc_type(uint16_t coff_type) {
    switch (coff_type) {
        case IMAGE_REL_AMD64_ADDR64: return 0; 
        case IMAGE_REL_AMD64_ADDR32: return 0; 
        case IMAGE_REL_AMD64_REL32:  return 1; 
        default: return 0;
    }
}


static ArkLinkResult coff_to_unit(ArkLinkContext* ctx, const ArkCOFFObject* coff, 
                                   const char* name, ArkLinkUnit** out_unit) {
    (void)ctx;
    
    ArkLinkUnit* unit = ark_link_unit_create(name);
    if (!unit) {
        return ARK_LINK_ERR_INTERNAL;
    }
    
    
    for (uint16_t i = 0; i < coff->header.NumberOfSections; i++) {
        
        char sec_name[9];
        memcpy(sec_name, coff->sections[i].Name, 8);
        sec_name[8] = '\0';
        
        for (int j = 7; j >= 0; j--) {
            if (sec_name[j] == ' ') {
                sec_name[j] = '\0';
            } else {
                break;
            }
        }
        
        
        const uint8_t* sec_data = NULL;
        size_t sec_size = 0;
        if (coff->section_data[i]) {
            sec_data = coff->section_data[i];
            sec_size = coff->sections[i].SizeOfRawData;
        }
        
        
        uint32_t flags = ARK_SECTION_READ;
        if (coff->sections[i].Characteristics & 0x20000000) { 
            flags |= ARK_SECTION_EXEC;
        }
        if (coff->sections[i].Characteristics & 0x80000000) { 
            flags |= ARK_SECTION_WRITE;
        }
        
        ArkSectionDesc sdesc = {
            .name = sec_name,
            .data = sec_data,
            .size = sec_size,
            .alignment = 16,  
            .flags = flags
        };
        
        ArkLinkSection* section = ark_link_unit_add_section(unit, &sdesc);
        if (!section) {
            ark_link_unit_destroy(unit);
            return ARK_LINK_ERR_INTERNAL;
        }
        
        
        if (coff->sections[i].NumberOfRelocations > 0 && 
            coff->sections[i].PointerToRelocations > 0) {
            
            size_t reloc_offset = coff->sections[i].PointerToRelocations;
            for (uint16_t r = 0; r < coff->sections[i].NumberOfRelocations; r++) {
                ArkCOFFRelocation coff_reloc;
                memcpy(&coff_reloc, coff->raw_data + reloc_offset, sizeof(ArkCOFFRelocation));
                reloc_offset += sizeof(ArkCOFFRelocation);
                
                ArkRelocationDesc rdesc = {
                    .offset = coff_reloc.VirtualAddress,
                    .sym_idx = coff_reloc.SymbolTableIndex,
                    .type = (uint16_t)coff_reloc_type(coff_reloc.Type),
                    .addend = 0
                };
                
                if (!ark_link_section_add_reloc(section, &rdesc)) {
                    ark_link_unit_destroy(unit);
                    return ARK_LINK_ERR_INTERNAL;
                }
            }
        }
    }
    
    
    for (uint32_t i = 0; i < coff->header.NumberOfSymbols; i++) {
        const ArkCOFFSymbol* coff_sym = &coff->symbols[i];
        
        
        const char* sym_name = ark_coff_get_symbol_name(coff, coff_sym);
        
        
        uint8_t binding;
        if (coff_sym->StorageClass == 2) { 
            binding = ARK_BIND_GLOBAL;
        } else if (coff_sym->StorageClass == 3) { 
            binding = ARK_BIND_LOCAL;
        } else {
            binding = ARK_BIND_LOCAL;
        }
        
        
        uint16_t sec_idx = 0;
        if (coff_sym->SectionNumber > 0) {
            sec_idx = coff_sym->SectionNumber - 1; 
        }
        
        ArkSymbolDesc sdesc = {
            .name = sym_name,
            .value = coff_sym->Value,
            .size = 0,
            .section_index = sec_idx,
            .type = ARK_SYM_NOTYPE,
            .binding = binding,
            .visibility = ARK_VISIBILITY_DEFAULT
        };
        
        if (!ark_link_unit_add_symbol(unit, &sdesc)) {
            ark_link_unit_destroy(unit);
            return ARK_LINK_ERR_INTERNAL;
        }
        
        i += coff_sym->NumberOfAuxSymbols;
    }
    
    
    for (size_t i = 0; i < unit->symbol_count; i++) {
        if (strcmp(unit->symbols[i].name, "_start") == 0 ||
            strcmp(unit->symbols[i].name, "mainCRTStartup") == 0) {
            unit->entry_point = unit->symbols[i].value;
            break;
        }
    }
    
    *out_unit = unit;
    return ARK_LINK_OK;
}





ArkLinkResult ark_loader_load_archive(ArkLinkContext* ctx, const char* path, 
                                       ArkArchive** out_archive, 
                                       ArkLoaderDiagnostics* diag) {
    (void)diag;
    
    if (!ctx || !path || !out_archive) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    *out_archive = NULL;
    
    
    if (!ark_archive_is_archive(path)) {
        return ARK_LINK_ERR_FORMAT;
    }
    
    
    ArkArchive* ar = NULL;
    ArkArchiveResult result = ark_archive_open(path, &ar);
    if (result != ARK_ARCHIVE_OK) {
        return ARK_LINK_ERR_FORMAT;
    }

    *out_archive = ar;
    
    return ARK_LINK_OK;
}

size_t ark_archive_member_count(ArkArchive* archive) {
    if (!archive) return 0;
    return archive->object_count;
}

const char* ark_archive_member_name(ArkArchive* archive, size_t index) {
    if (!archive) return NULL;
    if (index >= archive->object_count) return NULL;
    
    return archive->objects[index].name;
}

ArkLinkResult ark_archive_extract_unit(ArkLinkContext* ctx, ArkArchive* archive, 
                                        size_t index, ArkLinkUnit** out_unit) {
    if (!archive || !out_unit) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    if (index >= archive->object_count) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    
    const uint8_t* obj_data = archive->objects[index].data;
    size_t obj_size = archive->objects[index].size;
    const char* obj_name = archive->objects[index].name;
    
    
    ArkCOFFObject coff;
    int result = ark_coff_parse(obj_data, obj_size, &coff);
    if (result != 0) {
        return ARK_LINK_ERR_FORMAT;
    }
    
    
    return coff_to_unit(ctx, &coff, obj_name, out_unit);
}

ArkLinkResult ark_archive_extract_needed(ArkLinkContext* ctx, ArkArchive* archive,
                                          const char* const* undefined_symbols, 
                                          size_t undef_count,
                                          ArkLinkUnit*** out_units, 
                                          size_t* out_unit_count) {
    if (!archive || !undefined_symbols || !out_units || !out_unit_count) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    *out_units = NULL;
    *out_unit_count = 0;
    
    ArkLinkUnit** units = NULL;
    size_t unit_count = 0;
    
    
    for (size_t i = 0; i < archive->object_count; i++) {
        ArkLinkUnit* unit = NULL;
        ArkLinkResult result = ark_archive_extract_unit(ctx, archive, i, &unit);
        if (result != ARK_LINK_OK) {
            continue;
        }
        
        
        bool needed = false;
        for (size_t s = 0; s < unit->symbol_count && !needed; s++) {
            if (unit->symbols[s].binding == ARK_BIND_GLOBAL) {
                for (size_t u = 0; u < undef_count; u++) {
                    if (strcmp(unit->symbols[s].name, undefined_symbols[u]) == 0) {
                        needed = true;
                        break;
                    }
                }
            }
        }
        
        if (needed) {
            ArkLinkUnit** new_units = realloc(units, (unit_count + 1) * sizeof(ArkLinkUnit*));
            if (new_units) {
                units = new_units;
                units[unit_count++] = unit;
            } else {
                ark_link_unit_destroy(unit);
            }
        } else {
            ark_link_unit_destroy(unit);
        }
    }
    
    *out_units = units;
    *out_unit_count = unit_count;
    
    return ARK_LINK_OK;
}

ArkLinkResult ark_archive_list_symbols(ArkArchive* archive,
                                        const char*** out_symbols, 
                                        size_t* out_count) {
    if (!archive || !out_symbols || !out_count) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    *out_symbols = NULL;
    *out_count = 0;
    
    (void)archive;
    
    
    return ARK_LINK_OK;
}

void ark_archive_destroy(ArkLinkContext* ctx, ArkArchive* archive) {
    (void)ctx;
    if (archive) {
        ark_archive_close(archive);
    }
}


