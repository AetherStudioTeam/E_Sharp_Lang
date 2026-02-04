

#include "ArkLink/linker.h"
#include "ArkLink/pe.h"
#include "ArkLink/elf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


static const char* error_code_strings[] = {
    [ArkLink_OK] = "Success",
    [ArkLink_ERROR_GENERIC] = "Generic error",
    [ArkLink_ERROR_NOMEM] = "Out of memory",
    [ArkLink_ERROR_IO] = "I/O error",
    [ArkLink_ERROR_INVALID_PARAM] = "Invalid parameter",
    [ArkLink_ERROR_FILE_NOT_FOUND] = "File not found",
    [ArkLink_ERROR_FILE_OPEN] = "Cannot open file",
    [ArkLink_ERROR_FILE_READ] = "Cannot read file",
    [ArkLink_ERROR_FILE_WRITE] = "Cannot write file",
    [ArkLink_ERROR_FILE_TOO_LARGE] = "File too large",
    [ArkLink_ERROR_INVALID_MAGIC] = "Invalid file magic number",
    [ArkLink_ERROR_UNSUPPORTED_VERSION] = "Unsupported file version",
    [ArkLink_ERROR_CORRUPTED_FILE] = "Corrupted file",
    [ArkLink_ERROR_INVALID_SECTION] = "Invalid section",
    [ArkLink_ERROR_INVALID_SYMBOL] = "Invalid symbol",
    [ArkLink_ERROR_INVALID_RELOC] = "Invalid relocation",
    [ArkLink_ERROR_UNDEFINED_SYMBOL] = "Undefined symbol",
    [ArkLink_ERROR_DUPLICATE_SYMBOL] = "Duplicate symbol definition",
    [ArkLink_ERROR_UNRESOLVED_RELOC] = "Unresolved relocation",
    [ArkLink_ERROR_NO_ENTRY_POINT] = "No entry point found",
    [ArkLink_ERROR_SECTION_OVERFLOW] = "Section overflow",
    [ArkLink_ERROR_UNSUPPORTED_TARGET] = "Unsupported target platform",
    [ArkLink_ERROR_PLATFORM_SPECIFIC] = "Platform-specific error",
};


void es_linker_set_error(ArkLinker* linker, ArkLinkError code, const char* fmt, ...) {
    if (!linker) return;
    
    linker->error_info.code = code;
    linker->error_info.file[0] = '\0';
    linker->error_info.line = 0;
    linker->error_info.symbol[0] = '\0';
    linker->error_info.context[0] = '\0';
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(linker->error_info.message, sizeof(linker->error_info.message), fmt, args);
    va_end(args);
    
    
    snprintf(linker->error_msg, sizeof(linker->error_msg), 
             "[%s] %s", es_linker_error_string(code), linker->error_info.message);
}


void es_linker_set_error_with_file(ArkLinker* linker, ArkLinkError code, 
                                    const char* filename, const char* fmt, ...) {
    if (!linker) return;
    
    es_linker_set_error(linker, code, fmt);
    if (filename) {
        strncpy(linker->error_info.file, filename, sizeof(linker->error_info.file) - 1);
        linker->error_info.file[sizeof(linker->error_info.file) - 1] = '\0';
    }
}


void es_linker_set_error_with_symbol(ArkLinker* linker, ArkLinkError code,
                                      const char* symbol, const char* fmt, ...) {
    if (!linker) return;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(linker->error_info.message, sizeof(linker->error_info.message), fmt, args);
    va_end(args);
    
    linker->error_info.code = code;
    if (symbol) {
        strncpy(linker->error_info.symbol, symbol, sizeof(linker->error_info.symbol) - 1);
        linker->error_info.symbol[sizeof(linker->error_info.symbol) - 1] = '\0';
    }
    
    
    snprintf(linker->error_msg, sizeof(linker->error_msg), 
             "[%s] Symbol '%s': %s", es_linker_error_string(code), 
             symbol ? symbol : "?", linker->error_info.message);
}


const char* es_linker_error_string(ArkLinkError code) {
    if (code >= 0 && code < sizeof(error_code_strings) / sizeof(error_code_strings[0])) {
        const char* str = error_code_strings[code];
        if (str) return str;
    }
    return "Unknown error";
}


ArkLinkError es_linker_get_error_code(ArkLinker* linker) {
    return linker ? linker->error_info.code : ArkLink_ERROR_GENERIC;
}


const ArkLinkErrorInfo* es_linker_get_error_info(ArkLinker* linker) {
    return linker ? &linker->error_info : NULL;
}


void es_linker_clear_error(ArkLinker* linker) {
    if (!linker) return;
    memset(&linker->error_info, 0, sizeof(linker->error_info));
    linker->error_msg[0] = '\0';
}


static const ArkLinkerConfig default_config = {
    .output_file = "a.exe",
    .target = ArkLink_TARGET_PE,     
    .is_gui = false,
    .preferred_base = 0x140000000ULL,
    .runtime_lib = NULL,
    .no_default_runtime = false,
    .subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI,
    .major_os_version = 6,
    .minor_os_version = 0,
    .code_align_log2 = 4,           
    .data_align_log2 = 3,           
    .section_alignment = 0x1000,    
    .file_alignment = 0x200,        
};


#define INITIAL_SECTION_CAPACITY (64 * 1024)


ArkLinker* es_linker_create(const ArkLinkerConfig* config) {
    ArkLinker* linker = (ArkLinker*)calloc(1, sizeof(ArkLinker));
    if (!linker) return NULL;
    
    
    if (config) {
        linker->config = *config;
        if (config->output_file) {
            linker->config.output_file = _strdup(config->output_file);
        }
    } else {
        linker->config = default_config;
        linker->config.output_file = _strdup(default_config.output_file);
    }
    
    
    for (int i = 0; i < EO_SEC_MAX; i++) {
        linker->sections[i].data = (uint8_t*)malloc(INITIAL_SECTION_CAPACITY);
        linker->sections[i].file_size = 0;
        linker->sections[i].mem_size = 0;
        linker->sections[i].virtual_address = 0;
        linker->sections[i].align_log2 = (i == EO_SEC_CODE) ? 
            linker->config.code_align_log2 : linker->config.data_align_log2;
        linker->sections[i].flags = 0;
    }
    
    
    linker->sections[EO_SEC_CODE].flags = EO_SECF_EXEC | EO_SECF_READ;
    linker->sections[EO_SEC_DATA].flags = EO_SECF_READ | EO_SECF_WRITE;
    linker->sections[EO_SEC_RODATA].flags = EO_SECF_READ;
    linker->sections[EO_SEC_BSS].flags = EO_SECF_READ | EO_SECF_WRITE | EO_SECF_BSS;
    linker->sections[EO_SEC_TLS].flags = EO_SECF_READ | EO_SECF_WRITE | EO_SECF_TLS;
    
    
    
    linker->symbol_pool = objpool_create(sizeof(ESLibSymbol), 256);
    
    
    linker->reloc_pool = objpool_create(sizeof(ESLibRelocation), 512);
    
    
    linker->import_pool = objpool_create(sizeof(ESImportDll), 16);
    linker->import_func_pool = objpool_create(sizeof(ESImportFunction), 64);
    
    
    linker->string_pool = stringpool_create(1024);
    
    
    linker->base_reloc_pool = mempool_create();
    
    
    linker->input_capacity = 16;
    linker->input_files = (char**)calloc(linker->input_capacity, sizeof(char*));
    linker->symbols = (ESLibSymbol*)calloc(1024, sizeof(ESLibSymbol));  
    linker->relocs = (ESLibRelocation*)calloc(2048, sizeof(ESLibRelocation));
    linker->imports = (ESImportDll*)calloc(32, sizeof(ESImportDll));
    
    bool init_ok = true;
    for (int i = 0; i < EO_SEC_MAX; i++) {
        if (!linker->sections[i].data && i != EO_SEC_BSS && i != EO_SEC_TLS) {
            init_ok = false;
            break;
        }
    }
    
    if (!init_ok || !linker->symbol_pool || !linker->reloc_pool || 
        !linker->import_pool || !linker->import_func_pool ||
        !linker->string_pool || !linker->base_reloc_pool ||
        !linker->input_files || !linker->symbols || !linker->relocs || !linker->imports) {
        es_linker_destroy(linker);
        return NULL;
    }
    
    return linker;
}


void es_linker_destroy(ArkLinker* linker) {
    if (!linker) return;
    
    
    for (int i = 0; i < EO_SEC_MAX; i++) {
        free(linker->sections[i].data);
    }
    
    
    
    objpool_destroy(linker->symbol_pool);
    objpool_destroy(linker->reloc_pool);
    objpool_destroy(linker->import_pool);
    objpool_destroy(linker->import_func_pool);
    
    
    stringpool_destroy(linker->string_pool);
    
    
    mempool_destroy(linker->base_reloc_pool);
    
    
    free(linker->symbols);
    free(linker->relocs);
    free(linker->imports);
    
    
    for (int i = 0; i < linker->input_count; i++) {
        free(linker->input_files[i]);
    }
    free(linker->input_files);
    
    free((void*)linker->config.output_file);
    free(linker);
}


static bool expand_section(ESLibSection* section, uint32_t needed) {
    if (!section) return false;
    
    
    if (!section->data) {
        section->data = (uint8_t*)malloc(INITIAL_SECTION_CAPACITY);
        if (!section->data) return false;
        
        
    }

    
    uint32_t total_needed = section->file_size + needed;
    
    
    uint32_t current_capacity = INITIAL_SECTION_CAPACITY;
    while (current_capacity < section->file_size && current_capacity > 0) {
        current_capacity *= 2;
    }
    
    
    if (total_needed > current_capacity) {
        uint32_t new_capacity = current_capacity;
        if (new_capacity == 0) new_capacity = INITIAL_SECTION_CAPACITY;
        while (new_capacity < total_needed && new_capacity > 0) {
            new_capacity *= 2;
        }
        
        uint8_t* new_data = (uint8_t*)realloc(section->data, new_capacity);
        if (!new_data) return false;
        section->data = new_data;
    }
    
    return true;
}


static int add_symbol(ArkLinker* linker, const char* name, uint32_t type,
                      uint32_t section, uint32_t offset, uint32_t size, 
                      bool is_defined) {
    
    const char* interned_name = stringpool_intern(linker->string_pool, name);
    if (interned_name == NULL) return -1;
    
    
    for (int i = 0; i < linker->symbol_count; i++) {
        
        const char* sym_name = (const char*)linker->symbols[i].name;
        if (sym_name && sym_name == interned_name) {
            if (is_defined && linker->symbols[i].is_defined) {
                
                linker->symbols[i].section = section;
                linker->symbols[i].offset = offset;
                linker->symbols[i].size = size;
                linker->symbols[i].type = type;
                return i;
            }
            if (is_defined) {
                
                linker->symbols[i].section = section;
                linker->symbols[i].offset = offset;
                linker->symbols[i].size = size;
                linker->symbols[i].type = type;
                linker->symbols[i].is_defined = true;
            }
            return i;
        }
    }
    
    
    if (linker->symbol_count >= 1024) {
        int new_capacity = linker->symbol_count * 2;
        ESLibSymbol* new_symbols = (ESLibSymbol*)realloc(linker->symbols, 
            new_capacity * sizeof(ESLibSymbol));
        if (!new_symbols) return -1;
        linker->symbols = new_symbols;
    }
    
    int idx = linker->symbol_count++;
    
    linker->symbols[idx].name = (char*)interned_name;
    linker->symbols[idx].type = type;
    linker->symbols[idx].section = section;
    linker->symbols[idx].offset = offset;
    linker->symbols[idx].size = size;
    linker->symbols[idx].is_defined = is_defined;
    linker->symbols[idx].is_exported = false;
    
    linker->stats.symbols_allocated++;
    
    return idx;
}


static bool add_relocation(ArkLinker* linker, const char* symbol_name,
                           uint32_t section, uint32_t offset, 
                           uint16_t type, int32_t addend) {
    
    if (linker->reloc_count >= 2048) {
        int new_capacity = linker->reloc_count * 2;
        ESLibRelocation* new_relocs = (ESLibRelocation*)realloc(linker->relocs,
            new_capacity * sizeof(ESLibRelocation));
        if (!new_relocs) return false;
        linker->relocs = new_relocs;
    }
    
    
    const char* interned_name = stringpool_intern(linker->string_pool, symbol_name);
    if (interned_name == NULL) return false;
    
    int idx = linker->reloc_count++;
    
    linker->relocs[idx].symbol_name = (char*)interned_name;
    linker->relocs[idx].symbol_index = (uint32_t)-1;
    linker->relocs[idx].section = section;
    linker->relocs[idx].offset = offset;
    linker->relocs[idx].type = type;
    linker->relocs[idx].addend = addend;
    
    linker->stats.relocs_allocated++;
    
    return true;
}


static bool read_eo_file(ArkLinker* linker, const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "DEBUG: Failed to open file: %s, error code: %d\n", filename, errno);
        es_linker_set_error_with_file(linker, ArkLink_ERROR_FILE_OPEN,
            filename, "Cannot open file for reading");
        return false;
    }
    
    
    EOHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        es_linker_set_error_with_file(linker, ArkLink_ERROR_FILE_READ,
            filename, "Cannot read file header");
        fclose(fp);
        return false;
    }
    
    
    if (header.magic != EO_MAGIC) {
        fprintf(stderr, "DEBUG: EO_MAGIC mismatch! expected=0x%08X, got=0x%08X\n", EO_MAGIC, header.magic);
        es_linker_set_error_with_file(linker, ArkLink_ERROR_INVALID_MAGIC,
            filename, "Invalid magic number (expected 0x%08X, got 0x%08X)",
            EO_MAGIC, header.magic);
        fclose(fp);
        return false;
    }
    
    fprintf(stderr, "DEBUG: EO file '%s' loaded successfully\n", filename);
    fprintf(stderr, "DEBUG: entry_section=0x%02X, entry_point=%u\n",
            header.entry_section, header.entry_point);
    
    
    if (header.version != EO_VERSION) {
        es_linker_set_error_with_file(linker, ArkLink_ERROR_UNSUPPORTED_VERSION,
            filename, "Unsupported version (expected %d, got %d)",
            EO_VERSION, header.version);
        fclose(fp);
        return false;
    }
    
    
    fprintf(stderr, "DEBUG: Reading %s\n", filename);
    fprintf(stderr, "DEBUG: code_size=%u, data_size=%u, rodata_size=%u\n",
            header.sections[EO_SEC_CODE].file_size,
            header.sections[EO_SEC_DATA].file_size,
            header.sections[EO_SEC_RODATA].file_size);
    fprintf(stderr, "DEBUG: sym_count=%u, reloc_count=%u\n",
            header.sym_count, header.reloc_count);
    
    
    uint32_t code_size = header.sections[EO_SEC_CODE].file_size;
    uint32_t data_size = header.sections[EO_SEC_DATA].file_size;
    uint32_t rodata_size = header.sections[EO_SEC_RODATA].file_size;
    uint32_t tls_size = header.sections[EO_SEC_TLS].file_size;
    
    
    
    
    uint32_t sym_table_offset = EO_HEADER_SIZE + code_size + data_size + rodata_size;
    uint32_t reloc_table_offset = sym_table_offset + header.sym_count * sizeof(EOSymbol);
    uint32_t string_table_offset = reloc_table_offset + header.reloc_count * sizeof(EORelocation);
    
    fprintf(stderr, "DEBUG: File layout: header=%d, code=%d, data=%d, rodata=%d, tls=%d\n",
            EO_HEADER_SIZE, code_size, data_size, rodata_size, tls_size);
    fprintf(stderr, "DEBUG: sym_table_offset=%u, reloc_table_offset=%u, string_table_offset=%u\n",
            sym_table_offset, reloc_table_offset, string_table_offset);
    fprintf(stderr, "DEBUG: sym_count=%u, reloc_count=%u, string_table_size=%u\n",
            header.sym_count, header.reloc_count, header.string_table_size);
    
    
    
    uint32_t code_offset = (linker->sections[EO_SEC_CODE].file_size + 15) & ~15;
    uint32_t data_offset = (linker->sections[EO_SEC_DATA].file_size + 15) & ~15;
    uint32_t rodata_offset = (linker->sections[EO_SEC_RODATA].file_size + 15) & ~15;
    uint32_t tls_offset = (linker->sections[EO_SEC_TLS].file_size + 15) & ~15;
    uint32_t bss_offset = (linker->sections[EO_SEC_BSS].mem_size + 15) & ~15;

    fprintf(stderr, "DEBUG: %s: code_offset=0x%x, data_offset=0x%x, file_size before=0x%x\n",
            filename, code_offset, data_offset, linker->sections[EO_SEC_CODE].file_size);
    
    linker->sections[EO_SEC_CODE].file_size = linker->sections[EO_SEC_CODE].mem_size = code_offset;
    linker->sections[EO_SEC_DATA].file_size = linker->sections[EO_SEC_DATA].mem_size = data_offset;
    linker->sections[EO_SEC_RODATA].file_size = linker->sections[EO_SEC_RODATA].mem_size = rodata_offset;
    linker->sections[EO_SEC_TLS].file_size = linker->sections[EO_SEC_TLS].mem_size = tls_offset;
    linker->sections[EO_SEC_BSS].mem_size = bss_offset;

    
    if (code_size > 0) {
        if (!expand_section(&linker->sections[EO_SEC_CODE], code_size)) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_NOMEM,
                filename, "Out of memory when expanding code section");
            fclose(fp);
            return false;
        }
        fseek(fp, EO_CODE_OFFSET(&header), SEEK_SET);
        if (fread(linker->sections[EO_SEC_CODE].data + code_offset, 1, code_size, fp) != code_size) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_FILE_READ,
                filename, "Cannot read code section");
            fclose(fp);
            return false;
        }
        linker->sections[EO_SEC_CODE].file_size += code_size;
        linker->sections[EO_SEC_CODE].mem_size += code_size;
        
        
        if (strstr(filename, "es_runtime") != NULL) {
            fprintf(stderr, "DEBUG: es_putchar at offset 0x%x (code_offset=0x%x):\n", code_offset + 0x0, code_offset);
            uint8_t* code = linker->sections[EO_SEC_CODE].data + code_offset + 0x0;
            fprintf(stderr, "  ");
            for (int i = 0; i < 16; i++) {
                fprintf(stderr, "%02x ", code[i]);
            }
            fprintf(stderr, "\n");
        }
    }

    
    if (data_size > 0) {
        if (!expand_section(&linker->sections[EO_SEC_DATA], data_size)) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_NOMEM,
                filename, "Out of memory when expanding data section");
            fclose(fp);
            return false;
        }
        fseek(fp, EO_DATA_OFFSET(&header), SEEK_SET);
        if (fread(linker->sections[EO_SEC_DATA].data + data_offset, 1, data_size, fp) != data_size) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_FILE_READ,
                filename, "Cannot read data section");
            fclose(fp);
            return false;
        }
        linker->sections[EO_SEC_DATA].file_size += data_size;
        linker->sections[EO_SEC_DATA].mem_size += data_size;
    }

    
    if (rodata_size > 0) {
        if (!expand_section(&linker->sections[EO_SEC_RODATA], rodata_size)) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_NOMEM,
                filename, "Out of memory when expanding rodata section");
            fclose(fp);
            return false;
        }
        fseek(fp, EO_RODATA_OFFSET(&header), SEEK_SET);
        if (fread(linker->sections[EO_SEC_RODATA].data + rodata_offset, 1, rodata_size, fp) != rodata_size) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_FILE_READ,
                filename, "Cannot read rodata section");
            fclose(fp);
            return false;
        }
        linker->sections[EO_SEC_RODATA].file_size += rodata_size;
        linker->sections[EO_SEC_RODATA].mem_size += rodata_size;
    }
    
    
    if (tls_size > 0) {
        if (!expand_section(&linker->sections[EO_SEC_TLS], tls_size)) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_NOMEM,
                filename, "Out of memory when expanding TLS section");
            fclose(fp);
            return false;
        }
        fseek(fp, EO_HEADER_SIZE + code_size + data_size + rodata_size, SEEK_SET);
        if (fread(linker->sections[EO_SEC_TLS].data + tls_offset, 1, tls_size, fp) != tls_size) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_FILE_READ,
                filename, "Cannot read TLS section");
            fclose(fp);
            return false;
        }
        linker->sections[EO_SEC_TLS].file_size += tls_size;
        linker->sections[EO_SEC_TLS].mem_size += tls_size;
        
        if (header.sections[EO_SEC_TLS].align_log2 > linker->sections[EO_SEC_TLS].align_log2) {
            linker->sections[EO_SEC_TLS].align_log2 = header.sections[EO_SEC_TLS].align_log2;
        }
    }

    
    uint32_t bss_size = header.sections[EO_SEC_BSS].mem_size;
    if (bss_size > 0) {
        linker->sections[EO_SEC_BSS].mem_size += bss_size;
        
        if (header.sections[EO_SEC_BSS].align_log2 > linker->sections[EO_SEC_BSS].align_log2) {
            linker->sections[EO_SEC_BSS].align_log2 = header.sections[EO_SEC_BSS].align_log2;
        }
    }

    
    EOSymbol* symbols = NULL;
    if (header.sym_count > 0) {
        symbols = (EOSymbol*)malloc(header.sym_count * sizeof(EOSymbol));
        if (!symbols) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_NOMEM,
                filename, "Out of memory when allocating symbol table");
            fclose(fp);
            return false;
        }

        fseek(fp, sym_table_offset, SEEK_SET);
        if (fread(symbols, sizeof(EOSymbol), header.sym_count, fp) != header.sym_count) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_FILE_READ,
                filename, "Cannot read symbol table");
            free(symbols);
            fclose(fp);
            return false;
        }
    }

    
    EORelocation* relocs = NULL;
    if (header.reloc_count > 0) {
        relocs = (EORelocation*)malloc(header.reloc_count * sizeof(EORelocation));
        if (!relocs) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_NOMEM,
                filename, "Out of memory when allocating relocation table");
            free(symbols);
            fclose(fp);
            return false;
        }

        fseek(fp, reloc_table_offset, SEEK_SET);
        if (fread(relocs, sizeof(EORelocation), header.reloc_count, fp) != header.reloc_count) {
            es_linker_set_error_with_file(linker, ArkLink_ERROR_FILE_READ,
                filename, "Cannot read relocation table");
            free(symbols);
            free(relocs);
            fclose(fp);
            return false;
        }
    }
    
    
    char* strings = NULL;
    if (header.string_table_size > 0) {
        strings = (char*)malloc(header.string_table_size);
        if (!strings) {
            snprintf(linker->error_msg, sizeof(linker->error_msg),
                "Out of memory");
            free(symbols);
            free(relocs);
            fclose(fp);
            return false;
        }
        
        fseek(fp, string_table_offset, SEEK_SET);
        if (fread(strings, 1, header.string_table_size, fp) != header.string_table_size) {
            snprintf(linker->error_msg, sizeof(linker->error_msg),
                "Cannot read string table from: %s", filename);
            free(symbols);
            free(relocs);
            free(strings);
            fclose(fp);
            return false;
        }
    }
    
    
    fprintf(stderr, "DEBUG: String table size=%u, content: ", header.string_table_size);
    for (uint32_t j = 0; j < header.string_table_size && j < 100; j++) {
        if (strings[j] == '\0') fprintf(stderr, "|");
        else fprintf(stderr, "%c", strings[j]);
    }
    fprintf(stderr, "\n");
    
    
    for (uint32_t i = 0; i < header.sym_count; i++) {
        const char* sym_name = strings + symbols[i].name_offset;
        fprintf(stderr, "DEBUG: Symbol %u: name_offset=%u, name='%s'\n", 
                i, symbols[i].name_offset, sym_name ? sym_name : "(null)");
        uint32_t section = symbols[i].section;
        uint32_t offset = symbols[i].offset;
        
        
        switch (section) {
            case EO_SEC_CODE:
                offset += code_offset;
                break;
            case EO_SEC_DATA:
                offset += data_offset;
                break;
            case EO_SEC_RODATA:
                offset += rodata_offset;
                break;
            case EO_SEC_BSS:
                offset += bss_offset;
                break;
        }
        
        bool is_defined = (symbols[i].type != EO_SYM_UNDEFINED);
        if (strcmp(sym_name, "mainCRTStartup") == 0) {
            fprintf(stderr, "DEBUG: Adding mainCRTStartup: raw_offset=0x%x, code_offset=0x%x, final_offset=0x%x\n",
                    symbols[i].offset, code_offset, offset);
        }
        add_symbol(linker, sym_name, symbols[i].type, section, offset, 
                   symbols[i].size, is_defined);
    }
    
    
    fprintf(stderr, "DEBUG: Processing %u relocations\n", header.reloc_count);
    for (uint32_t i = 0; i < header.reloc_count; i++) {
        uint32_t sym_idx = relocs[i].symbol_index;
        fprintf(stderr, "DEBUG: Reloc %u: sym_idx=%u, section=%u, offset=%u\n",
                i, sym_idx, relocs[i].section, relocs[i].offset);
        if (sym_idx >= header.sym_count) {
            fprintf(stderr, "DEBUG: sym_idx out of range\n");
            continue;
        }
        
        const char* sym_name = strings + symbols[sym_idx].name_offset;
        fprintf(stderr, "DEBUG: Reloc %u: sym_name='%s', raw_offset=%u, code_offset=%u\n", 
                i, sym_name, relocs[i].offset, code_offset);
        uint32_t section = relocs[i].section;
        uint32_t offset = relocs[i].offset;
        
        
        switch (section) {
            case EO_SEC_CODE:
                offset += code_offset;
                break;
            case EO_SEC_DATA:
                offset += data_offset;
                break;
            case EO_SEC_RODATA:
                offset += rodata_offset;
                break;
        }
        
        fprintf(stderr, "DEBUG: Reloc %u: final_offset=%u\n", i, offset);
        add_relocation(linker, sym_name, section, offset, 
                       relocs[i].type, relocs[i].addend);
    }
    
    
    
    if (header.entry_section != 0xFF) {
        fprintf(stderr, "DEBUG: File %s has entry point: raw=%u, section=%u\n",
                filename, header.entry_point, header.entry_section);
        
        
        
        if (!linker->has_entry) {
            
            uint32_t entry_offset = header.entry_point;
            switch (header.entry_section) {
                case EO_SEC_CODE:
                    entry_offset += code_offset;
                    break;
                case EO_SEC_DATA:
                    entry_offset += data_offset;
                    break;
                case EO_SEC_RODATA:
                    entry_offset += rodata_offset;
                    break;
            }
            
            linker->entry_point = entry_offset;
            linker->entry_section = header.entry_section;
            linker->has_entry = true;
            fprintf(stderr, "DEBUG: Using entry point from %s: section=%u, offset=0x%x\n",
                    filename, header.entry_section, entry_offset);
        }
    }
    
    
    free(symbols);
    free(relocs);
    free(strings);
    fclose(fp);
    
    return true;
}


bool es_linker_add_file(ArkLinker* linker, const char* filename) {
    if (!linker || !filename) return false;
    
    
    char** new_files = (char**)realloc(linker->input_files, 
        (linker->input_count + 1) * sizeof(char*));
    if (!new_files) return false;
    
    linker->input_files = new_files;
    linker->input_files[linker->input_count] = _strdup(filename);
    linker->input_count++;
    
    return true;
}


bool es_linker_add_import(ArkLinker* linker, const char* dll_name, 
                          const char* func_name, uint32_t hint) {
    if (!linker || !dll_name || !func_name) return false;
    
    const char* dll_name_interned = stringpool_intern(linker->string_pool, dll_name);
    const char* func_name_interned = stringpool_intern(linker->string_pool, func_name);
    if (dll_name_interned == NULL || func_name_interned == NULL) return false;
    
    
    ESImportDll* dll = NULL;
    for (int i = 0; i < linker->import_count; i++) {
        if (linker->imports[i].dll_name == dll_name_interned) {
            dll = &linker->imports[i];
            break;
        }
    }
    
    if (!dll) {
        
        if (linker->import_count >= 32) {
            int new_capacity = linker->import_count * 2;
            ESImportDll* new_imports = (ESImportDll*)realloc(linker->imports,
                new_capacity * sizeof(ESImportDll));
            if (!new_imports) return false;
            linker->imports = new_imports;
        }
        
        dll = &linker->imports[linker->import_count++];
        dll->dll_name = (char*)dll_name_interned;
        dll->functions = NULL;
        dll->func_count = 0;
        dll->func_capacity = 0;
    }
    
    
    for (int i = 0; i < dll->func_count; i++) {
        if (dll->functions[i].func_name == func_name_interned) {
            return true;  
        }
    }
    
    
    if (dll->func_count >= dll->func_capacity) {
        int new_capacity = dll->func_capacity ? dll->func_capacity * 2 : 8;
        ESImportFunction* new_funcs = (ESImportFunction*)realloc(dll->functions,
            new_capacity * sizeof(ESImportFunction));
        if (!new_funcs) return false;
        dll->functions = new_funcs;
        dll->func_capacity = new_capacity;
    }
    
    ESImportFunction* func = &dll->functions[dll->func_count++];
    func->dll_name = (char*)dll_name_interned;
    func->func_name = (char*)func_name_interned;
    func->hint = hint;
    func->iat_offset = 0;  
    
    return true;
}


bool es_linker_add_kernel32_imports(ArkLinker* linker) {
    if (!linker) return false;
    
    
    es_linker_add_import(linker, "kernel32.dll", "ExitProcess", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetModuleHandleA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetProcAddress", 0);
    es_linker_add_import(linker, "kernel32.dll", "LoadLibraryA", 0);
    es_linker_add_import(linker, "kernel32.dll", "FreeLibrary", 0);
    es_linker_add_import(linker, "kernel32.dll", "VirtualAlloc", 0);
    es_linker_add_import(linker, "kernel32.dll", "VirtualFree", 0);
    es_linker_add_import(linker, "kernel32.dll", "HeapAlloc", 0);
    es_linker_add_import(linker, "kernel32.dll", "HeapFree", 0);
    es_linker_add_import(linker, "kernel32.dll", "HeapReAlloc", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetProcessHeap", 0);
    es_linker_add_import(linker, "kernel32.dll", "HeapCreate", 0);
    es_linker_add_import(linker, "kernel32.dll", "HeapDestroy", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetStdHandle", 0);
    es_linker_add_import(linker, "kernel32.dll", "WriteFile", 0);
    es_linker_add_import(linker, "kernel32.dll", "WriteConsoleA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetCommandLineA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetEnvironmentVariableA", 0);
    es_linker_add_import(linker, "kernel32.dll", "QueryPerformanceCounter", 0);
    es_linker_add_import(linker, "kernel32.dll", "QueryPerformanceFrequency", 0);
    es_linker_add_import(linker, "kernel32.dll", "Sleep", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetSystemTimeAsFileTime", 0);
    es_linker_add_import(linker, "kernel32.dll", "FileTimeToLocalFileTime", 0);
    es_linker_add_import(linker, "kernel32.dll", "FileTimeToSystemTime", 0);
    es_linker_add_import(linker, "kernel32.dll", "SystemTimeToFileTime", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetLocalTime", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetSystemTime", 0);
    
    
    es_linker_add_import(linker, "kernel32.dll", "CreateFileA", 0);
    es_linker_add_import(linker, "kernel32.dll", "ReadFile", 0);
    es_linker_add_import(linker, "kernel32.dll", "CloseHandle", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetFilePointer", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetFileSize", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetFileSizeEx", 0);
    es_linker_add_import(linker, "kernel32.dll", "DeleteFileA", 0);
    es_linker_add_import(linker, "kernel32.dll", "MoveFileA", 0);
    es_linker_add_import(linker, "kernel32.dll", "CreateDirectoryA", 0);
    es_linker_add_import(linker, "kernel32.dll", "RemoveDirectoryA", 0);
    es_linker_add_import(linker, "kernel32.dll", "FindFirstFileA", 0);
    es_linker_add_import(linker, "kernel32.dll", "FindNextFileA", 0);
    es_linker_add_import(linker, "kernel32.dll", "FindClose", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetCurrentDirectoryA", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetCurrentDirectoryA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetFileAttributesA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetFullPathNameA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetFullPathNameW", 0);
    es_linker_add_import(linker, "kernel32.dll", "WideCharToMultiByte", 0);
    es_linker_add_import(linker, "kernel32.dll", "MultiByteToWideChar", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetStringTypeW", 0);
    es_linker_add_import(linker, "kernel32.dll", "LCMapStringW", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetLocaleInfoW", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetUserDefaultLCID", 0);
    es_linker_add_import(linker, "kernel32.dll", "CompareStringW", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetTimeZoneInformation", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetTimeZoneInformation", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetTickCount", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetCurrentProcessId", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetCurrentThreadId", 0);
    es_linker_add_import(linker, "kernel32.dll", "TerminateProcess", 0);
    es_linker_add_import(linker, "kernel32.dll", "IsProcessorFeaturePresent", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetNativeSystemInfo", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetSystemInfo", 0);
    es_linker_add_import(linker, "kernel32.dll", "GlobalAlloc", 0);
    es_linker_add_import(linker, "kernel32.dll", "GlobalFree", 0);
    es_linker_add_import(linker, "kernel32.dll", "GlobalLock", 0);
    es_linker_add_import(linker, "kernel32.dll", "GlobalUnlock", 0);
    es_linker_add_import(linker, "kernel32.dll", "GlobalSize", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetModuleFileNameA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetModuleFileNameW", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetVersion", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetVersionExA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetVersionExW", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetEndOfFile", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetFilePointerEx", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetFileInformationByHandle", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetFileType", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetStdHandle", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetStdHandle", 0);
    es_linker_add_import(linker, "kernel32.dll", "DuplicateHandle", 0);
    es_linker_add_import(linker, "kernel32.dll", "FlushFileBuffers", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetConsoleMode", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleMode", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetConsoleOutputCP", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleOutputCP", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleCtrlHandler", 0);
    es_linker_add_import(linker, "kernel32.dll", "GenerateConsoleCtrlEvent", 0);
    es_linker_add_import(linker, "kernel32.dll", "AllocConsole", 0);
    es_linker_add_import(linker, "kernel32.dll", "FreeConsole", 0);
    es_linker_add_import(linker, "kernel32.dll", "AttachConsole", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetConsoleTitleA", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetConsoleTitleW", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleTitleA", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleTitleW", 0);
    es_linker_add_import(linker, "kernel32.dll", "ReadConsoleA", 0);
    es_linker_add_import(linker, "kernel32.dll", "ReadConsoleW", 0);
    es_linker_add_import(linker, "kernel32.dll", "WriteConsoleA", 0);
    es_linker_add_import(linker, "kernel32.dll", "WriteConsoleW", 0);
    es_linker_add_import(linker, "kernel32.dll", "WriteConsoleInputA", 0);
    es_linker_add_import(linker, "kernel32.dll", "WriteConsoleInputW", 0);
    es_linker_add_import(linker, "kernel32.dll", "ReadConsoleInputA", 0);
    es_linker_add_import(linker, "kernel32.dll", "ReadConsoleInputW", 0);
    es_linker_add_import(linker, "kernel32.dll", "PeekConsoleInputA", 0);
    es_linker_add_import(linker, "kernel32.dll", "PeekConsoleInputW", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetNumberOfConsoleInputEvents", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetConsoleCursorInfo", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleCursorInfo", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetConsoleScreenBufferInfo", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleScreenBufferInfo", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleScreenBufferSize", 0);
    es_linker_add_import(linker, "kernel32.dll", "ScrollConsoleScreenBufferA", 0);
    es_linker_add_import(linker, "kernel32.dll", "ScrollConsoleScreenBufferW", 0);
    es_linker_add_import(linker, "kernel32.dll", "FillConsoleOutputAttribute", 0);
    es_linker_add_import(linker, "kernel32.dll", "FillConsoleOutputCharacterA", 0);
    es_linker_add_import(linker, "kernel32.dll", "FillConsoleOutputCharacterW", 0);
    es_linker_add_import(linker, "kernel32.dll", "WriteConsoleOutputA", 0);
    es_linker_add_import(linker, "kernel32.dll", "WriteConsoleOutputW", 0);
    es_linker_add_import(linker, "kernel32.dll", "ReadConsoleOutputA", 0);
    es_linker_add_import(linker, "kernel32.dll", "ReadConsoleOutputW", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetLargestConsoleWindowSize", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleCursorPosition", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleWindowInfo", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleActiveScreenBuffer", 0);
    es_linker_add_import(linker, "kernel32.dll", "CreateConsoleScreenBuffer", 0);
    es_linker_add_import(linker, "kernel32.dll", "GetConsoleCP", 0);
    es_linker_add_import(linker, "kernel32.dll", "SetConsoleCP", 0);

    es_linker_add_import(linker, "user32.dll", "MessageBoxA", 0);
    es_linker_add_import(linker, "user32.dll", "CreateWindowExA", 0);
    es_linker_add_import(linker, "user32.dll", "RegisterClassExA", 0);
    es_linker_add_import(linker, "user32.dll", "DefWindowProcA", 0);
    es_linker_add_import(linker, "user32.dll", "ShowWindow", 0);
    es_linker_add_import(linker, "user32.dll", "UpdateWindow", 0);
    es_linker_add_import(linker, "user32.dll", "GetMessageA", 0);
    es_linker_add_import(linker, "user32.dll", "PeekMessageA", 0);
    es_linker_add_import(linker, "user32.dll", "TranslateMessage", 0);
    es_linker_add_import(linker, "user32.dll", "DispatchMessageA", 0);
    es_linker_add_import(linker, "user32.dll", "PostQuitMessage", 0);
    es_linker_add_import(linker, "user32.dll", "DestroyWindow", 0);
    es_linker_add_import(linker, "user32.dll", "SetWindowTextA", 0);
    es_linker_add_import(linker, "user32.dll", "GetWindowTextA", 0);
    es_linker_add_import(linker, "user32.dll", "SetWindowPos", 0);
    es_linker_add_import(linker, "user32.dll", "GetClientRect", 0);
    es_linker_add_import(linker, "user32.dll", "GetDC", 0);
    es_linker_add_import(linker, "user32.dll", "ReleaseDC", 0);
    es_linker_add_import(linker, "user32.dll", "BeginPaint", 0);
    es_linker_add_import(linker, "user32.dll", "EndPaint", 0);
    es_linker_add_import(linker, "user32.dll", "InvalidateRect", 0);
    es_linker_add_import(linker, "user32.dll", "LoadCursorA", 0);
    es_linker_add_import(linker, "user32.dll", "SetWindowLongPtrA", 0);
    es_linker_add_import(linker, "user32.dll", "GetWindowLongPtrA", 0);
    es_linker_add_import(linker, "user32.dll", "SendMessageA", 0);
    es_linker_add_import(linker, "user32.dll", "PostMessageA", 0);
    
    
    es_linker_add_import(linker, "gdi32.dll", "TextOutA", 0);
    es_linker_add_import(linker, "gdi32.dll", "SetTextColor", 0);
    es_linker_add_import(linker, "gdi32.dll", "SetBkMode", 0);
    es_linker_add_import(linker, "gdi32.dll", "CreateSolidBrush", 0);
    es_linker_add_import(linker, "gdi32.dll", "FillRect", 0);
    es_linker_add_import(linker, "gdi32.dll", "DeleteObject", 0);
    es_linker_add_import(linker, "gdi32.dll", "CreatePen", 0);
    es_linker_add_import(linker, "gdi32.dll", "SelectObject", 0);
    es_linker_add_import(linker, "gdi32.dll", "MoveToEx", 0);
    es_linker_add_import(linker, "gdi32.dll", "LineTo", 0);
    es_linker_add_import(linker, "gdi32.dll", "Rectangle", 0);
    es_linker_add_import(linker, "gdi32.dll", "Ellipse", 0);
    es_linker_add_import(linker, "gdi32.dll", "SetPixel", 0);
    es_linker_add_import(linker, "gdi32.dll", "GetStockObject", 0);
    
    
    es_linker_add_import(linker, "comdlg32.dll", "GetOpenFileNameA", 0);
    es_linker_add_import(linker, "comdlg32.dll", "GetSaveFileNameA", 0);
    es_linker_add_import(linker, "comdlg32.dll", "ChooseColorA", 0);
    es_linker_add_import(linker, "comdlg32.dll", "ChooseFontA", 0);
    
    
    es_linker_add_import(linker, "ws2_32.dll", "WSAStartup", 0);
    es_linker_add_import(linker, "ws2_32.dll", "WSACleanup", 0);
    es_linker_add_import(linker, "ws2_32.dll", "socket", 0);
    es_linker_add_import(linker, "ws2_32.dll", "connect", 0);
    es_linker_add_import(linker, "ws2_32.dll", "bind", 0);
    es_linker_add_import(linker, "ws2_32.dll", "listen", 0);
    es_linker_add_import(linker, "ws2_32.dll", "accept", 0);
    es_linker_add_import(linker, "ws2_32.dll", "send", 0);
    es_linker_add_import(linker, "ws2_32.dll", "recv", 0);
    es_linker_add_import(linker, "ws2_32.dll", "sendto", 0);
    es_linker_add_import(linker, "ws2_32.dll", "recvfrom", 0);
    es_linker_add_import(linker, "ws2_32.dll", "closesocket", 0);
    es_linker_add_import(linker, "ws2_32.dll", "shutdown", 0);
    es_linker_add_import(linker, "ws2_32.dll", "gethostbyname", 0);
    es_linker_add_import(linker, "ws2_32.dll", "gethostbyaddr", 0);
    es_linker_add_import(linker, "ws2_32.dll", "inet_addr", 0);
    es_linker_add_import(linker, "ws2_32.dll", "inet_ntoa", 0);
    es_linker_add_import(linker, "ws2_32.dll", "htons", 0);
    es_linker_add_import(linker, "ws2_32.dll", "ntohs", 0);
    es_linker_add_import(linker, "ws2_32.dll", "htonl", 0);
    es_linker_add_import(linker, "ws2_32.dll", "ntohl", 0);
    es_linker_add_import(linker, "ws2_32.dll", "select", 0);
    es_linker_add_import(linker, "ws2_32.dll", "setsockopt", 0);
    es_linker_add_import(linker, "ws2_32.dll", "getsockopt", 0);
    es_linker_add_import(linker, "ws2_32.dll", "ioctlsocket", 0);
    es_linker_add_import(linker, "ws2_32.dll", "WSAGetLastError", 0);
    es_linker_add_import(linker, "ws2_32.dll", "getsockname", 0);
    es_linker_add_import(linker, "ws2_32.dll", "getpeername", 0);
    
    
    es_linker_add_import(linker, "msvcrt.dll", "sin", 0);
    es_linker_add_import(linker, "msvcrt.dll", "cos", 0);
    es_linker_add_import(linker, "msvcrt.dll", "tan", 0);
    es_linker_add_import(linker, "msvcrt.dll", "asin", 0);
    es_linker_add_import(linker, "msvcrt.dll", "acos", 0);
    es_linker_add_import(linker, "msvcrt.dll", "atan", 0);
    es_linker_add_import(linker, "msvcrt.dll", "atan2", 0);
    es_linker_add_import(linker, "msvcrt.dll", "sinh", 0);
    es_linker_add_import(linker, "msvcrt.dll", "cosh", 0);
    es_linker_add_import(linker, "msvcrt.dll", "tanh", 0);
    es_linker_add_import(linker, "msvcrt.dll", "exp", 0);
    es_linker_add_import(linker, "msvcrt.dll", "log", 0);
    es_linker_add_import(linker, "msvcrt.dll", "log10", 0);
    es_linker_add_import(linker, "msvcrt.dll", "pow", 0);
    es_linker_add_import(linker, "msvcrt.dll", "sqrt", 0);
    es_linker_add_import(linker, "msvcrt.dll", "ceil", 0);
    es_linker_add_import(linker, "msvcrt.dll", "floor", 0);
    es_linker_add_import(linker, "msvcrt.dll", "fabs", 0);
    es_linker_add_import(linker, "msvcrt.dll", "fmod", 0);
    es_linker_add_import(linker, "msvcrt.dll", "modf", 0);
    es_linker_add_import(linker, "msvcrt.dll", "frexp", 0);
    es_linker_add_import(linker, "msvcrt.dll", "ldexp", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strlen", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strcmp", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strcpy", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strncpy", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strcat", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strncat", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strchr", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strrchr", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strstr", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strspn", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strcspn", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strpbrk", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strtok", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strdup", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strerror", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strcoll", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strxfrm", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strnicmp", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strupr", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strlwr", 0);
    es_linker_add_import(linker, "msvcrt.dll", "memcpy", 0);
    es_linker_add_import(linker, "msvcrt.dll", "memmove", 0);
    es_linker_add_import(linker, "msvcrt.dll", "memset", 0);
    es_linker_add_import(linker, "msvcrt.dll", "memcmp", 0);
    es_linker_add_import(linker, "msvcrt.dll", "memchr", 0);
    es_linker_add_import(linker, "msvcrt.dll", "atoi", 0);
    es_linker_add_import(linker, "msvcrt.dll", "atol", 0);
    es_linker_add_import(linker, "msvcrt.dll", "atof", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strtol", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strtoul", 0);
    es_linker_add_import(linker, "msvcrt.dll", "strtod", 0);
    es_linker_add_import(linker, "msvcrt.dll", "qsort", 0);
    es_linker_add_import(linker, "msvcrt.dll", "bsearch", 0);
    es_linker_add_import(linker, "msvcrt.dll", "abs", 0);
    es_linker_add_import(linker, "msvcrt.dll", "labs", 0);
    es_linker_add_import(linker, "msvcrt.dll", "div", 0);
    es_linker_add_import(linker, "msvcrt.dll", "rand", 0);
    es_linker_add_import(linker, "msvcrt.dll", "srand", 0);
    es_linker_add_import(linker, "msvcrt.dll", "exit", 0);
    es_linker_add_import(linker, "msvcrt.dll", "abort", 0);
    es_linker_add_import(linker, "msvcrt.dll", "atexit", 0);
    es_linker_add_import(linker, "msvcrt.dll", "system", 0);
    es_linker_add_import(linker, "msvcrt.dll", "getenv", 0);
    es_linker_add_import(linker, "msvcrt.dll", "putenv", 0);
    es_linker_add_import(linker, "msvcrt.dll", "mktime", 0);
    es_linker_add_import(linker, "msvcrt.dll", "time", 0);
    es_linker_add_import(linker, "msvcrt.dll", "difftime", 0);
    es_linker_add_import(linker, "msvcrt.dll", "asctime", 0);
    es_linker_add_import(linker, "msvcrt.dll", "ctime", 0);
    es_linker_add_import(linker, "msvcrt.dll", "gmtime", 0);
    es_linker_add_import(linker, "msvcrt.dll", "localtime", 0);
    
    
    
    return true;
}


void es_linker_get_import_sizes(ArkLinker* linker, 
                                 uint32_t* import_dir_size,
                                 uint32_t* iat_size,
                                 uint32_t* string_table_size) {
    if (!linker) return;
    
    uint32_t total_funcs = 0;
    uint32_t dll_name_size = 0;
    uint32_t func_name_size = 0;
    
    for (int i = 0; i < linker->import_count; i++) {
        total_funcs += linker->imports[i].func_count;
        dll_name_size += (uint32_t)strlen(linker->imports[i].dll_name) + 1;
        
        for (int j = 0; j < linker->imports[i].func_count; j++) {
            
            func_name_size += 2 + (uint32_t)strlen(linker->imports[i].functions[j].func_name) + 1;
            func_name_size = (func_name_size + 1) & ~1;  
        }
    }
    
    
    *import_dir_size = (linker->import_count + 1) * 20;
    
    
    *iat_size = (total_funcs + linker->import_count) * 8;
    
    
    *string_table_size = dll_name_size + func_name_size;
}


bool es_linker_add_base_reloc(ArkLinker* linker, uint32_t rva) {
    if (!linker || !linker->base_reloc_pool) return false;
    
    
    if (linker->base_reloc_count == 0) {
        
        linker->base_relocs = (uint32_t*)mempool_alloc(linker->base_reloc_pool, 
            256 * sizeof(uint32_t));
        if (!linker->base_relocs) return false;
    } else if ((linker->base_reloc_count & 255) == 0) {
        
        uint32_t* new_block = (uint32_t*)mempool_alloc(linker->base_reloc_pool,
            256 * sizeof(uint32_t));
        if (!new_block) return false;
        
        
    }
    
    
    
    int new_size = (linker->base_reloc_count + 256) * sizeof(uint32_t);
    uint32_t* new_relocs = (uint32_t*)mempool_alloc(linker->base_reloc_pool, new_size);
    if (!new_relocs) return false;
    
    
    if (linker->base_reloc_count > 0) {
        memcpy(new_relocs, linker->base_relocs, linker->base_reloc_count * sizeof(uint32_t));
    }
    
    linker->base_relocs = new_relocs;
    linker->base_relocs[linker->base_reloc_count++] = rva;
    return true;
}


const char* es_linker_get_error(ArkLinker* linker) {
    return linker ? linker->error_msg : "Unknown error";
}


extern bool write_pe_file(ArkLinker* linker, const char* filename);


bool es_linker_link(ArkLinker* linker) {
    if (!linker || linker->input_count == 0) {
        snprintf(linker->error_msg, sizeof(linker->error_msg),
            "No input files");
        return false;
    }
    
    
    for (int i = 0; i < linker->input_count; i++) {
        if (!read_eo_file(linker, linker->input_files[i])) {
            return false;
        }
    }
    
    
    struct { const char* alias; const char* target; } aliases[] = {
        {"Console__WriteLine", "es_println"},
        {"Console__Write", "es_print"},
        {"Console__WriteLineInt", "es_println_int"},
        {"Console__WriteInt", "es_print_int"},
        {NULL, NULL}
    };

    for (int i = 0; aliases[i].alias != NULL; i++) {
        int alias_idx = -1;
        int target_idx = -1;
        
        for (int j = 0; j < linker->symbol_count; j++) {
            if (strcmp(linker->symbols[j].name, aliases[i].alias) == 0) {
                alias_idx = j;
            }
            if (strcmp(linker->symbols[j].name, aliases[i].target) == 0) {
                target_idx = j;
            }
        }
        
        if (alias_idx != -1 && !linker->symbols[alias_idx].is_defined && target_idx != -1 && linker->symbols[target_idx].is_defined) {
            fprintf(stderr, "DEBUG: Aliasing %s -> %s\n", aliases[i].alias, aliases[i].target);
            linker->symbols[alias_idx].section = linker->symbols[target_idx].section;
            linker->symbols[alias_idx].offset = linker->symbols[target_idx].offset;
            linker->symbols[alias_idx].size = linker->symbols[target_idx].size;
            linker->symbols[alias_idx].type = linker->symbols[target_idx].type;
            linker->symbols[alias_idx].is_defined = true;
        }
    }
    
    
    if (!linker->has_entry) {
        fprintf(stderr, "DEBUG: No entry point from headers, searching for mainCRTStartup...\n");
        for (int i = 0; i < linker->symbol_count; i++) {
            const char* sym_name = (const char*)linker->symbols[i].name;
            fprintf(stderr, "DEBUG: Checking symbol[%d]: '%s'\n", i, sym_name ? sym_name : "(null)");
            if (sym_name && strcmp(sym_name, "mainCRTStartup") == 0) {
                linker->entry_point = linker->symbols[i].offset;
                linker->entry_section = linker->symbols[i].section;
                linker->has_entry = true;
                fprintf(stderr, "DEBUG: Found mainCRTStartup! offset=0x%x, section=%d\n",
                        linker->entry_point, linker->entry_section);
                break;
            }
        }
    }
    
    
    if (!linker->has_entry) {
        for (int i = 0; i < linker->symbol_count; i++) {
            const char* sym_name = (const char*)linker->symbols[i].name;
            if (sym_name && (strcmp(sym_name, "_start") == 0 ||
                strcmp(sym_name, "main") == 0)) {
                linker->entry_point = linker->symbols[i].offset;
                linker->entry_section = linker->symbols[i].section;
                linker->has_entry = true;
                fprintf(stderr, "DEBUG: Found entry point: %s @ 0x%x\n",
                        sym_name, linker->entry_point);
                break;
            }
        }
    }
    
    
    bool write_ok = false;
    switch (linker->config.target) {
        case ArkLink_TARGET_PE:
            
            if (linker->import_count == 0) {
                
                
            }
            write_ok = write_pe_file(linker, linker->config.output_file);
            break;
        case ArkLink_TARGET_ELF:
            
            
            snprintf(linker->error_msg, sizeof(linker->error_msg),
                "ELF target is not yet supported");
            write_ok = false;
            break;
        default:
            snprintf(linker->error_msg, sizeof(linker->error_msg),
                "Unsupported target platform");
            return false;
    }
    
    if (!write_ok) {
        return false;
    }
    
    return true;
}


bool es_link_files(const char** input_files, int file_count, 
                   const char* output_file, bool is_gui) {
    ArkLinkerConfig config = default_config;
    if (output_file) {
        config.output_file = output_file;
    }
    config.is_gui = is_gui;
    
    ArkLinker* linker = es_linker_create(&config);
    if (!linker) return false;
    
    for (int i = 0; i < file_count; i++) {
        if (!es_linker_add_file(linker, input_files[i])) {
            es_linker_destroy(linker);
            return false;
        }
    }
    
    bool result = es_linker_link(linker);
    es_linker_destroy(linker);
    return result;
}
