#include "vm.h"
#include "object.h"
#include <stdio.h>
#include <stdarg.h>

void es_vm_init(EsVM* vm) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 1024; 
}

void* es_vm_reallocate(EsVM* vm, void* pointer, size_t old_size, size_t new_size) {
    vm->bytes_allocated += new_size - old_size;
    if (new_size > old_size) {
        if (vm->bytes_allocated > vm->next_gc) {
            es_vm_collect_garbage(vm);
        }
    }

    if (new_size == 0) {
        ES_FREE(pointer);
        return NULL;
    }

    void* result = ES_REALLOC(pointer, new_size);
    if (result == NULL) exit(1);
    return result;
}

static void free_object(EsVM* vm, struct EsObject* object) {
    switch (object->type) {
        case OBJ_STRING: {
            EsString* string = (EsString*)object;
            es_vm_reallocate(vm, string->chars, string->length + 1, 0);
            es_vm_reallocate(vm, string, sizeof(EsString), 0);
            break;
        }
    }
}

void es_vm_free(EsVM* vm) {
    struct EsObject* object = vm->objects;
    while (object != NULL) {
        struct EsObject* next = object->next;
        free_object(vm, object);
        object = next;
    }
}

static void mark_object(struct EsObject* object) {
    if (object == NULL || object->is_marked) return;
    object->is_marked = true;
}

static void mark_value(EsValue value) {
    if (IS_OBJ(value)) mark_object((struct EsObject*)AS_OBJ(value));
}

static void mark_roots(EsVM* vm) {
    for (EsValue* slot = vm->stack; slot < vm->stack_top; slot++) {
        mark_value(*slot);
    }
    
    
    if (vm->chunk) {
        for (int i = 0; i < vm->chunk->constants.count; i++) {
            mark_value(vm->chunk->constants.values[i]);
        }
    }
}

static void sweep(EsVM* vm) {
    struct EsObject* previous = NULL;
    struct EsObject* object = vm->objects;
    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            struct EsObject* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm->objects = object;
            }
            free_object(vm, unreached);
        }
    }
}

void es_vm_collect_garbage(EsVM* vm) {
    size_t before = vm->bytes_allocated;
    mark_roots(vm);
    sweep(vm);
    
    vm->next_gc = vm->bytes_allocated * 2;
}

void es_vm_push(EsVM* vm, EsValue value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

EsValue es_vm_pop(EsVM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

static void runtime_error(EsVM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm->ip - vm->chunk->code - 1;
    int line = vm->chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    es_vm_init(vm);
}

static EsInterpretResult run(EsVM* vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_SHORT() \
    (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]))
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(vm->stack_top[-1]) || !IS_NUMBER(vm->stack_top[-2])) { \
            runtime_error(vm, "Operands must be numbers. (Types: %d, %d)", \
                          vm->stack_top[-1].type, vm->stack_top[-2].type); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(es_vm_pop(vm)); \
        double a = AS_NUMBER(es_vm_pop(vm)); \
        es_vm_push(vm, value_type(a op b)); \
    } while (false)

    for (;;) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                EsValue constant = READ_CONSTANT();
                es_vm_push(vm, constant);
                break;
            }
            case OP_NULL:  es_vm_push(vm, NULL_VAL); break;
            case OP_TRUE:  es_vm_push(vm, BOOL_VAL(true)); break;
            case OP_FALSE: es_vm_push(vm, BOOL_VAL(false)); break;
            
            case OP_POP: es_vm_pop(vm); break;

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                EsValue* slots = vm->frames[vm->frame_count - 1].slots;
                es_vm_push(vm, slots[slot]);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                EsValue* slots = vm->frames[vm->frame_count - 1].slots;
                slots[slot] = vm->stack_top[-1];
                break;
            }

            case OP_EQUAL: {
                EsValue b = es_vm_pop(vm);
                EsValue a = es_vm_pop(vm);
                bool eq = false;
                if (a.type == b.type || (IS_STRING_VAL(a) && IS_STRING_VAL(b))) {
                    if (IS_STRING_VAL(a) && IS_STRING_VAL(b)) {
                        const char* s1 = AS_CSTRING(a);
                        const char* s2 = AS_CSTRING(b);
                        int len1 = AS_STRING_LEN(a);
                        int len2 = AS_STRING_LEN(b);
                        eq = (len1 == len2) && (memcmp(s1, s2, len1) == 0);
                    } else {
                        switch (a.type) {
                            case VAL_BOOL:   eq = AS_BOOL(a) == AS_BOOL(b); break;
                            case VAL_NULL:   eq = true; break;
                            case VAL_NUMBER: eq = AS_NUMBER(a) == AS_NUMBER(b); break;
                            default: eq = AS_OBJ(a) == AS_OBJ(b); break;
                        }
                    }
                }
                es_vm_push(vm, BOOL_VAL(eq));
                break;
            }
            
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:    BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING_VAL(vm->stack_top[-1]) && IS_STRING_VAL(vm->stack_top[-2])) {
                    
                    EsValue b_val = vm->stack_top[-1];
                    EsValue a_val = vm->stack_top[-2];
                    
                    const char* b = AS_CSTRING(b_val);
                    const char* a = AS_CSTRING(a_val);
                    int b_len = AS_STRING_LEN(b_val);
                    int a_len = AS_STRING_LEN(a_val);
                    
                    int length = a_len + b_len;
                    char* chars = (char*)es_vm_reallocate(vm, NULL, 0, length + 1);
                    memcpy(chars, a, a_len);
                    memcpy(chars + a_len, b, b_len);
                    chars[length] = '\0';
                    
                    EsString* result = es_object_take_string(vm, chars, length);
                    
                    
                    es_vm_pop(vm);
                    es_vm_pop(vm);
                    
                    es_vm_push(vm, OBJ_VAL(result));
                } else if (IS_NUMBER(vm->stack_top[-1]) && IS_NUMBER(vm->stack_top[-2])) {
                    double b = AS_NUMBER(es_vm_pop(vm));
                    double a = AS_NUMBER(es_vm_pop(vm));
                    es_vm_push(vm, NUMBER_VAL(a + b));
                } else {
                    runtime_error(vm, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUB:     BINARY_OP(NUMBER_VAL, -); break;
            case OP_MUL:     BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIV:     BINARY_OP(NUMBER_VAL, /); break;
            
            case OP_NOT:
                vm->stack_top[-1] = BOOL_VAL(IS_NULL(vm->stack_top[-1]) || (IS_BOOL(vm->stack_top[-1]) && !AS_BOOL(vm->stack_top[-1])));
                break;
                
            case OP_NEGATE:
                if (!IS_NUMBER(vm->stack_top[-1])) {
                    runtime_error(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm->stack_top[-1].as.number = -vm->stack_top[-1].as.number;
                break;

            case OP_PRINT: {
                EsValue val = es_vm_pop(vm);
                es_value_print(val);
                printf("\n");
                break;
            }

            case OP_INT_TO_STRING: {
                EsValue val = es_vm_pop(vm);
                if (!IS_NUMBER(val)) {
                    runtime_error(vm, "Operand for int_to_string must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                char buffer[32];
                int len = snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(val));
                EsString* str = es_object_new_string(vm, buffer, len);
                es_vm_push(vm, OBJ_VAL(str));
                break;
            }

            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                vm->ip += offset;
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (IS_NULL(vm->stack_top[-1]) || (IS_BOOL(vm->stack_top[-1]) && !AS_BOOL(vm->stack_top[-1]))) {
                    vm->ip += offset;
                }
                es_vm_pop(vm); 
                break;
            }

            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                vm->ip -= offset;
                break;
            }

            case OP_CALL: {
                uint8_t arg_count = READ_BYTE();
                int16_t offset = (int16_t)READ_SHORT();
                
            
            if (vm->frame_count >= FRAMES_MAX) {
                runtime_error(vm, "Stack overflow (too many call frames).");
                return INTERPRET_RUNTIME_ERROR;
            }
            
            
            
            
            EsCallFrame* frame = &vm->frames[vm->frame_count++];
            frame->ip = vm->ip;
            frame->slots = vm->stack_top - arg_count;
            
            
            vm->ip += offset;
            break;
            }

            case OP_RETURN: {
                EsValue result = es_vm_pop(vm);
                vm->frame_count--;
                
                
                
                vm->stack_top = vm->frames[vm->frame_count].slots;
                vm->ip = vm->frames[vm->frame_count].ip;
                
                
                es_vm_push(vm, result);
                
                if (vm->frame_count == 0) {
                    
                    
                }
                break;
            }

            case OP_STK_ADJ: {
                uint8_t count = READ_BYTE();
                for (int i = 0; i < count; i++) {
                    es_vm_push(vm, NULL_VAL);
                }
                break;
            }
            
            case OP_HALT:
                return INTERPRET_OK;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

EsInterpretResult es_vm_interpret(EsVM* vm, EsChunk* chunk) {
    vm->chunk = chunk;
    
    
    
    for (int i = 0; i < chunk->constants.count; i++) {
        if (IS_STRING_LIT(chunk->constants.values[i])) {
            char* s = (char*)AS_STRING_LIT(chunk->constants.values[i]);
            int length = (int)strlen(s);
            
            EsString* es_str = es_object_take_string(vm, s, length);
            chunk->constants.values[i] = OBJ_VAL(es_str);
        }
    }
    
    vm->ip = vm->chunk->code;
    return run(vm);
}
