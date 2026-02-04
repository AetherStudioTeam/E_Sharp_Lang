


#ifndef ArkLink_PE_H
#define ArkLink_PE_H

#include <stdint.h>

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
} IMAGE_DOS_HEADER;


#define IMAGE_NT_SIGNATURE 0x00004550  


typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} IMAGE_FILE_HEADER;


#define IMAGE_FILE_MACHINE_AMD64 0x8664


#define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b


typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} IMAGE_DATA_DIRECTORY;


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
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;


#pragma pack(pop)


typedef struct {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64;


typedef struct {
    uint8_t Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} IMAGE_SECTION_HEADER;


#define IMAGE_SCN_CNT_CODE              0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA  0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_EXECUTE           0x20000000
#define IMAGE_SCN_MEM_READ              0x40000000
#define IMAGE_SCN_MEM_WRITE             0x80000000


#define IMAGE_SUBSYSTEM_WINDOWS_CUI     3   
#define IMAGE_SUBSYSTEM_WINDOWS_GUI     2   


#define IMAGE_DIRECTORY_ENTRY_IMPORT    1
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5
#define IMAGE_DIRECTORY_ENTRY_TLS       9
#define IMAGE_DIRECTORY_ENTRY_IAT       12


#pragma pack(push, 1)


typedef struct {
    uint32_t ImportLookupTableRVA;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t NameRVA;
    uint32_t ImportAddressTableRVA;
} IMAGE_IMPORT_DESCRIPTOR;


typedef struct {
    uint32_t PageRVA;
    uint32_t BlockSize;
    
} IMAGE_BASE_RELOCATION;


#define IMAGE_REL_BASED_DIR64   10
#define IMAGE_REL_BASED_HIGHLOW 3


typedef struct {
    uint64_t StartAddressOfRawData;     
    uint64_t EndAddressOfRawData;       
    uint64_t AddressOfIndex;            
    uint64_t AddressOfCallBacks;        
    uint32_t SizeOfZeroFill;            
    uint32_t Characteristics;           
} IMAGE_TLS_DIRECTORY64;

#pragma pack(pop)

#endif 
