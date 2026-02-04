


#ifndef ArkLink_ELF_H
#define ArkLink_ELF_H

#include <stdint.h>


#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'
#define ELFMAG          "\177ELF"
#define SELFMAG         4


#define ELFCLASSNONE    0
#define ELFCLASS32      1
#define ELFCLASS64      2


#define ELFDATANONE     0
#define ELFDATA2LSB     1       
#define ELFDATA2MSB     2       


#define ELFOSABI_NONE   0       
#define ELFOSABI_LINUX  3       


#define ET_NONE         0       
#define ET_REL          1       
#define ET_EXEC         2       
#define ET_DYN          3       
#define ET_CORE         4       


#define EM_NONE         0
#define EM_386          3       
#define EM_X86_64       62      


#define EV_NONE         0
#define EV_CURRENT      1


#define PT_NULL         0       
#define PT_LOAD         1       
#define PT_DYNAMIC      2       
#define PT_INTERP       3       
#define PT_NOTE         4       
#define PT_SHLIB        5       
#define PT_PHDR         6       
#define PT_TLS          7       


#define PT_GNU_EH_FRAME 0x6474E550  
#define PT_GNU_STACK    0x6474E551  
#define PT_GNU_RELRO    0x6474E552  


#define PF_X            0x1     
#define PF_W            0x2     
#define PF_R            0x4     


#define SHT_NULL        0       
#define SHT_PROGBITS    1       
#define SHT_SYMTAB      2       
#define SHT_STRTAB      3       
#define SHT_RELA        4       
#define SHT_HASH        5       
#define SHT_DYNAMIC     6       
#define SHT_NOTE        7       
#define SHT_NOBITS      8       
#define SHT_REL         9       
#define SHT_SHLIB       10      
#define SHT_DYNSYM      11      


#define SHF_WRITE       0x1     
#define SHF_ALLOC       0x2     
#define SHF_EXECINSTR   0x4     
#define SHF_TLS         0x400   


#define DT_NULL         0       
#define DT_NEEDED       1       
#define DT_PLTRELSZ     2       
#define DT_PLTGOT       3       
#define DT_HASH         4       
#define DT_STRTAB       5       
#define DT_SYMTAB       6       
#define DT_RELA         7       
#define DT_RELASZ       8       
#define DT_RELAENT      9       
#define DT_STRSZ        10      
#define DT_SYMENT       11      
#define DT_INIT         12      
#define DT_FINI         13      
#define DT_SONAME       14      
#define DT_RPATH        15      
#define DT_SYMBOLIC     16      
#define DT_REL          17      
#define DT_RELSZ        18      
#define DT_RELENT       19      
#define DT_PLTREL       20      
#define DT_DEBUG        21      
#define DT_TEXTREL      22      
#define DT_JMPREL       23      
#define DT_BIND_NOW     24      
#define DT_INIT_ARRAY   25      
#define DT_FINI_ARRAY   26      
#define DT_INIT_ARRAYSZ 27      
#define DT_FINI_ARRAYSZ 28      
#define DT_RUNPATH      29      
#define DT_FLAGS        30      
#define DT_GNU_HASH     0x6ffffef5  


#define STB_LOCAL       0       
#define STB_GLOBAL      1       
#define STB_WEAK        2       


#define STT_NOTYPE      0       
#define STT_OBJECT      1       
#define STT_FUNC        2       
#define STT_SECTION     3       
#define STT_FILE        4       
#define STT_TLS         6       


#define R_X86_64_NONE     0     
#define R_X86_64_64       1     
#define R_X86_64_PC32     2     
#define R_X86_64_GOT32    3     
#define R_X86_64_PLT32    4     
#define R_X86_64_COPY     5     
#define R_X86_64_GLOB_DAT 6     
#define R_X86_64_JUMP_SLOT 7    
#define R_X86_64_RELATIVE 8     
#define R_X86_64_GOTPCREL 9     
#define R_X86_64_32       10    
#define R_X86_64_32S      11    
#define R_X86_64_16       12    
#define R_X86_64_PC16     13    
#define R_X86_64_8        14    
#define R_X86_64_PC8      15    

#pragma pack(push, 1)


typedef struct {
    uint8_t  e_ident[16];       
    uint16_t e_type;            
    uint16_t e_machine;         
    uint32_t e_version;         
    uint64_t e_entry;           
    uint64_t e_phoff;           
    uint64_t e_shoff;           
    uint32_t e_flags;           
    uint16_t e_ehsize;          
    uint16_t e_phentsize;       
    uint16_t e_phnum;           
    uint16_t e_shentsize;       
    uint16_t e_shnum;           
    uint16_t e_shstrndx;        
} Elf64_Ehdr;


typedef struct {
    uint32_t p_type;            
    uint32_t p_flags;           
    uint64_t p_offset;          
    uint64_t p_vaddr;           
    uint64_t p_paddr;           
    uint64_t p_filesz;          
    uint64_t p_memsz;           
    uint64_t p_align;           
} Elf64_Phdr;


typedef struct {
    uint32_t sh_name;           
    uint32_t sh_type;           
    uint64_t sh_flags;          
    uint64_t sh_addr;           
    uint64_t sh_offset;         
    uint64_t sh_size;           
    uint32_t sh_link;           
    uint32_t sh_info;           
    uint64_t sh_addralign;      
    uint64_t sh_entsize;        
} Elf64_Shdr;


typedef struct {
    uint32_t st_name;           
    uint8_t  st_info;           
    uint8_t  st_other;          
    uint16_t st_shndx;          
    uint64_t st_value;          
    uint64_t st_size;           
} Elf64_Sym;


typedef struct {
    uint64_t r_offset;          
    uint64_t r_info;            
} Elf64_Rel;


typedef struct {
    uint64_t r_offset;          
    uint64_t r_info;            
    int64_t  r_addend;          
} Elf64_Rela;


typedef struct {
    int64_t  d_tag;             
    union {
        uint64_t d_val;         
        uint64_t d_ptr;         
    } d_un;
} Elf64_Dyn;

#pragma pack(pop)


#define ELF64_ST_BIND(info)     ((info) >> 4)
#define ELF64_ST_TYPE(info)     ((info) & 0xf)
#define ELF64_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))

#define ELF64_R_SYM(info)       ((info) >> 32)
#define ELF64_R_TYPE(info)      ((uint32_t)(info))
#define ELF64_R_INFO(sym, type) (((uint64_t)(sym) << 32) + (uint32_t)(type))


#define ELF_DEFAULT_BASE        0x400000ULL

#endif 
