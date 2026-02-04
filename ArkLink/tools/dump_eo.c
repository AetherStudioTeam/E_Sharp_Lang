#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    uint32_t file_size;     
    uint32_t mem_size;      
    uint8_t align_log2;     
    uint8_t flags;          
    uint16_t reserved;      
} EOSectionInfo;

typedef struct {
    uint32_t magic;             
    uint16_t version;           
    uint16_t flags;             
    EOSectionInfo sections[5];
    uint32_t sym_count;         
    uint32_t reloc_count;       
    uint32_t string_table_size; 
    uint32_t entry_point;       
    uint8_t entry_section;      
    uint8_t reserved[3];        
} EOHeader;

typedef struct {
    uint32_t name_offset;       
    uint8_t type;               
    uint8_t section;            
    uint16_t flags;             
    uint32_t offset;            
    uint32_t size;              
    uint64_t value;             
} EOSymbol;

typedef struct {
    uint32_t offset;            
    uint32_t symbol_index;      
    uint16_t type;              
    uint8_t section;            
    int8_t addend_shift;        
    int32_t addend;             
} EORelocation;
#pragma pack(pop)

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <file.eo>\n", argv[0]);
        return 1;
    }
    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        printf("Cannot open %s\n", argv[1]);
        return 1;
    }
    EOHeader h;
    fread(&h, sizeof(h), 1, fp);
    printf("Magic: %08x, Version: %u, Syms: %u, Relocs: %u\n", h.magic, h.version, h.sym_count, h.reloc_count);
    printf("Entry: Sec %u, Off %u\n", h.entry_section, h.entry_point);
    
    uint32_t code_size = h.sections[0].file_size;
    uint32_t data_size = h.sections[1].file_size;
    uint32_t rodata_size = h.sections[2].file_size;
    uint32_t bss_size = h.sections[3].file_size;
    uint32_t tls_size = h.sections[4].file_size;
    
    uint32_t sym_offset = sizeof(EOHeader) + code_size + data_size + rodata_size + bss_size + tls_size;
    fseek(fp, sym_offset, SEEK_SET);
    EOSymbol* syms = malloc(h.sym_count * sizeof(EOSymbol));
    fread(syms, sizeof(EOSymbol), h.sym_count, fp);
    
    uint32_t reloc_offset = sym_offset + h.sym_count * sizeof(EOSymbol);
    fseek(fp, reloc_offset, SEEK_SET);
    EORelocation* relocs = malloc(h.reloc_count * sizeof(EORelocation));
    fread(relocs, sizeof(EORelocation), h.reloc_count, fp);
    
    uint32_t str_offset = reloc_offset + h.reloc_count * sizeof(EORelocation);
    
    char* strs = malloc(h.string_table_size);
    fseek(fp, str_offset, SEEK_SET);
    fread(strs, 1, h.string_table_size, fp);
    
    printf("\nSymbols:\n");
    for (int i = 0; i < h.sym_count; i++) {
        printf("Sym %d: %s (Type: %u, Sec: %u, Off: %u, Size: %u, Val: %llu)\n", 
               i, strs + syms[i].name_offset, syms[i].type, syms[i].section, 
               syms[i].offset, syms[i].size, (unsigned long long)syms[i].value);
    }
    
    printf("\nRelocations:\n");
    for (int i = 0; i < h.reloc_count; i++) {
        uint32_t s_idx = relocs[i].symbol_index;
        const char* s_name = (s_idx < h.sym_count) ? (strs + syms[s_idx].name_offset) : "INVALID";
        printf("Reloc %d: Off %u, Sym %u (%s), Type %u, Sec %u, Addend %d\n",
               i, relocs[i].offset, s_idx, s_name, relocs[i].type, 
               relocs[i].section, relocs[i].addend);
    }
    
    free(syms);
    free(relocs);
    free(strs);
    fclose(fp);
    return 0;
}
