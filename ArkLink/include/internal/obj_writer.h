#ifndef ArkLink_OBJ_WRITER_H
#define ArkLink_OBJ_WRITER_H

#include "../ArkLink/object.h"
#include <stdio.h>
#include <stdbool.h>




typedef struct {
    
    uint8_t* code;
    uint8_t* data;
    uint8_t* rodata;
    uint8_t* tls;
    uint32_t code_capacity;
    uint32_t data_capacity;
    uint32_t rodata_capacity;
    uint32_t tls_capacity;
    
    
    uint8_t code_align;
    uint8_t data_align;
    uint8_t rodata_align;
    uint8_t bss_align;
    uint8_t tls_align;
    
    
    EOSymbol* symbols;
    uint32_t sym_capacity;
    uint32_t sym_count;
    
    
    EORelocation* relocs;
    uint32_t reloc_capacity;
    uint32_t reloc_count;
    
    
    char* strings;
    uint32_t string_capacity;
    uint32_t string_size;
    
    
    uint32_t code_offset;
    uint32_t data_offset;
    uint32_t rodata_offset;
    uint32_t tls_offset;
    uint32_t bss_size;          
    uint32_t bss_mem_size;      
    
    
    uint32_t entry_point;
    uint8_t entry_section;
    bool has_entry;
    
    
    uint16_t flags;
} EOWriter;


EOWriter* eo_writer_create(void);
void eo_writer_destroy(EOWriter* writer);


uint32_t eo_write_code(EOWriter* writer, const void* data, uint32_t size);
uint32_t eo_write_data(EOWriter* writer, const void* data, uint32_t size);
uint32_t eo_write_rodata(EOWriter* writer, const void* data, uint32_t size);
uint32_t eo_write_tls(EOWriter* writer, const void* data, uint32_t size);


uint32_t eo_write_code_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align);
uint32_t eo_write_data_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align);


void eo_set_code_align(EOWriter* writer, uint8_t align_log2);
void eo_set_data_align(EOWriter* writer, uint8_t align_log2);
void eo_set_rodata_align(EOWriter* writer, uint8_t align_log2);
void eo_set_bss_align(EOWriter* writer, uint8_t align_log2);


uint32_t eo_add_symbol(EOWriter* writer, const char* name, uint32_t type, 
                       uint32_t section, uint32_t offset, uint32_t size);
uint32_t eo_add_undefined_symbol(EOWriter* writer, const char* name);
int32_t eo_find_symbol(EOWriter* writer, const char* name);


void eo_add_reloc(EOWriter* writer, uint32_t section, uint32_t offset, 
                  uint32_t symbol_index, uint32_t type, int32_t addend);


void eo_set_entry_point(EOWriter* writer, uint32_t offset);
void eo_set_entry_point_ex(EOWriter* writer, uint32_t section, uint32_t offset);


void eo_reserve_bss(EOWriter* writer, uint32_t size);


bool eo_write_file(EOWriter* writer, const char* filename);


uint32_t eo_get_code_offset(EOWriter* writer);
uint32_t eo_get_data_offset(EOWriter* writer);
uint32_t eo_get_rodata_offset(EOWriter* writer);

#endif 
