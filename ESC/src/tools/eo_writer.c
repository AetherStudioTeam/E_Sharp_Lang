#include "eo_writer.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 1024
#define STRING_INITIAL_CAPACITY 256

#define DEFAULT_CODE_ALIGN 4   
#define DEFAULT_DATA_ALIGN 3   
#define DEFAULT_RODATA_ALIGN 3 
#define DEFAULT_BSS_ALIGN 3    


typedef struct {
    EORelocation* relocs;
    uint32_t count;
    uint32_t capacity;
} RelocList;


typedef struct EOWriter {
    
    uint8_t* text;
    uint8_t* data;
    uint8_t* rodata;

    uint32_t text_capacity;
    uint32_t data_capacity;
    uint32_t rodata_capacity;

    uint32_t text_offset;
    uint32_t data_offset;
    uint32_t rodata_offset;

    
    uint8_t text_align;
    uint8_t data_align;
    uint8_t rodata_align;
    uint8_t bss_align;

    
    uint32_t bss_size;
    uint32_t bss_mem_size;

    
    EOSymbol* symbols;
    uint32_t sym_capacity;
    uint32_t sym_count;

    
    RelocList text_relocs;
    RelocList data_relocs;
    RelocList rodata_relocs;
    RelocList bss_relocs;

    
    char* strings;
    uint32_t string_capacity;
    uint32_t string_size;

    
    uint64_t entry_point;
    bool has_entry;

    
    uint16_t flags;
} EOWriter;

EOWriter* eo_writer_create(void) {
    EOWriter* writer = (EOWriter*)calloc(1, sizeof(EOWriter));
    if (!writer) return NULL;

    
    writer->text_capacity = INITIAL_CAPACITY;
    writer->text = (uint8_t*)malloc(writer->text_capacity);
    if (!writer->text) goto fail;

    
    writer->data_capacity = INITIAL_CAPACITY;
    writer->data = (uint8_t*)malloc(writer->data_capacity);
    if (!writer->data) goto fail;

    
    writer->rodata_capacity = INITIAL_CAPACITY;
    writer->rodata = (uint8_t*)malloc(writer->rodata_capacity);
    if (!writer->rodata) goto fail;

    
    writer->sym_capacity = 64;
    writer->symbols = (EOSymbol*)calloc(writer->sym_capacity, sizeof(EOSymbol));
    if (!writer->symbols) goto fail;

    
    writer->text_relocs.capacity = 16;
    writer->text_relocs.relocs = (EORelocation*)calloc(writer->text_relocs.capacity, sizeof(EORelocation));
    if (!writer->text_relocs.relocs) goto fail;

    writer->data_relocs.capacity = 16;
    writer->data_relocs.relocs = (EORelocation*)calloc(writer->data_relocs.capacity, sizeof(EORelocation));
    if (!writer->data_relocs.relocs) goto fail;

    writer->rodata_relocs.capacity = 16;
    writer->rodata_relocs.relocs = (EORelocation*)calloc(writer->rodata_relocs.capacity, sizeof(EORelocation));
    if (!writer->rodata_relocs.relocs) goto fail;

    writer->bss_relocs.capacity = 16;
    writer->bss_relocs.relocs = (EORelocation*)calloc(writer->bss_relocs.capacity, sizeof(EORelocation));
    if (!writer->bss_relocs.relocs) goto fail;

    
    writer->string_capacity = STRING_INITIAL_CAPACITY;
    writer->strings = (char*)malloc(writer->string_capacity);
    if (!writer->strings) goto fail;
    writer->strings[0] = '\0';
    writer->string_size = 1;  

    
    writer->text_align = DEFAULT_CODE_ALIGN;
    writer->data_align = DEFAULT_DATA_ALIGN;
    writer->rodata_align = DEFAULT_RODATA_ALIGN;
    writer->bss_align = DEFAULT_BSS_ALIGN;

    return writer;

fail:
    eo_writer_destroy(writer);
    return NULL;
}

void eo_writer_destroy(EOWriter* writer) {
    if (!writer) return;

    free(writer->text);
    free(writer->data);
    free(writer->rodata);
    free(writer->symbols);
    free(writer->text_relocs.relocs);
    free(writer->data_relocs.relocs);
    free(writer->rodata_relocs.relocs);
    free(writer->bss_relocs.relocs);
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
    if (!writer || !data || size == 0) return writer ? writer->text_offset : 0;

    uint32_t offset = writer->text_offset;
    uint32_t new_size = offset + size;

    if (!ensure_capacity(&writer->text, &writer->text_capacity, new_size)) {
        return 0;
    }

    memcpy(writer->text + offset, data, size);
    writer->text_offset = new_size;
    return offset;
}

uint32_t eo_write_data(EOWriter* writer, const void* data, uint32_t size) {
    if (!writer || !data || size == 0) return writer ? writer->data_offset : 0;

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
    if (!writer || !data || size == 0) return writer ? writer->rodata_offset : 0;

    uint32_t offset = writer->rodata_offset;
    uint32_t new_size = offset + size;

    if (!ensure_capacity(&writer->rodata, &writer->rodata_capacity, new_size)) {
        return 0;
    }

    memcpy(writer->rodata + offset, data, size);
    writer->rodata_offset = new_size;
    return offset;
}

static uint32_t align_up(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

uint32_t eo_write_code_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align) {
    if (!writer || align == 0) return 0;

    uint32_t aligned_offset = align_up(writer->text_offset, align);
    uint32_t padding = aligned_offset - writer->text_offset;

    
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


static uint32_t hash_name(const char* name) {
    uint32_t hash = 5381;
    int c;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
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

int eo_add_symbol(EOWriter* writer, const char* name, uint8_t type, uint8_t bind,
                  uint32_t sec_idx, uint64_t value) {
    if (!writer || !name) return -1;

    if (writer->sym_count >= writer->sym_capacity) {
        uint32_t new_capacity = writer->sym_capacity * 2;
        EOSymbol* new_symbols = (EOSymbol*)realloc(writer->symbols,
                                                    new_capacity * sizeof(EOSymbol));
        if (!new_symbols) return -1;

        writer->symbols = new_symbols;
        writer->sym_capacity = new_capacity;
    }

    uint32_t index = writer->sym_count++;
    EOSymbol* sym = &writer->symbols[index];

    memset(sym, 0, sizeof(EOSymbol));

    
    size_t name_len = strlen(name);
    if (name_len <= 23) {
        memcpy(sym->name, name, name_len);
        sym->name[name_len] = '\0';
    } else {
        
        uint32_t hash = hash_name(name);
        snprintf(sym->name, 24, "#%08X", hash);
        
        add_string(writer, name);
    }

    sym->type = type;
    sym->bind = bind;
    sym->sec_idx = sec_idx;
    sym->value = value;
    
    fprintf(stderr, "[DEBUG] eo_add_symbol: name='%s', type=%u, bind=%u, sec_idx=%u, value=%llu\n",
            sym->name, sym->type, sym->bind, sym->sec_idx, (unsigned long long)sym->value);

    return (int)index;
}

int eo_add_undefined_symbol(EOWriter* writer, const char* name) {
    return eo_add_symbol(writer, name, EO_SYM_NOTYPE, EO_BIND_GLOBAL, 0, 0);
}


static const char* find_string_at_offset(const EOWriter* writer, uint32_t offset) {
    if (!writer || offset >= writer->string_size) return NULL;
    return writer->strings + offset;
}


static uint32_t find_string_offset(const EOWriter* writer, const char* str) {
    if (!writer || !str || !writer->strings) return 0;
    
    uint32_t offset = 0;
    while (offset < writer->string_size) {
        const char* candidate = writer->strings + offset;
        if (strcmp(candidate, str) == 0) {
            return offset;
        }
        offset += (uint32_t)strlen(candidate) + 1;
    }
    return 0;  
}

int eo_find_symbol(EOWriter* writer, const char* name) {
    if (!writer || !name) return -1;

    size_t name_len = strlen(name);
    
    for (uint32_t i = 0; i < writer->sym_count; i++) {
        const char* sym_name = writer->symbols[i].name;
        
        
        if (strcmp(sym_name, name) == 0) {
            return (int)i;
        }
        
        
        if (name_len > 23 && sym_name[0] == '#') {
            
            uint32_t search_hash = hash_name(name);
            char hash_str[24];
            snprintf(hash_str, 24, "#%08X", search_hash);
            
            
            if (strcmp(sym_name, hash_str) == 0) {
                
                
                
                uint32_t offset = 0;
                while (offset < writer->string_size) {
                    const char* candidate = writer->strings + offset;
                    if (strcmp(candidate, name) == 0) {
                        return (int)i;
                    }
                    offset += (uint32_t)strlen(candidate) + 1;
                }
            }
        }
    }

    return -1;
}

static bool add_reloc_to_list(RelocList* list, const EORelocation* reloc) {
    if (list->count >= list->capacity) {
        uint32_t new_capacity = list->capacity * 2;
        EORelocation* new_relocs = (EORelocation*)realloc(list->relocs,
                                                           new_capacity * sizeof(EORelocation));
        if (!new_relocs) return false;

        list->relocs = new_relocs;
        list->capacity = new_capacity;
    }

    list->relocs[list->count++] = *reloc;
    return true;
}

void eo_add_reloc(EOWriter* writer, uint32_t sec_idx, uint64_t offset,
                  uint32_t sym_idx, uint16_t type, int16_t addend) {
    if (!writer) return;

    EORelocation reloc = {
        .offset = offset,
        .sym_idx = sym_idx,
        .type = type,
        .addend = addend
    };
    
    fprintf(stderr, "[DEBUG] eo_add_reloc: offset=%llu, sym_idx=%u, type=%u, addend=%d\n",
            (unsigned long long)reloc.offset, reloc.sym_idx, reloc.type, reloc.addend);
    fprintf(stderr, "[DEBUG] sizeof(EORelocation)=%zu\n", sizeof(EORelocation));

    RelocList* list = NULL;
    switch (sec_idx) {
        case EO_SEC_TEXT:   list = &writer->text_relocs; break;
        case EO_SEC_DATA:   list = &writer->data_relocs; break;
        case EO_SEC_RODATA: list = &writer->rodata_relocs; break;
        case EO_SEC_BSS:    list = &writer->bss_relocs; break;
        default: return;
    }

    add_reloc_to_list(list, &reloc);
}

bool eo_set_entry_point(EOWriter* writer, uint64_t offset) {
    if (!writer) return false;
    writer->entry_point = offset;
    writer->has_entry = true;
    return true;
}

bool eo_reserve_bss(EOWriter* writer, uint32_t size, uint8_t align_log2) {
    if (!writer) return false;
    (void)align_log2;

    writer->bss_size += size;
    if (size > writer->bss_mem_size) {
        writer->bss_mem_size = size;
    }
    return true;
}

uint32_t eo_get_code_offset(EOWriter* writer) {
    return writer ? writer->text_offset : 0;
}

uint32_t eo_get_data_offset(EOWriter* writer) {
    return writer ? writer->data_offset : 0;
}

uint32_t eo_get_rodata_offset(EOWriter* writer) {
    return writer ? writer->rodata_offset : 0;
}

bool eo_write_file(EOWriter* writer, const char* filename) {
    if (!writer || !filename) return false;

    FILE* fp = fopen(filename, "wb");
    if (!fp) return false;

    
    uint32_t header_size = sizeof(EOHeader);
    uint32_t section_headers_size = EO_SEC_COUNT * sizeof(EOSection);

    uint32_t text_offset = header_size + section_headers_size;
    uint32_t data_offset = text_offset + writer->text_offset;
    uint32_t rodata_offset = data_offset + writer->data_offset;

    uint32_t data_end = rodata_offset + writer->rodata_offset;

    
    uint32_t text_reloc_offset = data_end;
    uint32_t data_reloc_offset = text_reloc_offset + writer->text_relocs.count * sizeof(EORelocation);
    uint32_t rodata_reloc_offset = data_reloc_offset + writer->data_relocs.count * sizeof(EORelocation);
    uint32_t bss_reloc_offset = rodata_reloc_offset + writer->rodata_relocs.count * sizeof(EORelocation);

    
    uint32_t total_relocs = writer->text_relocs.count + writer->data_relocs.count +
                            writer->rodata_relocs.count + writer->bss_relocs.count;
    uint32_t symbols_offset = data_end + total_relocs * sizeof(EORelocation);

    
    uint32_t strings_offset = symbols_offset + writer->sym_count * sizeof(EOSymbol);

    
    EOSection sections[EO_SEC_COUNT];
    memset(sections, 0, sizeof(sections));

    
    memcpy(sections[EO_SEC_TEXT].name, ".text", 6);
    sections[EO_SEC_TEXT].align_log2 = writer->text_align;
    sections[EO_SEC_TEXT].flags = EO_SECF_READ | EO_SECF_EXEC;
    sections[EO_SEC_TEXT].file_offset = writer->text_offset > 0 ? text_offset : 0;
    sections[EO_SEC_TEXT].file_size = writer->text_offset;
    sections[EO_SEC_TEXT].mem_size = writer->text_offset;
    sections[EO_SEC_TEXT].reloc_count = writer->text_relocs.count;
    sections[EO_SEC_TEXT].reloc_offset = writer->text_relocs.count > 0 ? text_reloc_offset : 0;

    
    memcpy(sections[EO_SEC_DATA].name, ".data", 6);
    sections[EO_SEC_DATA].align_log2 = writer->data_align;
    sections[EO_SEC_DATA].flags = EO_SECF_READ | EO_SECF_WRITE;
    sections[EO_SEC_DATA].file_offset = writer->data_offset > 0 ? data_offset : 0;
    sections[EO_SEC_DATA].file_size = writer->data_offset;
    sections[EO_SEC_DATA].mem_size = writer->data_offset;
    sections[EO_SEC_DATA].reloc_count = writer->data_relocs.count;
    sections[EO_SEC_DATA].reloc_offset = writer->data_relocs.count > 0 ? data_reloc_offset : 0;
    
    fprintf(stderr, "[DEBUG] data_relocs.count=%u, data_reloc_offset=%u, reloc_offset=%u\n",
            writer->data_relocs.count, data_reloc_offset, sections[EO_SEC_DATA].reloc_offset);

    
    memcpy(sections[EO_SEC_RODATA].name, ".rodata", 8);
    sections[EO_SEC_RODATA].align_log2 = writer->rodata_align;
    sections[EO_SEC_RODATA].flags = EO_SECF_READ;
    sections[EO_SEC_RODATA].file_offset = writer->rodata_offset > 0 ? rodata_offset : 0;
    sections[EO_SEC_RODATA].file_size = writer->rodata_offset;
    sections[EO_SEC_RODATA].mem_size = writer->rodata_offset;
    sections[EO_SEC_RODATA].reloc_count = writer->rodata_relocs.count;
    sections[EO_SEC_RODATA].reloc_offset = writer->rodata_relocs.count > 0 ? rodata_reloc_offset : 0;

    
    memcpy(sections[EO_SEC_BSS].name, ".bss", 5);
    sections[EO_SEC_BSS].align_log2 = writer->bss_align;
    sections[EO_SEC_BSS].flags = EO_SECF_READ | EO_SECF_WRITE | EO_SECF_BSS;
    sections[EO_SEC_BSS].file_offset = 0;  
    sections[EO_SEC_BSS].file_size = 0;
    sections[EO_SEC_BSS].mem_size = writer->bss_mem_size;
    sections[EO_SEC_BSS].reloc_count = writer->bss_relocs.count;
    sections[EO_SEC_BSS].reloc_offset = writer->bss_relocs.count > 0 ? bss_reloc_offset : 0;

    
    EOHeader header;
    memset(&header, 0, sizeof(header));

    header.magic = EO_MAGIC;
    header.version = EO_VERSION;
    header.flags = writer->flags;
    header.arch = EO_ARCH_X86_64;
    header.sec_count = EO_SEC_COUNT;
    header.sym_count = writer->sym_count;
    header.strtab_size = writer->string_size;
    header.entry_point = writer->has_entry ? writer->entry_point : 0;

    
    fprintf(stderr, "[DEBUG] sizeof(header)=%zu\n", sizeof(header));
    fprintf(stderr, "[DEBUG] Header hex:\n");
    for (int i = 0; i < (int)sizeof(header); i++) {
        fprintf(stderr, "%02X ", ((unsigned char*)&header)[i]);
        if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
    long pos_before_header = ftell(fp);
    fprintf(stderr, "[DEBUG] Position before header: %ld\n", pos_before_header);
    size_t header_written = fwrite(&header, sizeof(header), 1, fp);
    fprintf(stderr, "[DEBUG] fwrite returned %zu\n", header_written);
    long pos_after_header = ftell(fp);
    fprintf(stderr, "[DEBUG] Position after header: %ld\n", pos_after_header);
    if (header_written != 1) {
        fclose(fp);
        return false;
    }

    
    fprintf(stderr, "[DEBUG] Section headers at %p:\n", (void*)sections);
    for (int i = 0; i < EO_SEC_COUNT; i++) {
        fprintf(stderr, "[DEBUG] Section %d: name='%s', reloc_count=%u, reloc_offset=%u\n",
                i, sections[i].name, sections[i].reloc_count, sections[i].reloc_offset);
    }
    fprintf(stderr, "[DEBUG] Section headers hex:\n");
    for (int i = 0; i < EO_SEC_COUNT * (int)sizeof(EOSection); i++) {
        fprintf(stderr, "%02X ", ((unsigned char*)sections)[i]);
        if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
    long pos = ftell(fp);
    fprintf(stderr, "[DEBUG] File position before fwrite: %ld\n", pos);
    fprintf(stderr, "[DEBUG] fp: %p\n", (void*)fp);
    fprintf(stderr, "[DEBUG] sections address: %p\n", (void*)sections);
    fprintf(stderr, "[DEBUG] &sections[0] address: %p\n", (void*)&sections[0]);
    
    unsigned char* sec_bytes = (unsigned char*)&sections[0];
    fprintf(stderr, "[DEBUG] sec_bytes address: %p\n", (void*)sec_bytes);
    fprintf(stderr, "[DEBUG] sec_bytes[0-3]: %02X %02X %02X %02X\n", sec_bytes[0], sec_bytes[1], sec_bytes[2], sec_bytes[3]);
    size_t bytes_to_write = EO_SEC_COUNT * sizeof(EOSection);
    
    for (size_t i = 0; i < bytes_to_write; i++) {
        fputc(sec_bytes[i], fp);
    }
    size_t written = bytes_to_write;
    fflush(fp);
    fprintf(stderr, "[DEBUG] fwrite returned %zu, expected %zu\n", written, bytes_to_write);
    if (written != bytes_to_write) {
        fclose(fp);
        return false;
    }

    
    if (writer->text_offset > 0) {
        if (fwrite(writer->text, 1, writer->text_offset, fp) != writer->text_offset) {
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

    
    if (writer->text_relocs.count > 0) {
        if (fwrite(writer->text_relocs.relocs, sizeof(EORelocation),
                   writer->text_relocs.count, fp) != writer->text_relocs.count) {
            fclose(fp);
            return false;
        }
    }

    if (writer->data_relocs.count > 0) {
        fprintf(stderr, "[DEBUG] Writing data relocations: count=%u\n", writer->data_relocs.count);
        fprintf(stderr, "[DEBUG] data_relocs.relocs address: %p\n", (void*)writer->data_relocs.relocs);
        for (uint32_t i = 0; i < writer->data_relocs.count; i++) {
            EORelocation* r = &writer->data_relocs.relocs[i];
            fprintf(stderr, "[DEBUG] Reloc %u: offset=%llu, sym_idx=%u, type=%u, addend=%u\n",
                    i, (unsigned long long)r->offset, r->sym_idx, r->type, r->addend);
        }
        fprintf(stderr, "[DEBUG] data_relocs.relocs hex:\n");
        unsigned char* reloc_bytes = (unsigned char*)writer->data_relocs.relocs;
        for (int i = 0; i < (int)(writer->data_relocs.count * sizeof(EORelocation)); i++) {
            fprintf(stderr, "%02X ", reloc_bytes[i]);
        }
        fprintf(stderr, "\n");
        
        for (int i = 0; i < (int)(writer->data_relocs.count * sizeof(EORelocation)); i++) {
            fputc(reloc_bytes[i], fp);
        }
    }

    if (writer->rodata_relocs.count > 0) {
        if (fwrite(writer->rodata_relocs.relocs, sizeof(EORelocation),
                   writer->rodata_relocs.count, fp) != writer->rodata_relocs.count) {
            fclose(fp);
            return false;
        }
    }

    if (writer->bss_relocs.count > 0) {
        if (fwrite(writer->bss_relocs.relocs, sizeof(EORelocation),
                   writer->bss_relocs.count, fp) != writer->bss_relocs.count) {
            fclose(fp);
            return false;
        }
    }

    
    if (writer->sym_count > 0) {
        fprintf(stderr, "[DEBUG] Writing symbols: count=%u, sizeof=%zu\n", writer->sym_count, sizeof(EOSymbol));
        for (uint32_t i = 0; i < writer->sym_count; i++) {
            fprintf(stderr, "[DEBUG] Symbol %u: name='%s', bind=%u\n", i, writer->symbols[i].name, writer->symbols[i].bind);
        }
        if (fwrite(writer->symbols, sizeof(EOSymbol), writer->sym_count, fp) != writer->sym_count) {
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
