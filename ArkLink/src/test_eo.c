#include "eo_writer.h"
#include <stdio.h>
#include <string.h>




int main() {
    printf("Testing EO format writer...\n");
    
    EOWriter* writer = eo_writer_create();
    if (!writer) {
        printf("Failed to create writer\n");
        return 1;
    }
    
    
    uint8_t code[] = {
        0xB8, 0x2A, 0x00, 0x00, 0x00,  
        0xC3                           
    };
    
    uint32_t code_offset = eo_write_code(writer, code, sizeof(code));
    printf("Code written at offset: %u\n", code_offset);
    
    
    const char* hello = "Hello, E# World!\n";
    uint32_t data_offset = eo_write_rodata(writer, hello, (uint32_t)strlen(hello) + 1);
    printf("Rodata written at offset: %u\n", data_offset);
    
    
    uint32_t main_sym = eo_add_symbol(writer, "main", EO_SYM_FUNCTION, 
                                       EO_SEC_CODE, code_offset, sizeof(code));
    printf("Added symbol 'main' at index: %u\n", main_sym);
    
    uint32_t msg_sym = eo_add_symbol(writer, "message", EO_SYM_CONSTANT,
                                      EO_SEC_RODATA, data_offset, (uint32_t)strlen(hello) + 1);
    printf("Added symbol 'message' at index: %u\n", msg_sym);
    
    
    uint32_t printf_sym = eo_add_undefined_symbol(writer, "printf");
    printf("Added undefined symbol 'printf' at index: %u\n", printf_sym);
    
    
    
    eo_add_reloc(writer, EO_SEC_CODE, 10, printf_sym, EO_RELOC_REL32, -4);
    printf("Added relocation\n");
    
    
    eo_set_entry_point(writer, code_offset);
    printf("Entry point set to: %u\n", code_offset);
    
    
    const char* filename = "test.eo";
    if (eo_write_file(writer, filename)) {
        printf("Successfully wrote to %s\n", filename);
        
        
        FILE* fp = fopen(filename, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fclose(fp);
            printf("File size: %ld bytes\n", size);
        }
    } else {
        printf("Failed to write file\n");
    }
    
    eo_writer_destroy(writer);
    printf("Test completed!\n");
    
    return 0;
}
