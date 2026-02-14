#include "ArkLink/loader.h"
#include "coff_obj.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>







ArkLinkUnit* ark_link_unit_create(const char* path) {
    ArkLinkUnit* unit = (ArkLinkUnit*)calloc(1, sizeof(ArkLinkUnit));
    if (!unit) return NULL;
    
    unit->path = strdup(path);
    if (!unit->path) {
        free(unit);
        return NULL;
    }
    
    return unit;
}

void ark_link_unit_destroy(ArkLinkUnit* unit) {
    if (!unit) return;

    
    if (unit->sections) {
        for (size_t i = 0; i < unit->section_count; i++) {
            free(unit->sections[i].relocs);
            if (unit->sections[i].buffer) {
                free(unit->sections[i].buffer->data);
                free(unit->sections[i].buffer);
            }
        }
        free(unit->sections);
    }

    
    if (unit->symbols) {
        free(unit->symbols);
    }

    
    if (unit->file_data) {
        free(unit->file_data);
    }

    
    if (unit->path) {
        free((void*)unit->path);
    }

    free(unit);
}

ArkLinkSection* ark_link_unit_add_section(ArkLinkUnit* unit, const ArkSectionDesc* desc) {
    if (!unit || !desc) return NULL;

    
    size_t new_count = unit->section_count + 1;
    ArkLinkSection* new_sections = realloc(unit->sections, new_count * sizeof(ArkLinkSection));
    if (!new_sections) return NULL;

    unit->sections = new_sections;
    ArkLinkSection* sec = &unit->sections[unit->section_count];
    unit->section_count = new_count;

    
    memset(sec, 0, sizeof(ArkLinkSection));
    strncpy(sec->name, desc->name, sizeof(sec->name) - 1);
    sec->name[sizeof(sec->name) - 1] = '\0';
    sec->data = desc->data;
    sec->size = desc->size;
    sec->alignment = desc->alignment;
    sec->flags = desc->flags;

    
    sec->buffer = (ArkSectionBuffer*)malloc(sizeof(ArkSectionBuffer));
    if (sec->buffer) {
        memset(sec->buffer, 0, sizeof(ArkSectionBuffer));

        if (desc->data && desc->size > 0) {
            sec->buffer->data = (uint8_t*)malloc(desc->size);
            if (sec->buffer->data) {
                memcpy(sec->buffer->data, desc->data, desc->size);
                sec->buffer->size = desc->size;
                sec->buffer->capacity = desc->size;
            }
        }

        
        if (sec->flags & ARK_SECTION_EXEC) {
            sec->buffer->kind = ARK_SECTION_CODE;
        } else if (sec->flags & ARK_SECTION_WRITE) {
            sec->buffer->kind = ARK_SECTION_DATA;
        } else {
            sec->buffer->kind = ARK_SECTION_RODATA;
        }
        sec->buffer->flags = sec->flags;
        sec->buffer->alignment = (uint32_t)sec->alignment;
    }

    return sec;
}

int ark_link_unit_add_symbol(ArkLinkUnit* unit, const ArkSymbolDesc* desc) {
    if (!unit || !desc) return 0;
    
    
    size_t new_count = unit->symbol_count + 1;
    ArkSymbolDesc* new_symbols = realloc(unit->symbols, new_count * sizeof(ArkSymbolDesc));
    if (!new_symbols) return 0;
    
    unit->symbols = new_symbols;
    ArkSymbolDesc* sym = &unit->symbols[unit->symbol_count];
    unit->symbol_count = new_count;
    
    
    *sym = *desc;
    
    return 1;
}

int ark_link_section_add_reloc(ArkLinkSection* section, const ArkRelocationDesc* desc) {
    if (!section || !desc) return 0;
    
    
    if (section->reloc_count >= section->reloc_capacity) {
        size_t new_cap = section->reloc_capacity ? section->reloc_capacity * 2 : 16;
        ArkRelocationDesc* new_relocs = realloc(section->relocs, new_cap * sizeof(ArkRelocationDesc));
        if (!new_relocs) return 0;
        section->relocs = new_relocs;
        section->reloc_capacity = new_cap;
    }
    
    section->relocs[section->reloc_count++] = *desc;
    return 1;
}

int ark_link_unit_add_reloc(ArkLinkUnit* unit, const ArkRelocationDesc* desc) {
    if (!unit || !desc) return 0;
    
    
    size_t new_count = unit->relocations.count + 1;
    ArkRelocationDesc* new_relocs = realloc(unit->relocations.relocs, new_count * sizeof(ArkRelocationDesc));
    if (!new_relocs) return 0;
    
    unit->relocations.relocs = new_relocs;
    unit->relocations.relocs[unit->relocations.count] = *desc;
    unit->relocations.count = new_count;
    return 1;
}





ArkLinkResult ark_loader_load_unit(ArkLinkContext* ctx, const char* path, 
                                    const ArkLoaderOptions* opts, 
                                    ArkLinkUnit** out_unit, 
                                    ArkLoaderDiagnostics* diag) {
    (void)opts;
    (void)diag;
    
    if (!ctx || !path || !out_unit) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    
    ArkLinkResult result = ark_link_load_eo(path, out_unit);
    if (result == ARK_LINK_OK) {
        return ARK_LINK_OK;
    }
    
    
    
    FILE* fp = fopen(path, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        if (size > 0) {
            uint8_t* data = (uint8_t*)malloc((size_t)size);
            if (data) {
                if (fread(data, 1, (size_t)size, fp) == (size_t)size) {
                    result = ark_coff_load_unit(ctx, data, (size_t)size, path, out_unit);
                    if (result == ARK_LINK_OK) {
                        free(data);
                        fclose(fp);
                        return ARK_LINK_OK;
                    }
                }
                free(data);
            }
        }
        fclose(fp);
    }
    
    
    
    return ARK_LINK_ERR_FORMAT;
}

ArkLinkResult ark_loader_load_unit_memory(ArkLinkContext* ctx, const char* name, 
                                           const void* data, size_t size,
                                           const ArkLoaderOptions* opts, 
                                           ArkLinkUnit** out_unit, 
                                           ArkLoaderDiagnostics* diag) {
    (void)ctx;
    (void)opts;
    (void)diag;
    
    if (!name || !data || !out_unit) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    
    if (size >= 4) {
        const uint8_t* bytes = (const uint8_t*)data;
        
        if (bytes[0] == 0x4F && bytes[1] == 0x45 && bytes[2] == 0x23 && bytes[3] == 0x45) {
            
            char temp_path[256];
            snprintf(temp_path, sizeof(temp_path), "temp_%s.eo", name);
            
            FILE* fp = fopen(temp_path, "wb");
            if (!fp) {
                return ARK_LINK_ERR_IO;
            }
            
            if (fwrite(data, 1, size, fp) != size) {
                fclose(fp);
                remove(temp_path);
                return ARK_LINK_ERR_IO;
            }
            fclose(fp);
            
            ArkLinkResult result = ark_link_load_eo(temp_path, out_unit);
            remove(temp_path);
            
            if (result == ARK_LINK_OK && *out_unit) {
                
                free((void*)(*out_unit)->path);
                (*out_unit)->path = strdup(name);
            }
            
            return result;
        }
    }
    
    return ARK_LINK_ERR_FORMAT;
}

void ark_loader_unit_destroy(ArkLinkContext* ctx, ArkLinkUnit* unit) {
    (void)ctx;
    ark_link_unit_destroy(unit);
}





ArkLinkResult ark_loader_find_library(const char* name,
                                       const char* const* search_paths, size_t path_count,
                                       char* out_path, size_t out_path_size) {
    if (!name || !out_path || out_path_size == 0) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    
    for (size_t i = 0; i < path_count; i++) {
        const char* path = search_paths[i];
        if (!path) continue;
        
        
        snprintf(out_path, out_path_size, "%s/%s.lib", path, name);
        FILE* fp = fopen(out_path, "rb");
        if (fp) {
            fclose(fp);
            return ARK_LINK_OK;
        }
        
        
        snprintf(out_path, out_path_size, "%s/lib%s.a", path, name);
        fp = fopen(out_path, "rb");
        if (fp) {
            fclose(fp);
            return ARK_LINK_OK;
        }
        
        
        snprintf(out_path, out_path_size, "%s/%s", path, name);
        fp = fopen(out_path, "rb");
        if (fp) {
            fclose(fp);
            return ARK_LINK_OK;
        }
    }
    
    
    snprintf(out_path, out_path_size, "%s.lib", name);
    FILE* fp = fopen(out_path, "rb");
    if (fp) {
        fclose(fp);
        return ARK_LINK_OK;
    }
    
    return ARK_LINK_ERR_IO;
}
