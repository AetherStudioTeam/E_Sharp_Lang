#include "ArkLink/targets/elf.h"
#include "ArkLink/backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_EXEC 2
#define ET_DYN 3
#define EM_X86_64 62

#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_PHDR 6

#define PF_X 1
#define PF_W 2
#define PF_R 4

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_DYNSYM 11

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4

#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_PLTGOT 3
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_INIT 12
#define DT_FINI 13
#define DT_SONAME 14
#define DT_RPATH 15
#define DT_SYMBOLIC 16
#define DT_REL 17
#define DT_RELSZ 18
#define DT_RELENT 19
#define DT_PLTREL 20
#define DT_DEBUG 21
#define DT_TEXTREL 22
#define DT_JMPREL 23
#define DT_BIND_NOW 24
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH 29
#define DT_FLAGS 30

#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_GOT32 3
#define R_X86_64_PLT32 4
#define R_X86_64_COPY 5
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE 8
#define R_X86_64_GOTPCREL 9


typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;


typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;


typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;


typedef struct {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;


typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
} Elf64_Rela;


typedef struct {
    int64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} Elf64_Dyn;


typedef struct {
    char* name;
    uint8_t* data;
    size_t size;
    size_t capacity;
    uint64_t addr;
    uint64_t offset;
    uint32_t type;
    uint64_t flags;
    uint64_t addralign;
    uint32_t link;
    uint32_t info;
    uint64_t entsize;
} ArkELFSection;


typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} ArkELFSegment;


typedef struct {
    ArkBackendInput* input;
    ArkELFSection* sections;
    size_t section_count;
    ArkELFSegment* segments;
    size_t segment_count;
    char* shstrtab;
    size_t shstrtab_size;
    size_t shstrtab_capacity;
    char* dynstrtab;
    size_t dynstrtab_size;
    size_t dynstrtab_capacity;
    Elf64_Sym* dynsyms;
    size_t dynsym_count;
    size_t dynsym_capacity;
    Elf64_Rela* dynrelas;
    size_t dynrela_count;
    size_t dynrela_capacity;
    Elf64_Dyn* dynamics;
    size_t dynamic_count;
    size_t dynamic_capacity;
    uint64_t entry_addr;
    uint64_t image_base;
    uint64_t phdr_offset;
    uint64_t phdr_size;
    uint64_t shdr_offset;
    size_t total_file_size;
} ArkELFState;

static uint64_t ark_elf_align(uint64_t value, uint64_t alignment) {
    if (alignment == 0) return value;
    uint64_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

static uint32_t ark_elf_section_type(ArkSectionKind kind) {
    switch (kind) {
    case ARK_SECTION_CODE:
    case ARK_SECTION_DATA:
    case ARK_SECTION_RODATA:
        return SHT_PROGBITS;
    case ARK_SECTION_BSS:
        return SHT_NOBITS;
    case ARK_SECTION_TLS:
        return SHT_PROGBITS;
    default:
        return SHT_PROGBITS;
    }
}

static uint64_t ark_elf_section_flags(ArkSectionKind kind, uint32_t flags) {
    uint64_t elf_flags = SHF_ALLOC;
    switch (kind) {
    case ARK_SECTION_CODE:
        elf_flags |= SHF_EXECINSTR;
        break;
    case ARK_SECTION_DATA:
    case ARK_SECTION_TLS:
        elf_flags |= SHF_WRITE;
        break;
    case ARK_SECTION_BSS:
        elf_flags |= SHF_WRITE;
        break;
    case ARK_SECTION_RODATA:
        
        break;
    }
    (void)flags;
    return elf_flags;
}

static uint32_t ark_elf_reloc_type(uint32_t type) {
    switch (type) {
    case 0: 
        return R_X86_64_64;
    case 1: 
        return R_X86_64_PC32;
    default:
        return R_X86_64_NONE;
    }
}

static size_t ark_elf_add_shstr(ArkELFState* state, const char* str) {
    size_t len = strlen(str) + 1;
    size_t offset = state->shstrtab_size;
    
    if (offset + len > state->shstrtab_capacity) {
        state->shstrtab_capacity = state->shstrtab_capacity ? state->shstrtab_capacity * 2 : 256;
        while (state->shstrtab_capacity < offset + len) {
            state->shstrtab_capacity *= 2;
        }
        state->shstrtab = (char*)realloc(state->shstrtab, state->shstrtab_capacity);
        if (!state->shstrtab) {
            return 0;
        }
    }
    
    memcpy(state->shstrtab + offset, str, len);
    state->shstrtab_size += len;
    return offset;
}

static size_t ark_elf_add_dynstr(ArkELFState* state, const char* str) {
    size_t len = strlen(str) + 1;
    size_t offset = state->dynstrtab_size;
    
    
    for (size_t i = 0; i < state->dynstrtab_size; i++) {
        if (strcmp(state->dynstrtab + i, str) == 0) {
            return i;
        }
    }
    
    if (offset + len > state->dynstrtab_capacity) {
        state->dynstrtab_capacity = state->dynstrtab_capacity ? state->dynstrtab_capacity * 2 : 256;
        while (state->dynstrtab_capacity < offset + len) {
            state->dynstrtab_capacity *= 2;
        }
        state->dynstrtab = (char*)realloc(state->dynstrtab, state->dynstrtab_capacity);
        if (!state->dynstrtab) {
            return 0;
        }
    }
    
    memcpy(state->dynstrtab + offset, str, len);
    state->dynstrtab_size += len;
    return offset;
}

static int ark_elf_add_dynsym(ArkELFState* state, const char* name, uint32_t shndx, 
                               uint64_t value, uint64_t size, int binding, int type) {
    if (state->dynsym_count >= state->dynsym_capacity) {
        state->dynsym_capacity = state->dynsym_capacity ? state->dynsym_capacity * 2 : 16;
        state->dynsyms = (Elf64_Sym*)realloc(state->dynsyms, state->dynsym_capacity * sizeof(Elf64_Sym));
        if (!state->dynsyms) {
            return -1;
        }
    }
    
    Elf64_Sym* sym = &state->dynsyms[state->dynsym_count];
    sym->st_name = (uint32_t)ark_elf_add_dynstr(state, name);
    sym->st_info = (binding << 4) | (type & 0xf);
    sym->st_other = 0;
    sym->st_shndx = shndx;
    sym->st_value = value;
    sym->st_size = size;
    
    return (int)state->dynsym_count++;
}

static int ark_elf_add_dynamic(ArkELFState* state, int64_t tag, uint64_t val) {
    if (state->dynamic_count >= state->dynamic_capacity) {
        state->dynamic_capacity = state->dynamic_capacity ? state->dynamic_capacity * 2 : 16;
        state->dynamics = (Elf64_Dyn*)realloc(state->dynamics, state->dynamic_capacity * sizeof(Elf64_Dyn));
        if (!state->dynamics) {
            return -1;
        }
    }
    
    Elf64_Dyn* dyn = &state->dynamics[state->dynamic_count];
    dyn->d_tag = tag;
    dyn->d_un.d_val = val;
    
    state->dynamic_count++;
    return 0;
}

static ArkLinkResult ark_elf_prepare_sections(ArkELFState* state) {
    
    state->section_count = state->input->section_count + 4; 
    state->sections = (ArkELFSection*)calloc(state->section_count, sizeof(ArkELFSection));
    if (!state->sections) {
        return ARK_LINK_ERR_INTERNAL;
    }
    
    
    state->sections[0].name = "";
    state->sections[0].type = SHT_NULL;
    state->sections[0].flags = 0;
    state->sections[0].addr = 0;
    state->sections[0].offset = 0;
    state->sections[0].size = 0;
    state->sections[0].addralign = 0;
    state->sections[0].entsize = 0;
    
    
    for (size_t i = 0; i < state->input->section_count; i++) {
        ArkBackendInputSection* src = &state->input->sections[i];
        ArkELFSection* dst = &state->sections[i + 1];
        
        dst->name = strdup(src->name);
        dst->type = ark_elf_section_type(src->buffer->kind);
        dst->flags = ark_elf_section_flags(src->buffer->kind, src->buffer->flags);
        dst->size = src->buffer->size;
        dst->addralign = src->buffer->alignment > 0 ? src->buffer->alignment : 1;
        dst->entsize = 0;
        
        if (src->buffer->size > 0 && dst->type != SHT_NOBITS) {
            dst->data = (uint8_t*)malloc(src->buffer->size);
            if (!dst->data) {
                return ARK_LINK_ERR_INTERNAL;
            }
            memcpy(dst->data, src->buffer->data, src->buffer->size);
        }
    }
    
    return ARK_LINK_OK;
}

static void ark_elf_free_sections(ArkELFState* state) {
    for (size_t i = 0; i < state->section_count; i++) {
        free(state->sections[i].name);
        free(state->sections[i].data);
    }
    free(state->sections);
    state->sections = NULL;
}

static void ark_elf_compute_layout(ArkELFState* state) {
    const uint64_t page_size = 4096;
    state->image_base = 0x400000;
    
    
    size_t ehdr_size = sizeof(Elf64_Ehdr);
    size_t phdr_size = 0;
    
    
    state->segment_count = 0;
    for (size_t i = 0; i < state->input->section_count; i++) {
        ArkELFSection* sec = &state->sections[i + 1];
        if (sec->flags & SHF_ALLOC) {
            state->segment_count++;
        }
    }
    
    if (state->segment_count > 0) {
        phdr_size = (state->segment_count + 2) * sizeof(Elf64_Phdr); 
    }
    
    state->phdr_offset = ark_elf_align(ehdr_size, 8);
    state->phdr_size = phdr_size;
    
    
    uint64_t current_offset = ark_elf_align(state->phdr_offset + phdr_size, 8);
    
    for (size_t i = 0; i < state->input->section_count; i++) {
        ArkELFSection* sec = &state->sections[i + 1];
        
        if (sec->type == SHT_NOBITS) {
            sec->offset = 0;
        } else {
            current_offset = ark_elf_align(current_offset, sec->addralign);
            sec->offset = current_offset;
            current_offset += sec->size;
        }
    }
    
    
    size_t shstrtab_idx = state->section_count - 3;
    ArkELFSection* shstrtab = &state->sections[shstrtab_idx];
    shstrtab->name = ".shstrtab";
    shstrtab->type = SHT_STRTAB;
    shstrtab->flags = 0;
    shstrtab->addralign = 1;
    shstrtab->entsize = 0;
    
    
    ark_elf_add_shstr(state, ""); 
    for (size_t i = 0; i < state->input->section_count; i++) {
        ArkELFSection* sec = &state->sections[i + 1];
        ark_elf_add_shstr(state, sec->name);
    }
    ark_elf_add_shstr(state, ".shstrtab");
    ark_elf_add_shstr(state, ".symtab");
    ark_elf_add_shstr(state, ".strtab");
    
    shstrtab->size = state->shstrtab_size;
    current_offset = ark_elf_align(current_offset, shstrtab->addralign);
    shstrtab->offset = current_offset;
    current_offset += shstrtab->size;
    
    
    uint64_t current_addr = state->image_base + page_size;
    
    for (size_t i = 0; i < state->input->section_count; i++) {
        ArkELFSection* sec = &state->sections[i + 1];
        
        if (sec->flags & SHF_ALLOC) {
            current_addr = ark_elf_align(current_addr, sec->addralign);
            sec->addr = current_addr;
            current_addr += sec->size;
        } else {
            sec->addr = 0;
        }
    }
    
    
    state->entry_addr = 0;
    if (state->input->entry_section < state->input->section_count) {
        ArkELFSection* sec = &state->sections[state->input->entry_section + 1];
        state->entry_addr = sec->addr + state->input->entry_offset;
    }
    
    
    current_offset = ark_elf_align(current_offset, 8);
    state->shdr_offset = current_offset;
    current_offset += state->section_count * sizeof(Elf64_Shdr);
    
    state->total_file_size = current_offset;
}

static ArkLinkResult ark_elf_write_header(ArkELFState* state, FILE* fp) {
    Elf64_Ehdr ehdr = {0};
    
    
    ehdr.e_ident[0] = ELFMAG0;
    ehdr.e_ident[1] = ELFMAG1;
    ehdr.e_ident[2] = ELFMAG2;
    ehdr.e_ident[3] = ELFMAG3;
    ehdr.e_ident[4] = ELFCLASS64;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = 0; 
    
    ehdr.e_type = ET_EXEC;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = state->entry_addr;
    ehdr.e_phoff = state->phdr_offset;
    ehdr.e_shoff = state->shdr_offset;
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = (uint16_t)(state->segment_count + 2);
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = (uint16_t)state->section_count;
    ehdr.e_shstrndx = (uint16_t)(state->section_count - 3); 
    
    if (fwrite(&ehdr, sizeof(ehdr), 1, fp) != 1) {
        return ARK_LINK_ERR_IO;
    }
    
    return ARK_LINK_OK;
}

static ArkLinkResult ark_elf_write_program_headers(ArkELFState* state, FILE* fp) {
    
    if (fseek(fp, (long)state->phdr_offset, SEEK_SET) != 0) {
        return ARK_LINK_ERR_IO;
    }
    
    
    Elf64_Phdr phdr_phdr = {0};
    phdr_phdr.p_type = PT_PHDR;
    phdr_phdr.p_flags = PF_R;
    phdr_phdr.p_offset = state->phdr_offset;
    phdr_phdr.p_vaddr = state->image_base + state->phdr_offset;
    phdr_phdr.p_paddr = phdr_phdr.p_vaddr;
    phdr_phdr.p_filesz = state->phdr_size;
    phdr_phdr.p_memsz = state->phdr_size;
    phdr_phdr.p_align = 8;
    
    if (fwrite(&phdr_phdr, sizeof(phdr_phdr), 1, fp) != 1) {
        return ARK_LINK_ERR_IO;
    }
    
    
    for (size_t i = 0; i < state->input->section_count; i++) {
        ArkELFSection* sec = &state->sections[i + 1];
        
        if (!(sec->flags & SHF_ALLOC)) {
            continue;
        }
        
        Elf64_Phdr phdr = {0};
        phdr.p_type = PT_LOAD;
        phdr.p_flags = PF_R;
        if (sec->flags & SHF_WRITE) {
            phdr.p_flags |= PF_W;
        }
        if (sec->flags & SHF_EXECINSTR) {
            phdr.p_flags |= PF_X;
        }
        phdr.p_offset = sec->offset;
        phdr.p_vaddr = sec->addr;
        phdr.p_paddr = sec->addr;
        phdr.p_filesz = sec->type == SHT_NOBITS ? 0 : sec->size;
        phdr.p_memsz = sec->size;
        phdr.p_align = sec->addralign;
        
        if (fwrite(&phdr, sizeof(phdr), 1, fp) != 1) {
            return ARK_LINK_ERR_IO;
        }
    }
    
    return ARK_LINK_OK;
}

static ArkLinkResult ark_elf_write_section_data(ArkELFState* state, FILE* fp) {
    for (size_t i = 0; i < state->input->section_count; i++) {
        ArkELFSection* sec = &state->sections[i + 1];
        
        if (sec->type == SHT_NOBITS || sec->size == 0) {
            continue;
        }
        
        if (fseek(fp, (long)sec->offset, SEEK_SET) != 0) {
            return ARK_LINK_ERR_IO;
        }
        
        if (sec->data && sec->size > 0) {
            if (fwrite(sec->data, 1, sec->size, fp) != sec->size) {
                return ARK_LINK_ERR_IO;
            }
        }
    }
    
    
    ArkELFSection* shstrtab = &state->sections[state->section_count - 3];
    if (fseek(fp, (long)shstrtab->offset, SEEK_SET) != 0) {
        return ARK_LINK_ERR_IO;
    }
    if (fwrite(state->shstrtab, 1, state->shstrtab_size, fp) != state->shstrtab_size) {
        return ARK_LINK_ERR_IO;
    }
    
    return ARK_LINK_OK;
}

static ArkLinkResult ark_elf_write_section_headers(ArkELFState* state, FILE* fp) {
    if (fseek(fp, (long)state->shdr_offset, SEEK_SET) != 0) {
        return ARK_LINK_ERR_IO;
    }
    
    
    size_t name_offset = 0;
    size_t* name_offsets = (size_t*)calloc(state->section_count, sizeof(size_t));
    if (!name_offsets) {
        return ARK_LINK_ERR_INTERNAL;
    }
    
    name_offsets[0] = 0; 
    name_offset = 1; 
    
    for (size_t i = 0; i < state->input->section_count; i++) {
        name_offsets[i + 1] = name_offset;
        name_offset += strlen(state->sections[i + 1].name) + 1;
    }
    
    name_offsets[state->section_count - 3] = name_offset;
    name_offset += strlen(".shstrtab") + 1;
    name_offsets[state->section_count - 2] = name_offset;
    name_offset += strlen(".symtab") + 1;
    name_offsets[state->section_count - 1] = name_offset;
    
    for (size_t i = 0; i < state->section_count; i++) {
        ArkELFSection* sec = &state->sections[i];
        Elf64_Shdr shdr = {0};
        
        shdr.sh_name = (uint32_t)name_offsets[i];
        shdr.sh_type = sec->type;
        shdr.sh_flags = sec->flags;
        shdr.sh_addr = sec->addr;
        shdr.sh_offset = sec->offset;
        shdr.sh_size = sec->size;
        shdr.sh_link = sec->link;
        shdr.sh_info = sec->info;
        shdr.sh_addralign = sec->addralign;
        shdr.sh_entsize = sec->entsize;
        
        if (fwrite(&shdr, sizeof(shdr), 1, fp) != 1) {
            free(name_offsets);
            return ARK_LINK_ERR_IO;
        }
    }
    
    free(name_offsets);
    return ARK_LINK_OK;
}

static ArkLinkResult ark_elf_prepare(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendState** out_state) {
    (void)ctx;
    if (!input || !out_state) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    ArkELFState* state = (ArkELFState*)calloc(1, sizeof(ArkELFState));
    if (!state) {
        return ARK_LINK_ERR_INTERNAL;
    }
    
    state->input = input;
    
    
    ArkLinkResult res = ark_elf_prepare_sections(state);
    if (res != ARK_LINK_OK) {
        ark_elf_free_sections(state);
        free(state);
        return res;
    }
    
    
    ark_elf_compute_layout(state);
    
    *out_state = (ArkBackendState*)state;
    return ARK_LINK_OK;
}

static ArkLinkResult ark_elf_write_output(ArkBackendState* state_ptr, const char* output_path) {
    ArkELFState* state = (ArkELFState*)state_ptr;
    if (!state || !output_path) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    FILE* fp = fopen(output_path, "wb");
    if (!fp) {
        return ARK_LINK_ERR_IO;
    }
    
    ArkLinkResult res = ARK_LINK_OK;
    
    
    res = ark_elf_write_header(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    res = ark_elf_write_program_headers(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    res = ark_elf_write_section_data(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    res = ark_elf_write_section_headers(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        res = ARK_LINK_ERR_IO;
        goto cleanup;
    }
    
cleanup:
    fclose(fp);
    return res;
}

static void ark_elf_destroy_state(ArkBackendState* state_ptr) {
    ArkELFState* state = (ArkELFState*)state_ptr;
    if (!state) {
        return;
    }
    
    ark_elf_free_sections(state);
    free(state->shstrtab);
    free(state->dynstrtab);
    free(state->dynsyms);
    free(state->dynrelas);
    free(state->dynamics);
    free(state);
}

const ArkBackendOps* ark_elf_backend_ops(void) {
    static const ArkBackendOps ops = {
        .target = ARK_LINK_TARGET_ELF,
        .prepare = ark_elf_prepare,
        .write_output = ark_elf_write_output,
        .destroy_state = ark_elf_destroy_state,
    };
    return &ops;
}
