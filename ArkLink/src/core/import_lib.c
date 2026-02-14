


#include "import_lib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>


#define AR_MAGIC "!<arch>\n"
#define AR_MAGIC_LEN 8


typedef struct {
    char name[16];      
    char date[12];      
    char uid[6];        
    char gid[6];        
    char mode[8];       
    char size[10];      
    char end[2];        
} ArMemberHeader;


typedef struct {
    uint16_t machine;
    uint16_t num_sections;
    uint32_t time_stamp;
    uint32_t symtab_offset;
    uint32_t num_symbols;
    uint16_t opt_header_size;
    uint16_t characteristics;
} COFFHeader;


typedef struct {
    char name[8];
    uint32_t virtual_size;
    uint32_t virtual_addr;
    uint32_t raw_data_size;
    uint32_t raw_data_offset;
    uint32_t reloc_offset;
    uint32_t line_num_offset;
    uint16_t num_relocs;
    uint16_t num_line_nums;
    uint32_t characteristics;
} COFFSectionHeader;


typedef struct {
    union {
        char short_name[8];
        struct {
            uint32_t zeros;
            uint32_t offset;
        } long_name;
    } name;
    uint32_t value;
    int16_t section;
    uint16_t type;
    uint8_t storage_class;
    uint8_t num_aux;
} COFFSymbol;


#define IMPORT_TYPE_MASK    0x0003
#define IMPORT_NAME_MASK    0x0003
#define IMPORT_ORDINAL      0x0000
#define IMPORT_NAME         0x0001


#define IMPORT_CODE         0x0000
#define IMPORT_DATA         0x0001
#define IMPORT_CONST        0x0002


#define COFF_MACHINE_I386   0x14c
#define COFF_MACHINE_AMD64  0x8664

static int parse_decimal(const char* str, size_t len) {
    int result = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            result = result * 10 + (str[i] - '0');
        } else if (str[i] != ' ') {
            break;
        }
    }
    return result;
}

static uint16_t read_uint16_le(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static uint32_t read_uint32_le(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static int is_coff_object(const uint8_t* data, size_t size) {
    if (size < 20) return 0;
    uint16_t magic = read_uint16_le(data);
    return (magic == COFF_MACHINE_I386 || magic == COFF_MACHINE_AMD64);
}

static int is_msvc_import_header(const uint8_t* data, size_t size) {
    if (size < 20) return 0;
    uint16_t sig1 = read_uint16_le(data);
    uint16_t sig2 = read_uint16_le(data + 2);
    return (sig1 == 0x0000 && sig2 == 0xFFFF);
}


static ArkImportResult parse_msvc_import(ArkImportLib* lib, const uint8_t* data, size_t size) {
    if (size < 20) return ARK_IMPORT_ERR_FORMAT;
    
    




    uint16_t type_name = read_uint16_le(data + 18);
    
    
    ArkImportType type = ARK_IMPORT_TYPE_CODE;
    switch (type_name & IMPORT_TYPE_MASK) {
        case IMPORT_DATA: type = ARK_IMPORT_TYPE_DATA; break;
        case IMPORT_CONST: type = ARK_IMPORT_TYPE_CONST; break;
    }
    
    
    int by_ordinal = ((type_name >> 2) & IMPORT_NAME_MASK) == IMPORT_ORDINAL;
    uint16_t ordinal_hint = read_uint16_le(data + 16);
    
    
    const char* strings = (const char*)(data + 20);
    size_t strings_size = size - 20;
    
    
    const char* dll_name = strings;
    size_t dll_len = strnlen(dll_name, strings_size);
    if (dll_len >= strings_size) return ARK_IMPORT_ERR_FORMAT;
    
    
    const char* symbol_name = dll_name + dll_len + 1;
    if ((size_t)(symbol_name - strings) >= strings_size) return ARK_IMPORT_ERR_FORMAT;
    
    
    if (lib->entry_count >= lib->entry_capacity) {
        size_t new_cap = lib->entry_capacity ? lib->entry_capacity * 2 : 16;
        ArkImportEntry* new_entries = realloc(lib->entries, new_cap * sizeof(ArkImportEntry));
        if (!new_entries) return ARK_IMPORT_ERR_MEMORY;
        lib->entries = new_entries;
        lib->entry_capacity = new_cap;
    }
    
    ArkImportEntry* entry = &lib->entries[lib->entry_count++];
    entry->symbol_name = strdup(symbol_name);
    entry->dll_name = strdup(dll_name);
    entry->type = type;
    entry->hint = by_ordinal ? 0 : ordinal_hint;
    entry->ordinal = by_ordinal ? ordinal_hint : 0;
    entry->by_ordinal = by_ordinal;
    
    if (!entry->symbol_name || !entry->dll_name) {
        return ARK_IMPORT_ERR_MEMORY;
    }
    
    return ARK_IMPORT_OK;
}


static ArkImportResult parse_mingw_import(ArkImportLib* lib, const uint8_t* data, size_t size) {
    if (size < sizeof(COFFHeader)) return ARK_IMPORT_ERR_FORMAT;
    
    COFFHeader coff;
    coff.machine = read_uint16_le(data);
    coff.num_sections = read_uint16_le(data + 2);
    coff.time_stamp = read_uint32_le(data + 4);
    coff.symtab_offset = read_uint32_le(data + 8);
    coff.num_symbols = read_uint32_le(data + 12);
    coff.opt_header_size = read_uint16_le(data + 16);
    coff.characteristics = read_uint16_le(data + 18);
    
    
    const uint8_t* sections = data + 20 + coff.opt_header_size;
    uint32_t idata6_offset = 0;
    uint32_t idata6_size = 0;
    int has_idata = 0;
    
    for (uint16_t i = 0; i < coff.num_sections; i++) {
        const uint8_t* sec = sections + i * 40;
        char sec_name[9] = {0};
        memcpy(sec_name, sec, 8);
        
        
        if (strncmp(sec_name, ".idata$7", 8) == 0) {
            has_idata = 1;
            uint32_t raw_offset = read_uint32_le(sec + 20);
            uint32_t raw_size = read_uint32_le(sec + 16);
            if (raw_offset + raw_size <= size && raw_size > 0) {
                const char* new_dll_name = (const char*)(data + raw_offset);
                
                if (new_dll_name && new_dll_name[0] != '\0') {
                    
                    free(lib->current_dll_name);
                    lib->current_dll_name = strdup(new_dll_name);
                }
            }
        }
        
        else if (strncmp(sec_name, ".idata$6", 8) == 0) {
            has_idata = 1;
            idata6_offset = read_uint32_le(sec + 20);
            idata6_size = read_uint32_le(sec + 16);
        }
    }
    
    
    const char* dll_name = lib->current_dll_name ? lib->current_dll_name : "unknown.dll";

    if (idata6_offset + idata6_size <= size && idata6_size > 2) {
        
        uint32_t pos = idata6_offset;
        while (pos + 2 < idata6_offset + idata6_size) {
            uint16_t hint = read_uint16_le(data + pos);
            pos += 2;
            
            
            const char* name_start = (const char*)(data + pos);
            const char* name_end = name_start;
            while (name_end < (const char*)(data + size) && *name_end != '\0') {
                name_end++;
            }
            
            if (name_end > name_start && name_end < (const char*)(data + size)) {
                size_t name_len = name_end - name_start;
                
                
                if (lib->entry_count >= lib->entry_capacity) {
                    size_t new_cap = lib->entry_capacity ? lib->entry_capacity * 2 : 16;
                    ArkImportEntry* new_entries = realloc(lib->entries, new_cap * sizeof(ArkImportEntry));
                    if (!new_entries) return ARK_IMPORT_ERR_MEMORY;
                    lib->entries = new_entries;
                    lib->entry_capacity = new_cap;
                }
                
                ArkImportEntry* entry = &lib->entries[lib->entry_count++];
                entry->symbol_name = malloc(name_len + 1);
                if (entry->symbol_name) {
                    memcpy(entry->symbol_name, name_start, name_len);
                    entry->symbol_name[name_len] = '\0';
                }
                entry->dll_name = dll_name ? strdup(dll_name) : strdup("unknown.dll");
                entry->type = ARK_IMPORT_TYPE_CODE;
                entry->hint = hint;
                entry->ordinal = 0;
                entry->by_ordinal = 0;
                
                pos += name_len + 1; 
            } else {
                break;
            }
        }
    }
    
    return ARK_IMPORT_OK;
}

ArkImportResult ark_import_lib_open(const char* filename, ArkImportLib** lib) {
    if (!filename || !lib) {
        return ARK_IMPORT_ERR_FORMAT;
    }

    *lib = NULL;

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return ARK_IMPORT_ERR_IO;
    }

    
    char magic[AR_MAGIC_LEN];
    if (fread(magic, 1, AR_MAGIC_LEN, fp) != AR_MAGIC_LEN) {
        fclose(fp);
        return ARK_IMPORT_ERR_FORMAT;
    }

    if (memcmp(magic, AR_MAGIC, AR_MAGIC_LEN) != 0) {
        fclose(fp);
        return ARK_IMPORT_ERR_FORMAT;
    }

    
    ArkImportLib* result = calloc(1, sizeof(ArkImportLib));
    if (!result) {
        fclose(fp);
        return ARK_IMPORT_ERR_MEMORY;
    }

    result->filename = strdup(filename);
    if (!result->filename) {
        free(result);
        fclose(fp);
        return ARK_IMPORT_ERR_MEMORY;
    }

    
    long offset = AR_MAGIC_LEN;
    ArkImportResult parse_result = ARK_IMPORT_OK;
    char* longnames = NULL;

    while (parse_result == ARK_IMPORT_OK) {
        
        ArMemberHeader hdr;
        if (fseek(fp, offset, SEEK_SET) != 0) break;
        if (fread(&hdr, sizeof(hdr), 1, fp) != 1) break;

        
        if (hdr.end[0] != '`' || hdr.end[1] != '\n') {
            break;
        }

        
        int size = parse_decimal(hdr.size, 10);
        if (size <= 0) break;

        
        char member_name[17] = {0};
        memcpy(member_name, hdr.name, 16);
        
        
        if (member_name[0] == '/' && member_name[1] == ' ') {
            
        }
        else if (member_name[0] == '/' && member_name[1] == '/') {
            
            longnames = malloc(size);
            if (longnames) {
                long data_offset = offset + sizeof(ArMemberHeader);
                if (fseek(fp, data_offset, SEEK_SET) == 0) {
                    fread(longnames, 1, size, fp);
                }
            }
        }
        else {
            
            uint8_t* data = malloc(size);
            if (!data) {
                parse_result = ARK_IMPORT_ERR_MEMORY;
                break;
            }

            long data_offset = offset + sizeof(ArMemberHeader);
            if (fseek(fp, data_offset, SEEK_SET) != 0 ||
                fread(data, 1, (size_t)size, fp) != (size_t)size) {
                free(data);
                parse_result = ARK_IMPORT_ERR_IO;
                break;
            }

            
            if (is_msvc_import_header(data, size)) {
                parse_result = parse_msvc_import(result, data, size);
            }
            else if (is_coff_object(data, size)) {
                parse_result = parse_mingw_import(result, data, size);
            }

            free(data);
        }

        
        offset += sizeof(ArMemberHeader) + size;
        if (size % 2 != 0) offset++; 
    }

    free(longnames);
    fclose(fp);

    if (parse_result != ARK_IMPORT_OK && parse_result != ARK_IMPORT_ERR_IO) {
        ark_import_lib_close(result);
        return parse_result;
    }

    *lib = result;
    return ARK_IMPORT_OK;
}

void ark_import_lib_close(ArkImportLib* lib) {
    if (!lib) return;

    for (size_t i = 0; i < lib->entry_count; i++) {
        free(lib->entries[i].symbol_name);
        free(lib->entries[i].dll_name);
    }
    free(lib->entries);
    free(lib->filename);
    free(lib->current_dll_name);
    free(lib);
}

const ArkImportEntry* ark_import_lib_find(const ArkImportLib* lib, const char* symbol_name) {
    if (!lib || !symbol_name) return NULL;

    for (size_t i = 0; i < lib->entry_count; i++) {
        
        const char* entry_name = lib->entries[i].symbol_name;
        if (strcmp(entry_name, symbol_name) == 0) {
            return &lib->entries[i];
        }
        
        if (entry_name[0] == '_' && strcmp(entry_name + 1, symbol_name) == 0) {
            return &lib->entries[i];
        }
        
        if (symbol_name[0] != '_' && entry_name[0] == '_') {
            char temp[256];
            snprintf(temp, sizeof(temp), "_%s", symbol_name);
            if (strcmp(entry_name, temp) == 0) {
                return &lib->entries[i];
            }
        }
    }
    return NULL;
}

ArkImportResult ark_import_lib_add_entry(ArkImportLib* lib, const char* symbol_name,
                                          const char* dll_name, ArkImportType type) {
    if (!lib || !symbol_name || !dll_name) {
        return ARK_IMPORT_ERR_FORMAT;
    }

    if (lib->entry_count >= lib->entry_capacity) {
        size_t new_cap = lib->entry_capacity ? lib->entry_capacity * 2 : 16;
        ArkImportEntry* new_entries = realloc(lib->entries, new_cap * sizeof(ArkImportEntry));
        if (!new_entries) return ARK_IMPORT_ERR_MEMORY;
        lib->entries = new_entries;
        lib->entry_capacity = new_cap;
    }

    ArkImportEntry* entry = &lib->entries[lib->entry_count++];
    entry->symbol_name = strdup(symbol_name);
    entry->dll_name = strdup(dll_name);
    entry->type = type;
    entry->hint = 0;
    entry->ordinal = 0;
    entry->by_ordinal = 0;

    if (!entry->symbol_name || !entry->dll_name) {
        return ARK_IMPORT_ERR_MEMORY;
    }

    return ARK_IMPORT_OK;
}
