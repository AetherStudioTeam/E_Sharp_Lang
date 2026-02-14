#ifndef EO_WRITER_H
#define EO_WRITER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


#define EO_MAGIC    0x4523454Fu
#define EO_VERSION  1

#define EO_ARCH_X86_64  0x8664
#define EO_ARCH_ARM64   0xAA64

#define EO_SEC_COUNT    4

typedef enum {
    EO_SEC_TEXT   = 0,
    EO_SEC_DATA   = 1,
    EO_SEC_RODATA = 2,
    EO_SEC_BSS    = 3,
} EOSectionIndex;

#define EO_SECF_READ    0x01
#define EO_SECF_WRITE   0x02
#define EO_SECF_EXEC    0x04
#define EO_SECF_BSS     0x08

#define EO_SYM_NOTYPE   0
#define EO_SYM_FUNC     1
#define EO_SYM_OBJECT   2

#define EO_BIND_LOCAL   0
#define EO_BIND_GLOBAL  1
#define EO_BIND_WEAK    2

#define EO_RELOC_ABS64  0
#define EO_RELOC_PC32   1
#define EO_RELOC_GOT32  2
#define EO_RELOC_SECREL 3

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint16_t arch;
    uint16_t reserved;
    uint32_t sec_count;
    uint32_t sym_count;
    uint64_t strtab_size;
    uint64_t entry_point;
} EOHeader;


typedef struct __attribute__((packed)) {
    char     name[8];         
    uint8_t  align_log2;      
    uint8_t  flags;           
    uint16_t reserved;        

    uint32_t file_offset;     
    uint32_t file_size;       
    uint32_t mem_size;        
    uint32_t reloc_count;     
    uint32_t reloc_offset;    
} EOSection;


typedef struct __attribute__((packed)) {
    char     name[24];        
    uint64_t value;           
    uint32_t sec_idx;         
    uint8_t  type;            
    uint8_t  bind;            
    uint16_t reserved;        
} EOSymbol;





typedef struct __attribute__((packed)) {
    uint64_t offset;          
    uint32_t sym_idx;         
    uint16_t type;            
    int16_t addend;           
} EORelocation;


typedef struct EOWriter EOWriter;


EOWriter* eo_writer_create(void);
void eo_writer_destroy(EOWriter* writer);


uint32_t eo_write_code(EOWriter* writer, const void* data, uint32_t size);
uint32_t eo_write_data(EOWriter* writer, const void* data, uint32_t size);
uint32_t eo_write_rodata(EOWriter* writer, const void* data, uint32_t size);


uint32_t eo_write_code_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align);
uint32_t eo_write_data_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align);


bool eo_reserve_bss(EOWriter* writer, uint32_t size, uint8_t align_log2);


int eo_add_symbol(EOWriter* writer, const char* name, uint8_t type, uint8_t bind,
                  uint32_t sec_idx, uint64_t value);
int eo_add_undefined_symbol(EOWriter* writer, const char* name);
int eo_find_symbol(EOWriter* writer, const char* name);


void eo_add_reloc(EOWriter* writer, uint32_t sec_idx, uint64_t offset,
                  uint32_t sym_idx, uint16_t type, int16_t addend);


bool eo_set_entry_point(EOWriter* writer, uint64_t offset);


uint32_t eo_get_code_offset(EOWriter* writer);
uint32_t eo_get_data_offset(EOWriter* writer);
uint32_t eo_get_rodata_offset(EOWriter* writer);


bool eo_write_file(EOWriter* writer, const char* filename);

#endif
