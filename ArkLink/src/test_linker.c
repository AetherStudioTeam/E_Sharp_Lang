

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eo_format.h"
#include "eo_writer.h"
#include "es_linker.h"


bool create_test_eo(const char* filename) {
    EOWriter* writer = eo_writer_create();
    if (!writer) return false;
    
    
    
    
    uint8_t code[] = {0x31, 0xC0, 0xC3};
    uint32_t code_offset = eo_write_code(writer, code, sizeof(code));
    (void)code_offset;
    
    
    const char* msg = "Hello, E#!\n";
    uint32_t data_offset = eo_write_data(writer, msg, strlen(msg) + 1);
    (void)data_offset;
    
    
    eo_add_symbol(writer, "main", EO_SYM_FUNCTION, 
                  EO_SEC_CODE, 0, sizeof(code));
    
    
    eo_add_symbol(writer, "message", EO_SYM_VARIABLE,
                  EO_SEC_DATA, 0, strlen(msg) + 1);
    
    
    writer->entry_point = 0;
    writer->has_entry = true;
    
    
    bool result = eo_write_file(writer, filename);
    
    eo_writer_destroy(writer);
    return result;
}


bool create_test_lib(const char* filename) {
    EOWriter* writer = eo_writer_create();
    if (!writer) return false;
    
    
    
    
    uint8_t code[] = {0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3};
    eo_write_code(writer, code, sizeof(code));
    
    
    eo_add_symbol(writer, "get_answer", EO_SYM_FUNCTION,
                  EO_SEC_CODE, 0, sizeof(code));
    
    bool result = eo_write_file(writer, filename);
    
    eo_writer_destroy(writer);
    return result;
}

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("E# Linker Test\n");
    printf("========================================\n\n");
    
    const char* test_eo = "test_program.eo";
    const char* test_lib = "test_lib.eo";
    const char* output_exe = "test_output.exe";
    
    
    printf("Creating test .eo files...\n");
    
    if (!create_test_eo(test_eo)) {
        fprintf(stderr, "Failed to create %s\n", test_eo);
        return 1;
    }
    printf("  Created: %s\n", test_eo);
    
    if (!create_test_lib(test_lib)) {
        fprintf(stderr, "Failed to create %s\n", test_lib);
        return 1;
    }
    printf("  Created: %s\n", test_lib);
    
    
    printf("\nLinking...\n");
    
    ArkLinkerConfig config = {0};
    config.output_file = output_exe;
    config.is_gui = false;
    config.subsystem = 3;  
    config.preferred_base = 0x140000000ULL;
    
    ArkLinker* linker = es_linker_create(&config);
    if (!linker) {
        fprintf(stderr, "Failed to create linker\n");
        return 1;
    }
    
    if (!es_linker_add_file(linker, test_eo)) {
        fprintf(stderr, "Error adding %s: %s\n", test_eo, es_linker_get_error(linker));
        es_linker_destroy(linker);
        return 1;
    }
    printf("  Added: %s\n", test_eo);
    
    if (!es_linker_add_file(linker, test_lib)) {
        fprintf(stderr, "Error adding %s: %s\n", test_lib, es_linker_get_error(linker));
        es_linker_destroy(linker);
        return 1;
    }
    printf("  Added: %s\n", test_lib);
    
    printf("\nLinker state:\n");
    printf("  Code size: %u bytes\n", linker->code_size);
    printf("  Data size: %u bytes\n", linker->data_size);
    printf("  Rodata size: %u bytes\n", linker->rodata_size);
    printf("  BSS size: %u bytes\n", linker->bss_size);
    printf("  Symbols: %d\n", linker->symbol_count);
    printf("  Relocations: %d\n", linker->reloc_count);
    printf("  Entry point: 0x%X\n", linker->entry_point);
    
    if (!es_linker_link(linker)) {
        fprintf(stderr, "Link failed: %s\n", es_linker_get_error(linker));
        es_linker_destroy(linker);
        return 1;
    }
    
    printf("\n  Output: %s\n", output_exe);
    
    es_linker_destroy(linker);
    
    printf("\n========================================\n");
    printf("Link successful!\n");
    printf("========================================\n");
    
    
    remove(test_eo);
    remove(test_lib);
    
    return 0;
}
