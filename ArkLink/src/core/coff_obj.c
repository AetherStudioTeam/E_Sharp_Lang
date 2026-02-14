#include "coff_obj.h"
#include "ArkLink/context.h"
#include "ArkLink/loader.h"
#include <stdlib.h>
#include <string.h>


#define IMAGE_FILE_MACHINE_I386  0x14c
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_FILE_MACHINE_ARM64 0xaa64


#define IMAGE_SYM_CLASS_EXTERNAL 2
#define IMAGE_SYM_CLASS_STATIC   3
#define IMAGE_SYM_CLASS_LABEL    6


#define IMAGE_SYM_UNDEFINED 0
#define IMAGE_SYM_ABSOLUTE  -1
#define IMAGE_SYM_DEBUG     -2


#define IMAGE_REL_AMD64_ADDR64 1
#define IMAGE_REL_AMD64_ADDR32 2
#define IMAGE_REL_AMD64_REL32  4

bool ark_coff_is_valid(const uint8_t* data, size_t size) {
    if (size < sizeof(ArkCOFFFileHeader)) {
        return false;
    }
    
    const ArkCOFFFileHeader* hdr = (const ArkCOFFFileHeader*)data;
    
    
    if (hdr->Machine != IMAGE_FILE_MACHINE_AMD64 &&
        hdr->Machine != IMAGE_FILE_MACHINE_I386 &&
        hdr->Machine != IMAGE_FILE_MACHINE_ARM64) {
        return false;
    }
    
    
    if (hdr->PointerToSymbolTable >= size) {
        return false;
    }
    
    return true;
}

const char* ark_coff_get_symbol_name(const ArkCOFFObject* obj, const ArkCOFFSymbol* sym) {
    static char short_name[9];
    
    if (sym->Name.LongName.Zeroes != 0) {
        
        memcpy(short_name, sym->Name.ShortName, 8);
        short_name[8] = '\0';
        return short_name;
    } else {
        
        if (obj->string_table && sym->Name.LongName.Offset < obj->string_table_size) {
            return (const char*)obj->string_table + sym->Name.LongName.Offset;
        }
    }
    
    return "";
}

int ark_coff_parse(const uint8_t* data, size_t size, ArkCOFFObject* obj) {
    if (!data || !obj || size < sizeof(ArkCOFFFileHeader)) {
        return -1;
    }
    
    memset(obj, 0, sizeof(ArkCOFFObject));
    
    
    obj->raw_data = malloc(size);
    if (!obj->raw_data) {
        return -1;
    }
    memcpy(obj->raw_data, data, size);
    obj->raw_size = size;
    
    
    memcpy(&obj->header, obj->raw_data, sizeof(ArkCOFFFileHeader));
    
    
    if (obj->header.NumberOfSections > 0) {
        obj->sections = calloc(obj->header.NumberOfSections, sizeof(ArkCOFFSectionHeader));
        obj->section_data = calloc(obj->header.NumberOfSections, sizeof(uint8_t*));
        
        if (!obj->sections || !obj->section_data) {
            ark_coff_free(obj);
            return -1;
        }
        
        size_t section_offset = sizeof(ArkCOFFFileHeader) + obj->header.SizeOfOptionalHeader;
        for (uint16_t i = 0; i < obj->header.NumberOfSections; i++) {
            memcpy(&obj->sections[i], obj->raw_data + section_offset, sizeof(ArkCOFFSectionHeader));
            section_offset += sizeof(ArkCOFFSectionHeader);
            
            
            if (obj->sections[i].PointerToRawData > 0 && obj->sections[i].SizeOfRawData > 0) {
                if (obj->sections[i].PointerToRawData + obj->sections[i].SizeOfRawData <= size) {
                    obj->section_data[i] = obj->raw_data + obj->sections[i].PointerToRawData;
                }
            }
        }
    }
    
    
    if (obj->header.NumberOfSymbols > 0 && obj->header.PointerToSymbolTable > 0) {
        obj->symbols = calloc(obj->header.NumberOfSymbols, sizeof(ArkCOFFSymbol));
        if (!obj->symbols) {
            ark_coff_free(obj);
            return -1;
        }
        
        size_t sym_offset = obj->header.PointerToSymbolTable;
        for (uint32_t i = 0; i < obj->header.NumberOfSymbols; i++) {
            memcpy(&obj->symbols[i], obj->raw_data + sym_offset, sizeof(ArkCOFFSymbol));
            sym_offset += sizeof(ArkCOFFSymbol);
        }
        
        
        size_t string_table_offset = obj->header.PointerToSymbolTable + 
                                     obj->header.NumberOfSymbols * sizeof(ArkCOFFSymbol);
        if (string_table_offset + 4 <= size) {
            uint32_t string_table_size;
            memcpy(&string_table_size, obj->raw_data + string_table_offset, sizeof(uint32_t));
            if (string_table_size > 4 && string_table_offset + string_table_size <= size) {
                obj->string_table = obj->raw_data + string_table_offset;
                obj->string_table_size = string_table_size;
            }
        }
    }
    
    return 0;
}

void ark_coff_free(ArkCOFFObject* obj) {
    if (!obj) return;
    
    free(obj->sections);
    free(obj->section_data);
    free(obj->symbols);
    free(obj->raw_data);
    
    memset(obj, 0, sizeof(ArkCOFFObject));
}


static int coff_section_kind(uint32_t characteristics) {
    if (characteristics & 0x20000000) { 
        return 0; 
    } else if (characteristics & 0x40000000) { 
        return 1; 
    } else if (characteristics & 0x80000000) { 
        return 2; 
    }
    return 1; 
}


static uint32_t coff_section_flags(uint32_t characteristics) {
    uint32_t flags = 0;
    if (characteristics & 0x20000000) { 
        flags |= ARK_SECTION_EXEC;
    }
    if (characteristics & 0x80000000) { 
        flags |= ARK_SECTION_WRITE;
    }
    flags |= ARK_SECTION_READ;
    return flags;
}


static int coff_reloc_type(uint16_t coff_type) {
    switch (coff_type) {
        case IMAGE_REL_AMD64_ADDR64: return ARK_RELOC_ABS64;
        case IMAGE_REL_AMD64_ADDR32: return ARK_RELOC_ABS64; 
        case IMAGE_REL_AMD64_REL32:  return ARK_RELOC_PC32;
        default: return ARK_RELOC_ABS64;
    }
}

ArkLinkResult ark_coff_load_unit(ArkLinkContext* ctx, const uint8_t* data, size_t size,
                                  const char* path, ArkLinkUnit** out_unit) {
    (void)ctx;
    
    if (!path || !data || !out_unit) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }

    *out_unit = NULL;

    
    ArkCOFFObject coff;
    if (ark_coff_parse(data, size, &coff) != 0) {
        return ARK_LINK_ERR_FORMAT;
    }

    
    ArkLinkUnit* unit = ark_link_unit_create(path);
    if (!unit) {
        ark_coff_free(&coff);
        return ARK_LINK_ERR_INTERNAL;
    }

    
    for (uint16_t i = 0; i < coff.header.NumberOfSections; i++) {
        ArkCOFFSectionHeader* sec = &coff.sections[i];
        
        
        char sec_name[9];
        memcpy(sec_name, sec->Name, 8);
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
        if (coff.section_data[i] && sec->SizeOfRawData > 0) {
            sec_data = coff.section_data[i];
            sec_size = sec->SizeOfRawData;
        }
        
        
        size_t alignment = 1;
        if (sec->Characteristics & 0x00F00000) {
            alignment = 1 << (((sec->Characteristics >> 20) & 0xF) - 1);
        }
        
        
        ArkSectionDesc sdesc = {
            .name = sec_name,
            .data = sec_data,
            .size = sec_size,
            .alignment = alignment,
            .flags = coff_section_flags(sec->Characteristics)
        };
        
        ArkLinkSection* section = ark_link_unit_add_section(unit, &sdesc);
        if (!section) {
            ark_link_unit_destroy(unit);
            ark_coff_free(&coff);
            return ARK_LINK_ERR_INTERNAL;
        }
        
        
        if (sec->NumberOfRelocations > 0 && sec->PointerToRelocations > 0) {
            size_t reloc_offset = sec->PointerToRelocations;
            for (uint16_t r = 0; r < sec->NumberOfRelocations; r++) {
                ArkCOFFRelocation coff_reloc;
                memcpy(&coff_reloc, coff.raw_data + reloc_offset, sizeof(ArkCOFFRelocation));
                reloc_offset += sizeof(ArkCOFFRelocation);
                
                ArkRelocationDesc rdesc = {
                    .offset = coff_reloc.VirtualAddress,
                    .sym_idx = coff_reloc.SymbolTableIndex,
                    .type = coff_reloc_type(coff_reloc.Type),
                    .addend = 0
                };
                
                if (!ark_link_section_add_reloc(section, &rdesc)) {
                    ark_link_unit_destroy(unit);
                    ark_coff_free(&coff);
                    return ARK_LINK_ERR_INTERNAL;
                }
            }
        }
    }

    
    for (uint32_t i = 0; i < coff.header.NumberOfSymbols; i++) {
        ArkCOFFSymbol* coff_sym = &coff.symbols[i];
        
        const char* name = ark_coff_get_symbol_name(&coff, coff_sym);
        
        
        uint8_t binding;
        if (coff_sym->StorageClass == IMAGE_SYM_CLASS_EXTERNAL) {
            binding = ARK_BIND_GLOBAL;
        } else {
            binding = ARK_BIND_LOCAL;
        }
        
        
        uint16_t section_index = 0;
        if (coff_sym->SectionNumber == IMAGE_SYM_UNDEFINED) {
            section_index = 0; 
        } else if (coff_sym->SectionNumber > 0 && coff_sym->SectionNumber <= coff.header.NumberOfSections) {
            section_index = coff_sym->SectionNumber - 1; 
        } else {
            section_index = 0; 
        }
        
        ArkSymbolDesc sdesc = {
            .name = name,
            .value = coff_sym->Value,
            .size = 0,  
            .section_index = section_index,
            .type = ARK_SYM_NOTYPE,
            .binding = binding,
            .visibility = ARK_VISIBILITY_DEFAULT
        };
        
        if (!ark_link_unit_add_symbol(unit, &sdesc)) {
            ark_link_unit_destroy(unit);
            ark_coff_free(&coff);
            return ARK_LINK_ERR_INTERNAL;
        }
        
        i += coff_sym->NumberOfAuxSymbols;
    }

    
    for (size_t i = 0; i < unit->symbol_count; i++) {
        if (strcmp(unit->symbols[i].name, "main") == 0 ||
            strcmp(unit->symbols[i].name, "_start") == 0) {
            unit->entry_point = unit->symbols[i].value;
            break;
        }
    }

    *out_unit = unit;
    ark_coff_free(&coff);
    return ARK_LINK_OK;
}
