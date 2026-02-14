#include "eo_codegen.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif





#define X86_MOV_R64_IMM64   0x48B8  
#define X86_MOV_R64_R64     0x4889  
#define X86_PUSH_R64        0x50    
#define X86_POP_R64         0x58    
#define X86_RET             0xC3    
#define X86_CALL_REL32      0xE8    
#define X86_JMP_REL32       0xE9    
#define X86_ADD_R64_IMM32   0x4881  
#define X86_SUB_R64_IMM32   0x4881  
#define X86_XOR_R64_R64     0x4831  


#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6
#define REG_RDI 7
#define REG_R8  8
#define REG_R9  9
#define REG_R10 10
#define REG_R11 11
#define REG_R12 12
#define REG_R13 13
#define REG_R14 14
#define REG_R15 15


static void emit_byte(EOCodegenContext* ctx, uint8_t byte) {
    uint8_t data = byte;
    eo_write_code(ctx->writer, &data, 1);
}


static void emit_bytes(EOCodegenContext* ctx, const uint8_t* data, uint32_t size) {
    eo_write_code(ctx->writer, data, size);
}


static void emit_u32(EOCodegenContext* ctx, uint32_t value) {
    eo_write_code(ctx->writer, &value, 4);
}


static void emit_u64(EOCodegenContext* ctx, uint64_t value) {
    eo_write_code(ctx->writer, &value, 8);
}


static void emit_load_imm64_to_reg(EOCodegenContext* ctx, uint64_t value, int reg) {
    if (reg < 8) {
        
        emit_byte(ctx, 0x48);
        emit_byte(ctx, 0xB8 + reg);
        emit_u64(ctx, value);
    } else {
        
        emit_byte(ctx, 0x49);
        emit_byte(ctx, 0xB8 + (reg - 8));
        emit_u64(ctx, value);
    }
}


static void emit_load_string_addr_to_reg(EOCodegenContext* ctx, int32_t sym_idx, int reg) {
    
    
    if (reg < 8) {
        emit_byte(ctx, 0x48);
        emit_byte(ctx, 0xB8 + reg);
    } else {
        emit_byte(ctx, 0x49);
        emit_byte(ctx, 0xB8 + (reg - 8));
    }
    
    uint32_t offset = eo_get_code_offset(ctx->writer);
    emit_u64(ctx, 0); 
    
    
    eo_add_reloc(ctx->writer, EO_SEC_TEXT, offset, sym_idx, EO_RELOC_ABS64, 0);
}


static void emit_function_prologue(EOCodegenContext* ctx, int stack_size) {
    
    emit_byte(ctx, 0x55);
    
    emit_bytes(ctx, (const uint8_t*)"\x48\x89\xE5", 3);
    
    if (stack_size > 0) {
        emit_bytes(ctx, (const uint8_t*)"\x48\x81\xEC", 3);
        emit_u32(ctx, (uint32_t)stack_size);
    }
}


static void emit_function_epilogue(EOCodegenContext* ctx) {
    
    emit_bytes(ctx, (const uint8_t*)"\x48\x89\xEC", 3);
    
    emit_byte(ctx, 0x5D);
    
    emit_byte(ctx, X86_RET);
}


static void emit_load_imm64(EOCodegenContext* ctx, uint64_t value) {
    
    emit_byte(ctx, 0x48);
    emit_byte(ctx, 0xB8);
    emit_u64(ctx, value);
}


static void emit_call_external(EOCodegenContext* ctx, const char* func_name) {
    
    int32_t sym_idx = eo_find_symbol(ctx->writer, func_name);
    if (sym_idx < 0) {
        sym_idx = eo_add_undefined_symbol(ctx->writer, func_name);
    }

    
    uint32_t call_offset = eo_get_code_offset(ctx->writer);
    emit_byte(ctx, X86_CALL_REL32);
    emit_u32(ctx, 0);  

    
    eo_add_reloc(ctx->writer, EO_SEC_TEXT, call_offset + 1, sym_idx, EO_RELOC_PC32, (uint16_t)(-4));
}


static void emit_return(EOCodegenContext* ctx) {
    emit_function_epilogue(ctx);
}


static void emit_binary_op(EOCodegenContext* ctx, int op) {
    
    
    switch (op) {
        case 0: 
            
            emit_bytes(ctx, (const uint8_t*)"\x48\x01\xD8", 3);
            break;
        case 1: 
            
            emit_bytes(ctx, (const uint8_t*)"\x48\x29\xD8", 3);
            break;
        case 2: 
            
            emit_bytes(ctx, (const uint8_t*)"\x48\x0F\xAF\xC3", 4);
            break;
        case 3: 
            
            emit_bytes(ctx, (const uint8_t*)"\x48\x99", 2);  
            emit_bytes(ctx, (const uint8_t*)"\x48\xF7\xFB", 3);  
            break;
    }
}


static void eo_generate_instruction(EOCodegenContext* ctx, EsIRInst* inst) {
    if (!inst) return;

    switch (inst->opcode) {
        case ES_IR_IMM: {
            
            if (inst->operand_count >= 1 && inst->operands[0].type == ES_IR_VALUE_IMM) {
                uint64_t value = (uint64_t)inst->operands[0].data.imm;
                emit_load_imm64(ctx, value);
            }
            break;
        }

        case ES_IR_ADD:
        case ES_IR_SUB:
        case ES_IR_MUL:
        case ES_IR_DIV: {
            
            int op = inst->opcode - ES_IR_ADD;
            emit_binary_op(ctx, op);
            break;
        }

        case ES_IR_CALL: {
            
            if (inst->operand_count >= 1 && inst->operands[0].type == ES_IR_VALUE_FUNCTION) {
                const char* func_name = inst->operands[0].data.function_name;
                
                
                static const int arg_regs[] = { REG_RCX, REG_RDX, REG_R8, REG_R9 };
                int arg_count = inst->operand_count - 1;
                if (arg_count > 4) arg_count = 4; 
                
                for (int i = 0; i < arg_count; i++) {
                    EsIRValue* arg = &inst->operands[i + 1];
                    int target_reg = arg_regs[i];
                    
                    if (arg->type == ES_IR_VALUE_IMM) {
                        emit_load_imm64_to_reg(ctx, (uint64_t)arg->data.imm, target_reg);
                    } else if (arg->type == ES_IR_VALUE_STRING_CONST) {
                        int32_t sym_idx = -1;
                        if (arg->data.string_const_id >= 0) {
                            sym_idx = ctx->string_const_sym_indices[arg->data.string_const_id];
                        } else {
                            
                            sym_idx = eo_find_symbol(ctx->writer, "empty_str");
                            if (sym_idx < 0) {
                                uint32_t off = eo_get_rodata_offset(ctx->writer);
                                eo_write_rodata(ctx->writer, "\0", 1);
                                sym_idx = eo_add_symbol(ctx->writer, "empty_str", EO_SYM_OBJECT, EO_BIND_LOCAL, EO_SEC_RODATA, off);
                            }
                        }
                        emit_load_string_addr_to_reg(ctx, sym_idx, target_reg);
                    }
                    
                }
                
                emit_call_external(ctx, func_name);
            }
            break;
        }

        case ES_IR_RETURN: {
            
            if (inst->operand_count >= 1) {
                
                EsIRValue* ret_val = &inst->operands[0];
                if (ret_val->type == ES_IR_VALUE_IMM) {
                    
                    uint64_t value = (uint64_t)ret_val->data.imm;
                    emit_load_imm64(ctx, value);
                }
                
            }
            emit_function_epilogue(ctx);
            break;
        }

        case ES_IR_STORE: {
            
            
            
            break;
        }

        case ES_IR_LOAD: {
            
            
            break;
        }

        default:
            
            break;
    }
}


static void eo_generate_block(EOCodegenContext* ctx, EsIRBasicBlock* block) {
    if (!block) return;

    EsIRInst* inst = block->first_inst;
    while (inst) {
        eo_generate_instruction(ctx, inst);
        inst = inst->next;
    }
}


static int calculate_function_stack_size(EsIRFunction* func) {
    int stack_size = 32;  

    
    
    stack_size += 64;

    
    stack_size = (stack_size + 15) & ~15;
    if (stack_size < 48) stack_size = 48;

    return stack_size;
}


static void eo_generate_function(EOCodegenContext* ctx, EsIRFunction* func) {
    if (!func) return;

    
    uint32_t func_offset = eo_get_code_offset(ctx->writer);

    
    eo_add_symbol(ctx->writer, func->name, EO_SYM_FUNC, EO_BIND_GLOBAL, EO_SEC_TEXT, func_offset);

    
    if (strcmp(func->name, "main") == 0) {
        eo_set_entry_point(ctx->writer, func_offset);
    }

    
    int stack_size = calculate_function_stack_size(func);

    
    emit_function_prologue(ctx, stack_size);

    
    bool has_explicit_return = false;
    EsIRBasicBlock* block = func->entry_block;
    while (block) {
        
        EsIRInst* inst = block->first_inst;
        while (inst) {
            if (inst->opcode == ES_IR_RETURN) {
                has_explicit_return = true;
            }
            inst = inst->next;
        }
        eo_generate_block(ctx, block);
        block = block->next;
    }

    
    if (!has_explicit_return) {
        
        emit_load_imm64(ctx, 0);
        emit_function_epilogue(ctx);
    }
}


static void eo_generate_data_section(EOCodegenContext* ctx, EsIRModule* module) {
    if (!module) return;

    
    if (module->string_const_count > 0) {
        ctx->string_const_sym_indices = (int32_t*)malloc(module->string_const_count * sizeof(int32_t));
        for (int i = 0; i < module->string_const_count; i++) {
            const char* str = module->string_constants[i];
            uint32_t rodata_offset = eo_get_rodata_offset(ctx->writer);
            
            
            eo_write_rodata(ctx->writer, str, (uint32_t)strlen(str) + 1);
            
            
            char sym_name[32];
            snprintf(sym_name, sizeof(sym_name), "str_const_%d", i);
            ctx->string_const_sym_indices[i] = eo_add_symbol(ctx->writer, sym_name, EO_SYM_OBJECT, EO_BIND_LOCAL, EO_SEC_RODATA, rodata_offset);
        }
    }

    
    
}


void es_eo_generate(FILE* output_file, EsIRModule* module) {
    if (!output_file || !module) return;

    EOCodegenContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.output_file = output_file;
    ctx.writer = eo_writer_create();
    ctx.temp_var_count = 0;
    ctx.label_count = 0;

    if (!ctx.writer) {
        fprintf(stderr, "Error: Failed to create EO writer\n");
        return;
    }

    
    eo_generate_data_section(&ctx, module);

    
    EsIRFunction* func = module->functions;
    while (func) {
        eo_generate_function(&ctx, func);
        func = func->next;
    }

    
    int fd = fileno(output_file);
    char filename[256] = "output.eo";

    #ifdef _WIN32
        
        HANDLE hFile = (HANDLE)_get_osfhandle(fd);
        if (hFile != INVALID_HANDLE_VALUE) {
            wchar_t wpath[MAX_PATH];
            DWORD len = GetFinalPathNameByHandleW(hFile, wpath, MAX_PATH, FILE_NAME_NORMALIZED);
            if (len > 0 && len < MAX_PATH) {
                
                WideCharToMultiByte(CP_UTF8, 0, wpath, -1, filename, sizeof(filename), NULL, NULL);
                
                if (strncmp(filename, "\\\\?\\", 4) == 0) {
                    memmove(filename, filename + 4, strlen(filename) - 3);
                }
            }
        }
    #else
        char fd_path[64];
        snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
        ssize_t len = readlink(fd_path, filename, sizeof(filename) - 1);
        if (len > 0) {
            filename[len] = '\0';
        }
    #endif

    fclose(output_file);

    if (!eo_write_file(ctx.writer, filename)) {
        fprintf(stderr, "Error: Failed to write EO file: %s\n", filename);
    } else {
        printf("Successfully generated EO file: %s\n", filename);
    }

    if (ctx.string_const_sym_indices) {
        free(ctx.string_const_sym_indices);
    }
    eo_writer_destroy(ctx.writer);
}
