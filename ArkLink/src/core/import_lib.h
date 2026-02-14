
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    ARK_IMPORT_OK = 0,
    ARK_IMPORT_ERR_IO,
    ARK_IMPORT_ERR_FORMAT,
    ARK_IMPORT_ERR_MEMORY,
    ARK_IMPORT_ERR_NOT_FOUND
} ArkImportResult;


typedef enum {
    ARK_IMPORT_TYPE_CODE = 0,    
    ARK_IMPORT_TYPE_DATA = 1,    
    ARK_IMPORT_TYPE_CONST = 2    
} ArkImportType;


typedef struct {
    char* symbol_name;           
    char* dll_name;              
    ArkImportType type;          
    uint16_t hint;               
    uint16_t ordinal;            
    int by_ordinal;              
} ArkImportEntry;


typedef struct {
    char* filename;              
    ArkImportEntry* entries;     
    size_t entry_count;          
    size_t entry_capacity;       
    char* current_dll_name;      
} ArkImportLib;


ArkImportResult ark_import_lib_open(const char* filename, ArkImportLib** lib);


void ark_import_lib_close(ArkImportLib* lib);


const ArkImportEntry* ark_import_lib_find(const ArkImportLib* lib, const char* symbol_name);


ArkImportResult ark_import_lib_add_entry(ArkImportLib* lib, const char* symbol_name,
                                          const char* dll_name, ArkImportType type);

#ifdef __cplusplus
}
#endif
