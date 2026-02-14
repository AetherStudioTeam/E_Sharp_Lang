#ifndef ARK_COFF_OBJ_H
#define ARK_COFF_OBJ_H

#include "ArkLink/arklink.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define IMAGE_FILE_MACHINE_I386  0x14c
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_FILE_MACHINE_ARM64 0xaa64


#define IMAGE_SYM_CLASS_EXTERNAL 2
#define IMAGE_SYM_CLASS_STATIC   3


#define IMAGE_SYM_UNDEFINED 0
#define IMAGE_SYM_ABSOLUTE  -1
#define IMAGE_SYM_DEBUG     -2


#define IMAGE_REL_AMD64_ADDR64 1
#define IMAGE_REL_AMD64_ADDR32 2
#define IMAGE_REL_AMD64_REL32  4


typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} ArkCOFFFileHeader;


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
} ArkCOFFSectionHeader;


typedef struct {
    union {
        char ShortName[8];
        struct {
            uint32_t Zeroes;
            uint32_t Offset;
        } LongName;
    } Name;
    uint32_t Value;
    int16_t SectionNumber;
    uint16_t Type;
    uint8_t StorageClass;
    uint8_t NumberOfAuxSymbols;
} ArkCOFFSymbol;


typedef struct {
    uint32_t VirtualAddress;
    uint32_t SymbolTableIndex;
    uint16_t Type;
} ArkCOFFRelocation;


typedef struct {
    ArkCOFFFileHeader header;
    ArkCOFFSectionHeader* sections;
    uint8_t** section_data;
    ArkCOFFSymbol* symbols;
    char* string_table;
    uint32_t string_table_size;
    uint8_t* raw_data;
    size_t raw_size;
} ArkCOFFObject;


typedef struct {
    char* name;
    uint32_t value;
    int16_t section;
    uint8_t storage_class;
    bool is_external;
} ArkCOFFSymbolInfo;


typedef struct {
    uint32_t offset;
    char* symbol_name;
    uint16_t type;
    uint16_t section_index;
} ArkCOFFRelocationInfo;




int ark_coff_parse(const uint8_t* data, size_t size, ArkCOFFObject* obj);




void ark_coff_free(ArkCOFFObject* obj);




const char* ark_coff_get_symbol_name(const ArkCOFFObject* obj, const ArkCOFFSymbol* sym);




int ark_coff_extract_symbols(const ArkCOFFObject* obj, ArkCOFFSymbolInfo** symbols, size_t* count);




int ark_coff_extract_relocations(const ArkCOFFObject* obj, ArkCOFFRelocationInfo** relocs, size_t* count);




bool ark_coff_is_valid(const uint8_t* data, size_t size);


struct ArkLinkContext;
typedef struct ArkLinkContext ArkLinkContext;
struct ArkLinkUnit;
typedef struct ArkLinkUnit ArkLinkUnit;




ArkLinkResult ark_coff_load_unit(ArkLinkContext* ctx, const uint8_t* data, size_t size,
                                  const char* path, ArkLinkUnit** out_unit);

#ifdef __cplusplus
}
#endif

#endif 
