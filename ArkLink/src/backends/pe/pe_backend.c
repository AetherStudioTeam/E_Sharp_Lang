#include "ArkLink/targets/pe.h"
#include "ArkLink/backend.h"
#include "ArkLink/context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define PE_MAGIC_DOS 0x5A4D
#define PE_MAGIC_NT 0x00004550
#define PE_MACHINE_AMD64 0x8664
#define PE_OPTIONAL_HEADER_MAGIC_PE32_PLUS 0x20B

#define PE_IMAGE_FILE_EXECUTABLE_IMAGE 0x0002
#define PE_IMAGE_FILE_LARGE_ADDRESS_AWARE 0x0020

#define PE_IMAGE_SUBSYSTEM_WINDOWS_GUI 2
#define PE_IMAGE_SUBSYSTEM_WINDOWS_CUI 3

#define PE_IMAGE_DLLCHARACTERISTIC_DYNAMIC_BASE 0x0040
#define PE_IMAGE_DLLCHARACTERISTIC_NX_COMPAT 0x0100

#define PE_SCN_CNT_CODE 0x00000020
#define PE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define PE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define PE_SCN_MEM_EXECUTE 0x20000000
#define PE_SCN_MEM_READ 0x40000000
#define PE_SCN_MEM_WRITE 0x80000000
#define PE_SCN_ALIGN_16BYTES 0x00500000

#define PE_REL_BASED_DIR64 10
#define PE_REL_BASED_HIGHLOW 3

#define PE_IMAGE_REL_AMD64_ADDR64 0x0001
#define PE_IMAGE_REL_AMD64_ADDR32 0x0002
#define PE_IMAGE_REL_AMD64_REL32 0x0004






#pragma pack(push, 1)


typedef struct {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
} ArkPEDOSHeader;


typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} ArkPEFileHeader;


typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} ArkPEDataDirectory;


typedef struct {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    ArkPEDataDirectory DataDirectory[16];
} ArkPEOptionalHeader64;


typedef struct {
    char Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} ArkPESectionHeader;


typedef struct {
    uint32_t ImportLookupTableRVA;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t NameRVA;
    uint32_t ImportAddressTableRVA;
} ArkPEImportDirectory;


typedef struct {
    uint32_t PageRVA;
    uint32_t BlockSize;
} ArkPEBaseRelocBlock;

#pragma pack(pop)


_Static_assert(sizeof(ArkPEDOSHeader) == 64, "ArkPEDOSHeader must be 64 bytes");
_Static_assert(sizeof(ArkPEFileHeader) == 20, "ArkPEFileHeader must be 20 bytes");
_Static_assert(sizeof(ArkPEOptionalHeader64) == 240, "ArkPEOptionalHeader64 must be 240 bytes");
_Static_assert(sizeof(ArkPESectionHeader) == 40, "ArkPESectionHeader must be 40 bytes");
_Static_assert(sizeof(ArkPEImportDirectory) == 20, "ArkPEImportDirectory must be 20 bytes");
_Static_assert(sizeof(ArkPEBaseRelocBlock) == 8, "ArkPEBaseRelocBlock must be 8 bytes");


typedef struct {
    char name[8];
    uint8_t* data;
    size_t size;
    size_t raw_size;
    uint32_t virtual_address;
    uint32_t raw_offset;
    uint32_t characteristics;
    uint32_t alignment;
} ArkPESection;


typedef struct {
    char* module_name;
    char** symbols;
    size_t symbol_count;
    uint32_t ilt_rva;
    uint32_t iat_rva;
    uint32_t name_rva;
} ArkPEImportModule;


typedef struct {
    size_t module_index;
    size_t symbol_index;
} ArkPEImportMapping;


typedef struct {
    ArkBackendInput* input;
    ArkPESection* sections;
    size_t section_count;
    ArkPEImportModule* imports;
    ArkPEImportMapping* import_map;
    size_t import_count;
    uint64_t image_base;
    uint32_t entry_point_rva;
    uint32_t entry_section;
    uint32_t iat_rva;
    uint32_t idt_rva;
    uint32_t idt_file_offset;
    uint32_t iat_size;
    uint32_t reloc_rva;
    uint32_t reloc_size;
    uint32_t image_size;
    uint32_t headers_size;
    size_t idata_section_index;
    size_t reloc_section_index;
    uint16_t subsystem;
    uint64_t stack_reserve;
    uint64_t stack_commit;
    uint64_t heap_reserve;
    uint64_t heap_commit;
    int is_dll;
} ArkPEState;

static uint32_t ark_pe_align(uint32_t value, uint32_t alignment) {
    uint32_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

static uint32_t ark_pe_section_characteristics(ArkSectionKind kind, uint32_t flags) {
    uint32_t chars = 0;
    switch (kind) {
    case ARK_SECTION_CODE:
        chars = PE_SCN_CNT_CODE | PE_SCN_MEM_EXECUTE | PE_SCN_MEM_READ;
        break;
    case ARK_SECTION_DATA:
        chars = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
        break;
    case ARK_SECTION_RODATA:
        chars = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ;
        break;
    case ARK_SECTION_BSS:
        chars = PE_SCN_CNT_UNINITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
        break;
    case ARK_SECTION_TLS:
        chars = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
        break;
    }
    return chars;
}

static int ark_pe_collect_imports(ArkPEState* state) {
    
    size_t max_imports = state->input->import_count;
    if (max_imports == 0) {
        state->imports = NULL;
        state->import_map = NULL;
        state->import_count = 0;
        return 0;
    }
    
    
    state->imports = (ArkPEImportModule*)calloc(max_imports, sizeof(ArkPEImportModule));
    if (!state->imports) {
        return -1;
    }

    
    state->import_map = (ArkPEImportMapping*)calloc(max_imports, sizeof(ArkPEImportMapping));
    if (!state->import_map) {
        free(state->imports);
        state->imports = NULL;
        return -1;
    }
    
    
    for (size_t i = 0; i < state->input->import_count; i++) {
        ArkBackendInputImport* imp = &state->input->imports[i];
        
        ArkPEImportModule* mod = NULL;
        size_t mod_idx = 0;
        
        for (size_t j = 0; j < state->import_count; j++) {
            if (strcmp(state->imports[j].module_name, imp->module) == 0) {
                mod = &state->imports[j];
                mod_idx = j;
                break;
            }
        }
        
        if (!mod) {
            mod = &state->imports[state->import_count];
            mod_idx = state->import_count;
            state->import_count++;
            
            mod->module_name = strdup(imp->module);
            mod->symbols = NULL;
            mod->symbol_count = 0;
        }
        
        
        char** new_symbols = (char**)realloc(mod->symbols, (mod->symbol_count + 1) * sizeof(char*));
        if (!new_symbols) {
            return ARK_LINK_ERR_MEMORY;
        }
        mod->symbols = new_symbols;
        mod->symbols[mod->symbol_count] = strdup(imp->symbol);
        if (!mod->symbols[mod->symbol_count]) {
            return ARK_LINK_ERR_MEMORY;
        }
        
        
        state->import_map[i].module_index = mod_idx;
        state->import_map[i].symbol_index = mod->symbol_count;
        
        mod->symbol_count++;
    }
    
    return 0;
}

static void ark_pe_free_imports(ArkPEState* state) {
    for (size_t i = 0; i < state->import_count; i++) {
        ArkPEImportModule* mod = &state->imports[i];
        free(mod->module_name);
        for (size_t j = 0; j < mod->symbol_count; j++) {
            free(mod->symbols[j]);
        }
        free(mod->symbols);
    }
    free(state->imports);
    free(state->import_map);
    state->imports = NULL;
    state->import_map = NULL;
    state->import_count = 0;
}

static ArkLinkResult ark_pe_prepare_sections(ArkPEState* state) {
    
    size_t extra_sections = 0;
    size_t idata_idx = 0, reloc_idx = 0;
    
    if (state->import_count > 0) {
        idata_idx = state->input->section_count + extra_sections;
        extra_sections++;
    }
    if (state->input->reloc_count > 0) {
        reloc_idx = state->input->section_count + extra_sections;
        extra_sections++;
    }
    
    state->section_count = state->input->section_count + extra_sections;
    state->idata_section_index = idata_idx;
    state->reloc_section_index = reloc_idx;
    
    if (state->section_count == 0) {
        state->sections = NULL;
        return ARK_LINK_OK;
    }
    
    state->sections = (ArkPESection*)calloc(state->section_count, sizeof(ArkPESection));
    if (!state->sections) {
        return ARK_LINK_ERR_INTERNAL;
    }
    
    
    for (size_t i = 0; i < state->input->section_count; i++) {
        ArkBackendInputSection* src = &state->input->sections[i];
        ArkPESection* dst = &state->sections[i];
        
        
        size_t name_len = strlen(src->name);
        if (name_len > 8) name_len = 8;
        memcpy(dst->name, src->name, name_len);
        
        dst->size = src->buffer->size;
        dst->raw_size = (uint32_t)ark_pe_align((uint32_t)src->buffer->size, 512);
        dst->characteristics = ark_pe_section_characteristics(src->buffer->kind, src->buffer->flags);
        dst->alignment = src->buffer->alignment > 0 ? src->buffer->alignment : 4096;
        
        if (src->buffer->size > 0) {
            dst->data = (uint8_t*)malloc(src->buffer->size);
            if (!dst->data) {
                return ARK_LINK_ERR_INTERNAL;
            }
            memcpy(dst->data, src->buffer->data, src->buffer->size);
        }
    }
    
    
    if (state->import_count > 0) {
        ArkPESection* idata = &state->sections[idata_idx];
        memcpy(idata->name, ".idata", 6);
        idata->characteristics = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
        idata->alignment = 4096;
        
    }
    
    
    if (state->input->reloc_count > 0) {
        ArkPESection* reloc = &state->sections[reloc_idx];
        memcpy(reloc->name, ".reloc", 6);
        reloc->characteristics = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ;
        reloc->alignment = 4096;
        
    }
    
    return ARK_LINK_OK;
}

static void ark_pe_free_sections(ArkPEState* state) {
    for (size_t i = 0; i < state->section_count; i++) {
        free(state->sections[i].data);
    }
    free(state->sections);
    state->sections = NULL;
}

static uint32_t ark_pe_calc_reloc_size(ArkPEState* state) {
    if (state->input->reloc_count == 0) return 0;
    
    
    uint32_t total_size = 0;
    uint32_t current_page = 0xFFFFFFFF;
    uint32_t entries_in_block = 0;
    
    for (size_t i = 0; i < state->input->reloc_count; i++) {
        ArkBackendInputReloc* reloc = &state->input->relocs[i];
        uint32_t rva = 0;
        if (reloc->section_id < state->input->section_count) {
            rva = state->sections[reloc->section_id].virtual_address + reloc->offset;
        }
        uint32_t page = rva & ~0xFFFu;
        
        if (page != current_page) {
            
            if (current_page != 0xFFFFFFFF) {
                uint32_t block_size = (uint32_t)(sizeof(ArkPEBaseRelocBlock) + entries_in_block * sizeof(uint16_t));
                block_size = ark_pe_align(block_size, 4);
                total_size += block_size;
            }
            current_page = page;
            entries_in_block = 0;
        }
        entries_in_block++;
    }
    
    
    if (entries_in_block > 0) {
        uint32_t block_size = (uint32_t)(sizeof(ArkPEBaseRelocBlock) + entries_in_block * sizeof(uint16_t));
        block_size = ark_pe_align(block_size, 4);
        total_size += block_size;
    }
    
    return total_size;
}

static void ark_pe_compute_layout(ArkPEState* state) {
    const uint32_t section_alignment = 4096;
    const uint32_t file_alignment = 512;
    
    
    size_t dos_header_size = sizeof(ArkPEDOSHeader);
    size_t pe_sig_size = 4;
    size_t file_header_size = sizeof(ArkPEFileHeader);
    size_t optional_header_size = sizeof(ArkPEOptionalHeader64);
    size_t section_headers_size = state->section_count * sizeof(ArkPESectionHeader);
    
    state->headers_size = (uint32_t)ark_pe_align(
        (uint32_t)(dos_header_size + pe_sig_size + file_header_size +
                   optional_header_size + section_headers_size),
        file_alignment
    );
    
    
    uint32_t idt_size = 0;
    uint32_t ilt_size = 0;
    uint32_t iat_size = 0;
    uint32_t hint_names_size = 0;
    uint32_t module_names_size = 0;
    
    if (state->import_count > 0) {
        idt_size = (uint32_t)((state->import_count + 1) * sizeof(ArkPEImportDirectory));
        
        for (size_t i = 0; i < state->import_count; i++) {
            ArkPEImportModule* mod = &state->imports[i];
            ilt_size += (uint32_t)((mod->symbol_count + 1) * sizeof(uint64_t));
            iat_size += (uint32_t)((mod->symbol_count + 1) * sizeof(uint64_t));
            module_names_size += (uint32_t)(strlen(mod->module_name) + 1);
            for (size_t j = 0; j < mod->symbol_count; j++) {
                hint_names_size += 2 + (uint32_t)strlen(mod->symbols[j]) + 1; 
                hint_names_size = ark_pe_align(hint_names_size, 2); 
            }
        }
        
        
        uint32_t import_table_size = idt_size + ilt_size + iat_size + hint_names_size + module_names_size;
        import_table_size = (uint32_t)ark_pe_align(import_table_size, file_alignment);
        
        
        ArkPESection* idata = &state->sections[state->idata_section_index];
        idata->size = import_table_size;
        idata->raw_size = import_table_size;
    }
    
    

    uint32_t current_va = state->headers_size;
    uint32_t current_raw = state->headers_size;
    
    for (size_t i = 0; i < state->section_count; i++) {
        ArkPESection* sec = &state->sections[i];
        
        
        if (state->input->reloc_count > 0 && i == state->reloc_section_index) {
            continue;
        }
        
        
        current_va = ark_pe_align(current_va, section_alignment);
        sec->virtual_address = current_va;
        
        current_va += (uint32_t)(sec->size > 0 ? sec->size : section_alignment);
        
        
        current_raw = ark_pe_align(current_raw, file_alignment);
        sec->raw_offset = current_raw;
        current_raw += sec->raw_size;
    }
    
    
    if (state->import_count > 0) {
        ArkPESection* idata = &state->sections[state->idata_section_index];
        state->idt_rva = idata->virtual_address;
        state->idt_file_offset = idata->raw_offset;
        state->iat_rva = state->idt_rva + idt_size + ilt_size; 
        state->iat_size = iat_size;
    }
    
    
    if (state->input->reloc_count > 0) {
        uint32_t reloc_data_size = ark_pe_calc_reloc_size(state);
        
        ArkPESection* reloc_sec = &state->sections[state->reloc_section_index];
        reloc_sec->size = reloc_data_size;
        reloc_sec->raw_size = ark_pe_align(reloc_data_size, file_alignment);
        
        
        current_va = ark_pe_align(current_va, section_alignment);
        reloc_sec->virtual_address = current_va;
        current_va += reloc_sec->size;
        
        current_raw = ark_pe_align(current_raw, file_alignment);
        reloc_sec->raw_offset = current_raw;
        current_raw += reloc_sec->raw_size;
        
        state->reloc_rva = reloc_sec->virtual_address;
        state->reloc_size = reloc_data_size;
    }
    
    
    state->image_size = ark_pe_align(current_va, section_alignment);
    
    
    
    state->entry_section = state->input->entry_section;
    state->entry_point_rva = 0;
    if (state->entry_section > 0 && state->entry_section <= state->section_count) {
        state->entry_point_rva = state->sections[state->entry_section - 1].virtual_address +
                                  state->input->entry_offset;
    }
}

static ArkLinkResult ark_pe_write_dos_header(FILE* fp) {
    ArkPEDOSHeader dos = {0};
    dos.e_magic = PE_MAGIC_DOS;
    dos.e_lfanew = sizeof(ArkPEDOSHeader);
    
    if (fwrite(&dos, sizeof(dos), 1, fp) != 1) {
        return ARK_LINK_ERR_IO;
    }
    return ARK_LINK_OK;
}

static ArkLinkResult ark_pe_write_nt_headers(ArkPEState* state, FILE* fp) {
    
    uint32_t pe_sig = PE_MAGIC_NT;
    if (fwrite(&pe_sig, sizeof(pe_sig), 1, fp) != 1) {
        return ARK_LINK_ERR_IO;
    }
    
    
    ArkPEFileHeader file_hdr = {0};
    file_hdr.Machine = PE_MACHINE_AMD64;
    file_hdr.NumberOfSections = (uint16_t)state->section_count;
    file_hdr.TimeDateStamp = 0;
    file_hdr.PointerToSymbolTable = 0;
    file_hdr.NumberOfSymbols = 0;
    file_hdr.SizeOfOptionalHeader = sizeof(ArkPEOptionalHeader64);
    file_hdr.Characteristics = PE_IMAGE_FILE_EXECUTABLE_IMAGE | PE_IMAGE_FILE_LARGE_ADDRESS_AWARE;
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] sizeof(ArkPEOptionalHeader64) = %zu\n", sizeof(ArkPEOptionalHeader64));
#endif
    if (state->is_dll) {
        file_hdr.Characteristics |= 0x2000; 
    }
    
    if (fwrite(&file_hdr, sizeof(file_hdr), 1, fp) != 1) {
        return ARK_LINK_ERR_IO;
    }
    
    
    ArkPEOptionalHeader64 opt_hdr = {0};
    opt_hdr.Magic = PE_OPTIONAL_HEADER_MAGIC_PE32_PLUS;
    opt_hdr.MajorLinkerVersion = 1;
    opt_hdr.MinorLinkerVersion = 0;
    opt_hdr.SizeOfCode = 0;
    opt_hdr.SizeOfInitializedData = 0;
    opt_hdr.SizeOfUninitializedData = 0;
    opt_hdr.AddressOfEntryPoint = state->entry_point_rva;
    opt_hdr.BaseOfCode = state->section_count > 0 ? state->sections[0].virtual_address : 0;
    opt_hdr.ImageBase = state->image_base;
    opt_hdr.SectionAlignment = 4096;
    opt_hdr.FileAlignment = 512;
    opt_hdr.MajorOperatingSystemVersion = 6;
    opt_hdr.MinorOperatingSystemVersion = 0;
    opt_hdr.MajorImageVersion = 0;
    opt_hdr.MinorImageVersion = 0;
    opt_hdr.MajorSubsystemVersion = 6;
    opt_hdr.MinorSubsystemVersion = 0;
    opt_hdr.Win32VersionValue = 0;
    opt_hdr.SizeOfImage = state->image_size;
    opt_hdr.SizeOfHeaders = state->headers_size;
    opt_hdr.CheckSum = 0;
    opt_hdr.Subsystem = state->subsystem;
    opt_hdr.DllCharacteristics = PE_IMAGE_DLLCHARACTERISTIC_DYNAMIC_BASE | PE_IMAGE_DLLCHARACTERISTIC_NX_COMPAT;
    opt_hdr.SizeOfStackReserve = state->stack_reserve;
    opt_hdr.SizeOfStackCommit = state->stack_commit;
    opt_hdr.SizeOfHeapReserve = state->heap_reserve;
    opt_hdr.SizeOfHeapCommit = state->heap_commit;
    opt_hdr.LoaderFlags = 0;
    opt_hdr.NumberOfRvaAndSizes = 16;
    
    
    if (state->import_count > 0) {
        opt_hdr.DataDirectory[1].VirtualAddress = state->idt_rva;
        opt_hdr.DataDirectory[1].Size = (uint32_t)((state->import_count + 1) * sizeof(ArkPEImportDirectory));
        
        opt_hdr.DataDirectory[12].VirtualAddress = state->iat_rva;
        opt_hdr.DataDirectory[12].Size = state->iat_size;
    }
    if (state->input->reloc_count > 0) {
        opt_hdr.DataDirectory[5].VirtualAddress = state->reloc_rva;
        opt_hdr.DataDirectory[5].Size = state->reloc_size;
    }
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] opt_hdr address: %p\n", (void*)&opt_hdr);
    fprintf(stderr, "[DEBUG] opt_hdr.Magic = 0x%04X\n", opt_hdr.Magic);
    fprintf(stderr, "[DEBUG] opt_hdr.AddressOfEntryPoint = 0x%08X\n", opt_hdr.AddressOfEntryPoint);
    fprintf(stderr, "[DEBUG] opt_hdr.ImageBase = 0x%016llX\n", (unsigned long long)opt_hdr.ImageBase);
    fprintf(stderr, "[DEBUG] opt_hdr.SectionAlignment = 0x%08X\n", opt_hdr.SectionAlignment);
    fprintf(stderr, "[DEBUG] opt_hdr.FileAlignment = 0x%08X\n", opt_hdr.FileAlignment);
    fprintf(stderr, "[DEBUG] opt_hdr.Subsystem = 0x%04X\n", opt_hdr.Subsystem);
    fprintf(stderr, "[DEBUG] opt_hdr.NumberOfRvaAndSizes = %u\n", opt_hdr.NumberOfRvaAndSizes);
    
    
    fprintf(stderr, "[DEBUG] opt_hdr first 32 bytes: ");
    uint8_t* opt_bytes = (uint8_t*)&opt_hdr;
    for (int i = 0; i < 32; i++) {
        fprintf(stderr, "%02X ", opt_bytes[i]);
    }
    fprintf(stderr, "\n");
    
    long pos_before = ftell(fp);
    fprintf(stderr, "[DEBUG] File position before fwrite: %ld\n", pos_before);
#endif
    
    size_t written = fwrite(&opt_hdr, sizeof(opt_hdr), 1, fp);
    
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] fwrite returned: %zu\n", written);
    
    long pos_after = ftell(fp);
    fprintf(stderr, "[DEBUG] File position after fwrite: %ld\n", pos_after);
#endif
    
    if (written != 1) {
        return ARK_LINK_ERR_IO;
    }
    
    return ARK_LINK_OK;
}

static ArkLinkResult ark_pe_write_section_headers(ArkPEState* state, FILE* fp) {
    for (size_t i = 0; i < state->section_count; i++) {
        ArkPESection* sec = &state->sections[i];
        ArkPESectionHeader hdr = {0};
        
        memcpy(hdr.Name, sec->name, 8);
        
        hdr.VirtualSize = (uint32_t)(sec->size > 0 ? sec->size : 1);
        hdr.VirtualAddress = sec->virtual_address;
        hdr.SizeOfRawData = sec->raw_size;
        hdr.PointerToRawData = sec->raw_offset;
        hdr.PointerToRelocations = 0;
        hdr.PointerToLinenumbers = 0;
        hdr.NumberOfRelocations = 0;
        hdr.NumberOfLinenumbers = 0;
        hdr.Characteristics = sec->characteristics;
        
        if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1) {
            return ARK_LINK_ERR_IO;
        }
    }
    return ARK_LINK_OK;
}


static void ark_pe_apply_relocs(ArkPEState* state, uint8_t* data, size_t size, uint32_t section_rva) {
#ifdef ARKLINK_DEBUG
    fprintf(stderr, "[DEBUG] ark_pe_apply_relocs: reloc_count=%zu, section_rva=0x%X, iat_rva=0x%X\n",
            state->input->reloc_count, section_rva, state->iat_rva);
#endif
    for (size_t i = 0; i < state->input->reloc_count; i++) {
        ArkBackendInputReloc* reloc = &state->input->relocs[i];
        
        
        if (reloc->section_id >= state->input->section_count) {
            continue;
        }
        
        
        if (reloc->offset >= size) {
            continue;
        }
        
        
        uint64_t target_addr = 0;
        
        if (reloc->symbol_index < state->input->symbol_count) {
            ArkBackendInputSymbol* sym = &state->input->symbols[reloc->symbol_index];
#ifdef ARKLINK_DEBUG
            fprintf(stderr, "[DEBUG] Symbol %u: name=%s, section_id=0x%X, import_id=%d\n",
                    reloc->symbol_index, sym->name ? sym->name : "(null)", sym->section_id, sym->import_id);
#endif
            
            if (sym->section_id == 0xFFFFFFFF) {
                
                if (sym->import_id >= 0 && (size_t)sym->import_id < state->input->import_count) {
                    
                    ArkPEImportMapping* map = &state->import_map[sym->import_id];
                    
                    
                    uint32_t iat_entry_offset = 0;
                    for (size_t m = 0; m < map->module_index; m++) {
                        ArkPEImportModule* mod = &state->imports[m];
                        
                        iat_entry_offset += (uint32_t)((mod->symbol_count + 1) * sizeof(uint64_t));
                    }
                    
                    
                    iat_entry_offset += (uint32_t)(map->symbol_index * sizeof(uint64_t));
                    
                    target_addr = state->image_base + state->iat_rva + iat_entry_offset;
                }
            } else if (sym->section_id < state->section_count) {
                
                ArkPESection* sym_sec = &state->sections[sym->section_id];
                target_addr = state->image_base + sym_sec->virtual_address + sym->value;
            }
        }
        
#ifdef ARKLINK_DEBUG
        fprintf(stderr, "[DEBUG] Applying reloc %zu: section_id=%u, offset=%u, type=%d, symbol_index=%u, target_addr=0x%llX\n",
                i, reloc->section_id, reloc->offset, reloc->type, reloc->symbol_index, (unsigned long long)target_addr);
#endif
        
        






        switch (reloc->type) {
            case 0: 
                if (reloc->offset + 8 <= size) {
                    uint64_t value = target_addr + reloc->addend;
                    memcpy(data + reloc->offset, &value, sizeof(value));
                }
                break;
                
            case 1: 
                if (reloc->offset + 4 <= size) {
                    
                    uint32_t next_rip = section_rva + reloc->offset + 4;
                    
                    uint64_t effective_target = target_addr + reloc->addend;
                    int32_t displacement = (int32_t)(effective_target - (state->image_base + next_rip));
#ifdef ARKLINK_DEBUG
                    fprintf(stderr, "[DEBUG] target_addr=0x%llX, addend=%d, effective_target=0x%llX, next_rip=0x%X, displacement=0x%X\n",
                            (unsigned long long)target_addr, reloc->addend, (unsigned long long)effective_target, next_rip, (uint32_t)displacement);
#endif
                    memcpy(data + reloc->offset, &displacement, sizeof(displacement));
                }
                break;
        }
    }
}

static ArkLinkResult ark_pe_write_section_data(ArkPEState* state, FILE* fp) {
    for (size_t i = 0; i < state->section_count; i++) {
        ArkPESection* sec = &state->sections[i];
        
        
        if (fseek(fp, sec->raw_offset, SEEK_SET) != 0) {
            return ARK_LINK_ERR_IO;
        }
        
        
        if (sec->size > 0 && sec->data != NULL) {
            
            uint8_t* data_copy = (uint8_t*)malloc(sec->size);
            if (!data_copy) {
                return ARK_LINK_ERR_INTERNAL;
            }
            memcpy(data_copy, sec->data, sec->size);
            
#ifdef ARKLINK_DEBUG
            
            fprintf(stderr, "[DEBUG] Original data (size=%zu):", sec->size);
            for (size_t j = 0; j < sec->size && j < 32; j++) {
                fprintf(stderr, " %02X", sec->data[j]);
            }
            fprintf(stderr, "\n");
#endif
            
            
            ark_pe_apply_relocs(state, data_copy, sec->size, sec->virtual_address);
            
#ifdef ARKLINK_DEBUG
            
            fprintf(stderr, "[DEBUG] Modified data:");
            for (size_t j = 0; j < sec->size && j < 32; j++) {
                fprintf(stderr, " %02X", data_copy[j]);
            }
            fprintf(stderr, "\n");
#endif
            
            if (fwrite(data_copy, 1, sec->size, fp) != sec->size) {
                free(data_copy);
                return ARK_LINK_ERR_IO;
            }
            free(data_copy);
        }
        
        
        size_t written = (sec->data != NULL) ? sec->size : 0;
        size_t padding = sec->raw_size - written;
        while (padding > 0) {
            uint8_t zero = 0;
            if (fwrite(&zero, 1, 1, fp) != 1) {
                return ARK_LINK_ERR_IO;
            }
            padding--;
        }
    }
    return ARK_LINK_OK;
}

static ArkLinkResult ark_pe_write_import_table(ArkPEState* state, FILE* fp) {
    if (state->import_count == 0) {
        return ARK_LINK_OK;
    }
    
    
    if (fseek(fp, state->idt_file_offset, SEEK_SET) != 0) {
        return ARK_LINK_ERR_IO;
    }
    
    
    uint32_t idt_size = (uint32_t)((state->import_count + 1) * sizeof(ArkPEImportDirectory));
    uint32_t ilt_size = 0;
    uint32_t iat_size = 0;
    
    for (size_t i = 0; i < state->import_count; i++) {
        ArkPEImportModule* mod = &state->imports[i];
        ilt_size += (uint32_t)((mod->symbol_count + 1) * sizeof(uint64_t));
        iat_size += (uint32_t)((mod->symbol_count + 1) * sizeof(uint64_t));
    }
    
    
    uint32_t idt_rva = state->idt_rva;
    uint32_t ilt_rva = idt_rva + idt_size;
    uint32_t iat_rva = ilt_rva + ilt_size;
    uint32_t hint_names_rva = iat_rva + iat_size;
    uint32_t module_names_rva = hint_names_rva;
    
    
    for (size_t i = 0; i < state->import_count; i++) {
        ArkPEImportModule* mod = &state->imports[i];
        for (size_t j = 0; j < mod->symbol_count; j++) {
            module_names_rva += 2 + (uint32_t)strlen(mod->symbols[j]) + 1;
            module_names_rva = ark_pe_align(module_names_rva, 2);
        }
    }
    
    
    uint32_t current_ilt_rva = ilt_rva;
    uint32_t current_iat_rva = iat_rva;
    uint32_t current_module_name_rva = module_names_rva;
    
    for (size_t i = 0; i < state->import_count; i++) {
        ArkPEImportModule* mod = &state->imports[i];
        ArkPEImportDirectory dir = {0};
        dir.ImportLookupTableRVA = current_ilt_rva;
        dir.TimeDateStamp = 0;
        dir.ForwarderChain = 0;
        dir.NameRVA = current_module_name_rva;
        dir.ImportAddressTableRVA = current_iat_rva;
        
        if (fwrite(&dir, sizeof(dir), 1, fp) != 1) {
            return ARK_LINK_ERR_IO;
        }
        
        current_ilt_rva += (uint32_t)((mod->symbol_count + 1) * sizeof(uint64_t));
        current_iat_rva += (uint32_t)((mod->symbol_count + 1) * sizeof(uint64_t));
        current_module_name_rva += (uint32_t)(strlen(mod->module_name) + 1);
    }
    
    
    ArkPEImportDirectory null_dir = {0};
    if (fwrite(&null_dir, sizeof(null_dir), 1, fp) != 1) {
        return ARK_LINK_ERR_IO;
    }
    
    
    uint32_t current_hint_name_rva = hint_names_rva;
    for (size_t i = 0; i < state->import_count; i++) {
        ArkPEImportModule* mod = &state->imports[i];
        
        for (size_t j = 0; j < mod->symbol_count; j++) {
            
            uint64_t entry = current_hint_name_rva;
            if (fwrite(&entry, sizeof(entry), 1, fp) != 1) {
                return ARK_LINK_ERR_IO;
            }
            current_hint_name_rva += 2 + (uint32_t)strlen(mod->symbols[j]) + 1;
            current_hint_name_rva = ark_pe_align(current_hint_name_rva, 2);
        }
        
        
        uint64_t null_entry = 0;
        if (fwrite(&null_entry, sizeof(null_entry), 1, fp) != 1) {
            return ARK_LINK_ERR_IO;
        }
    }
    
    
    current_hint_name_rva = hint_names_rva;
    for (size_t i = 0; i < state->import_count; i++) {
        ArkPEImportModule* mod = &state->imports[i];
        
        for (size_t j = 0; j < mod->symbol_count; j++) {
            
            uint64_t entry = current_hint_name_rva;
            if (fwrite(&entry, sizeof(entry), 1, fp) != 1) {
                return ARK_LINK_ERR_IO;
            }
            current_hint_name_rva += 2 + (uint32_t)strlen(mod->symbols[j]) + 1;
            current_hint_name_rva = ark_pe_align(current_hint_name_rva, 2);
        }
        
        uint64_t null_entry = 0;
        if (fwrite(&null_entry, sizeof(null_entry), 1, fp) != 1) {
            return ARK_LINK_ERR_IO;
        }
    }
    
    
    for (size_t i = 0; i < state->import_count; i++) {
        ArkPEImportModule* mod = &state->imports[i];
        
        for (size_t j = 0; j < mod->symbol_count; j++) {
            uint16_t hint = 0;
            if (fwrite(&hint, sizeof(hint), 1, fp) != 1) {
                return ARK_LINK_ERR_IO;
            }
            if (fwrite(mod->symbols[j], 1, strlen(mod->symbols[j]) + 1, fp) != 
                strlen(mod->symbols[j]) + 1) {
                return ARK_LINK_ERR_IO;
            }
            
            if ((2 + strlen(mod->symbols[j]) + 1) % 2 != 0) {
                uint8_t pad = 0;
                if (fwrite(&pad, 1, 1, fp) != 1) {
                    return ARK_LINK_ERR_IO;
                }
            }
        }
    }
    
    
    for (size_t i = 0; i < state->import_count; i++) {
        ArkPEImportModule* mod = &state->imports[i];
        if (fwrite(mod->module_name, 1, strlen(mod->module_name) + 1, fp) != 
            strlen(mod->module_name) + 1) {
            return ARK_LINK_ERR_IO;
        }
    }
    
    return ARK_LINK_OK;
}

static ArkLinkResult ark_pe_write_relocs(ArkPEState* state, FILE* fp) {
    if (state->input->reloc_count == 0) {
        return ARK_LINK_OK;
    }
    
    
    ArkPESection* reloc_sec = &state->sections[state->reloc_section_index];
    if (fseek(fp, reloc_sec->raw_offset, SEEK_SET) != 0) {
        return ARK_LINK_ERR_IO;
    }
    
    
    uint32_t current_page = 0xFFFFFFFF;
    uint16_t* entries = NULL;
    size_t entry_count = 0;
    size_t entry_capacity = 0;
    
    for (size_t i = 0; i <= state->input->reloc_count; i++) {
        uint32_t page = 0;
        uint32_t offset = 0;
        uint16_t type = 0;
        
        if (i < state->input->reloc_count) {
            ArkBackendInputReloc* reloc = &state->input->relocs[i];
            
            if (reloc->section_id < state->section_count) {
                page = state->sections[reloc->section_id].virtual_address + 
                       (uint32_t)(reloc->offset & ~0xFFF);
                offset = (uint32_t)(reloc->offset & 0xFFF);
            }
            
            




            switch (reloc->type) {
            case 0: 
                type = PE_REL_BASED_DIR64 << 12;
                break;
            case 1: 
                type = PE_REL_BASED_HIGHLOW << 12;
                break;
            default:
                type = PE_REL_BASED_HIGHLOW << 12;
                break;
            }
        }
        
        
        if ((page != current_page && current_page != 0xFFFFFFFF) || i == state->input->reloc_count) {
            if (entry_count > 0) {
                ArkPEBaseRelocBlock block;
                block.PageRVA = current_page;
                block.BlockSize = (uint32_t)(sizeof(ArkPEBaseRelocBlock) + entry_count * sizeof(uint16_t));
                
                if (fwrite(&block, sizeof(block), 1, fp) != 1) {
                    free(entries);
                    return ARK_LINK_ERR_IO;
                }
                if (fwrite(entries, sizeof(uint16_t), entry_count, fp) != entry_count) {
                    free(entries);
                    return ARK_LINK_ERR_IO;
                }
                
                
                size_t padding = (4 - (entry_count * sizeof(uint16_t)) % 4) % 4;
                for (size_t p = 0; p < padding; p++) {
                    uint8_t zero = 0;
                    if (fwrite(&zero, 1, 1, fp) != 1) {
                        free(entries);
                        return ARK_LINK_ERR_IO;
                    }
                }
            }
            entry_count = 0;
        }
        
        if (i < state->input->reloc_count) {
            if (page != current_page) {
                current_page = page;
            }
            
            if (entry_count >= entry_capacity) {
                entry_capacity = entry_capacity ? entry_capacity * 2 : 16;
                entries = (uint16_t*)realloc(entries, entry_capacity * sizeof(uint16_t));
                if (!entries) {
                    return ARK_LINK_ERR_INTERNAL;
                }
            }
            entries[entry_count++] = type | (uint16_t)offset;
        }
    }
    
    free(entries);
    return ARK_LINK_OK;
}

static ArkLinkResult ark_pe_prepare(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendState** out_state) {
    (void)ctx;
    if (!input || !out_state) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    ArkPEState* state = (ArkPEState*)calloc(1, sizeof(ArkPEState));
    if (!state) {
        return ARK_LINK_ERR_INTERNAL;
    }
    
    state->input = input;

    
    const ArkParsedConfig* config = ark_context_config(ctx);
    if (config) {
        state->image_base = config->image_base > 0 ? config->image_base : 0x140000000ULL;
        state->subsystem = (config->subsystem == ARK_SUBSYSTEM_WINDOWS) ? PE_IMAGE_SUBSYSTEM_WINDOWS_GUI : PE_IMAGE_SUBSYSTEM_WINDOWS_CUI;
        state->stack_reserve = config->stack_size > 0 ? config->stack_size : 0x100000;
        state->stack_commit = 0x1000;
        state->heap_reserve = 0x100000;
        state->heap_commit = 0x1000;
        state->is_dll = (config->output_kind == ARK_LINK_OUTPUT_SHARED_LIBRARY);
    } else {
        state->image_base = 0x140000000ULL;
        state->subsystem = PE_IMAGE_SUBSYSTEM_WINDOWS_CUI;
        state->stack_reserve = 0x100000;
        state->stack_commit = 0x1000;
        state->heap_reserve = 0x100000;
        state->heap_commit = 0x1000;
        state->is_dll = 0;
    }
    
    
    if (ark_pe_collect_imports(state) != 0) {
        ark_pe_free_imports(state);
        free(state);
        return ARK_LINK_ERR_INTERNAL;
    }
    
    
    ArkLinkResult res = ark_pe_prepare_sections(state);
    if (res != ARK_LINK_OK) {
        ark_pe_free_imports(state);
        ark_pe_free_sections(state);
        free(state);
        return res;
    }
    
    
    ark_pe_compute_layout(state);
    
    *out_state = (ArkBackendState*)state;
    return ARK_LINK_OK;
}

static ArkLinkResult ark_pe_write_output(ArkBackendState* state_ptr, const char* output_path) {
    ArkPEState* state = (ArkPEState*)state_ptr;
    if (!state || !output_path) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }
    
    FILE* fp = fopen(output_path, "wb");
    if (!fp) {
        return ARK_LINK_ERR_IO;
    }
    
    ArkLinkResult res = ARK_LINK_OK;
    
    
    res = ark_pe_write_dos_header(fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    res = ark_pe_write_nt_headers(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    res = ark_pe_write_section_headers(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    long current_pos = ftell(fp);
    if (current_pos < 0) {
        res = ARK_LINK_ERR_IO;
        goto cleanup;
    }
    while ((size_t)current_pos < state->headers_size) {
        uint8_t zero = 0;
        if (fwrite(&zero, 1, 1, fp) != 1) {
            res = ARK_LINK_ERR_IO;
            goto cleanup;
        }
        current_pos++;
    }
    
    
    res = ark_pe_write_section_data(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    res = ark_pe_write_import_table(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
    
    res = ark_pe_write_relocs(state, fp);
    if (res != ARK_LINK_OK) goto cleanup;
    
cleanup:
    fclose(fp);
    return res;
}

static void ark_pe_destroy_state(ArkBackendState* state_ptr) {
    ArkPEState* state = (ArkPEState*)state_ptr;
    if (!state) {
        return;
    }
    
    ark_pe_free_imports(state);
    ark_pe_free_sections(state);
    free(state);
}

const ArkBackendOps* ark_pe_backend_ops(void) {
    static const ArkBackendOps ops = {
        .target = ARK_LINK_TARGET_PE,
        .prepare = ark_pe_prepare,
        .write_output = ark_pe_write_output,
        .destroy_state = ark_pe_destroy_state,
    };
    return &ops;
}
