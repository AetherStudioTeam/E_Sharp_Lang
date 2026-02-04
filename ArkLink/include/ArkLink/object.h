#ifndef ArkLink_OBJECT_H
#define ArkLink_OBJECT_H

#include <stdint.h>






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
    uint8_t type;               
    uint8_t section;            
    uint16_t flags;             
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











#define EO_HEADER_SIZE 88  
#define EO_SYMBOL_SIZE 24  
#define EO_RELOC_SIZE 16


#define EO_CODE_OFFSET(header) (EO_HEADER_SIZE)
#define EO_DATA_OFFSET(header) (EO_CODE_OFFSET(header) + (header)->sections[EO_SEC_CODE].file_size)
#define EO_RODATA_OFFSET(header) (EO_DATA_OFFSET(header) + (header)->sections[EO_SEC_DATA].file_size)
#define EO_SYMTAB_OFFSET(header) (EO_RODATA_OFFSET(header) + (header)->sections[EO_SEC_RODATA].file_size)
#define EO_RELOCTAB_OFFSET(header) ((EO_SYMTAB_OFFSET(header)) + (header)->sym_count * EO_SYMBOL_SIZE)
#define EO_STRTAB_OFFSET(header) ((EO_RELOCTAB_OFFSET(header)) + (header)->reloc_count * EO_RELOC_SIZE)
#define EO_FILE_SIZE(header) (EO_STRTAB_OFFSET(header) + (header)->string_table_size)


#define EO_ALIGN_UP(value, align) (((value) + (align) - 1) & ~((align) - 1))
#define EO_ALIGN_UP_LOG2(value, log2) EO_ALIGN_UP((value), (1U << (log2)))


#define EO_DEFAULT_CODE_ALIGN 4   
#define EO_DEFAULT_DATA_ALIGN 3   
#define EO_DEFAULT_BSS_ALIGN  3   

#endif 
