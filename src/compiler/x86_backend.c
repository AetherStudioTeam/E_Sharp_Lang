#include "x86_backend.h"
#include "ir.h"
#include "../common/es_common.h"
#include "../common/stack_calculator.h"
#include "../common/output_cache.h"
#include <stdio.h>
#include <string.h>

static EsIRModule* g_current_module = NULL;
static char* g_temp_registers[64] = {0};

static EsIRGlobal* find_global_symbol(const char* name) {
    if (!g_current_module || !name) return NULL;
    for (int i = 0; i < g_current_module->global_count; i++) {
        EsIRGlobal* global = &g_current_module->globals[i];
        if (global->name && strcmp(global->name, name) == 0) {
            return global;
        }
    }
    return NULL;
}


static const char* x86_registers[] = {
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};


static int next_register = 0;

static const char* get_next_register(void) {
    if (next_register >= 16) {
        next_register = 0;
    }
    return x86_registers[next_register++];
}


static void init_variable_allocator();
static int get_variable_offset(const char* var_name, int param_count);



static char* escape_string_for_asm(const char* str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    if (len == 0) {

        char* empty = ES_MALLOC(1);
        if (empty) empty[0] = '\0';
        return empty;
    }


    size_t escape_count = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"':
            case '\\':
            case '\n':
            case '\r':
            case '\t':
                escape_count++;
                break;
            default:

                if (str[i] < 32 || str[i] > 126) {
                    escape_count += 3;
                }
                break;
        }
    }


    char* escaped = ES_MALLOC(len + escape_count + 1);
    if (!escaped) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"':
                escaped[j++] = '\\';
                escaped[j++] = '"';
                break;
            case '\\':
                escaped[j++] = '\\';
                escaped[j++] = '\\';
                break;
            case '\n':
                escaped[j++] = '\\';
                escaped[j++] = 'n';
                break;
            case '\r':
                escaped[j++] = '\\';
                escaped[j++] = 'r';
                break;
            case '\t':
                escaped[j++] = '\\';
                escaped[j++] = 't';
                break;
            default:

                if (str[i] >= 32 && str[i] <= 126) {
                    escaped[j++] = str[i];
                } else {


                    escaped[j++] = '\\';
                    escaped[j++] = 'x';

                    unsigned char c = (unsigned char)str[i];
                    unsigned char high = (c >> 4) & 0x0F;
                    escaped[j++] = (high < 10) ? ('0' + high) : ('a' + high - 10);

                    unsigned char low = c & 0x0F;
                    escaped[j++] = (low < 10) ? ('0' + low) : ('a' + low - 10);
                }
                break;
        }
    }
    escaped[j] = '\0';
    return escaped;
}


static int count_local_variables(EsIRFunction* func) {
    int var_count = 0;


    EsIRBasicBlock* block = func->entry_block;
    while (block) {
        EsIRInst* inst = block->first_inst;
        while (inst) {

            for (int i = 0; i < inst->operand_count; i++) {
                EsIRValue* operand = &inst->operands[i];
                if (operand->type == ES_IR_VALUE_VAR) {

                    if (strncmp(operand->data.name, "arg", 3) != 0) {
                        var_count++;
                    }
                }
            }
            inst = inst->next;
        }
        block = block->next;
    }

    return var_count;
}


static int calculate_stack_size(EsIRFunction* func) {

    int local_var_count = count_local_variables(func);
    int stack_size = 0;


    if (local_var_count > 0) {
        stack_size += local_var_count * 8;
    }


    if (func->param_count > 6) {
        stack_size += (func->param_count - 6) * 8;
    }


    if (stack_size < 128) {
        stack_size = 128;
    }

    func->stack_size = stack_size;
    return stack_size;
}


static void generate_function_prologue(FILE* output, EsIRFunction* func) {
    fprintf(output, "\n; Function: %s\n", func->name);
    fprintf(output, "%s:\n", func->name);
    fprintf(output, "    push rbp\n");
    fprintf(output, "    mov rbp, rsp\n");


    init_variable_allocator();

    int stack_size = calculate_stack_size(func);

    if (stack_size % 16 != 0) {
        stack_size = ((stack_size / 16) + 1) * 16;
    }
    func->stack_size = stack_size;
    fprintf(output, "    sub rsp, %d\n", stack_size);

    func->saved_registers_pushed = 0;
    if (func->param_count > 0) {
        fprintf(output, "    push rbx\n");
        fprintf(output, "    push r12\n");
        fprintf(output, "    push r13\n");
        fprintf(output, "    push r14\n");
        fprintf(output, "    push r15\n");
        func->saved_registers_pushed = 1;

        const char* param_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

        for (int i = 0; i < func->param_count && i < 6; i++) {
            int offset = 16 + i * 8;
            fprintf(output, "    mov [rbp + %d], %s\n", offset, param_regs[i]);
        }

        for (int i = 6; i < func->param_count; i++) {
            int incoming_offset = 16 + (i - 6) * 8;
            int local_offset = 16 + i * 8;
            fprintf(output, "    mov rax, [rbp + %d]\n", incoming_offset);
            fprintf(output, "    mov [rbp + %d], rax\n", local_offset);
        }
    }
}


static void generate_function_epilogue(FILE* output, EsIRFunction* func) {
    if (func && func->saved_registers_pushed) {
        fprintf(output, "    pop r15\n");
        fprintf(output, "    pop r14\n");
        fprintf(output, "    pop r13\n");
        fprintf(output, "    pop r12\n");
        fprintf(output, "    pop rbx\n");
    }

    fprintf(output, "    mov rsp, rbp\n");
    fprintf(output, "    pop rbp\n");
    fprintf(output, "    ret\n");
}


static void generate_imm(FILE* output, EsIRValue* value) {
    if (value->type == ES_IR_VALUE_IMM) {
        fprintf(output, "    mov rax, %d\n", (int)value->data.imm);
    } else if (value->type == ES_IR_VALUE_STRING_CONST) {
        fprintf(output, "    lea rax, [rel string_const_%d]\n", value->data.string_const_id);
    }
}



typedef struct {
    char names[100][64];
    int offsets[100];
    int count;
    int next_offset;
} VariableAllocator;

static VariableAllocator g_var_allocator = {0};

static void init_variable_allocator() {
    g_var_allocator.count = 0;
    g_var_allocator.next_offset = 8;
}

static int get_variable_offset(const char* var_name, int param_count) {

    if (strncmp(var_name, "arg", 3) == 0) {
        int arg_index = atoi(var_name + 3);
        if (arg_index >= 0 && arg_index < param_count) {
            return 16 + arg_index * 8;
        }
    }


    for (int i = 0; i < g_var_allocator.count; i++) {
        if (strcmp(g_var_allocator.names[i], var_name) == 0) {
            return g_var_allocator.offsets[i];
        }
    }


    if (g_var_allocator.count < 100) {
        strcpy(g_var_allocator.names[g_var_allocator.count], var_name);
        g_var_allocator.offsets[g_var_allocator.count] = -g_var_allocator.next_offset;
        g_var_allocator.next_offset += 8;
        return g_var_allocator.offsets[g_var_allocator.count++];
    }


    int hash = 0;
    for (const char* p = var_name; *p; p++) {
        hash = hash * 31 + *p;
    }
    return -((abs(hash) % 10 + 1) * 8);
}


static void generate_load_to_reg(FILE* output, const char* var_name, const char* reg) {

    EsIRGlobal* global = find_global_symbol(var_name);
    if (global) {
        fprintf(output, "    mov %s, qword [rel %s]\n", reg, var_name);
        return;
    }

    int offset = get_variable_offset(var_name, 0);

    if (offset >= 0) {
        fprintf(output, "    mov %s, [rbp + %d]\n", reg, offset);
    } else {
        fprintf(output, "    mov %s, [rbp - %d]\n", reg, -offset);
    }
}


static void generate_load(FILE* output, const char* var_name) {
    generate_load_to_reg(output, var_name, "rax");
}


static void generate_store(FILE* output, const char* var_name, EsIRValue* value) {
#ifdef DEBUG
    ES_COMPILER_DEBUG("generate_store called with var_name: %s, value type: %d", var_name, value->type);
#endif
    if (value->type == ES_IR_VALUE_TEMP) {
#ifdef DEBUG
        ES_COMPILER_DEBUG(", temp index: %d", value->data.index);
#endif
    }

    if (value->type == ES_IR_VALUE_IMM) {
        fprintf(output, "    mov rax, %d\n", (int)value->data.imm);
    } else if (value->type == ES_IR_VALUE_STRING_CONST) {
        fprintf(output, "    lea rax, [rel string_const_%d]\n", value->data.string_const_id);
    } else if (value->type == ES_IR_VALUE_VAR) {
        generate_load(output, value->data.name);
    } else if (value->type == ES_IR_VALUE_TEMP) {


#ifdef DEBUG
        ES_COMPILER_DEBUG("Using temp value index %d, assuming it's already in rax", value->data.index);
#endif

    }

    EsIRGlobal* global = find_global_symbol(var_name);
    if (global) {
        fprintf(output, "    mov qword [rel %s], rax\n", var_name);
        return;
    }

    int offset = get_variable_offset(var_name, 0);

    if (offset >= 0) {
        fprintf(output, "    mov [rbp + %d], rax\n", offset);
    } else {
        fprintf(output, "    mov [rbp - %d], rax\n", -offset);
    }
}


static void generate_arithmetic(FILE* output, EsIROpcode opcode, EsIRValue* lhs, EsIRValue* rhs) {
    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
    ES_DEBUG_LOG("BACKEND", "generate_arithmetic called, opcode=%d", opcode);
#endif
#endif


    if (lhs->type == ES_IR_VALUE_IMM) {
        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
        ES_DEBUG_LOG("BACKEND", "Loading immediate lhs: %d", (int)lhs->data.imm);
#endif
#endif
        fprintf(output, "    mov rax, %d\n", (int)lhs->data.imm);
    } else if (lhs->type == ES_IR_VALUE_STRING_CONST) {
        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
        ES_DEBUG_LOG("BACKEND", "Loading string constant lhs: %d", lhs->data.string_const_id);
#endif
#endif
        fprintf(output, "    lea rax, [rel string_const_%d]\n", lhs->data.string_const_id);
    } else if (lhs->type == ES_IR_VALUE_VAR) {
        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
        ES_DEBUG_LOG("BACKEND", "Loading variable lhs: %s", lhs->data.name);
#endif
#endif
        generate_load(output, lhs->data.name);
    }


    const char* reg = get_next_register();
    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
    ES_DEBUG_LOG("BACKEND", "Got register for rhs: %s", reg);
#endif
    #endif


    if (strcmp(reg, "rax") == 0) {
        reg = get_next_register();
        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
        ES_DEBUG_LOG("BACKEND", "Skipping rax, using register for rhs: %s", reg);
#endif
        #endif
    }
    if (rhs->type == ES_IR_VALUE_IMM) {
        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
        ES_DEBUG_LOG("BACKEND", "Loading immediate rhs: %d", (int)rhs->data.imm);
#endif
#endif
        fprintf(output, "    mov %s, %d\n", reg, (int)rhs->data.imm);
    } else if (rhs->type == ES_IR_VALUE_STRING_CONST) {
        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
        ES_DEBUG_LOG("BACKEND", "Loading string constant rhs: %d", rhs->data.string_const_id);
#endif
#endif
        fprintf(output, "    lea %s, [rel string_const_%d]\n", reg, rhs->data.string_const_id);
    } else if (rhs->type == ES_IR_VALUE_VAR) {
        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
        ES_DEBUG_LOG("BACKEND", "Loading variable rhs: %s", rhs->data.name);
#endif
#endif
        generate_load_to_reg(output, rhs->data.name, reg);
    }


    switch (opcode) {
        case ES_IR_ADD:
            fprintf(output, "    add rax, %s\n", reg);
            break;
        case ES_IR_SUB:
            fprintf(output, "    sub rax, %s\n", reg);
            break;
        case ES_IR_MUL:
            fprintf(output, "    imul rax, %s\n", reg);
            break;
        case ES_IR_DIV:
            fprintf(output, "    cqo\n");
            fprintf(output, "    idiv %s\n", reg);
            break;
        default:
            ES_ERROR("不支持的算术运算操作码: %d", opcode);
            break;
    }
}


static void generate_compare(FILE* output, EsIROpcode opcode, EsIRValue* lhs, EsIRValue* rhs) {

    if (lhs->type == ES_IR_VALUE_IMM) {
        fprintf(output, "    mov rax, %d\n", (int)lhs->data.imm);
    } else if (lhs->type == ES_IR_VALUE_STRING_CONST) {
        fprintf(output, "    lea rax, [rel string_const_%d]\n", lhs->data.string_const_id);
    } else if (lhs->type == ES_IR_VALUE_VAR) {
        generate_load(output, lhs->data.name);
    }


    const char* reg = get_next_register();
    if (rhs->type == ES_IR_VALUE_IMM) {
        fprintf(output, "    mov %s, %d\n", reg, (int)rhs->data.imm);
    } else if (rhs->type == ES_IR_VALUE_STRING_CONST) {
        fprintf(output, "    lea %s, [rel string_const_%d]\n", reg, rhs->data.string_const_id);
    } else if (rhs->type == ES_IR_VALUE_VAR) {
        generate_load_to_reg(output, rhs->data.name, reg);
    }


    fprintf(output, "    cmp rax, %s\n", reg);


    switch (opcode) {
        case ES_IR_EQ:
            fprintf(output, "    sete al\n");
            break;
        case ES_IR_NE:
            fprintf(output, "    setne al\n");
            break;
        case ES_IR_LT:
            fprintf(output, "    setl al\n");
            break;
        case ES_IR_LE:
            fprintf(output, "    setle al\n");
            break;
        case ES_IR_GT:
            fprintf(output, "    setg al\n");
            break;
        case ES_IR_GE:
            fprintf(output, "    setge al\n");
            break;
        default:
            ES_ERROR("不支持的比较操作码: %d", opcode);
            break;
    }


    fprintf(output, "    movzx rax, al\n");
}


static void generate_jump(FILE* output, const char* label) {
    fprintf(output, "    jmp %s\n", label);
}


static void generate_branch(FILE* output, EsIRValue* cond, const char* true_label, const char* false_label) {

    fprintf(output, "    test rax, rax\n");
    fprintf(output, "    jnz %s\n", true_label);
    fprintf(output, "    jmp %s\n", false_label);
}


static void generate_call(FILE* output, const char* func_name, int arg_count) {

    #ifdef DEBUG
    #ifdef ES_DEBUG_VERBOSE
    #endif
#endif

#ifdef _WIN32
    #ifdef DEBUG
    #ifdef ES_DEBUG_VERBOSE
    #endif
    #endif
    if (strcmp(func_name, "print_number") == 0) {
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
        fprintf(output, "    call _print_number\n");
    } else if (strcmp(func_name, "print_string") == 0) {
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
        fprintf(output, "    call _print_string\n");
    } else if (strcmp(func_name, "print_int") == 0) {
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
        fprintf(output, "    call _print_int\n");
    } else {
        fprintf(output, "    call %s\n", func_name);
    }
#else
    #ifdef DEBUG
    #ifdef ES_DEBUG_VERBOSE
    #endif
    #endif
    fprintf(output, "    call %s\n", func_name);
#endif
}


static void generate_call_with_args(FILE* output, EsIRValue* operands, int operand_count) {
    #ifdef DEBUG

    ES_COMPILER_DEBUG("generate_call_with_args called with %d operands", operand_count);
#endif
    if (!output) {
        ES_COMPILE_ERROR("generate_call_with_args called with NULL output");
        return;
    }
    if (!operands && operand_count > 0) {
        ES_COMPILE_ERROR("generate_call_with_args called with NULL operands but operand_count > 0");
        return;
    }
    if (operand_count > 0) {
        #ifdef DEBUG
        ES_COMPILER_DEBUG("First operand type: %d", operands[0].type);
        #endif
    }

    if (operand_count < 1) return;


    const char* func_name = NULL;
    if (operand_count > 0 && operands && operands[0].type == ES_IR_VALUE_IMM) {

        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
        func_name = (const char*)(intptr_t)operands[0].data.imm;
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
    } else if (operand_count > 0 && operands && operands[0].type == ES_IR_VALUE_STRING_CONST) {

        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
        if (g_current_module && operands[0].data.string_const_id >= 0 &&
            operands[0].data.string_const_id < g_current_module->string_const_count &&
            g_current_module->string_constants) {
            func_name = g_current_module->string_constants[operands[0].data.string_const_id];
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
            #endif
            #endif
        } else {
            func_name = "";
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
            #endif
            #endif
        }
    } else if (operand_count > 0 && operands && operands[0].type == ES_IR_VALUE_VAR) {

        func_name = operands[0].data.name;
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
    } else if (operand_count > 0 && operands) {
        func_name = operands[0].data.name;
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
    } else {
        func_name = "";
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif
    }

    int arg_count = operand_count - 1;

    #ifdef DEBUG
    #ifdef ES_DEBUG_VERBOSE
    #endif
#endif
    if (strcmp(func_name, "print") == 0 && arg_count >= 1) {
        EsIRValue* arg = &operands[1];
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        #endif
        #endif

        if (arg->type == ES_IR_VALUE_IMM) {
            if (arg->data.imm < 0) {
                func_name = "print_string";
                #ifdef DEBUG
                #ifdef ES_DEBUG_VERBOSE
                #endif
                #endif
            } else {
                func_name = "print_number";
                #ifdef DEBUG
                #ifdef ES_DEBUG_VERBOSE
                #endif
                #endif
            }
        } else if (arg->type == ES_IR_VALUE_VAR) {
            func_name = "print_number";
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif
        } else if (arg->type == ES_IR_VALUE_TEMP) {
            func_name = "print_string";
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif
        }

        fprintf(output, "; print函数 %s\n", func_name);
    }






    if (strcmp(func_name, "print_number") == 0 && arg_count >= 1) {

        EsIRValue* arg = &operands[1];

        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
                #endif
        #endif

        if (arg->type == ES_IR_VALUE_IMM) {
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif

            fprintf(output, "    mov eax, %d\n", (int)arg->data.imm);
            fprintf(output, "    cvtsi2sd xmm0, eax\n");
        } else if (arg->type == ES_IR_VALUE_STRING_CONST) {
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif
            fprintf(output, "    lea rax, [rel string_const_%d]\n", arg->data.string_const_id);
            fprintf(output, "    cvtsi2sd xmm0, rax\n");
        } else if (arg->type == ES_IR_VALUE_VAR) {
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif
            generate_load_to_reg(output, arg->data.name, "rax");
            fprintf(output, "    cvtsi2sd xmm0, rax\n");
        } else if (arg->type == ES_IR_VALUE_TEMP) {
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif

            fprintf(output, "    cvtsi2sd xmm0, rax\n");
        }
    } else if (strcmp(func_name, "print_string") == 0 && arg_count >= 1) {
        EsIRValue* arg = &operands[1];
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
                #endif
        #endif

        if (arg->type == ES_IR_VALUE_IMM) {
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif
            if (arg->data.imm < 0) {
                int str_index = -(int)arg->data.imm - 1;
                fprintf(output, "    lea rcx, [rel string_const_%d]\n", str_index);
            } else {
                fprintf(output, "    mov rcx, %d\n", (int)arg->data.imm);
            }
        } else if (arg->type == ES_IR_VALUE_STRING_CONST) {
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif
            fprintf(output, "    lea rcx, [rel string_const_%d]\n", arg->data.string_const_id);
        } else if (arg->type == ES_IR_VALUE_VAR) {
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif
            generate_load_to_reg(output, arg->data.name, "rcx");
        } else if (arg->type == ES_IR_VALUE_TEMP) {
            #ifdef DEBUG
            #ifdef ES_DEBUG_VERBOSE
                #endif
            #endif
            fprintf(output, "    mov rcx, rax\n");
        }
    } else {

        for (int i = 0; i < arg_count && i < 4; i++) {
            const char* target_reg = NULL;
            switch (i) {
                case 0: target_reg = "rcx"; break;
                case 1: target_reg = "rdx"; break;
                case 2: target_reg = "r8"; break;
                case 3: target_reg = "r9"; break;
            }

            if (target_reg) {

                EsIRValue* arg = &operands[i + 1];

                #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                ES_DEBUG_LOG("BACKEND", "Processing argument %d, type: %d", i, arg->type);
#endif
#endif

                if (arg->type == ES_IR_VALUE_IMM) {
                    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                        #endif
#endif
                    if (arg->data.imm < 0) {
                        int str_index = -(int)arg->data.imm - 1;
                        fprintf(output, "    lea %s, [rel string_const_%d]\n", target_reg, str_index);
                    } else {

                        fprintf(output, "    mov %s, %lld\n", target_reg, (long long)arg->data.imm);
                    }
                } else if (arg->type == ES_IR_VALUE_STRING_CONST) {
                    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                        #endif
#endif
                    fprintf(output, "    lea %s, [rel string_const_%d]\n", target_reg, arg->data.string_const_id);
                } else if (arg->type == ES_IR_VALUE_VAR) {
                    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                        #endif
#endif
                    generate_load_to_reg(output, arg->data.name, target_reg);
                } else if (arg->type == ES_IR_VALUE_TEMP) {
                    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                        #endif
#endif
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Loading temp value index %d to register %s", arg->data.index, target_reg);
#endif
                    fflush(stderr);



                    if (arg->data.index < 64 && g_temp_registers[arg->data.index]) {
                        const char* temp_reg = g_temp_registers[arg->data.index];
                        if (strcmp(temp_reg, target_reg) != 0) {
                            fprintf(output, "    mov %s, %s\n", target_reg, temp_reg);
                        }
                    } else {


#ifdef DEBUG
                        ES_COMPILER_DEBUG("Temp value %d not in register mapping, assuming it's in rax", arg->data.index);
#endif
                        fflush(stderr);
                        if (strcmp("rax", target_reg) != 0) {
                            fprintf(output, "    mov %s, rax\n", target_reg);
                        }
                    }
                }
            }
        }
    }


    generate_call(output, func_name, arg_count);
}


static void generate_return(FILE* output, EsIRFunction* func, EsIRValue* value) {
#ifdef DEBUG
    ES_COMPILER_DEBUG("generate_return called with func=%p, value=%p", func, value);
#endif
    fflush(stderr);

    if (value && value->type != ES_IR_VALUE_VOID) {
#ifdef DEBUG
        ES_COMPILER_DEBUG("Return value type: %d", value->type);
#endif
        if (value->type == ES_IR_VALUE_IMM) {
#ifdef DEBUG
            ES_COMPILER_DEBUG("Returning immediate value: %d", (int)value->data.imm);
#endif
            fprintf(output, "    mov rax, %d\n", (int)value->data.imm);
        } else if (value->type == ES_IR_VALUE_STRING_CONST) {
#ifdef DEBUG
            ES_COMPILER_DEBUG("Returning string constant ID: %d", value->data.string_const_id);
#endif
            fprintf(output, "    lea rax, [rel string_const_%d]\n", value->data.string_const_id);
        } else if (value->type == ES_IR_VALUE_VAR) {
#ifdef DEBUG
            ES_COMPILER_DEBUG("Returning variable: %s", value->data.name);
#endif
            generate_load_to_reg(output, value->data.name, "rax");
        } else {
#ifdef DEBUG
            ES_COMPILER_DEBUG("Unknown return value type: %d", value->type);
#endif
        }
    } else {
#ifdef DEBUG
        ES_COMPILER_DEBUG("No return value or void return");
#endif
    }

#ifdef DEBUG
    ES_COMPILER_DEBUG("Calling generate_function_epilogue");
#endif
    generate_function_epilogue(output, func);
#ifdef DEBUG
    ES_COMPILER_DEBUG("generate_function_epilogue completed");
#endif
}


static void generate_label(FILE* output, const char* label) {
    fprintf(output, "%s:\n", label);
}

static void generate_jump_with_func_prefix(FILE* output, const char* func_name, const char* label) {
    char unique_label[256];
    snprintf(unique_label, sizeof(unique_label), "%s_%s", func_name, label);
    generate_jump(output, unique_label);
}

static void generate_branch_with_func_prefix(FILE* output, const char* func_name, EsIRValue* cond, const char* true_label, const char* false_label) {
    char true_unique_label[256];
    char false_unique_label[256];
    snprintf(true_unique_label, sizeof(true_unique_label), "%s_%s", func_name, true_label);
    snprintf(false_unique_label, sizeof(false_unique_label), "%s_%s", func_name, false_label);
    generate_branch(output, cond, true_unique_label, false_unique_label);
}

static void generate_cast(FILE* output, EsIRValue* value, EsIRValue* target_type) {

    if (value->type == ES_IR_VALUE_IMM) {
        fprintf(output, "    mov rax, %d\n", (int)value->data.imm);
    } else if (value->type == ES_IR_VALUE_VAR) {
        generate_load(output, value->data.name);
    } else if (value->type == ES_IR_VALUE_TEMP) {

    }


    EsTokenType cast_type = (EsTokenType)(int)target_type->data.imm;


    switch (cast_type) {
        case TOKEN_INT32:
        case TOKEN_INT64:

            break;

        case TOKEN_FLOAT32:
        case TOKEN_FLOAT64:

            fprintf(output, "    cvtsi2sd xmm0, rax\n");
            fprintf(output, "    movq rax, xmm0\n");
            break;

        case TOKEN_BOOL:

            fprintf(output, "    test rax, rax\n");
            fprintf(output, "    setne al\n");
            fprintf(output, "    movzx rax, al\n");
            break;

        case TOKEN_UINT32:
        case TOKEN_UINT64:

            break;

        default:

            break;
    }
}

static void generate_strcat(FILE* output, EsIRValue* lhs, EsIRValue* rhs) {
    if (lhs->type == ES_IR_VALUE_IMM) {
        if (lhs->data.imm < 0) {
            int str_index = -(int)lhs->data.imm - 1;
            fprintf(output, "    lea rcx, [rel string_const_%d]\n", str_index);
        } else {
            fprintf(output, "    mov rcx, %.0f\n", lhs->data.imm);
        }
    } else if (lhs->type == ES_IR_VALUE_STRING_CONST) {
        fprintf(output, "    lea rcx, [rel string_const_%d]\n", lhs->data.string_const_id);
    } else if (lhs->type == ES_IR_VALUE_VAR) {
        generate_load_to_reg(output, lhs->data.name, "rcx");
    } else if (lhs->type == ES_IR_VALUE_TEMP) {
        fprintf(output, "    mov rcx, rax\n");
    }

    if (rhs->type == ES_IR_VALUE_IMM) {
        if (rhs->data.imm < 0) {
            int str_index = -(int)rhs->data.imm - 1;
            fprintf(output, "    lea rdx, [rel string_const_%d]\n", str_index);
        } else {
            fprintf(output, "    mov rdx, %.0f\n", rhs->data.imm);
        }
    } else if (rhs->type == ES_IR_VALUE_STRING_CONST) {
        fprintf(output, "    lea rdx, [rel string_const_%d]\n", rhs->data.string_const_id);
    } else if (rhs->type == ES_IR_VALUE_VAR) {
        generate_load_to_reg(output, rhs->data.name, "rdx");
    } else if (rhs->type == ES_IR_VALUE_TEMP) {
        fprintf(output, "    mov rdx, rax\n");
    }

#ifdef _WIN32
    fprintf(output, "    call _es_strcat\n");
#else
    fprintf(output, "    call es_strcat\n");
#endif
}


void es_x86_generate(FILE* output, EsIRModule* module) {
#ifdef DEBUG
    ES_COMPILER_DEBUG("es_x86_generate called with module %p, output %p", module, output);
#endif

    if (!output || !module) return;

    g_current_module = module;

#ifdef DEBUG
    ES_COMPILER_DEBUG("Module has %d string constants", module->string_const_count);
#endif


    if (module->string_const_count > 0 && !module->string_constants) {
        ES_COMPILE_ERROR("Module has string constants but string_constants array is NULL");
        return;
    }


    g_current_module = module;

    bool has_main_function = false;
    EsIRFunction* func = module->functions;
    while (func) {
        if (strcmp(func->name, "main") == 0) {
            has_main_function = true;
            break;
        }
        func = func->next;
    }


    fprintf(output, "section .data\n");
    for (int i = 0; i < module->global_count; i++) {
        EsIRGlobal* global = &module->globals[i];
        long long init_value = global->has_initializer ? (long long)global->init_number : 0;
        fprintf(output, "%s: dq %lld\n", global->name, init_value);
    }

    fprintf(output, "\nsection .rodata\n");

    for (int i = 0; i < module->string_const_count; i++) {

        char* escaped_str = escape_string_for_asm(module->string_constants[i]);
        if (escaped_str) {
            fprintf(output, "string_const_%d: db \"%s\", 0\n", i, escaped_str);
            ES_FREE(escaped_str);
        } else {

            fprintf(output, "string_const_%d: db \"%s\", 0\n", i, module->string_constants[i]);
        }
    }

    int string_count = module->string_const_count;
    func = module->functions;
    while (func) {
        EsIRBasicBlock* block = func->entry_block;
        while (block) {
            EsIRInst* inst = block->first_inst;
            int instruction_count = 0;
            while (inst) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Processing instruction %d, opcode: %d", instruction_count++, inst->opcode);
#endif

#ifdef DEBUG
                    ES_COMPILER_DEBUG("Instruction pointer: %p", inst);
#endif

                    if (!inst) {
                        ES_COMPILE_ERROR("Instruction pointer is NULL");
                        return;
                    }
                if (inst->opcode == ES_IR_CALL && inst->operand_count >= 2) {

                    if (inst->operands[0].type == ES_IR_VALUE_STRING_CONST) {
                        int func_name_id = inst->operands[0].data.string_const_id;
                        if (func_name_id >= 0 && func_name_id < module->string_const_count) {
                            const char* func_name = module->string_constants[func_name_id];
                            if (strcmp(func_name, "print_string") == 0 ||
                                strcmp(func_name, "_print_string") == 0) {
                                if (inst->operands[1].type == ES_IR_VALUE_STRING_CONST) {
                                    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                                #endif
#endif
                                } else if (inst->operands[1].type == ES_IR_VALUE_IMM) {
                                    int64_t val = inst->operands[1].data.imm;
                                    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                                    #endif
#endif
                                    if (val != 0 && val != -2147483648) {
                                        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                                        #endif
#endif

                                        char* escaped_str = escape_string_for_asm((char*)val);
                                        if (escaped_str) {
                                            fprintf(output, "string_const_%d: db \"%s\", 0\n", string_count, escaped_str);
                                            ES_FREE(escaped_str);
                                        } else {

                                            fprintf(output, "string_const_%d: db \"%s\", 0\n", string_count, (char*)val);
                                        }
                                        inst->operands[1].data.imm = -string_count - 1;
                                        string_count++;
                                    } else {
                                        #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                                        #endif
#endif
                                    }
                                } else {
                                    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                                #endif
#endif
                                }
                            }
                        }
                    }
                } else if (inst->opcode == ES_IR_STRCAT && inst->operand_count >= 2) {
                    for (int i = 0; i < 2; i++) {
                        if (inst->operands[i].type == ES_IR_VALUE_STRING_CONST) {
                            #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                            #endif
#endif
                        } else if (inst->operands[i].type == ES_IR_VALUE_IMM) {
                            int64_t val = inst->operands[i].data.imm;
                            #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                            #endif
#endif
                            if (val != 0 && val != -2147483648) {
                                #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                                #endif
#endif

                                char* escaped_str = escape_string_for_asm((char*)val);
                                if (escaped_str) {
                                    fprintf(output, "string_const_%d: db \"%s\", 0\n", string_count, escaped_str);
                                    ES_FREE(escaped_str);
                                } else {

                                    fprintf(output, "string_const_%d: db \"%s\", 0\n", string_count, (char*)val);
                                }
                                inst->operands[i].data.imm = -string_count - 1;
                                string_count++;
                            } else {
                                #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                                #endif
#endif
                            }
                        }
                    }
                }
                inst = inst->next;
            }
            block = block->next;
        }
        func = func->next;
    }
    #ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
    #endif
#endif

    fprintf(output, "\nsection .text\n");


    fprintf(output, "global main\n\n");


    fprintf(output, "extern printf\n");
    fprintf(output, "extern exit\n");
#ifdef _WIN32
    fprintf(output, "extern _print_number\n");
    fprintf(output, "extern _print_int\n");
    fprintf(output, "extern _print_string\n");
    fprintf(output, "extern _es_strcat\n");
    fprintf(output, "extern _es_int_to_string\n");
    fprintf(output, "extern _es_int_to_string_value\n");
    fprintf(output, "extern es_int_to_string_value\n\n");
#else
    fprintf(output, "extern print_number\n");
    fprintf(output, "extern print_int\n");
    fprintf(output, "extern print_string\n");
    fprintf(output, "extern es_strcat\n");
    fprintf(output, "extern es_int_to_string\n\n");
#endif

    fprintf(output, "extern es_malloc\n");
    fprintf(output, "extern es_free\n\n");


    func = module->functions;
    while (func) {

        if (func->entry_block == NULL) {
            func = func->next;
            continue;
        }


        for (int i = 0; i < 64; i++) {
            g_temp_registers[i] = NULL;
        }

        generate_function_prologue(output, func);


        bool has_return = false;
        EsIRBasicBlock* block = func->entry_block;
        while (block) {
#ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
            #endif
#endif

            char unique_label[256];
            snprintf(unique_label, sizeof(unique_label), "%s_%s", func->name, block->label);
            generate_label(output, unique_label);

            EsIRInst* inst = block->first_inst;
            while (inst) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Processing instruction opcode: %d", inst->opcode);
#endif
                switch (inst->opcode) {
                    case ES_IR_LOAD:
                        if (inst->operand_count >= 1) {
                            generate_load(output, inst->operands[0].data.name);

                            if (inst->result.type == ES_IR_VALUE_TEMP) {
                                g_temp_registers[inst->result.data.index] = "rax";
#ifdef DEBUG
                                ES_COMPILER_DEBUG("Set temp[%d] = rax", inst->result.data.index);
#endif
                            }
                        }
                        break;
                    case ES_IR_STORE:
                        if (inst->operand_count >= 2) {
#ifdef DEBUG
                            ES_COMPILER_DEBUG("ES_IR_STORE - storing to variable: %s, value type: %d",
                                    inst->operands[0].data.name, inst->operands[1].type);
#endif
                            if (inst->operands[1].type == ES_IR_VALUE_TEMP) {
#ifdef DEBUG
                                ES_COMPILER_DEBUG("Temp value index: %d", inst->operands[1].data.index);
#endif
                            }
#ifdef DEBUG
                            ES_COMPILER_DEBUG("About to call generate_store with var_name: %s, value type: %d",
                                    inst->operands[0].data.name, inst->operands[1].type);
#endif
                            generate_store(output, inst->operands[0].data.name, &inst->operands[1]);
                        }
                        break;
                    case ES_IR_ADD:
                    case ES_IR_SUB:
                    case ES_IR_MUL:
                    case ES_IR_DIV:
                        if (inst->operand_count >= 2) {
                            generate_arithmetic(output, inst->opcode, &inst->operands[0], &inst->operands[1]);
                        }
                        break;
                    case ES_IR_LT:
                    case ES_IR_GT:
                    case ES_IR_EQ:
                    case ES_IR_LE:
                    case ES_IR_GE:
                    case ES_IR_NE:
                        if (inst->operand_count >= 2) {
                            generate_compare(output, inst->opcode, &inst->operands[0], &inst->operands[1]);
                        }
                        break;
                    case ES_IR_JUMP:
                        if (inst->operand_count >= 1) {
#ifdef ES_DEBUG_VERBOSE
#ifdef DEBUG
                            #endif
#endif
                            generate_jump_with_func_prefix(output, func->name, inst->operands[0].data.name);
                        }
                        break;
                    case ES_IR_BRANCH:
                        if (inst->operand_count >= 3) {
                            generate_branch_with_func_prefix(output, func->name, &inst->operands[0], inst->operands[1].data.name, inst->operands[2].data.name);
                        }
                        break;
                    case ES_IR_CALL:
#ifdef DEBUG
                        ES_COMPILER_DEBUG("Processing ES_IR_CALL instruction");
#endif

                        if (!inst) {
                            ES_COMPILE_ERROR("ES_IR_CALL instruction pointer is NULL");
                            return;
                        }
#ifdef DEBUG
                        ES_COMPILER_DEBUG("Instruction pointer is valid: %p", inst);
#endif

#ifdef DEBUG
                        ES_COMPILER_DEBUG("Instruction has %d operands", inst->operand_count);
#endif

                        if (inst->operand_count > 0) {
#ifdef DEBUG
                            ES_COMPILER_DEBUG("Checking operands pointer");
#endif

                            if (!inst->operands) {
                                ES_COMPILE_ERROR("ES_IR_CALL instruction has operands but operands pointer is NULL");
                                return;
                            }
#ifdef DEBUG
                            ES_COMPILER_DEBUG("Operands pointer is valid: %p", inst->operands);
#endif

#ifdef DEBUG
                            ES_COMPILER_DEBUG("First operand type: %d", inst->operands[0].type);
#endif

                            if (inst->operands[0].type == ES_IR_VALUE_STRING_CONST) {
#ifdef DEBUG
                                ES_COMPILER_DEBUG("String const id: %d", inst->operands[0].data.string_const_id);
#endif

                                if (module && module->string_constants && inst->operands[0].data.string_const_id >= 0 && inst->operands[0].data.string_const_id < module->string_const_count) {
#ifdef DEBUG
                                    ES_COMPILER_DEBUG("String const value: %s", module->string_constants[inst->operands[0].data.string_const_id]);
#endif
                                } else {
                                    ES_COMPILE_ERROR("Invalid string constant access - module: %p, string_constants: %p, id: %d, count: %d",
                                            module, module ? module->string_constants : NULL, inst->operands[0].data.string_const_id, module ? module->string_const_count : -1);
                                    return;
                                }
                            }
                        }

#ifdef DEBUG
                        ES_COMPILER_DEBUG("About to call generate_call_with_args");
#endif

                        if (inst->operand_count >= 1) {
                            generate_call_with_args(output, inst->operands, inst->operand_count);
                        }

#ifdef DEBUG
                        ES_COMPILER_DEBUG("generate_call_with_args completed");
#endif


                        if (inst->result.type == ES_IR_VALUE_TEMP) {
                            g_temp_registers[inst->result.data.index] = "rax";
#ifdef DEBUG
                            ES_COMPILER_DEBUG("CALL set temp[%d] = rax", inst->result.data.index);
#endif
                        }
                        break;
                    case ES_IR_STRCAT:
                        if (inst->operand_count >= 2) {
                            generate_strcat(output, &inst->operands[0], &inst->operands[1]);
                        }
                        break;
                    case ES_IR_CAST:
                        if (inst->operand_count >= 2) {
                            generate_cast(output, &inst->operands[0], &inst->operands[1]);
                        }
                        break;
                    case ES_IR_LOADPTR:
                        if (inst->operand_count >= 2) {


                            if (inst->operands[0].type == ES_IR_VALUE_VAR) {
                                generate_load_to_reg(output, inst->operands[0].data.name, "r13");
                            } else if (inst->operands[0].type == ES_IR_VALUE_ARG) {
                                fprintf(output, "    mov r13, [rbp + %d]\n", 16 + inst->operands[0].data.index * 8);
                            } else if (inst->operands[0].type == ES_IR_VALUE_TEMP) {

                            if (inst->operands[0].data.index < 64 && g_temp_registers[inst->operands[0].data.index]) {
                                fprintf(output, "    mov r13, %s\n", g_temp_registers[inst->operands[0].data.index]);
                            } else {
                                ES_COMPILER_DEBUG("ES_IR_LOADPTR: Temp value %d not in register mapping, assuming it's in rax", inst->operands[0].data.index);
                                fprintf(output, "    mov r13, rax\n");
                            }
                            }


                            if (inst->operands[1].type == ES_IR_VALUE_IMM) {
                                fprintf(output, "    mov rcx, %d\n", (int)inst->operands[1].data.imm);
                            } else if (inst->operands[1].type == ES_IR_VALUE_VAR) {
                                generate_load_to_reg(output, inst->operands[1].data.name, "rcx");
                            } else if (inst->operands[1].type == ES_IR_VALUE_TEMP) {

                            if (inst->operands[1].data.index < 64 && g_temp_registers[inst->operands[1].data.index]) {
                                if (strcmp(g_temp_registers[inst->operands[1].data.index], "rcx") != 0) {
                                    fprintf(output, "    mov rcx, %s\n", g_temp_registers[inst->operands[1].data.index]);
                                }
                            } else {
                                ES_COMPILER_DEBUG("ES_IR_LOADPTR: Temp value %d not in register mapping, assuming it's in rax", inst->operands[1].data.index);
                                fprintf(output, "    mov rcx, rax\n");
                            }
                            }


                            fprintf(output, "    mov rdx, rcx\n");
                            fprintf(output, "    shl rdx, 3\n");
                            fprintf(output, "    add rdx, r13\n");


                            fprintf(output, "    mov rax, [rdx]\n");
                        }
                        break;
                    case ES_IR_STOREPTR:
                        if (inst->operand_count >= 3) {


                            if (inst->operands[0].type == ES_IR_VALUE_VAR) {
                                generate_load_to_reg(output, inst->operands[0].data.name, "r13");
                            } else if (inst->operands[0].type == ES_IR_VALUE_ARG) {
                                fprintf(output, "    mov r13, [rbp + %d]\n", 16 + inst->operands[0].data.index * 8);
                            } else if (inst->operands[0].type == ES_IR_VALUE_TEMP) {

                            if (inst->operands[0].data.index < 64 && g_temp_registers[inst->operands[0].data.index]) {
                                fprintf(output, "    mov r13, %s\n", g_temp_registers[inst->operands[0].data.index]);
                            } else {
                                ES_COMPILER_DEBUG("ES_IR_STOREPTR: Temp value %d not in register mapping, assuming it's in rax", inst->operands[0].data.index);
                                fprintf(output, "    mov r13, rax\n");
                            }
                            }
                            EsIRValue* val = &inst->operands[2];
                            if (val->type == ES_IR_VALUE_IMM) {
                                fprintf(output, "    mov rax, %d\n", (int)val->data.imm);
                            } else if (val->type == ES_IR_VALUE_VAR) {
                                generate_load_to_reg(output, val->data.name, "rax");
                            } else if (val->type == ES_IR_VALUE_TEMP) {

                                if (val->data.index < 16 && g_temp_registers[val->data.index]) {
                                    if (strcmp(g_temp_registers[val->data.index], "rax") != 0) {
                                        fprintf(output, "    mov rax, %s\n", g_temp_registers[val->data.index]);
                                    }
                                } else {
                                    ES_COMPILER_DEBUG("ES_IR_STOREPTR: Temp value %d not in register mapping, assuming it's in rax", val->data.index);

                                }
                            }
                            int offset = (int)inst->operands[1].data.imm;
                            fprintf(output, "    mov [r13 + %d], rax\n", offset);
                        }
                        break;
                    case ES_IR_ARRAY_STORE:
                        if (inst->operand_count >= 3) {


                            if (inst->operands[0].type == ES_IR_VALUE_VAR) {
                                generate_load_to_reg(output, inst->operands[0].data.name, "r13");
                            } else if (inst->operands[0].type == ES_IR_VALUE_ARG) {
                                fprintf(output, "    mov r13, [rbp + %d]\n", 16 + inst->operands[0].data.index * 8);
                            } else if (inst->operands[0].type == ES_IR_VALUE_TEMP) {

                            if (inst->operands[0].data.index < 64 && g_temp_registers[inst->operands[0].data.index]) {
                                fprintf(output, "    mov r13, %s\n", g_temp_registers[inst->operands[0].data.index]);
                            } else {
                                ES_COMPILER_DEBUG("ES_IR_ARRAY_STORE: Temp value %d not in register mapping, assuming it's in rax", inst->operands[0].data.index);
                                fprintf(output, "    mov r13, rax\n");
                            }
                            }


                            if (inst->operands[1].type == ES_IR_VALUE_IMM) {
                                fprintf(output, "    mov rcx, %d\n", (int)inst->operands[1].data.imm);
                            } else if (inst->operands[1].type == ES_IR_VALUE_VAR) {
                                generate_load_to_reg(output, inst->operands[1].data.name, "rcx");
                            } else if (inst->operands[1].type == ES_IR_VALUE_TEMP) {

                            if (inst->operands[1].data.index < 64 && g_temp_registers[inst->operands[1].data.index]) {
                                if (strcmp(g_temp_registers[inst->operands[1].data.index], "rcx") != 0) {
                                    fprintf(output, "    mov rcx, %s\n", g_temp_registers[inst->operands[1].data.index]);
                                }
                            } else {
                                ES_COMPILER_DEBUG("ES_IR_ARRAY_STORE: Temp value %d not in register mapping, assuming it's in rax", inst->operands[1].data.index);
                                fprintf(output, "    mov rcx, rax\n");
                            }
                            }


                            EsIRValue* val = &inst->operands[2];
                            if (val->type == ES_IR_VALUE_IMM) {
                                fprintf(output, "    mov rax, %d\n", (int)val->data.imm);
                            } else if (val->type == ES_IR_VALUE_VAR) {
                                generate_load_to_reg(output, val->data.name, "rax");
                            } else if (val->type == ES_IR_VALUE_TEMP) {

                            }


                            fprintf(output, "    mov rdx, rcx\n");
                            fprintf(output, "    shl rdx, 3\n");
                            fprintf(output, "    add rdx, r13\n");


                            fprintf(output, "    mov [rdx], rax\n");




                            fprintf(output, "    mov rax, r13\n");


                            if (inst->result.type != ES_IR_VALUE_VOID) {
#ifdef DEBUG
                                ES_COMPILER_DEBUG("ES_IR_ARRAY_STORE has unexpected result type: %d", inst->result.type);
#endif
                            }
                        }
                        break;
                    case ES_IR_RETURN:
#ifdef DEBUG
                        ES_COMPILER_DEBUG("Processing ES_IR_RETURN instruction");
#endif


                        if (!inst) {
                            ES_COMPILE_ERROR("ES_IR_RETURN instruction pointer is NULL");
                            return;
                        }

#ifdef DEBUG
                        ES_COMPILER_DEBUG("About to access instruction data");
#endif


                        int operand_count = 0;
                        if (inst) {
                            operand_count = inst->operand_count;
                        }

#ifdef DEBUG
                        ES_COMPILER_DEBUG("ES_IR_RETURN has %d operands", operand_count);
#endif

                        if (operand_count >= 1) {
#ifdef DEBUG
                            ES_COMPILER_DEBUG("Calling generate_return with value");
#endif
                            if (inst && inst->operands) {
                                generate_return(output, func, &inst->operands[0]);
                            } else {
                                ES_COMPILE_ERROR("Invalid operands pointer");
                                generate_return(output, func, NULL);
                            }
                        } else {
#ifdef DEBUG
                            ES_COMPILER_DEBUG("Calling generate_return without value");
#endif
                            generate_return(output, func, NULL);
                        }

#ifdef DEBUG
                        ES_COMPILER_DEBUG("ES_IR_RETURN completed");
#endif
                        has_return = true;
                        break;
                    case ES_IR_LABEL:

                        break;
                    case ES_IR_ALLOC:
                        if (inst->operand_count >= 1) {
#ifdef DEBUG
                            ES_COMPILER_DEBUG("ES_IR_ALLOC for variable: %s", inst->operands[0].data.name);
#endif

                            fprintf(output, "    sub rsp, 8\n");

                            fprintf(output, "    mov qword [rsp], 0\n");
                        }
                        break;
                    case ES_IR_NOP:
                        break;
                    default:
                        ES_ERROR("不支持的IR指令操作码: %d", inst->opcode);
                        break;
                }

#ifdef DEBUG
                ES_COMPILER_DEBUG("About to advance to next instruction");
#endif

                if (inst) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Current inst: %p, next: %p", inst, inst->next);
#endif
                }

                inst = inst->next;

#ifdef DEBUG
                ES_COMPILER_DEBUG("Advanced to next instruction: %p", inst);
#endif
            }

#ifdef DEBUG
            ES_COMPILER_DEBUG("About to advance to next block");
#endif

            if (block) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Current block: %p, next: %p", block, block->next);
#endif
            }

            block = block->next;

#ifdef DEBUG
            ES_COMPILER_DEBUG("Advanced to next block: %p", block);
#endif
        }

#ifdef DEBUG
        ES_COMPILER_DEBUG("Finished processing blocks for function");
#endif
#ifdef DEBUG
        ES_COMPILER_DEBUG("has_return = %d", has_return);
#endif

        if (!has_return) {
#ifdef DEBUG
            ES_COMPILER_DEBUG("No return found, adding default return");
#endif
            fprintf(output, "    mov rax, 0\n");
            generate_function_epilogue(output, func);
        }

#ifdef DEBUG
        ES_COMPILER_DEBUG("About to advance to next function");
#endif

        if (func) {
#ifdef DEBUG
            ES_COMPILER_DEBUG("Current func: %p, next: %p", func, func->next);
#endif
        }

        func = func->next;

#ifdef DEBUG
        ES_COMPILER_DEBUG("Advanced to next function: %p", func);
#endif
    }



    if (!has_main_function) {
#ifdef DEBUG
        ES_COMPILER_DEBUG("About to generate default main function");
#endif
        fprintf(output, "main:\n");
        fprintf(output, "    call _start\n");
        fprintf(output, "    mov rax, 0\n");
        fprintf(output, "    ret\n");
#ifdef DEBUG
        ES_COMPILER_DEBUG("Default main function generated");
#endif
    }

#ifdef DEBUG
    ES_COMPILER_DEBUG("About to reset g_current_module");
#endif

    g_current_module = NULL;

#ifdef DEBUG
    ES_COMPILER_DEBUG("g_current_module reset completed");
#endif

#ifdef DEBUG
    ES_COMPILER_DEBUG("es_x86_generate completed successfully");
#endif
}