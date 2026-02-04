

#include "ArkLink/linker.h"
#include "ArkLink/pe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static uint32_t align_up(uint32_t value, uint32_t alignment) {
    if (alignment == 0) return value;
    uint32_t remainder = value % alignment;
    if (remainder == 0) return value;
    return value + (alignment - remainder);
}


static void write_dos_header(FILE* fp) {
    IMAGE_DOS_HEADER dos_header = {0};
    dos_header.e_magic = 0x5A4D;  
    dos_header.e_lfanew = 0x80;   
    
    fwrite(&dos_header, sizeof(dos_header), 1, fp);
    
    
    uint8_t dos_stub[64] = {
        0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
        0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
        0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72,
        0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
        0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E,
        0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
        0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A,
        0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    fwrite(dos_stub, sizeof(dos_stub), 1, fp);
}


static void calculate_import_table_size(ArkLinker* linker, uint32_t* total_size, uint32_t* iat_size_out) {
    if (linker->import_count == 0) {
        *total_size = 0;
        *iat_size_out = 0;
        return;
    }

    uint32_t import_desc_size = (linker->import_count + 1) * 20;
    uint32_t int_size = 0;
    uint32_t iat_size = 0;
    uint32_t strings_size = 0;
    uint32_t current_iat_offset = 0;

    for (int i = 0; i < linker->import_count; i++) {
        int func_count = linker->imports[i].func_count;
        int table_size = (func_count + 1) * 8; 
        int_size += table_size;
        iat_size += table_size;

        for (int j = 0; j < func_count; j++) {
            linker->imports[i].functions[j].iat_offset = current_iat_offset + j * 8;
        }
        current_iat_offset += table_size;

        if (linker->imports[i].dll_name) {
            strings_size += (uint32_t)strlen(linker->imports[i].dll_name) + 1;
        }

        for (int j = 0; j < func_count; j++) {
            if (linker->imports[i].functions[j].func_name) {
                uint32_t entry_size = 2 + (uint32_t)strlen(linker->imports[i].functions[j].func_name) + 1;
                if (entry_size % 2 != 0) entry_size++;
                strings_size += entry_size;
            }
        }
    }
    
    int_size = (int_size + 7) & ~7;
    iat_size = (iat_size + 7) & ~7;

    *iat_size_out = iat_size;
    *total_size = import_desc_size + int_size + iat_size + strings_size;
    *total_size = (*total_size + 7) & ~7;
}


static uint32_t write_import_table(FILE* fp, ArkLinker* linker, uint32_t rdata_rva, uint32_t* iat_rva_out) {
    if (linker->import_count == 0) return 0;
    
    uint32_t start_pos = (uint32_t)ftell(fp);
    
    uint32_t import_desc_size = (linker->import_count + 1) * 20;
    
    
    uint32_t int_size = 0;
    uint32_t iat_size = 0;
    uint32_t dll_names_size = 0;
    uint32_t func_names_size = 0;
    
    for (int i = 0; i < linker->import_count; i++) {
        int func_count = linker->imports[i].func_count;
        
        int int_table_size = (func_count + 1) * 8;
        int_size += int_table_size;
        iat_size += (func_count + 1) * 8;
        
        if (linker->imports[i].dll_name) {
            dll_names_size += (uint32_t)strlen(linker->imports[i].dll_name) + 1;
        }
        
        for (int j = 0; j < func_count; j++) {
            if (linker->imports[i].functions[j].func_name) {
                uint32_t name_len = (uint32_t)strlen(linker->imports[i].functions[j].func_name);
                uint32_t entry_size = 2 + name_len + 1;
                if (entry_size % 2 != 0) entry_size++;
                func_names_size += entry_size;
            }
        }
    }
    int_size = align_up(int_size, 8);
    iat_size = align_up(iat_size, 8);
    
    
    
    
    
    
    
    
    uint32_t dll_names_offset = import_desc_size + int_size + iat_size;
    uint32_t func_names_offset = dll_names_offset + dll_names_size;
    
    fprintf(stderr, "DEBUG write_import_table: import_desc_size=%u, int_size=%u, iat_size=%u, dll_names_offset=%u, func_names_offset=%u\n",
            import_desc_size, int_size, iat_size, dll_names_offset, func_names_offset);
    
    
    uint32_t dll_name_offsets[64] = {0}; 
    uint32_t current_dll_offset = dll_names_offset;
    for (int i = 0; i < linker->import_count && i < 64; i++) {
        dll_name_offsets[i] = current_dll_offset;
        if (linker->imports[i].dll_name) {
            current_dll_offset += (uint32_t)strlen(linker->imports[i].dll_name) + 1;
        }
    }
    
    
    uint32_t func_name_offsets[256] = {0}; 
    uint32_t current_func_offset = func_names_offset;
    int func_idx = 0;
    for (int i = 0; i < linker->import_count; i++) {
        int func_count = linker->imports[i].func_count;
        for (int j = 0; j < func_count && func_idx < 256; j++) {
            func_name_offsets[func_idx++] = current_func_offset;
            if (linker->imports[i].functions[j].func_name) {
                uint32_t name_len = (uint32_t)strlen(linker->imports[i].functions[j].func_name);
                current_func_offset += 2 + name_len + 1;
                
                if (current_func_offset % 2 != 0) {
                    current_func_offset++;
                }
            }
        }
    }
    
    
    uint32_t int_offset = import_desc_size;  
    uint32_t iat_offset = import_desc_size + int_size;  
    
    for (int i = 0; i < linker->import_count; i++) {
        
        uint32_t int_rva = rdata_rva + int_offset;
        fwrite(&int_rva, 4, 1, fp);
        
        
        uint32_t zero = 0;
        fwrite(&zero, 4, 1, fp);
        
        
        fwrite(&zero, 4, 1, fp);
        
        
        uint32_t name_rva = rdata_rva + dll_name_offsets[i];
        fwrite(&name_rva, 4, 1, fp);
        
        
        uint32_t iat_entry_rva = rdata_rva + iat_offset;
        fwrite(&iat_entry_rva, 4, 1, fp);
        
        int func_count = linker->imports[i].func_count;
        int int_table_size = (func_count + 1) * 8;   
        int iat_table_size = (func_count + 1) * 8;   
        
        int_offset += int_table_size;
        iat_offset += iat_table_size;
    }
    
    
    uint8_t zeros[20] = {0};
    fwrite(zeros, 20, 1, fp);
    
    
    func_idx = 0;
    for (int i = 0; i < linker->import_count; i++) {
        int func_count = linker->imports[i].func_count;
        for (int j = 0; j < func_count; j++) {
            uint64_t name_rva_64 = (uint64_t)(rdata_rva + func_name_offsets[func_idx++]);
            fprintf(stderr, "DEBUG: INT entry: func=%s, name_rva_64=0x%016llX\n",
                    linker->imports[i].functions[j].func_name, (unsigned long long)name_rva_64);
            fwrite(&name_rva_64, 8, 1, fp);
        }
        
        uint64_t null_entry = 0;
        fwrite(&null_entry, 8, 1, fp);
    }
    
    
    uint32_t int_end_pos = (uint32_t)ftell(fp);
    uint32_t int_actual_size = int_end_pos - start_pos - import_desc_size;
    fprintf(stderr, "DEBUG: INT actual_size=%u, expected int_size=%u\n", int_actual_size, int_size);
    while (int_actual_size < int_size) {
        uint8_t zero = 0;
        fwrite(&zero, 1, 1, fp);
        int_actual_size++;
    }
    
    
    uint32_t iat_start = (uint32_t)ftell(fp);
    func_idx = 0;
    for (int i = 0; i < linker->import_count; i++) {
        int func_count = linker->imports[i].func_count;
        for (int j = 0; j < func_count; j++) {
            uint64_t name_rva_64 = (uint64_t)(rdata_rva + func_name_offsets[func_idx++]);
            fprintf(stderr, "DEBUG: IAT entry: func=%s, name_rva_64=0x%016llX\n",
                    linker->imports[i].functions[j].func_name, (unsigned long long)name_rva_64);
            fwrite(&name_rva_64, 8, 1, fp);
        }
        uint64_t null_entry = 0;
        fwrite(&null_entry, 8, 1, fp);
    }
    
    uint32_t iat_actual_size = (uint32_t)ftell(fp) - iat_start;
    while (iat_actual_size < iat_size) {
        uint8_t zero = 0;
        fwrite(&zero, 1, 1, fp);
        iat_actual_size++;
    }
    
    *iat_rva_out = rdata_rva + (iat_start - start_pos);
    fprintf(stderr, "DEBUG write_import_table: rdata_rva=0x%X, iat_start=0x%X, start_pos=0x%X, calculated_iat_rva=0x%X\n",
            rdata_rva, iat_start, start_pos, *iat_rva_out);
    
    
    
    
    uint32_t dll_name_start = (uint32_t)ftell(fp);
    for (int i = 0; i < linker->import_count; i++) {
        if (linker->imports[i].dll_name) {
            fwrite(linker->imports[i].dll_name, 1, strlen(linker->imports[i].dll_name) + 1, fp);
        }
    }
    
    
    for (int i = 0; i < linker->import_count; i++) {
        for (int j = 0; j < linker->imports[i].func_count; j++) {
            
            uint16_t hint = linker->imports[i].functions[j].hint;
            fwrite(&hint, 2, 1, fp);
            
            const char* func_name = linker->imports[i].functions[j].func_name;
            if (func_name) {
                size_t name_len = strlen(func_name);
                if (name_len > 0) {
                    fwrite(func_name, 1, name_len + 1, fp);
                    fprintf(stderr, "DEBUG: Wrote function name: '%s' (len=%zu)\n", func_name, name_len);
                    
                    if ((2 + name_len + 1) % 2 != 0) {
                        uint8_t zero = 0;
                        fwrite(&zero, 1, 1, fp);
                    }
                } else {
                    uint8_t null_byte = 0;
                    fwrite(&null_byte, 1, 1, fp);
                }
            } else {
                uint8_t null_byte = 0;
                fwrite(&null_byte, 1, 1, fp);
            }
        }
    }
    
    uint32_t total_size = (uint32_t)ftell(fp) - start_pos;
    
    while (total_size % 8 != 0) {
        uint8_t zero = 0;
        fwrite(&zero, 1, 1, fp);
        total_size++;
    }
    
    return total_size;
}


#define IMAGE_REL_BASED_DIR64  10  
#define IMAGE_REL_BASED_HIGHLOW 3  
#define IMAGE_REL_BASED_HIGHADJ 4  


static void add_base_relocation(ArkLinker* linker, uint32_t rva) {
    if (!linker->base_reloc_pool) {
        linker->base_reloc_pool = mempool_create();
    }
    
    uint32_t* new_entry = (uint32_t*)mempool_alloc(linker->base_reloc_pool, sizeof(uint32_t));
    if (new_entry) {
        *new_entry = rva;
        fprintf(stderr, "DEBUG: Added reloc %d: RVA=0x%08X at %p, value=0x%08X\n", 
                linker->base_reloc_count, rva, (void*)new_entry, *new_entry);
        linker->base_reloc_count++;
    }
}


static bool apply_relocations(ArkLinker* linker, uint32_t iat_rva, 
                               uint32_t text_rva, uint32_t data_rva, uint32_t rdata_rva, uint32_t bss_rva) {
    for (int i = 0; i < linker->reloc_count; i++) {
        const char* sym_name = linker->relocs[i].symbol_name;
        uint32_t section = linker->relocs[i].section;
        uint32_t offset = linker->relocs[i].offset;
        uint16_t type = linker->relocs[i].type;
        
        
        int sym_idx = -1;
        for (int j = 0; j < linker->symbol_count; j++) {
            if (strcmp(linker->symbols[j].name, sym_name) == 0) {
                sym_idx = j;
                break;
            }
        }
        
        
        uint8_t* target_data = NULL;
        uint32_t section_rva = 0;
        
        fprintf(stderr, "DEBUG: Reloc %d: section=%u, offset=%u\n", i, section, offset);
        
        switch (section) {
            case EO_SEC_CODE:
                target_data = linker->sections[EO_SEC_CODE].data;
                section_rva = text_rva;
                fprintf(stderr, "DEBUG:   -> CODE section, text_rva=0x%X\n", text_rva);
                break;
            case EO_SEC_DATA:
                target_data = linker->sections[EO_SEC_DATA].data;
                section_rva = data_rva;
                fprintf(stderr, "DEBUG:   -> DATA section, data_rva=0x%X\n", data_rva);
                break;
            case EO_SEC_RODATA:
                target_data = linker->sections[EO_SEC_RODATA].data;
                section_rva = rdata_rva;
                fprintf(stderr, "DEBUG:   -> RODATA section, rdata_rva=0x%X\n", rdata_rva);
                break;
            default:
                fprintf(stderr, "DEBUG:   -> Unknown section %u, skipping\n", section);
                continue;
        }
        
        if (!target_data) continue;
        
        
        
        const char* import_name = sym_name;
        if (strncmp(sym_name, "__imp_", 6) == 0) {
            import_name = sym_name + 6;  
        }
        
        bool found_in_import = false;
        for (int d = 0; d < linker->import_count && !found_in_import; d++) {
            for (int f = 0; f < linker->imports[d].func_count; f++) {
                
                if (strcmp(linker->imports[d].functions[f].func_name, import_name) == 0) {
                    uint32_t iat_entry_rva = iat_rva + linker->imports[d].functions[f].iat_offset;
                    fprintf(stderr, "DEBUG:   -> Found in import table: %s, iat_offset=%u, iat_entry_rva=0x%X\n",
                            sym_name, linker->imports[d].functions[f].iat_offset, iat_entry_rva);
                    
        if (type == EO_RELOC_REL32) {  
            
            
            bool is_import = (strncmp(sym_name, "__imp_", 6) == 0);
            uint32_t insn_length = is_import ? 7 : 5;
            uint32_t next_insn = section_rva + offset + insn_length;
            
            
            
            int32_t* ptr = (int32_t*)(target_data + (is_import ? offset + 3 : offset));
            
            int32_t old_val = *ptr;
            *ptr = (int32_t)(iat_entry_rva - next_insn) + old_val;
            fprintf(stderr, "DEBUG:   -> REL32 (Import): insn_len=%u, next_insn=0x%X, iat_entry_rva=0x%X, old_val=%d, new_val=%d\n",
                    insn_length, next_insn, iat_entry_rva, old_val, *ptr);
        } else if (type == EO_RELOC_ABS64) {  
            uint64_t* ptr = (uint64_t*)(target_data + offset);
            uint64_t old_val = *ptr;
            *ptr = linker->config.preferred_base + iat_entry_rva + old_val;
            
            add_base_relocation(linker, section_rva + offset);
        }
                    
                    found_in_import = true;
                    break;
                }
            }
        }
        
        if (found_in_import) continue;
        
        
        if (sym_idx < 0 || !linker->symbols[sym_idx].is_defined) {
            snprintf(linker->error_msg, sizeof(linker->error_msg),
                "Undefined symbol: %s", sym_name);
            return false;
        }
        
        
        uint32_t sym_rva = 0;
        
        
        switch (linker->symbols[sym_idx].section) {
            case EO_SEC_CODE:
                sym_rva = text_rva + linker->symbols[sym_idx].offset;
                break;
            case EO_SEC_DATA:
                sym_rva = data_rva + linker->symbols[sym_idx].offset;
                break;
            case EO_SEC_RODATA:
                sym_rva = rdata_rva + linker->symbols[sym_idx].offset;
                break;
            case EO_SEC_BSS:
                sym_rva = bss_rva + linker->symbols[sym_idx].offset;
                break;
        }
        fprintf(stderr, "DEBUG:   -> Symbol %s: section=%d, offset=%u, sym_rva=0x%X\n",
                sym_name, linker->symbols[sym_idx].section, linker->symbols[sym_idx].offset, sym_rva);
        
        
        if (type == EO_RELOC_REL32) {
            fprintf(stderr, "DEBUG:     Processing REL32 at offset=%u, old_val=%d\n", offset, *(int32_t*)(target_data + offset));
            
            
            
            
            
            
            uint32_t section_size = linker->sections[section].file_size;
            uint32_t insn_length = 5;
            uint32_t insn_start_offset = offset;
            
            
            
            
            if (offset > 0 && offset < section_size) {
                uint8_t opcode = target_data[offset - 1];
                fprintf(stderr, "DEBUG:     Check offset-1: opcode=0x%02X\n", opcode);
                
                
                if (opcode == 0xE8 || opcode == 0xE9) {
                    insn_start_offset = offset - 1;
                    insn_length = 5;
                    fprintf(stderr, "DEBUG:     Detected CALL/JMP (0x%02X), insn_start=%u, len=%u\n", opcode, insn_start_offset, insn_length);
                }
            }
            
            
            if (offset >= 2 && offset < section_size) {
                if (target_data[offset - 2] == 0xFF && offset < section_size &&
                    (target_data[offset - 1] == 0x15 || target_data[offset - 1] == 0x25)) {
                    insn_start_offset = offset - 2;
                    insn_length = 6;
                    fprintf(stderr, "DEBUG:     Detected FF 15/25, insn_start=%u, len=%u\n", insn_start_offset, insn_length);
                }
            }
            
            
            uint32_t next_insn_rva = text_rva + insn_start_offset + insn_length;
            
            int32_t* ptr = (int32_t*)(target_data + offset);
            int32_t old_val = *ptr;
            int32_t expected_rel32 = (int32_t)(sym_rva - next_insn_rva);
            *ptr = expected_rel32 + old_val;
            fprintf(stderr, "DEBUG:     REL32: sym_rva=0x%X, next_insn=0x%X, expected_rel32=%d, old_val=%d, final=%d\n",
                    sym_rva, next_insn_rva, expected_rel32, old_val, *ptr);
        } else if (type == EO_RELOC_ABS64) {
            uint64_t* ptr = (uint64_t*)(target_data + offset);
            uint64_t old_val = *ptr;
            *ptr = linker->config.preferred_base + sym_rva + old_val;
            
            uint32_t reloc_rva = section_rva + offset;
            fprintf(stderr, "DEBUG: Adding base reloc: section_rva=0x%X, offset=%u, reloc_rva=0x%X\n",
                    section_rva, offset, reloc_rva);
            add_base_relocation(linker, reloc_rva);
        }
    }
    
    return true;
}


static uint32_t calculate_base_reloc_size(ArkLinker* linker) {
    if (linker->base_reloc_count == 0) return 0;
    
    uint32_t total_size = 0;
    uint32_t current_page = 0xFFFFFFFF;
    int page_entry_count = 0;
    
    
    MempoolBlock* block = linker->base_reloc_pool->blocks;
    int total_processed = 0;
    
    while (block && total_processed < linker->base_reloc_count) {
        uint8_t* data = block->data;
        int in_block = block->used / sizeof(uint32_t); 
        
        for (int j = 0; j < in_block && total_processed < linker->base_reloc_count; j++) {
            uint32_t rva = *(uint32_t*)(data + j * sizeof(uint32_t)); 
            uint32_t page = rva & ~0xFFF;
            
            if (page != current_page) {
                if (page_entry_count > 0) {
                    uint32_t block_size = 8 + page_entry_count * 2;
                    block_size = (block_size + 3) & ~3;
                    total_size += block_size;
                }
                current_page = page;
                page_entry_count = 1;
            } else {
                page_entry_count++;
            }
            total_processed++;
        }
        block = block->next;
    }
    
    if (page_entry_count > 0) {
        uint32_t block_size = 8 + page_entry_count * 2;
        block_size = (block_size + 3) & ~3;
        total_size += block_size;
    }
    
    
    total_size += 8;
    
    fprintf(stderr, "DEBUG: calculate_base_reloc_size: count=%d, total_size=%u\n", 
            linker->base_reloc_count, total_size);
    return total_size;
}


static uint32_t write_base_reloc_table(FILE* fp, ArkLinker* linker) {
    if (linker->base_reloc_count == 0) return 0;
    
    fprintf(stderr, "DEBUG: Writing base reloc table, count=%d\n", linker->base_reloc_count);
    
    uint32_t start_pos = (uint32_t)ftell(fp);
    
    
    uint32_t current_page = 0xFFFFFFFF;
    uint16_t page_entries[2048];
    int page_entry_count = 0;
    
    
    MempoolBlock* block = linker->base_reloc_pool->blocks;
    int total_processed = 0;
    
    while (block || page_entry_count > 0) {
        uint32_t rva = 0xFFFFFFFF;
        
        if (block && total_processed < linker->base_reloc_count) {
            uint8_t* data = block->data;
            int in_block = block->used / sizeof(uint32_t); 
            
            
            int processed_in_prev_blocks = 0;
            MempoolBlock* temp = linker->base_reloc_pool->blocks;
            while (temp != block) {
                processed_in_prev_blocks += temp->used / sizeof(uint32_t);
                temp = temp->next;
            }
            int j = total_processed - processed_in_prev_blocks;
            
            rva = *(uint32_t*)(data + j * sizeof(uint32_t));
            total_processed++;
            
            
            if (j + 1 >= in_block) {
                block = block->next;
            }
        } else {
            
            rva = 0xFFFFFFFF;
            block = NULL; 
        }
        
        uint32_t page = (rva == 0xFFFFFFFF) ? 0xFFFFFFFF : (rva & ~0xFFF);
        
        if (page != current_page && page_entry_count > 0) {
            uint32_t block_size = 8 + page_entry_count * 2;
            block_size = (block_size + 3) & ~3;
            
            fprintf(stderr, "DEBUG: Writing reloc block: PageRVA=0x%X, BlockSize=%u, Entries=%d\n", 
                    current_page, block_size, page_entry_count);
            
            fwrite(&current_page, 4, 1, fp);
            fwrite(&block_size, 4, 1, fp);
            
            for (int j = 0; j < page_entry_count; j++) {
                fwrite(&page_entries[j], 2, 1, fp);
            }
            
            uint32_t written = 8 + page_entry_count * 2;
            while (written < block_size) {
                uint16_t zero = 0;
                fwrite(&zero, 2, 1, fp);
                written += 2;
            }
            
            page_entry_count = 0;
        }
        
        if (rva != 0xFFFFFFFF) {
            current_page = page;
            uint16_t entry = (rva & 0xFFF) | (IMAGE_REL_BASED_DIR64 << 12);
            page_entries[page_entry_count++] = entry;
        } else if (total_processed >= linker->base_reloc_count) {
            break;
        }
    }
    
    
    uint32_t zero = 0;
    fwrite(&zero, 4, 1, fp);
    fwrite(&zero, 4, 1, fp);
    
    return (uint32_t)ftell(fp) - start_pos;
}


bool write_pe_file(ArkLinker* linker, const char* filename) {
    fprintf(stderr, "DEBUG: write_pe_file called for %s\n", filename);
    
    if (!linker || !filename) return false;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        snprintf(linker->error_msg, sizeof(linker->error_msg),
            "Cannot create output file: %s", filename);
        return false;
    }
    
    
    uint32_t section_alignment = 0x1000;
    uint32_t file_alignment = 0x200;
    
    
    uint32_t import_table_size = 0;
    uint32_t iat_size = 0;
    calculate_import_table_size(linker, &import_table_size, &iat_size);
    
    
    uint32_t code_size = linker->sections[EO_SEC_CODE].file_size;
    uint32_t data_size = linker->sections[EO_SEC_DATA].file_size;
    uint32_t rodata_size = linker->sections[EO_SEC_RODATA].file_size;
    uint32_t bss_size = linker->sections[EO_SEC_BSS].mem_size;
    uint32_t tls_size = linker->sections[EO_SEC_TLS].file_size;
    bool has_tls = tls_size > 0 && linker->sections[EO_SEC_TLS].data;
    
    fprintf(stderr, "DEBUG PE: code_size=%u, data_size=%u, rodata_size=%u, bss_size=%u, tls_size=%u\n",
            code_size, data_size, rodata_size, bss_size, tls_size);
    
    
    uint32_t actual_import_size = (import_table_size > 0) ? import_table_size : 20;
    
    
    uint32_t text_rva = 0x1000;
    uint32_t data_rva = text_rva + (code_size > 0 ? align_up(code_size, section_alignment) : 0);
    uint32_t rodata_rva = data_rva + (data_size > 0 ? align_up(data_size, section_alignment) : 0);
    uint32_t idata_rva = rodata_rva + (rodata_size > 0 ? align_up(rodata_size, section_alignment) : 0);
    uint32_t bss_rva = idata_rva + (actual_import_size > 0 ? align_up(actual_import_size, section_alignment) : 0);
    uint32_t tls_rva = bss_rva + (bss_size > 0 ? align_up(bss_size, section_alignment) : 0);
    
    
    uint32_t first_section_offset = 0x400;
    uint32_t code_file_offset = first_section_offset;
    uint32_t data_file_offset = code_file_offset + (code_size > 0 ? align_up(code_size, file_alignment) : 0);
    uint32_t rodata_file_offset = data_file_offset + (data_size > 0 ? align_up(data_size, file_alignment) : 0);
    uint32_t idata_file_offset = rodata_file_offset + (rodata_size > 0 ? align_up(rodata_size, file_alignment) : 0);
    uint32_t bss_file_offset = idata_file_offset + (actual_import_size > 0 ? align_up(actual_import_size, file_alignment) : 0);
    uint32_t tls_file_offset = bss_file_offset + (bss_size > 0 ? align_up(bss_size, file_alignment) : 0);
    
    
    uint32_t iat_rva = 0;
    if (linker->import_count > 0) {
        uint32_t import_desc_size = (linker->import_count + 1) * 20;
        uint32_t int_size = 0;
        for (int i = 0; i < linker->import_count; i++) {
            int func_count = linker->imports[i].func_count;
            int_size += (func_count + 1) * 8;  
        }
        iat_rva = idata_rva + import_desc_size + align_up(int_size, 8);
    }
    
    
    if (!apply_relocations(linker, iat_rva, text_rva, data_rva, rodata_rva, bss_rva)) {
        fclose(fp);
        return false;
    }
    
    
    uint32_t base_reloc_size = calculate_base_reloc_size(linker);
    fprintf(stderr, "DEBUG PE: base_reloc_count=%d, base_reloc_size=%u\n", 
            linker->base_reloc_count, base_reloc_size);
    uint32_t reloc_rva = tls_rva + (tls_size > 0 ? align_up(tls_size, section_alignment) : 0);
    uint32_t reloc_file_offset = tls_file_offset + (tls_size > 0 ? align_up(tls_size, file_alignment) : 0);
    
    
    uint32_t image_size = reloc_rva + (base_reloc_size > 0 ? align_up(base_reloc_size, section_alignment) : 0);
    
    
    int num_sections = 1;  
    if (data_size > 0) num_sections++;          
    if (rodata_size > 0) num_sections++;        
    if (import_table_size > 0 || linker->import_count == 0) num_sections++;  
    if (bss_size > 0) num_sections++;           
    if (has_tls) num_sections++;                
    if (base_reloc_size > 0) num_sections++;    
    
    
    write_dos_header(fp);
    
    
    uint32_t pe_sig = 0x00004550;
    fwrite(&pe_sig, sizeof(pe_sig), 1, fp);
    
    
    uint16_t machine = IMAGE_FILE_MACHINE_AMD64;
    uint16_t num_sections_16 = (uint16_t)num_sections;
    uint32_t time_stamp = 0;
    uint32_t ptr_sym_table = 0;
    uint32_t num_symbols = 0;
    uint16_t opt_header_size = 240;
    uint16_t charact = 0x26;
    
    
    uint8_t coff_bytes[24] = {0};
    coff_bytes[0] = machine & 0xFF;
    coff_bytes[1] = (machine >> 8) & 0xFF;
    coff_bytes[2] = num_sections_16 & 0xFF;
    coff_bytes[3] = (num_sections_16 >> 8) & 0xFF;
    
    coff_bytes[4] = time_stamp & 0xFF;
    coff_bytes[5] = (time_stamp >> 8) & 0xFF;
    coff_bytes[6] = (time_stamp >> 16) & 0xFF;
    coff_bytes[7] = (time_stamp >> 24) & 0xFF;
    
    coff_bytes[8] = ptr_sym_table & 0xFF;
    coff_bytes[9] = (ptr_sym_table >> 8) & 0xFF;
    coff_bytes[10] = (ptr_sym_table >> 16) & 0xFF;
    coff_bytes[11] = (ptr_sym_table >> 24) & 0xFF;
    
    coff_bytes[12] = num_symbols & 0xFF;
    coff_bytes[13] = (num_symbols >> 8) & 0xFF;
    coff_bytes[14] = (num_symbols >> 16) & 0xFF;
    coff_bytes[15] = (num_symbols >> 24) & 0xFF;
    
    coff_bytes[16] = opt_header_size & 0xFF;
    coff_bytes[17] = (opt_header_size >> 8) & 0xFF;
    
    coff_bytes[18] = charact & 0xFF;
    coff_bytes[19] = (charact >> 8) & 0xFF;
    
    fwrite(coff_bytes, 20, 1, fp);
    
    fprintf(stderr, "DEBUG: COFF header written with correct endianness, SizeOfOptionalHeader=%u\n", opt_header_size);
    
    
    IMAGE_OPTIONAL_HEADER64 opt_header = {0};
    opt_header.Magic = 0x20B;
    opt_header.MajorLinkerVersion = 1;
    opt_header.MinorLinkerVersion = 0;
    opt_header.SizeOfCode = code_size > 0 ? align_up(code_size, file_alignment) : 0;
    opt_header.SizeOfInitializedData = (data_size > 0 ? align_up(data_size, file_alignment) : 0) +
                                        (rodata_size > 0 ? align_up(rodata_size, file_alignment) : 0) +
                                        (import_table_size > 0 ? align_up(import_table_size, file_alignment) : 0) +
                                        (base_reloc_size > 0 ? align_up(base_reloc_size, file_alignment) : 0);
    opt_header.SizeOfUninitializedData = bss_size > 0 ? align_up(bss_size, section_alignment) : 0;
    opt_header.AddressOfEntryPoint = text_rva + linker->entry_point;
    fprintf(stderr, "DEBUG PE: text_rva=0x%X, entry_point=%u, AddressOfEntryPoint=0x%X\n",
            text_rva, linker->entry_point, opt_header.AddressOfEntryPoint);
    fflush(stderr);
    opt_header.BaseOfCode = text_rva;
    opt_header.ImageBase = linker->config.preferred_base;
    opt_header.SectionAlignment = section_alignment;
    opt_header.FileAlignment = file_alignment;
    opt_header.MajorOperatingSystemVersion = 6;
    opt_header.MinorOperatingSystemVersion = 0;
    opt_header.MajorSubsystemVersion = 5;
    opt_header.MinorSubsystemVersion = 2;
    opt_header.SizeOfImage = image_size;
    opt_header.SizeOfHeaders = first_section_offset;
    opt_header.Subsystem = linker->config.subsystem;
    
    
    if (base_reloc_size > 0) {
        opt_header.DllCharacteristics = 0x140;  
    } else {
        opt_header.DllCharacteristics = 0x100;  
    }
    opt_header.SizeOfStackReserve = 0x100000;
    opt_header.SizeOfStackCommit = 0x1000;
    opt_header.SizeOfHeapReserve = 0x100000;
    opt_header.SizeOfHeapCommit = 0x1000;
    opt_header.NumberOfRvaAndSizes = 16;
    
    
    {
        uint32_t import_desc_table_size = (linker->import_count + 1) * 20;
        fprintf(stderr, "DEBUG PE: idata_rva=0x%X, import_desc_table_size=%u, import_count=%d\n",
                idata_rva, import_desc_table_size, linker->import_count);
        fprintf(stderr, "DEBUG PE: Pre-calculated iat_rva=0x%X (from early calc)\n", iat_rva);
        opt_header.DataDirectory[1].VirtualAddress = idata_rva;
        opt_header.DataDirectory[1].Size = import_desc_table_size;
        fprintf(stderr, "DEBUG PE: iat_rva from write_import_table=0x%X, iat_size=%u\n", iat_rva, iat_size);
        if (iat_rva > 0) {
            opt_header.DataDirectory[12].VirtualAddress = iat_rva;
            opt_header.DataDirectory[12].Size = iat_size;
            fprintf(stderr, "DEBUG PE: Set DataDirectory[12] to iat_rva=0x%X, size=%u\n", iat_rva, iat_size);
        } else {
            fprintf(stderr, "DEBUG PE: WARNING! iat_rva is 0, IAT DataDirectory not set!\n");
        }
    }
    
    
    if (base_reloc_size > 0) {
        opt_header.DataDirectory[5].VirtualAddress = reloc_rva;
        opt_header.DataDirectory[5].Size = base_reloc_size;
        fprintf(stderr, "DEBUG PE: Set DataDirectory[5] to reloc_rva=0x%X, size=%u\n", reloc_rva, base_reloc_size);
    }
    
    
    if (has_tls) {
        opt_header.DataDirectory[9].VirtualAddress = tls_rva;
        opt_header.DataDirectory[9].Size = sizeof(IMAGE_TLS_DIRECTORY64);
    }
    
    fprintf(stderr, "DEBUG PE: sizeof(opt_header)=%zu\n", sizeof(opt_header));
    fprintf(stderr, "DEBUG PE: opt_header.AddressOfEntryPoint=0x%X\n", opt_header.AddressOfEntryPoint);
    fprintf(stderr, "DEBUG PE: opt_header.ImageBase=0x%llX\n", opt_header.ImageBase);
    fprintf(stderr, "DEBUG PE: NumberOfRvaAndSizes offset=%zu, value=%u\n", 
            offsetof(IMAGE_OPTIONAL_HEADER64, NumberOfRvaAndSizes), opt_header.NumberOfRvaAndSizes);
    fprintf(stderr, "DEBUG PE: DataDirectory offset=%zu\n", offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory));
    fprintf(stderr, "DEBUG PE: DataDirectory[1].VirtualAddress=0x%X\n", opt_header.DataDirectory[1].VirtualAddress);
    fprintf(stderr, "DEBUG PE: DataDirectory[1].Size=%u\n", opt_header.DataDirectory[1].Size);
    fprintf(stderr, "DEBUG PE: DataDirectory[12].VirtualAddress=0x%X\n", opt_header.DataDirectory[12].VirtualAddress);
    fprintf(stderr, "DEBUG PE: DataDirectory[12].Size=%u\n", opt_header.DataDirectory[12].Size);
    fprintf(stderr, "DEBUG PE: NumberOfRvaAndSizes=%u\n", opt_header.NumberOfRvaAndSizes);
    
    fprintf(stderr, "DEBUG PE: opt_header bytes around NumberOfRvaAndSizes (offset 92-99): ");
    uint8_t* opt_bytes = (uint8_t*)&opt_header;
    for (int i = 92; i < 100 && i < sizeof(opt_header); i++) {
        fprintf(stderr, "%02X ", opt_bytes[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
    
    
    fwrite(&opt_header.Magic, 2, 1, fp);
    fwrite(&opt_header.MajorLinkerVersion, 1, 1, fp);
    fwrite(&opt_header.MinorLinkerVersion, 1, 1, fp);
    fwrite(&opt_header.SizeOfCode, 4, 1, fp);
    fwrite(&opt_header.SizeOfInitializedData, 4, 1, fp);
    fwrite(&opt_header.SizeOfUninitializedData, 4, 1, fp);
    fwrite(&opt_header.AddressOfEntryPoint, 4, 1, fp);
    fwrite(&opt_header.BaseOfCode, 4, 1, fp);
    fwrite(&opt_header.ImageBase, 8, 1, fp);
    fwrite(&opt_header.SectionAlignment, 4, 1, fp);
    fwrite(&opt_header.FileAlignment, 4, 1, fp);
    fwrite(&opt_header.MajorOperatingSystemVersion, 2, 1, fp);
    fwrite(&opt_header.MinorOperatingSystemVersion, 2, 1, fp);
    fwrite(&opt_header.MajorImageVersion, 2, 1, fp);
    fwrite(&opt_header.MinorImageVersion, 2, 1, fp);
    fwrite(&opt_header.MajorSubsystemVersion, 2, 1, fp);
    fwrite(&opt_header.MinorSubsystemVersion, 2, 1, fp);
    fwrite(&opt_header.Win32VersionValue, 4, 1, fp);
    fwrite(&opt_header.SizeOfImage, 4, 1, fp);
    fwrite(&opt_header.SizeOfHeaders, 4, 1, fp);
    fwrite(&opt_header.CheckSum, 4, 1, fp);
    fwrite(&opt_header.Subsystem, 2, 1, fp);
    fwrite(&opt_header.DllCharacteristics, 2, 1, fp);
    fwrite(&opt_header.SizeOfStackReserve, 8, 1, fp);
    fwrite(&opt_header.SizeOfStackCommit, 8, 1, fp);
    fwrite(&opt_header.SizeOfHeapReserve, 8, 1, fp);
    fwrite(&opt_header.SizeOfHeapCommit, 8, 1, fp);
    fwrite(&opt_header.LoaderFlags, 4, 1, fp);
    fwrite(&opt_header.NumberOfRvaAndSizes, 4, 1, fp);
    
    
    for (int i = 0; i < 16; i++) {
        fwrite(&opt_header.DataDirectory[i].VirtualAddress, 4, 1, fp);
        fwrite(&opt_header.DataDirectory[i].Size, 4, 1, fp);
    }
    
    
    IMAGE_SECTION_HEADER sections[7] = {0};
    int sec_idx = 0;
    
    
    memcpy(sections[sec_idx].Name, ".text", 5);
    sections[sec_idx].VirtualSize = code_size > 0 ? align_up(code_size, section_alignment) : 0;
    sections[sec_idx].VirtualAddress = text_rva;
    sections[sec_idx].SizeOfRawData = code_size > 0 ? align_up(code_size, file_alignment) : 0;
    sections[sec_idx].PointerToRawData = code_file_offset;
    sections[sec_idx].Characteristics = 0x60000020;
    sec_idx++;
    
    
    if (data_size > 0) {
        memcpy(sections[sec_idx].Name, ".data", 5);
        sections[sec_idx].VirtualSize = data_size > 0 ? data_size : 0;
        sections[sec_idx].VirtualAddress = data_rva;
        sections[sec_idx].SizeOfRawData = data_size > 0 ? align_up(data_size, file_alignment) : 0;
        sections[sec_idx].PointerToRawData = data_file_offset;
        sections[sec_idx].Characteristics = 0x40000040;  
        sec_idx++;
    }

    
    if (rodata_size > 0) {
        memcpy(sections[sec_idx].Name, ".rdata", 6);
        sections[sec_idx].VirtualSize = rodata_size > 0 ? rodata_size : 0;
        sections[sec_idx].VirtualAddress = rodata_rva;
        sections[sec_idx].SizeOfRawData = rodata_size > 0 ? align_up(rodata_size, file_alignment) : 0;
        sections[sec_idx].PointerToRawData = rodata_file_offset;
        sections[sec_idx].Characteristics = 0x40000040; 
        sec_idx++;
    }
    
    
    {
        memcpy(sections[sec_idx].Name, ".idata", 6);
        
        uint32_t actual_import_size = (import_table_size > 0) ? import_table_size : 20;
        sections[sec_idx].VirtualSize = actual_import_size > 0 ? actual_import_size : 0;
        sections[sec_idx].VirtualAddress = idata_rva;
        sections[sec_idx].SizeOfRawData = actual_import_size > 0 ? align_up(actual_import_size, file_alignment) : 0;
        sections[sec_idx].PointerToRawData = idata_file_offset;
        sections[sec_idx].Characteristics = 0xC0000040;  
        sec_idx++;
    }


    if (bss_size > 0) {
        memcpy(sections[sec_idx].Name, ".bss", 4);
        sections[sec_idx].VirtualSize = bss_size > 0 ? bss_size : 0;
        sections[sec_idx].VirtualAddress = bss_rva;
        sections[sec_idx].SizeOfRawData = bss_size > 0 ? align_up(bss_size, file_alignment) : 0;
        sections[sec_idx].PointerToRawData = bss_file_offset;
        sections[sec_idx].Characteristics = 0xC0000080;
        sec_idx++;
    }
    
    
    if (has_tls) {
        uint32_t total_tls_size = sizeof(IMAGE_TLS_DIRECTORY64) + tls_size;
        memcpy(sections[sec_idx].Name, ".tls", 4);
        sections[sec_idx].VirtualSize = total_tls_size > 0 ? total_tls_size : 0;
        sections[sec_idx].VirtualAddress = tls_rva;
        sections[sec_idx].SizeOfRawData = total_tls_size > 0 ? align_up(total_tls_size, file_alignment) : 0;
        sections[sec_idx].PointerToRawData = tls_file_offset;
        sections[sec_idx].Characteristics = 0xC0000040;  
        sec_idx++;
    }
    
    
    if (base_reloc_size > 0) {
        memcpy(sections[sec_idx].Name, ".reloc", 6);
        sections[sec_idx].VirtualSize = base_reloc_size > 0 ? base_reloc_size : 0;
        sections[sec_idx].VirtualAddress = reloc_rva;
        sections[sec_idx].SizeOfRawData = base_reloc_size > 0 ? align_up(base_reloc_size, file_alignment) : 0;
        sections[sec_idx].PointerToRawData = reloc_file_offset;
        sections[sec_idx].Characteristics = 0x42000040;
        sec_idx++;
    }
    
    fwrite(sections, sizeof(IMAGE_SECTION_HEADER) * sec_idx, 1, fp);
    
    
    uint32_t current_offset = (uint32_t)ftell(fp);
    while (current_offset < first_section_offset) {
        uint8_t zero = 0;
        fwrite(&zero, 1, 1, fp);
        current_offset++;
    }
    
    
    if (code_size > 0 && linker->sections[EO_SEC_CODE].data) {
        fprintf(stderr, "DEBUG PE: Writing code section, size=%u, data=%p\n", 
                code_size, (void*)linker->sections[EO_SEC_CODE].data);
        fprintf(stderr, "DEBUG PE: First 16 bytes: ");
        for (uint32_t i = 0; i < 16 && i < code_size; i++) {
            fprintf(stderr, "%02x ", linker->sections[EO_SEC_CODE].data[i]);
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "DEBUG PE: Bytes at offset 0x230: ");
        for (uint32_t i = 0; i < 16 && 0x230 + i < code_size; i++) {
            fprintf(stderr, "%02x ", linker->sections[EO_SEC_CODE].data[0x230 + i]);
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "DEBUG PE: Bytes at offset 0x460: ");
        for (uint32_t i = 0; i < 16 && 0x460 + i < code_size; i++) {
            fprintf(stderr, "%02x ", linker->sections[EO_SEC_CODE].data[0x460 + i]);
        }
        fprintf(stderr, "\n");
        size_t written = fwrite(linker->sections[EO_SEC_CODE].data, 1, code_size, fp);
        fprintf(stderr, "DEBUG PE: Written %zu bytes\n", written);
        uint32_t padding = align_up(code_size, file_alignment) - code_size;
        for (uint32_t i = 0; i < padding; i++) {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, fp);
        }
    } else {
        fprintf(stderr, "DEBUG PE: Skipping code section (code_size=%u, data=%p)\n",
                code_size, (void*)linker->sections[EO_SEC_CODE].data);
    }
    
    
    if (data_size > 0 && linker->sections[EO_SEC_DATA].data) {
        fwrite(linker->sections[EO_SEC_DATA].data, 1, data_size, fp);
        uint32_t padding = align_up(data_size, file_alignment) - data_size;
        for (uint32_t i = 0; i < padding; i++) {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, fp);
        }
    }

    
    if (rodata_size > 0 && linker->sections[EO_SEC_RODATA].data) {
        fwrite(linker->sections[EO_SEC_RODATA].data, 1, rodata_size, fp);
        uint32_t padding = align_up(rodata_size, file_alignment) - rodata_size;
        for (uint32_t i = 0; i < padding; i++) {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, fp);
        }
    }
    
    
    {
        uint32_t actual_import_size = (import_table_size > 0) ? import_table_size : 20;
        if (import_table_size > 0) {
            fprintf(stderr, "DEBUG: Before write_import_table: ftell=%u, idata_file_offset=%u\n",
                    (uint32_t)ftell(fp), idata_file_offset);
            write_import_table(fp, linker, idata_rva, &iat_rva);
            fprintf(stderr, "DEBUG: After write_import_table: ftell=%u\n", (uint32_t)ftell(fp));
        } else {
            
            uint8_t zero[20] = {0};
            fwrite(zero, 1, 20, fp);
        }
        uint32_t padding = align_up(actual_import_size, file_alignment) - actual_import_size;
        for (uint32_t i = 0; i < padding; i++) {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, fp);
        }
    }
    
    
    if (has_tls) {
        
        IMAGE_TLS_DIRECTORY64 tls_dir = {0};
        uint64_t image_base = linker->config.preferred_base;
        uint32_t tls_data_rva = tls_rva + sizeof(IMAGE_TLS_DIRECTORY64);
        tls_dir.StartAddressOfRawData = image_base + tls_data_rva;
        tls_dir.EndAddressOfRawData = image_base + tls_data_rva + tls_size;
        tls_dir.AddressOfIndex = image_base + tls_data_rva + tls_size;  
        tls_dir.AddressOfCallBacks = 0;  
        tls_dir.SizeOfZeroFill = 0;
        tls_dir.Characteristics = 0;
        fwrite(&tls_dir, sizeof(tls_dir), 1, fp);
        
        
        fwrite(linker->sections[EO_SEC_TLS].data, 1, tls_size, fp);
        
        
        uint32_t total_tls_size = sizeof(IMAGE_TLS_DIRECTORY64) + tls_size;
        uint32_t padding = align_up(total_tls_size, file_alignment) - total_tls_size;
        for (uint32_t i = 0; i < padding; i++) {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, fp);
        }
    }
    
    
    if (base_reloc_size > 0) {
        uint32_t reloc_start = (uint32_t)ftell(fp);
        uint32_t bytes_written = write_base_reloc_table(fp, linker);
        uint32_t reloc_end = (uint32_t)ftell(fp);
        fprintf(stderr, "DEBUG: Base reloc table: expected=%u, written=%u, file_pos=%u\n",
                base_reloc_size, bytes_written, reloc_end - reloc_start);
        uint32_t padding = align_up(base_reloc_size, file_alignment) - base_reloc_size;
        fprintf(stderr, "DEBUG: Base reloc padding: %u bytes\n", padding);
        for (uint32_t i = 0; i < padding; i++) {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, fp);
        }
    }
    
    uint32_t final_size = (uint32_t)ftell(fp);
    fprintf(stderr, "DEBUG: Final file size: %u bytes (0x%X)\n", final_size, final_size);
    
    
    
    
    uint32_t pe_header_offset = 0x80; 
    uint32_t checksum_file_offset = pe_header_offset + 4 + 20 + 64;
    uint32_t checksum = final_size;
    
    if (final_size > checksum_file_offset + 4) {
        fseek(fp, checksum_file_offset, SEEK_SET);
        fwrite(&checksum, 4, 1, fp);
        fprintf(stderr, "DEBUG: Checksum 0x%X written at offset %u\n", checksum, checksum_file_offset);
    }
    
    fclose(fp);
    return true;
}
