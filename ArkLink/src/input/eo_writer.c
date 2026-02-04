

#include "internal/obj_writer.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 1024
#define STRING_INITIAL_CAPACITY 256


#define DEFAULT_CODE_ALIGN 4   
#define DEFAULT_DATA_ALIGN 3   
#define DEFAULT_RODATA_ALIGN 3 
#define DEFAULT_BSS_ALIGN 3    
#define DEFAULT_TLS_ALIGN 3    

EOWriter* eo_writer_create(void) {
    EOWriter* writer = (EOWriter*)calloc(1, sizeof(EOWriter));
    if (!writer) return NULL;
    
    
    writer->code_capacity = INITIAL_CAPACITY;
    writer->code = (uint8_t*)malloc(writer->code_capacity);
    if (!writer->code) goto fail;
    
    
    writer->data_capacity = INITIAL_CAPACITY;
    writer->data = (uint8_t*)malloc(writer->data_capacity);
    if (!writer->data) goto fail;
    
    
    writer->rodata_capacity = INITIAL_CAPACITY;
    writer->rodata = (uint8_t*)malloc(writer->rodata_capacity);
    if (!writer->rodata) goto fail;
    
    
    writer->tls_capacity = INITIAL_CAPACITY;
    writer->tls = (uint8_t*)malloc(writer->tls_capacity);
    if (!writer->tls) goto fail;
    
    
    writer->sym_capacity = 64;
    writer->symbols = (EOSymbol*)calloc(writer->sym_capacity, sizeof(EOSymbol));
    if (!writer->symbols) goto fail;
    
    
    writer->reloc_capacity = 128;
    writer->relocs = (EORelocation*)calloc(writer->reloc_capacity, sizeof(EORelocation));
    if (!writer->relocs) goto fail;
    
    
    writer->string_capacity = STRING_INITIAL_CAPACITY;
    writer->strings = (char*)malloc(writer->string_capacity);
    if (!writer->strings) goto fail;
    writer->strings[0] = '\0';
    writer->string_size = 1;  
    
    
    writer->code_align = DEFAULT_CODE_ALIGN;
    writer->data_align = DEFAULT_DATA_ALIGN;
    writer->rodata_align = DEFAULT_RODATA_ALIGN;
    writer->bss_align = DEFAULT_BSS_ALIGN;
    writer->tls_align = DEFAULT_TLS_ALIGN;
    
    
    writer->entry_section = EO_SEC_CODE;
    
    return writer;
    
fail:
    eo_writer_destroy(writer);
    return NULL;
}

void eo_writer_destroy(EOWriter* writer) {
    if (!writer) return;
    
    free(writer->code);
    free(writer->data);
    free(writer->rodata);
    free(writer->tls);
    free(writer->symbols);
    free(writer->relocs);
    free(writer->strings);
    free(writer);
}

static bool ensure_capacity(uint8_t** buffer, uint32_t* capacity, uint32_t required) {
    if (required <= *capacity) return true;
    
    uint32_t new_capacity = *capacity;
    while (new_capacity < required) {
        new_capacity *= 2;
    }
    
    uint8_t* new_buffer = (uint8_t*)realloc(*buffer, new_capacity);
    if (!new_buffer) return false;
    
    *buffer = new_buffer;
    *capacity = new_capacity;
    return true;
}

uint32_t eo_write_code(EOWriter* writer, const void* data, uint32_t size) {
    if (!writer || !data || size == 0) return 0;
    
    uint32_t offset = writer->code_offset;
    uint32_t new_size = offset + size;
    
    if (!ensure_capacity(&writer->code, &writer->code_capacity, new_size)) {
        return 0;
    }
    
    memcpy(writer->code + offset, data, size);
    writer->code_offset = new_size;
    return offset;
}

uint32_t eo_write_data(EOWriter* writer, const void* data, uint32_t size) {
    if (!writer || !data || size == 0) return 0;
    
    uint32_t offset = writer->data_offset;
    uint32_t new_size = offset + size;
    
    if (!ensure_capacity(&writer->data, &writer->data_capacity, new_size)) {
        return 0;
    }
    
    memcpy(writer->data + offset, data, size);
    writer->data_offset = new_size;
    return offset;
}

uint32_t eo_write_rodata(EOWriter* writer, const void* data, uint32_t size) {
    if (!writer || !data || size == 0) return 0;
    
    uint32_t offset = writer->rodata_offset;
    uint32_t new_size = offset + size;
    
    if (!ensure_capacity(&writer->rodata, &writer->rodata_capacity, new_size)) {
        return 0;
    }
    
    memcpy(writer->rodata + offset, data, size);
    writer->rodata_offset = new_size;
    return offset;
}

uint32_t eo_write_tls(EOWriter* writer, const void* data, uint32_t size) {
    if (!writer || !data || size == 0) return 0;
    
    uint32_t offset = writer->tls_offset;
    uint32_t new_size = offset + size;
    
    if (!ensure_capacity(&writer->tls, &writer->tls_capacity, new_size)) {
        return 0;
    }
    
    memcpy(writer->tls + offset, data, size);
    writer->tls_offset = new_size;
    
    
    writer->flags |= EO_FLAG_HAS_TLS;
    
    return offset;
}


static uint32_t align_up(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

uint32_t eo_write_code_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align) {
    if (!writer || align == 0) return 0;
    
    uint32_t aligned_offset = align_up(writer->code_offset, align);
    uint32_t padding = aligned_offset - writer->code_offset;
    
    
    if (padding > 0) {
        static const uint8_t nops[16] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                                          0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        for (uint32_t i = 0; i < padding; i++) {
            eo_write_code(writer, &nops[i % 16], 1);
        }
    }
    
    return eo_write_code(writer, data, size);
}

uint32_t eo_write_data_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align) {
    if (!writer || align == 0) return 0;
    
    uint32_t aligned_offset = align_up(writer->data_offset, align);
    uint32_t padding = aligned_offset - writer->data_offset;
    
    
    if (padding > 0) {
        uint8_t zeros[16] = {0};
        for (uint32_t i = 0; i < padding; i += 16) {
            uint32_t to_write = (padding - i < 16) ? (padding - i) : 16;
            eo_write_data(writer, zeros, to_write);
        }
    }
    
    return eo_write_data(writer, data, size);
}


void eo_set_code_align(EOWriter* writer, uint8_t align_log2) {
    if (writer) writer->code_align = align_log2;
}

void eo_set_data_align(EOWriter* writer, uint8_t align_log2) {
    if (writer) writer->data_align = align_log2;
}

void eo_set_rodata_align(EOWriter* writer, uint8_t align_log2) {
    if (writer) writer->rodata_align = align_log2;
}

void eo_set_bss_align(EOWriter* writer, uint8_t align_log2) {
    if (writer) writer->bss_align = align_log2;
}


static uint32_t add_string(EOWriter* writer, const char* str) {
    if (!writer || !str) return 0;
    
    uint32_t len = (uint32_t)strlen(str) + 1;  
    uint32_t new_size = writer->string_size + len;
    
    if (new_size > writer->string_capacity) {
        uint32_t new_capacity = writer->string_capacity;
        while (new_capacity < new_size) {
            new_capacity *= 2;
        }
        
        char* new_strings = (char*)realloc(writer->strings, new_capacity);
        if (!new_strings) return 0;
        
        writer->strings = new_strings;
        writer->string_capacity = new_capacity;
    }
    
    uint32_t offset = writer->string_size;
    memcpy(writer->strings + offset, str, len);
    writer->string_size = new_size;
    
    return offset;
}

uint32_t eo_add_symbol(EOWriter* writer, const char* name, uint32_t type,
                       uint32_t section, uint32_t offset, uint32_t size) {
    if (!writer || !name) return (uint32_t)-1;
    
    if (writer->sym_count >= writer->sym_capacity) {
        uint32_t new_capacity = writer->sym_capacity * 2;
        EOSymbol* new_symbols = (EOSymbol*)realloc(writer->symbols, 
                                                    new_capacity * sizeof(EOSymbol));
        if (!new_symbols) return (uint32_t)-1;
        
        writer->symbols = new_symbols;
        writer->sym_capacity = new_capacity;
    }
    
    uint32_t index = writer->sym_count++;
    EOSymbol* sym = &writer->symbols[index];
    
    memset(sym, 0, sizeof(EOSymbol));
    sym->name_offset = add_string(writer, name);
    sym->type = type;
    sym->section = section;
    sym->offset = offset;
    sym->size = size;
    
    return index;
}

uint32_t eo_add_undefined_symbol(EOWriter* writer, const char* name) {
    return eo_add_symbol(writer, name, EO_SYM_UNDEFINED, 0, 0, 0);
}

int32_t eo_find_symbol(EOWriter* writer, const char* name) {
    if (!writer || !name) return -1;
    
    for (uint32_t i = 0; i < writer->sym_count; i++) {
        const char* sym_name = writer->strings + writer->symbols[i].name_offset;
        if (strcmp(sym_name, name) == 0) {
            return (int32_t)i;
        }
    }
    
    return -1;
}

void eo_add_reloc(EOWriter* writer, uint32_t section, uint32_t offset,
                  uint32_t symbol_index, uint32_t type, int32_t addend) {
    if (!writer) return;
    
    if (writer->reloc_count >= writer->reloc_capacity) {
        uint32_t new_capacity = writer->reloc_capacity * 2;
        EORelocation* new_relocs = (EORelocation*)realloc(writer->relocs,
                                                           new_capacity * sizeof(EORelocation));
        if (!new_relocs) return;
        
        writer->relocs = new_relocs;
        writer->reloc_capacity = new_capacity;
    }
    
    uint32_t index = writer->reloc_count++;
    EORelocation* reloc = &writer->relocs[index];
    
    reloc->offset = offset;
    reloc->symbol_index = symbol_index;
    reloc->type = type;
    reloc->section = section;
    reloc->addend = addend;
    reloc->addend_shift = 0;
}

void eo_set_entry_point(EOWriter* writer, uint32_t offset) {
    if (!writer) return;
    writer->entry_point = offset;
    writer->entry_section = EO_SEC_CODE;
    writer->has_entry = true;
}

void eo_set_entry_point_ex(EOWriter* writer, uint32_t section, uint32_t offset) {
    if (!writer) return;
    writer->entry_point = offset;
    writer->entry_section = section;
    writer->has_entry = true;
}

void eo_reserve_bss(EOWriter* writer, uint32_t size) {
    if (!writer) return;
    
    writer->bss_size += size;
    if (size > writer->bss_mem_size) {
        writer->bss_mem_size = size;
    }
}

bool eo_write_file(EOWriter* writer, const char* filename) {
    if (!writer || !filename) return false;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return false;
    
    
    EOHeader header;
    memset(&header, 0, sizeof(header));
    
    header.magic = EO_MAGIC;
    header.version = EO_VERSION;
    header.flags = writer->flags;
    
    
    
    header.sections[EO_SEC_CODE].file_size = writer->code_offset;
    header.sections[EO_SEC_CODE].mem_size = writer->code_offset;
    header.sections[EO_SEC_CODE].align_log2 = writer->code_align;
    header.sections[EO_SEC_CODE].flags = EO_SECF_EXEC | EO_SECF_READ;
    
    
    header.sections[EO_SEC_DATA].file_size = writer->data_offset;
    header.sections[EO_SEC_DATA].mem_size = writer->data_offset;
    header.sections[EO_SEC_DATA].align_log2 = writer->data_align;
    header.sections[EO_SEC_DATA].flags = EO_SECF_READ | EO_SECF_WRITE;
    
    
    header.sections[EO_SEC_RODATA].file_size = writer->rodata_offset;
    header.sections[EO_SEC_RODATA].mem_size = writer->rodata_offset;
    header.sections[EO_SEC_RODATA].align_log2 = writer->rodata_align;
    header.sections[EO_SEC_RODATA].flags = EO_SECF_READ;
    
    
    header.sections[EO_SEC_BSS].file_size = 0;  
    header.sections[EO_SEC_BSS].mem_size = writer->bss_mem_size;
    header.sections[EO_SEC_BSS].align_log2 = writer->bss_align;
    header.sections[EO_SEC_BSS].flags = EO_SECF_READ | EO_SECF_WRITE | EO_SECF_BSS;
    
    
    header.sections[EO_SEC_TLS].file_size = writer->tls_offset;
    header.sections[EO_SEC_TLS].mem_size = writer->tls_offset;
    header.sections[EO_SEC_TLS].align_log2 = writer->tls_align;
    header.sections[EO_SEC_TLS].flags = EO_SECF_READ | EO_SECF_WRITE | EO_SECF_TLS;
    
    header.sym_count = writer->sym_count;
    header.reloc_count = writer->reloc_count;
    header.string_table_size = writer->string_size;
    header.entry_point = writer->has_entry ? writer->entry_point : 0;
    header.entry_section = writer->has_entry ? writer->entry_section : 0xFF;  
    
    fprintf(stderr, "DEBUG eo_write_file: has_entry=%d, entry_point=%u, entry_section=%u\n",
            writer->has_entry, header.entry_point, header.entry_section);
    
    
    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return false;
    }
    
    
    if (writer->code_offset > 0) {
        if (fwrite(writer->code, 1, writer->code_offset, fp) != writer->code_offset) {
            fclose(fp);
            return false;
        }
    }
    
    
    if (writer->data_offset > 0) {
        if (fwrite(writer->data, 1, writer->data_offset, fp) != writer->data_offset) {
            fclose(fp);
            return false;
        }
    }
    
    
    if (writer->rodata_offset > 0) {
        if (fwrite(writer->rodata, 1, writer->rodata_offset, fp) != writer->rodata_offset) {
            fclose(fp);
            return false;
        }
    }
    
    
    if (writer->tls_offset > 0) {
        if (fwrite(writer->tls, 1, writer->tls_offset, fp) != writer->tls_offset) {
            fclose(fp);
            return false;
        }
    }
    
    
    if (writer->sym_count > 0) {
        if (fwrite(writer->symbols, sizeof(EOSymbol), writer->sym_count, fp) != writer->sym_count) {
            fclose(fp);
            return false;
        }
    }
    
    
    if (writer->reloc_count > 0) {
        if (fwrite(writer->relocs, sizeof(EORelocation), writer->reloc_count, fp) != writer->reloc_count) {
            fclose(fp);
            return false;
        }
    }
    
    
    if (writer->string_size > 0) {
        if (fwrite(writer->strings, 1, writer->string_size, fp) != writer->string_size) {
            fclose(fp);
            return false;
        }
    }
    
    fclose(fp);
    return true;
}

uint32_t eo_get_code_offset(EOWriter* writer) {
    return writer ? writer->code_offset : 0;
}

uint32_t eo_get_data_offset(EOWriter* writer) {
    return writer ? writer->data_offset : 0;
}

uint32_t eo_get_rodata_offset(EOWriter* writer) {
    return writer ? writer->rodata_offset : 0;
}

bool eo_writer_has_entry(EOWriter* writer) {
    return writer ? writer->has_entry : false;
}

uint32_t eo_writer_get_entry_point(EOWriter* writer) {
    return writer ? writer->entry_point : 0;
}
