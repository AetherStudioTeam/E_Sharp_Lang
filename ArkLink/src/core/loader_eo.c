#include "ArkLink/loader.h"
#include "ArkLink/context.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>









#define EO_MAGIC    0x4523454Fu  
#define EO_VERSION  1


#define EO_ARCH_X86_64  0x8664
#define EO_ARCH_ARM64   0xAA64


#define EO_SEC_COUNT    4

typedef enum {
    EO_SEC_TEXT   = 0,  
    EO_SEC_DATA   = 1,  
    EO_SEC_RODATA = 2,  
    EO_SEC_BSS    = 3,  
} EOSectionIndex;


#define EO_SECF_READ    0x01
#define EO_SECF_WRITE   0x02
#define EO_SECF_EXEC    0x04
#define EO_SECF_BSS     0x08  


#define EO_SYM_NOTYPE   0
#define EO_SYM_FUNC     1
#define EO_SYM_OBJECT   2


#define EO_BIND_LOCAL   0
#define EO_BIND_GLOBAL  1
#define EO_BIND_WEAK    2


#define EO_RELOC_ABS64  0   
#define EO_RELOC_PC32   1   
#define EO_RELOC_GOT32  2   
#define EO_RELOC_SECREL 3   





#pragma pack(push, 1)


typedef struct {
    uint32_t magic;           
    uint16_t version;         
    uint16_t flags;           
    uint16_t arch;            
    uint16_t reserved;        
    
    uint32_t sec_count;       
    uint32_t sym_count;       
    
    uint64_t strtab_size;     
    uint64_t entry_point;     
} EOHeader;


typedef struct {
    char     name[8];         
    uint8_t  align_log2;      
    uint8_t  flags;           
    uint16_t reserved;        
    
    uint32_t file_offset;     
    uint32_t file_size;       
    uint32_t mem_size;        
    uint32_t reloc_count;     
    uint32_t reloc_offset;    
} EOSection;


typedef struct {
    char     name[24];        
    uint64_t value;           
    uint32_t sec_idx;         
    uint8_t  type;            
    uint8_t  bind;            
    uint16_t reserved;        
} EOSymbol;


typedef struct {
    uint64_t offset;          
    uint32_t sym_idx;         
    uint16_t type;            
    int16_t addend;           
} EOReloc;

#pragma pack(pop)





typedef struct EOReader {
    const uint8_t* data;
    size_t size;
    size_t offset;
} EOReader;

static ArkLinkResult eo_read_file(const char* path, uint8_t** out_data, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return ARK_LINK_ERR_IO;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ARK_LINK_ERR_IO;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return ARK_LINK_ERR_IO;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return ARK_LINK_ERR_IO;
    }
    uint8_t* data = (uint8_t*)malloc((size_t)size);
    if (!data) {
        fclose(f);
        return ARK_LINK_ERR_MEMORY;
    }
    size_t read_size = fread(data, 1, (size_t)size, f);
    if (read_size != (size_t)size) {
        free(data);
        fclose(f);
        return ARK_LINK_ERR_IO;
    }
    fclose(f);
    *out_data = data;
    *out_size = (size_t)size;
    return ARK_LINK_OK;
}

static int eo_reader_read(EOReader* reader, void* out, size_t size) {
    if (reader->offset + size > reader->size) {
        return 0;
    }
    memcpy(out, reader->data + reader->offset, size);
    reader->offset += size;
    return 1;
}





static ArkLinkResult eo_parse_header(EOReader* reader, EOHeader* header) {
    if (!eo_reader_read(reader, header, sizeof(EOHeader))) {
        return ARK_LINK_ERR_FORMAT;
    }
    
    if (header->magic != EO_MAGIC) {
        fprintf(stderr, "[ERROR] Invalid EO magic: expected 0x%08X, got 0x%08X\n", 
                EO_MAGIC, header->magic);
        return ARK_LINK_ERR_FORMAT;
    }
    
    if (header->version != EO_VERSION) {
        fprintf(stderr, "[ERROR] Unsupported EO version: expected %d, got %d\n", 
                EO_VERSION, header->version);
        return ARK_LINK_ERR_FORMAT;
    }
    
    return ARK_LINK_OK;
}

static ArkLinkResult eo_parse_sections(EOReader* reader, EOSection* sections, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (!eo_reader_read(reader, &sections[i], sizeof(EOSection))) {
            return ARK_LINK_ERR_FORMAT;
        }
    }
    return ARK_LINK_OK;
}





static int eo_reloc_type_to_arklink(uint16_t eo_type) {
    switch (eo_type) {
        case EO_RELOC_ABS64:  return ARK_RELOC_ABS64;
        case EO_RELOC_PC32:   return ARK_RELOC_PC32;
        case EO_RELOC_GOT32:  return ARK_RELOC_GOTPC32;
        case EO_RELOC_SECREL: return ARK_RELOC_SECREL32;
        default: return -1;
    }
}

static int eo_bind_to_arklink(uint8_t bind) {
    switch (bind) {
        case EO_BIND_LOCAL:  return ARK_BIND_LOCAL;
        case EO_BIND_GLOBAL: return ARK_BIND_GLOBAL;
        case EO_BIND_WEAK:   return ARK_BIND_WEAK;
        default: return ARK_BIND_LOCAL;
    }
}

static int eo_type_to_arklink(uint8_t type) {
    switch (type) {
        case EO_SYM_FUNC:   return ARK_SYM_FUNC;
        case EO_SYM_OBJECT: return ARK_SYM_OBJECT;
        default: return ARK_SYM_NOTYPE;
    }
}

ArkLinkResult ark_link_load_eo(const char* path, ArkLinkUnit** unit) {
    uint8_t* data = NULL;
    size_t size = 0;
    
    ArkLinkResult result = eo_read_file(path, &data, &size);
    if (result != ARK_LINK_OK) {
        return result;
    }
    
    EOReader reader = { data, size, 0 };
    
    
    EOHeader header;
    result = eo_parse_header(&reader, &header);
    if (result != ARK_LINK_OK) {
        free(data);
        return result;
    }
    
    
    *unit = ark_link_unit_create(path);
    if (!*unit) {
        free(data);
        return ARK_LINK_ERR_MEMORY;
    }
    
    
    EOSection sections[EO_SEC_COUNT];
    uint32_t sec_count = header.sec_count < EO_SEC_COUNT ? header.sec_count : EO_SEC_COUNT;
    result = eo_parse_sections(&reader, sections, sec_count);
    if (result != ARK_LINK_OK) {
        ark_link_unit_destroy(*unit);
        free(data);
        return result;
    }

    


    int sec_idx_map[EO_SEC_COUNT];
    for (uint32_t i = 0; i < EO_SEC_COUNT; i++) {
        sec_idx_map[i] = -1;  
    }

    
    for (uint32_t i = 0; i < sec_count; i++) {
        EOSection* esec = &sections[i];

        
        if (esec->flags & EO_SECF_BSS) {
            
            ArkSectionDesc sdesc = {
                .name = esec->name,
                .data = NULL,
                .size = (size_t)esec->file_size,
                .alignment = 1u << esec->align_log2,
                .flags = ARK_SECTION_WRITE  
            };

            ArkLinkSection* section = ark_link_unit_add_section(*unit, &sdesc);
            if (!section) {
                ark_link_unit_destroy(*unit);
                free(data);
                return ARK_LINK_ERR_MEMORY;
            }
            sec_idx_map[i] = (int)(*unit)->section_count - 1;
            continue;
        }

        
        const uint8_t* sec_data = NULL;
        if (esec->file_size > 0) {
            if (esec->file_offset + esec->file_size > size) {
                ark_link_unit_destroy(*unit);
                free(data);
                return ARK_LINK_ERR_FORMAT;
            }
            sec_data = data + esec->file_offset;
        }

        
        ArkSectionDesc sdesc = {
            .name = esec->name,
            .data = sec_data,
            .size = (size_t)esec->file_size,
            .alignment = 1u << esec->align_log2,
            .flags = 0
        };

        if (esec->flags & EO_SECF_EXEC) {
            sdesc.flags |= ARK_SECTION_EXEC;
        }
        if (esec->flags & EO_SECF_WRITE) {
            sdesc.flags |= ARK_SECTION_WRITE;
        }
        if (esec->flags & EO_SECF_READ) {
            sdesc.flags |= ARK_SECTION_READ;
        }

        ArkLinkSection* section = ark_link_unit_add_section(*unit, &sdesc);
        if (!section) {
            ark_link_unit_destroy(*unit);
            free(data);
            return ARK_LINK_ERR_MEMORY;
        }

        
        sec_idx_map[i] = (int)(*unit)->section_count - 1;

        
        if (esec->reloc_count > 0 && esec->reloc_offset > 0) {
            if (esec->reloc_offset + esec->reloc_count * sizeof(EOReloc) > size) {
                ark_link_unit_destroy(*unit);
                free(data);
                return ARK_LINK_ERR_FORMAT;
            }

            const EOReloc* relocs = (const EOReloc*)(data + esec->reloc_offset);

            for (uint32_t r = 0; r < esec->reloc_count; r++) {
                const EOReloc* erel = &relocs[r];

                int reloc_type = eo_reloc_type_to_arklink(erel->type);
                if (reloc_type < 0) {
                    fprintf(stderr, "[WARNING] Unknown relocation type: %d\n", erel->type);
                    continue;
                }

                ArkRelocationDesc rdesc = {
                    .offset = erel->offset,
                    .sym_idx = erel->sym_idx,
                    .type = reloc_type,
                    .addend = erel->addend
                };
                
#ifdef ARKLINK_DEBUG
                fprintf(stderr, "[DEBUG] Adding reloc: offset=%llu, sym_idx=%u, type=%d, addend=%d\n",
                        (unsigned long long)rdesc.offset, rdesc.sym_idx, rdesc.type, rdesc.addend);
#endif

                if (!ark_link_section_add_reloc(section, &rdesc)) {
                    ark_link_unit_destroy(*unit);
                    free(data);
                    return ARK_LINK_ERR_MEMORY;
                }
                
                
                if (!ark_link_unit_add_reloc(*unit, &rdesc)) {
                    ark_link_unit_destroy(*unit);
                    free(data);
                    return ARK_LINK_ERR_MEMORY;
                }
            }
        }
    }
    
    
    if (header.sym_count > 0) {
        
        size_t sym_offset = 0;
        
        
        for (uint32_t i = 0; i < sec_count; i++) {
            if (sections[i].reloc_count > 0 && sections[i].reloc_offset > 0) {
                size_t reloc_end = sections[i].reloc_offset + 
                                   sections[i].reloc_count * sizeof(EOReloc);
                if (reloc_end > sym_offset) {
                    sym_offset = reloc_end;
                }
            }
        }
        
        
        if (sym_offset == 0) {
            for (uint32_t i = 0; i < sec_count; i++) {
                if (!(sections[i].flags & EO_SECF_BSS)) {
                    size_t sec_end = sections[i].file_offset + sections[i].file_size;
                    if (sec_end > sym_offset) {
                        sym_offset = sec_end;
                    }
                }
            }
        }
        
        if (sym_offset == 0) {
            
            sym_offset = sizeof(EOHeader) + sec_count * sizeof(EOSection);
        }
        
        if (sym_offset + header.sym_count * sizeof(EOSymbol) > size) {
            fprintf(stderr, "[ERROR] Symbol table extends beyond file: offset=%zu, count=%u, file_size=%zu\n",
                    sym_offset, header.sym_count, size);
            ark_link_unit_destroy(*unit);
            free(data);
            return ARK_LINK_ERR_FORMAT;
        }
        
#ifdef ARKLINK_DEBUG
        fprintf(stderr, "[DEBUG] Symbol table at offset %zu, count=%u\n", sym_offset, header.sym_count);
#endif
        
        const EOSymbol* symbols = (const EOSymbol*)(data + sym_offset);
        
        for (uint32_t i = 0; i < header.sym_count; i++) {
            const EOSymbol* esym = &symbols[i];

#ifdef ARKLINK_DEBUG
            fprintf(stderr, "[DEBUG] Symbol %u: name='%s', sec_idx=%u, value=0x%llX\n",
                    i, esym->name, esym->sec_idx, (unsigned long long)esym->value);
#endif

            
            char* name_copy = strdup(esym->name);
            if (!name_copy) {
                ark_link_unit_destroy(*unit);
                free(data);
                return ARK_LINK_ERR_MEMORY;
            }

            


            uint16_t arklink_sec_idx = 0;
            if (esym->sec_idx > 0 && esym->sec_idx <= sec_count) {
                int mapped = sec_idx_map[esym->sec_idx - 1];
                if (mapped >= 0) {
                    arklink_sec_idx = (uint16_t)(mapped + 1);  
                }
            }

            ArkSymbolDesc sdesc = {
                .name = name_copy,
                .value = esym->value,
                .size = 0,  
                .section_index = arklink_sec_idx,
                .type = eo_type_to_arklink(esym->type),
                .binding = eo_bind_to_arklink(esym->bind),
                .visibility = ARK_VISIBILITY_DEFAULT
            };

            if (!ark_link_unit_add_symbol(*unit, &sdesc)) {
                free(name_copy);
                ark_link_unit_destroy(*unit);
                free(data);
                return ARK_LINK_ERR_MEMORY;
            }
        }
    }
    
    
    if (header.entry_point != 0) {
        (*unit)->entry_point = header.entry_point;
    }
    
    
    (*unit)->file_data = data;
    (*unit)->file_size = size;
    
    return ARK_LINK_OK;
}
