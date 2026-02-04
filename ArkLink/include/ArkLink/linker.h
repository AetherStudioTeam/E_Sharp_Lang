


#ifndef ArkLink_LINKER_H
#define ArkLink_LINKER_H

#include <stdbool.h>
#include <stdint.h>
#include "object.h"
#include "internal/mempool.h"


typedef enum {
    ArkLink_OK = 0,                  
    
    
    ArkLink_ERROR_GENERIC = 1,       
    ArkLink_ERROR_NOMEM = 2,         
    ArkLink_ERROR_IO = 3,            
    ArkLink_ERROR_INVALID_PARAM = 4, 
    
    
    ArkLink_ERROR_FILE_NOT_FOUND = 100,      
    ArkLink_ERROR_FILE_OPEN = 101,           
    ArkLink_ERROR_FILE_READ = 102,           
    ArkLink_ERROR_FILE_WRITE = 103,          
    ArkLink_ERROR_FILE_TOO_LARGE = 104,      
    
    
    ArkLink_ERROR_INVALID_MAGIC = 200,       
    ArkLink_ERROR_UNSUPPORTED_VERSION = 201, 
    ArkLink_ERROR_CORRUPTED_FILE = 202,      
    ArkLink_ERROR_INVALID_SECTION = 203,     
    ArkLink_ERROR_INVALID_SYMBOL = 204,      
    ArkLink_ERROR_INVALID_RELOC = 205,       
    
    
    ArkLink_ERROR_UNDEFINED_SYMBOL = 300,    
    ArkLink_ERROR_DUPLICATE_SYMBOL = 301,    
    ArkLink_ERROR_UNRESOLVED_RELOC = 302,    
    ArkLink_ERROR_NO_ENTRY_POINT = 303,      
    ArkLink_ERROR_SECTION_OVERFLOW = 304,    
    
    
    ArkLink_ERROR_UNSUPPORTED_TARGET = 400,  
    ArkLink_ERROR_PLATFORM_SPECIFIC = 401,   
} ArkLinkError;


typedef struct {
    ArkLinkError code;               
    char message[512];              
    char file[256];                 
    int line;                       
    char symbol[128];               
    char context[256];              
} ArkLinkErrorInfo;


typedef enum {
    ArkLink_TARGET_PE,       
    ArkLink_TARGET_ELF,      
    ArkLink_TARGET_MACHO,    
} ArkLinkTarget;


typedef struct {
    
    const char* output_file;
    ArkLinkTarget target;            
    bool is_gui;                    
    uint64_t preferred_base;        
    
    
    const char* runtime_lib;        
    bool no_default_runtime;        
    
    
    uint16_t subsystem;
    uint16_t major_os_version;
    uint16_t minor_os_version;
    
    
    uint8_t code_align_log2;        
    uint8_t data_align_log2;        
    uint32_t section_alignment;     
    uint32_t file_alignment;        
} ArkLinkerConfig;


typedef struct {
    uint8_t* data;
    uint32_t file_size;             
    uint32_t mem_size;              
    uint32_t virtual_address;       
    uint8_t align_log2;             
    uint8_t flags;                  
} ESLibSection;


typedef struct {
    char* name;
    uint32_t type;                  
    uint32_t section;               
    uint32_t offset;                
    uint32_t size;                  
    bool is_defined;                
    bool is_exported;               
} ESLibSymbol;


typedef struct {
    char* symbol_name;              
    uint32_t symbol_index;          
    uint32_t section;               
    uint32_t offset;                
    uint16_t type;                  
    int32_t addend;                 
} ESLibRelocation;


typedef struct {
    char* dll_name;                 
    char* func_name;                
    uint32_t hint;                  
    uint32_t iat_offset;            
} ESImportFunction;


typedef struct {
    char* dll_name;
    ESImportFunction* functions;
    int func_count;
    int func_capacity;
} ESImportDll;


typedef struct {
    
    char** input_files;
    int input_count;
    int input_capacity;
    
    
    ESLibSection sections[EO_SEC_MAX];
    
    
    ESLibSymbol* symbols;
    int symbol_count;
    ObjPool* symbol_pool;           
    
    
    ESLibRelocation* relocs;
    int reloc_count;
    ObjPool* reloc_pool;            
    
    
    ESImportDll* imports;
    int import_count;
    ObjPool* import_pool;           
    ObjPool* import_func_pool;      
    
    
    StringPool* string_pool;        
    
    
    uint32_t entry_point;
    uint8_t entry_section;
    bool has_entry;
    
    
    uint32_t* base_relocs;
    int base_reloc_count;
    Mempool* base_reloc_pool;       
    
    
    ArkLinkerConfig config;
    
    
    struct {
        size_t symbols_allocated;
        size_t symbols_freed;
        size_t relocs_allocated;
        size_t relocs_freed;
        size_t strings_interned;
        size_t string_bytes_saved;  
    } stats;
    
    
    ArkLinkErrorInfo error_info;
    char error_msg[512];            
} ArkLinker;


ArkLinker* es_linker_create(const ArkLinkerConfig* config);
void es_linker_destroy(ArkLinker* linker);


bool es_linker_add_file(ArkLinker* linker, const char* filename);


bool es_linker_link(ArkLinker* linker);


const char* es_linker_get_error(ArkLinker* linker);


bool es_link_files(const char** input_files, int file_count, 
                   const char* output_file, bool is_gui);


bool es_linker_add_import(ArkLinker* linker, const char* dll_name, 
                          const char* func_name, uint32_t hint);
bool es_linker_add_import_by_ordinal(ArkLinker* linker, const char* dll_name,
                                     uint16_t ordinal);


bool es_linker_add_kernel32_imports(ArkLinker* linker);


void es_linker_get_import_sizes(ArkLinker* linker, 
                                 uint32_t* import_dir_size,
                                 uint32_t* iat_size,
                                 uint32_t* string_table_size);


bool es_linker_add_base_reloc(ArkLinker* linker, uint32_t rva);


bool es_linker_write_elf(ArkLinker* linker, const char* filename);


static inline uint32_t es_linker_get_section_align(ArkLinker* linker) {
    return linker ? linker->config.section_alignment : 0x1000;
}

static inline uint32_t es_linker_get_file_align(ArkLinker* linker) {
    return linker ? linker->config.file_alignment : 0x200;
}


void es_linker_set_error(ArkLinker* linker, ArkLinkError code, const char* fmt, ...);
void es_linker_set_error_with_file(ArkLinker* linker, ArkLinkError code, 
                                    const char* filename, const char* fmt, ...);
void es_linker_set_error_with_symbol(ArkLinker* linker, ArkLinkError code,
                                      const char* symbol, const char* fmt, ...);
const char* es_linker_error_string(ArkLinkError code);
ArkLinkError es_linker_get_error_code(ArkLinker* linker);
const ArkLinkErrorInfo* es_linker_get_error_info(ArkLinker* linker);
void es_linker_clear_error(ArkLinker* linker);

#endif 
