#ifndef ARK_ARCHIVE_H
#define ARK_ARCHIVE_H

#include "ArkLink/arklink.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    ARK_ARCHIVE_UNKNOWN,
    ARK_ARCHIVE_COFF_IMPORT,  
    ARK_ARCHIVE_COFF_STATIC,  
    ARK_ARCHIVE_ELF_STATIC    
} ArkArchiveType;


typedef struct {
    char* name;           
    uint8_t* data;        
    size_t size;          
    size_t offset;        
} ArkArchiveObject;


typedef struct ArkArchive {
    ArkArchiveType type;
    ArkArchiveObject* objects;
    size_t object_count;
    size_t object_capacity;
    char* filename;
} ArkArchive;


typedef enum {
    ARK_ARCHIVE_OK = 0,
    ARK_ARCHIVE_ERR_IO = -1,
    ARK_ARCHIVE_ERR_FORMAT = -2,
    ARK_ARCHIVE_ERR_MEMORY = -3,
    ARK_ARCHIVE_ERR_UNSUPPORTED = -4
} ArkArchiveResult;




ArkArchiveResult ark_archive_open(const char* filename, ArkArchive** archive);




void ark_archive_close(ArkArchive* archive);




ArkArchiveType ark_archive_detect_type(const char* filename);




bool ark_archive_is_archive(const char* filename);

#ifdef __cplusplus
}
#endif

#endif 
