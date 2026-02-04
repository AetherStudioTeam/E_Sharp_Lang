#ifndef EO_WRITER_H
#define EO_WRITER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>






#define EO_MAGIC 0x424F2345
#define EO_VERSION 2


#define EO_FLAG_HAS_DEBUG    0x0001  
#define EO_FLAG_HAS_TLS      0x0002  
#define EO_FLAG_STRIP_LOCAL  0x0004  
#define EO_FLAG_PIC          0x0008  
#define EO_FLAG_HAS_BSS      0x0010  


enum EOSymbolType {
    EO_SYM_UNDEFINED = 0,   
    EO_SYM_FUNCTION = 1,    
    EO_SYM_VARIABLE = 2,    
    EO_SYM_CONSTANT = 3,    
    EO_SYM_SECTION = 4,     
};


enum EORelocType {
    
    EO_RELOC_ABS32 = 0,     
    EO_RELOC_ABS64 = 1,     

    
    EO_RELOC_REL32 = 2,     
    EO_RELOC_REL64 = 3,     

    
    EO_RELOC_GOTPC32 = 4,   
    EO_RELOC_PLT32 = 5,     

    
    EO_RELOC_SECREL32 = 6,  

    
    EO_RELOC_ADDR32NB = 7,  
};


enum EOSectionType {
    EO_SEC_CODE = 0,        
    EO_SEC_DATA = 1,        
    EO_SEC_RODATA = 2,      
    EO_SEC_BSS = 3,         
    EO_SEC_TLS = 4,         
    EO_SEC_MAX = 5,         
};


#define EO_SECF_EXEC    0x01  
#define EO_SECF_WRITE   0x02  
#define EO_SECF_READ    0x04  
#define EO_SECF_BSS     0x08  
#define EO_SECF_TLS     0x10  


typedef struct __attribute__((packed)) {
    uint32_t file_size;     
    uint32_t mem_size;      
    uint8_t align_log2;     
    uint8_t flags;          
    uint16_t reserved;      
} EOSectionInfo;


typedef struct __attribute__((packed)) {
    uint32_t magic;             
    uint16_t version;           
    uint16_t flags;             

    
    EOSectionInfo sections[EO_SEC_MAX];

    
    uint32_t sym_count;         
    uint32_t reloc_count;       
    uint32_t string_table_size; 

    
    uint32_t entry_point;       
    uint8_t entry_section;      
    uint8_t reserved[3];        
} EOHeader;


typedef struct __attribute__((packed)) {
    uint32_t name_offset;       
    uint32_t type : 8;          
    uint32_t section : 8;       
    uint32_t flags : 16;        
    uint32_t offset;            
    uint32_t size;              
    uint64_t value;             
} EOSymbol;


typedef struct __attribute__((packed)) {
    uint32_t offset;            
    uint32_t symbol_index;      
    uint16_t type;              
    uint8_t section;            
    int8_t addend_shift;        
    int32_t addend;             
} EORelocation;


typedef struct EOWriter EOWriter;


EOWriter* eo_writer_create(void);
void eo_writer_destroy(EOWriter* writer);


uint32_t eo_write_code(EOWriter* writer, const void* data, uint32_t size);
uint32_t eo_write_data(EOWriter* writer, const void* data, uint32_t size);
uint32_t eo_write_rodata(EOWriter* writer, const void* data, uint32_t size);
uint32_t eo_write_tls(EOWriter* writer, const void* data, uint32_t size);


uint32_t eo_write_code_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align);
uint32_t eo_write_data_aligned(EOWriter* writer, const void* data, uint32_t size, uint32_t align);


bool eo_reserve_bss(EOWriter* writer, uint32_t size, uint8_t align_log2);


int eo_add_symbol(EOWriter* writer, const char* name, int type, int section, uint32_t offset, uint32_t size);
int eo_add_undefined_symbol(EOWriter* writer, const char* name);
int eo_find_symbol(EOWriter* writer, const char* name);


void eo_add_reloc(EOWriter* writer, int section, uint32_t offset, int symbol_index, int type, int32_t addend);


bool eo_set_entry_point(EOWriter* writer, uint32_t offset);


uint32_t eo_get_code_offset(EOWriter* writer);
uint32_t eo_get_data_offset(EOWriter* writer);
uint32_t eo_get_rodata_offset(EOWriter* writer);


bool eo_write_file(EOWriter* writer, const char* filename);

#endif 
