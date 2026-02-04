

#include "ArkLink/linker.h"
#include "ArkLink/elf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static uint64_t align_up(uint64_t value, uint64_t alignment) {
    if (alignment == 0) return value;
    uint64_t remainder = value % alignment;
    if (remainder == 0) return value;
    return value + (alignment - remainder);
}


static bool apply_elf_relocations(ArkLinker* linker, uint64_t base_addr,
                                   uint64_t text_vaddr, uint64_t data_vaddr,
                                   uint64_t plt_vaddr, uint64_t got_plt_vaddr) {
    for (int i = 0; i < linker->reloc_count; i++) {
        const char* sym_name = linker->relocs[i].symbol_name;
        uint32_t section = linker->relocs[i].section;
        uint32_t offset = linker->relocs[i].offset;
        uint16_t type = linker->relocs[i].type;
        int32_t addend = linker->relocs[i].addend;

        
        uint8_t* target_data = NULL;
        uint64_t section_vaddr = 0;

        switch (section) {
            case EO_SEC_CODE:
                target_data = linker->sections[EO_SEC_CODE].data;
                section_vaddr = text_vaddr;
                break;
            case EO_SEC_DATA:
                target_data = linker->sections[EO_SEC_DATA].data;
                section_vaddr = data_vaddr;
                break;
            case EO_SEC_RODATA:
                target_data = linker->sections[EO_SEC_RODATA].data;
                
                section_vaddr = base_addr + sizeof(Elf64_Ehdr); 
                break;
            default:
                continue;
        }

        if (!target_data) continue;

        
        int sym_idx = -1;
        for (int j = 0; j < linker->symbol_count; j++) {
            if (linker->symbols[j].name == sym_name) {
                sym_idx = j;
                break;
            }
        }

        
        bool is_import = false;
        int import_idx = -1;
        int func_counter = 0;
        for (int d = 0; d < linker->import_count && !is_import; d++) {
            for (int f = 0; f < linker->imports[d].func_count; f++) {
                if (strcmp(linker->imports[d].functions[f].func_name, sym_name) == 0) {
                    is_import = true;
                    import_idx = func_counter;  
                    break;
                }
                func_counter++;
            }
            if (!is_import) {
                func_counter += linker->imports[d].func_count;
            }
        }

        
        switch (type) {
            case EO_RELOC_REL32: {
                
                int32_t* ptr = (int32_t*)(target_data + offset);
                uint64_t next_insn = section_vaddr + offset + 4;

                if (is_import) {
                    
                    uint64_t got_entry = got_plt_vaddr + (import_idx + 3) * 8;
                    *ptr = (int32_t)(got_entry - next_insn) + addend;
                } else if (sym_idx >= 0 && linker->symbols[sym_idx].is_defined) {
                    
                    uint64_t sym_vaddr = 0;
                    switch (linker->symbols[sym_idx].section) {
                        case EO_SEC_CODE:
                            sym_vaddr = text_vaddr + linker->symbols[sym_idx].offset;
                            break;
                        case EO_SEC_DATA:
                            sym_vaddr = data_vaddr + linker->symbols[sym_idx].offset;
                            break;
                    }
                    *ptr = (int32_t)(sym_vaddr - next_insn) + addend;
                }
                break;
            }

            case EO_RELOC_ABS32: {
                
                uint32_t* ptr = (uint32_t*)(target_data + offset);
                if (is_import) {
                    *ptr = (uint32_t)(plt_vaddr + 16 + import_idx * 16) + addend;
                } else if (sym_idx >= 0 && linker->symbols[sym_idx].is_defined) {
                    uint64_t sym_vaddr = 0;
                    switch (linker->symbols[sym_idx].section) {
                        case EO_SEC_CODE:
                            sym_vaddr = text_vaddr + linker->symbols[sym_idx].offset;
                            break;
                        case EO_SEC_DATA:
                            sym_vaddr = data_vaddr + linker->symbols[sym_idx].offset;
                            break;
                    }
                    *ptr = (uint32_t)sym_vaddr + addend;
                }
                break;
            }

            case EO_RELOC_ABS64: {
                
                uint64_t* ptr = (uint64_t*)(target_data + offset);
                if (is_import) {
                    *ptr = plt_vaddr + 16 + import_idx * 16 + addend;
                } else if (sym_idx >= 0 && linker->symbols[sym_idx].is_defined) {
                    uint64_t sym_vaddr = 0;
                    switch (linker->symbols[sym_idx].section) {
                        case EO_SEC_CODE:
                            sym_vaddr = text_vaddr + linker->symbols[sym_idx].offset;
                            break;
                        case EO_SEC_DATA:
                            sym_vaddr = data_vaddr + linker->symbols[sym_idx].offset;
                            break;
                    }
                    *ptr = sym_vaddr + addend;
                }
                break;
            }

            default:
                fprintf(stderr, "Warning: Unknown relocation type %u\n", type);
                break;
        }
    }

    return true;
}


enum {
    SHN_UNDEF = 0,
    SHN_ABS = 0xFFF1,
    SHN_COMMON = 0xFFF2,
};


static void write_elf_header(FILE* fp, uint64_t entry, uint64_t phoff, uint16_t phnum,
                              uint64_t shoff, uint16_t shnum, uint16_t shstrndx) {
    Elf64_Ehdr ehdr = {0};

    ehdr.e_ident[0] = ELFMAG0;
    ehdr.e_ident[1] = ELFMAG1;
    ehdr.e_ident[2] = ELFMAG2;
    ehdr.e_ident[3] = ELFMAG3;
    ehdr.e_ident[4] = ELFCLASS64;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = ELFOSABI_LINUX;

    ehdr.e_type = ET_EXEC;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = entry;
    ehdr.e_phoff = phoff;
    ehdr.e_shoff = shoff;
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = phnum;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = shnum;
    ehdr.e_shstrndx = shstrndx;

    fwrite(&ehdr, sizeof(ehdr), 1, fp);
}


static void write_program_header(FILE* fp, uint32_t type, uint32_t flags,
                                  uint64_t offset, uint64_t vaddr,
                                  uint64_t filesz, uint64_t memsz, uint64_t align) {
    Elf64_Phdr phdr = {0};
    phdr.p_type = type;
    phdr.p_flags = flags;
    phdr.p_offset = offset;
    phdr.p_vaddr = vaddr;
    phdr.p_paddr = vaddr;
    phdr.p_filesz = filesz;
    phdr.p_memsz = memsz;
    phdr.p_align = align;
    fwrite(&phdr, sizeof(phdr), 1, fp);
}


static void write_symbol(FILE* fp, uint32_t name, uint8_t info, uint16_t shndx,
                         uint64_t value, uint64_t size) {
    Elf64_Sym sym = {0};
    sym.st_name = name;
    sym.st_info = info;
    sym.st_shndx = shndx;
    sym.st_value = value;
    sym.st_size = size;
    fwrite(&sym, sizeof(sym), 1, fp);
}


static void write_rela(FILE* fp, uint64_t offset, uint32_t sym, uint32_t type, int64_t addend) {
    Elf64_Rela rela = {0};
    rela.r_offset = offset;
    rela.r_info = ELF64_R_INFO(sym, type);
    rela.r_addend = addend;
    fwrite(&rela, sizeof(rela), 1, fp);
}


static void write_dynamic(FILE* fp, int64_t tag, uint64_t val) {
    Elf64_Dyn dyn = {0};
    dyn.d_tag = tag;
    dyn.d_un.d_val = val;
    fwrite(&dyn, sizeof(dyn), 1, fp);
}


static int count_total_imports(ArkLinker* linker) {
    int count = 0;
    for (int i = 0; i < linker->import_count; i++) {
        count += linker->imports[i].func_count;
    }
    return count;
}


bool es_linker_write_elf(ArkLinker* linker, const char* filename) {
    if (!linker || !filename) return false;

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create output file: %s\n", filename);
        return false;
    }

    
    uint32_t code_size = linker->sections[EO_SEC_CODE].file_size;
    uint32_t data_size = linker->sections[EO_SEC_DATA].file_size;
    uint32_t bss_size = linker->sections[EO_SEC_BSS].mem_size;
    uint32_t tls_size = linker->sections[EO_SEC_TLS].file_size;
    bool has_tls = tls_size > 0 && linker->sections[EO_SEC_TLS].data;

    
    int total_imports = count_total_imports(linker);
    bool has_imports = total_imports > 0;

    
    uint64_t page_size = 0x1000;
    uint64_t ptr_size = sizeof(uint64_t);

    
    const char* interp_path = "/lib64/ld-linux-x86-64.so.2";
    size_t interp_size = has_imports ? (strlen(interp_path) + 1) : 0;
    size_t dynsym_size = (total_imports + 1) * sizeof(Elf64_Sym);
    size_t dynstr_size = 1;
    size_t rela_plt_size = total_imports * sizeof(Elf64_Rela);
    size_t plt_size = has_imports ? (16 + total_imports * 16) : 0;
    size_t got_plt_size = (total_imports + 3) * ptr_size;
    size_t dynamic_size = has_imports ? (14 * sizeof(Elf64_Dyn)) : 0;

    
    
    size_t* dll_name_offsets = NULL;
    if (has_imports && linker->import_count > 0) {
        dll_name_offsets = (size_t*)malloc(linker->import_count * sizeof(size_t));
    }

    
    for (int i = 0; i < linker->import_count; i++) {
        for (int j = 0; j < linker->imports[i].func_count; j++) {
            if (linker->imports[i].functions[j].func_name) {
                dynstr_size += strlen(linker->imports[i].functions[j].func_name) + 1;
            }
        }
    }

    
    for (int i = 0; i < linker->import_count; i++) {
        if (linker->imports[i].dll_name) {
            dll_name_offsets[i] = dynstr_size;
            dynstr_size += strlen(linker->imports[i].dll_name) + 1;
        } else {
            dll_name_offsets[i] = 0;
        }
    }

    
    
    
    

    uint64_t base_addr = linker->config.preferred_base ? linker->config.preferred_base : ELF_DEFAULT_BASE;

    
    uint64_t phdr_count = 3;  
    if (has_imports) phdr_count += 4;  
    if (has_tls) phdr_count += 1;  

    uint64_t headers_size = sizeof(Elf64_Ehdr) + phdr_count * sizeof(Elf64_Phdr);

    
    
    uint64_t rodata_content_size = align_up(interp_size, ptr_size) +
                                   align_up(dynsym_size, ptr_size) +
                                   align_up(dynstr_size, ptr_size) +
                                   rela_plt_size;
    uint64_t first_load_filesz = align_up(headers_size + rodata_content_size, page_size);
    uint64_t first_load_memsz = first_load_filesz;

    
    uint64_t text_file_offset = first_load_filesz;
    uint64_t text_vaddr = base_addr + text_file_offset;
    uint64_t plt_vaddr = text_vaddr;
    uint64_t code_vaddr = text_vaddr + plt_size;
    uint64_t text_filesz = plt_size + code_size;
    uint64_t text_memsz = text_filesz;  
    uint64_t text_aligned_size = align_up(text_filesz, page_size);

    
    uint64_t data_file_offset = text_file_offset + text_aligned_size;
    uint64_t data_vaddr = base_addr + data_file_offset;
    uint64_t dynamic_vaddr = data_vaddr;
    uint64_t got_plt_vaddr = dynamic_vaddr + dynamic_size;
    uint64_t data_sec_vaddr = got_plt_vaddr + got_plt_size;
    (void)data_sec_vaddr;  

    uint64_t data_filesz = dynamic_size + got_plt_size + data_size;
    uint64_t data_memsz = data_filesz + bss_size;
    uint64_t data_aligned_size = align_up(data_filesz, page_size);

    
    uint64_t tls_file_offset = data_file_offset + data_aligned_size;
    uint64_t tls_vaddr = base_addr + tls_file_offset;
    uint64_t tls_filesz = tls_size;
    uint64_t tls_memsz = tls_size;
    uint64_t tls_aligned_size = align_up(tls_filesz, page_size);

    
    uint64_t interp_vaddr = base_addr + headers_size;
    uint64_t dynsym_vaddr = interp_vaddr + align_up(interp_size, ptr_size);
    uint64_t dynstr_vaddr = dynsym_vaddr + align_up(dynsym_size, ptr_size);
    uint64_t rela_plt_vaddr = dynstr_vaddr + align_up(dynstr_size, ptr_size);

    
    uint64_t entry = code_vaddr + linker->entry_point;

    
    apply_elf_relocations(linker, base_addr, text_vaddr, data_vaddr, plt_vaddr, got_plt_vaddr);

    
    write_elf_header(fp, entry, sizeof(Elf64_Ehdr), (uint16_t)phdr_count,
                     0, 0, 0);

    
    
    write_program_header(fp, PT_PHDR, PF_R,
                         sizeof(Elf64_Ehdr), base_addr + sizeof(Elf64_Ehdr),
                         phdr_count * sizeof(Elf64_Phdr),
                         phdr_count * sizeof(Elf64_Phdr), ptr_size);

    
    if (has_imports) {
        write_program_header(fp, PT_INTERP, PF_R,
                             headers_size, interp_vaddr,
                             interp_size, interp_size, 1);
    }

    
    write_program_header(fp, PT_LOAD, PF_R,
                         0, base_addr,
                         first_load_filesz, first_load_memsz, page_size);

    
    write_program_header(fp, PT_LOAD, PF_R | PF_X,
                         text_file_offset, text_vaddr,
                         text_filesz, text_memsz, page_size);

    
    write_program_header(fp, PT_LOAD, PF_R | PF_W,
                         data_file_offset, data_vaddr,
                         data_filesz, data_memsz, page_size);

    
    if (has_imports) {
        write_program_header(fp, PT_DYNAMIC, PF_R | PF_W,
                             data_file_offset, dynamic_vaddr,
                             dynamic_size, dynamic_size, ptr_size);
    }

    
    if (has_imports) {
        uint64_t relro_size = got_plt_size + dynamic_size;
        write_program_header(fp, PT_GNU_RELRO, PF_R,
                             data_file_offset, data_vaddr,
                             relro_size, relro_size, 1);
    }

    
    if (has_tls) {
        write_program_header(fp, PT_TLS, PF_R,
                             tls_file_offset, tls_vaddr,
                             tls_filesz, tls_memsz, page_size);
    }

    
    
    if ((uint64_t)ftell(fp) != headers_size) {
        fseek(fp, (long)headers_size, SEEK_SET);
    }

    
    if (has_imports) {
        fwrite(interp_path, 1, interp_size, fp);
    }

    
    while (ftell(fp) % ptr_size != 0) {
        fputc(0, fp);
    }

    
    if (has_imports) {
        write_symbol(fp, 0, 0, SHN_UNDEF, 0, 0);
        uint32_t dynstr_offset = 1;
        for (int i = 0; i < linker->import_count; i++) {
            for (int j = 0; j < linker->imports[i].func_count; j++) {
                const char* func_name = linker->imports[i].functions[j].func_name;
                uint32_t name_offset = dynstr_offset;
                dynstr_offset += strlen(func_name) + 1;
                write_symbol(fp, name_offset,
                            ELF64_ST_INFO(STB_GLOBAL, STT_FUNC),
                            SHN_UNDEF, 0, 0);
            }
        }
        while (ftell(fp) % ptr_size != 0) {
            fputc(0, fp);
        }
    }

    
    if (has_imports) {
        fputc(0, fp);
        for (int i = 0; i < linker->import_count; i++) {
            for (int j = 0; j < linker->imports[i].func_count; j++) {
                const char* func_name = linker->imports[i].functions[j].func_name;
                fwrite(func_name, 1, strlen(func_name) + 1, fp);
            }
        }
        for (int i = 0; i < linker->import_count; i++) {
            const char* dll_name = linker->imports[i].dll_name;
            fwrite(dll_name, 1, strlen(dll_name) + 1, fp);
        }
        while (ftell(fp) % ptr_size != 0) {
            fputc(0, fp);
        }
    }

    
    if (has_imports) {
        int sym_idx = 1;
        for (int i = 0; i < total_imports; i++) {
            uint64_t got_entry_addr = got_plt_vaddr + (i + 3) * ptr_size;
            write_rela(fp, got_entry_addr, sym_idx, R_X86_64_JUMP_SLOT, 0);
            sym_idx++;
        }
        while (ftell(fp) % ptr_size != 0) {
            fputc(0, fp);
        }
    }

    
    while (ftell(fp) < (long)first_load_filesz) {
        fputc(0, fp);
    }

    
    
    if (has_imports) {
        
        uint8_t plt0[16] = {
            0xFF, 0x35, 0, 0, 0, 0,   
            0xFF, 0x25, 0, 0, 0, 0,   
            0x0F, 0x1F, 0x40, 0x00    
        };
        int32_t push_offset = (int32_t)(got_plt_vaddr + ptr_size - (plt_vaddr + 6));
        int32_t jmp_offset = (int32_t)(got_plt_vaddr + 2*ptr_size - (plt_vaddr + 12));
        memcpy(plt0 + 2, &push_offset, 4);
        memcpy(plt0 + 8, &jmp_offset, 4);
        fwrite(plt0, 1, 16, fp);

        
        for (int i = 0; i < total_imports; i++) {
            uint8_t plt_entry[16] = {
                0xFF, 0x25, 0, 0, 0, 0,   
                0x68, 0, 0, 0, 0,          
                0xE9, 0, 0, 0, 0          
            };
            int32_t got_offset = (int32_t)(got_plt_vaddr + (i + 3) * ptr_size - (plt_vaddr + 16 + i * 16 + 6));
            int32_t plt0_offset = (int32_t)(plt_vaddr - (plt_vaddr + 16 + i * 16 + 11));
            memcpy(plt_entry + 2, &got_offset, 4);
            memcpy(plt_entry + 7, &i, 4);
            memcpy(plt_entry + 12, &plt0_offset, 4);
            fwrite(plt_entry, 1, 16, fp);
        }
    }

    
    if (code_size > 0) {
        fwrite(linker->sections[EO_SEC_CODE].data, 1, code_size, fp);
    }

    
    while (ftell(fp) < (long)(text_file_offset + text_aligned_size)) {
        fputc(0, fp);
    }

    
    
    if (has_imports) {
        write_dynamic(fp, DT_PLTGOT, got_plt_vaddr);
        write_dynamic(fp, DT_PLTRELSZ, rela_plt_size);
        write_dynamic(fp, DT_PLTREL, DT_RELA);
        write_dynamic(fp, DT_JMPREL, rela_plt_vaddr);
        write_dynamic(fp, DT_RELA, 0);
        write_dynamic(fp, DT_RELASZ, 0);
        write_dynamic(fp, DT_RELAENT, sizeof(Elf64_Rela));
        write_dynamic(fp, DT_SYMTAB, dynsym_vaddr);
        write_dynamic(fp, DT_SYMENT, sizeof(Elf64_Sym));
        write_dynamic(fp, DT_STRTAB, dynstr_vaddr);
        write_dynamic(fp, DT_STRSZ, dynstr_size);
        for (int i = 0; i < linker->import_count; i++) {
            write_dynamic(fp, DT_NEEDED, dll_name_offsets[i]);
        }
        write_dynamic(fp, DT_NULL, 0);
    }

    
    if (has_imports) {
        uint64_t dynamic_addr = dynamic_vaddr;
        fwrite(&dynamic_addr, ptr_size, 1, fp);
        uint64_t zero = 0;
        fwrite(&zero, ptr_size, 1, fp);
        fwrite(&zero, ptr_size, 1, fp);
        for (int i = 0; i < total_imports; i++) {
            uint64_t plt_entry_addr = plt_vaddr + 16 + i * 16 + 6;
            fwrite(&plt_entry_addr, ptr_size, 1, fp);
        }
    }

    
    if (data_size > 0) {
        fwrite(linker->sections[EO_SEC_DATA].data, 1, data_size, fp);
    }

    
    while (ftell(fp) < (long)(data_file_offset + data_aligned_size)) {
        fputc(0, fp);
    }

    
    if (has_tls) {
        fwrite(linker->sections[EO_SEC_TLS].data, 1, tls_size, fp);
        
        while (ftell(fp) < (long)(tls_file_offset + tls_aligned_size)) {
            fputc(0, fp);
        }
    }

    
    if (dll_name_offsets) {
        free(dll_name_offsets);
    }

    fclose(fp);

    printf("ELF executable written to: %s\n", filename);
    printf("  Entry point: 0x%llX\n", entry);
    printf("  Code size: %u bytes\n", code_size);
    printf("  Data size: %u bytes\n", data_size);
    if (has_tls) {
        printf("  TLS size: %u bytes\n", tls_size);
    }
    printf("  Imports: %d functions from %d libraries\n", total_imports, linker->import_count);

    return true;
}
