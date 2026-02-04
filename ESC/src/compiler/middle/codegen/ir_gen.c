#define _POSIX_C_SOURCE 200809L
#include <stddef.h>


extern size_t strlen(const char *s);
extern int strcmp(const char *s1, const char *s2);
extern char *strstr(const char *haystack, const char *needle);

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include "../ir/ir.h"
#include "../../compiler.h"
#include "../../../core/utils/output_cache.h"
#include "../../../core/utils/es_common.h"
#include "type_checker.h"

static EsIRValue es_ir_generate_expression(EsIRBuilder* builder, ASTNode* expr);
static void es_ir_generate_statement(EsIRBuilder* builder, ASTNode* stmt);
static void es_ir_generate_block(EsIRBuilder* builder, ASTNode* block);
static int es_ir_check_has_return(ASTNode* node);
static void es_ir_ensure_main_entry(EsIRBuilder* builder);
static void es_ir_push_class_context(EsIRBuilder* builder, const char* class_name);
static void es_ir_pop_class_context(EsIRBuilder* builder);
static const char* es_ir_current_class_context(EsIRBuilder* builder);
static char* es_ir_mangle_static_member(const char* class_name, const char* member_name);
static int es_ir_evaluate_numeric_constant(ASTNode* expr, double* out_value);

static bool is_string_expression(EsIRBuilder* builder, ASTNode* expr) {
    if (!expr) {
        return false;
    }
    if (expr->type == AST_STRING) {
        return true;
    }
    if (expr->type == AST_IDENTIFIER) {
        return false;
    }
    if (expr->type == AST_BINARY_OPERATION && expr->data.binary_op.operator == TOKEN_PLUS) {
        return is_string_expression(builder, expr->data.binary_op.left) ||
               is_string_expression(builder, expr->data.binary_op.right);
    }
    return false;
}

static void es_ir_ensure_main_entry(EsIRBuilder* builder) {
    if (!builder) {
        return;
    }
    if (!builder->module->main_function) {
        EsIRFunction* main_func = es_ir_function_create(builder, "main", NULL, 0, TOKEN_VOID);
        builder->module->main_function = main_func;
        es_ir_function_set_entry(builder, main_func);
    } else {
        es_ir_function_set_entry(builder, builder->module->main_function);
    }
    EsIRFunction* func = builder->module->main_function;
    if (!func->entry_block) {
        EsIRBasicBlock* entry = es_ir_block_create(builder, "entry");
        es_ir_block_set_current(builder, entry);
    } else {
        
        es_ir_block_set_current(builder, func->entry_block);
    }
}

static void es_ir_push_class_context(EsIRBuilder* builder, const char* class_name) {
    if (!builder || !class_name) {
        return;
    }
    if (builder->class_stack_size >= builder->class_stack_capacity) {
        int new_capacity = builder->class_stack_capacity == 0 ? 8 : builder->class_stack_capacity * 2;
        char** new_stack = (char**)ES_REALLOC(builder->class_name_stack, new_capacity * sizeof(char*));
        if (!new_stack) {
            return;
        }
        builder->class_name_stack = new_stack;
        builder->class_stack_capacity = new_capacity;
    }
    builder->class_name_stack[builder->class_stack_size] = ES_STRDUP(class_name);
    builder->class_stack_size++;
}

static void es_ir_pop_class_context(EsIRBuilder* builder) {
    if (!builder || builder->class_stack_size <= 0) {
        return;
    }
    builder->class_stack_size--;
    if (builder->class_name_stack[builder->class_stack_size]) {
        ES_FREE(builder->class_name_stack[builder->class_stack_size]);
        builder->class_name_stack[builder->class_stack_size] = NULL;
    }
}

static const char* es_ir_current_class_context(EsIRBuilder* builder) {
    if (!builder || builder->class_stack_size <= 0) {
        return NULL;
    }
    return builder->class_name_stack[builder->class_stack_size - 1];
}

static char* es_ir_mangle_static_member(const char* class_name, const char* member_name) {
    if (!class_name || !member_name) {
        return NULL;
    }
    size_t class_len = strlen(class_name);
    size_t member_len = strlen(member_name);
    size_t total_len = class_len + member_len + 3;
    char* mangled = (char*)ES_MALLOC(total_len);
    if (!mangled) {
        return NULL;
    }
    snprintf(mangled, total_len, "%s__%s", class_name, member_name);
    return mangled;
}

static int es_ir_evaluate_numeric_constant(ASTNode* expr, double* out_value) {
    if (!expr || !out_value) {
        return 0;
    }
    switch (expr->type) {
        case AST_NUMBER:
            *out_value = expr->data.number_value;
            return 1;
        case AST_BOOLEAN:
            *out_value = expr->data.boolean_value ? 1 : 0;
            return 1;
        case AST_BINARY_OPERATION: {
            double left_val, right_val;
            if (es_ir_evaluate_numeric_constant(expr->data.binary_op.left, &left_val) &&
                es_ir_evaluate_numeric_constant(expr->data.binary_op.right, &right_val)) {
                switch (expr->data.binary_op.operator) {
                    case TOKEN_PLUS: *out_value = left_val + right_val; return 1;
                    case TOKEN_MINUS: *out_value = left_val - right_val; return 1;
                    case TOKEN_MULTIPLY: *out_value = left_val * right_val; return 1;
                    case TOKEN_DIVIDE: 
                        if (right_val != 0) {
                            *out_value = left_val / right_val;
                            return 1;
                        }
                        return 0;
                    case TOKEN_BITWISE_AND: *out_value = (double)((long long)left_val & (long long)right_val); return 1;
                    case TOKEN_BITWISE_OR: *out_value = (double)((long long)left_val | (long long)right_val); return 1;
                    case TOKEN_BITWISE_XOR: *out_value = (double)((long long)left_val ^ (long long)right_val); return 1;
                    case TOKEN_LSHIFT: *out_value = (double)((long long)left_val << (long long)right_val); return 1;
                    case TOKEN_RSHIFT: *out_value = (double)((long long)left_val >> (long long)right_val); return 1;
                    case TOKEN_POWER: *out_value = pow(left_val, right_val); return 1;
                    default: return 0;
                }
            }
            return 0;
        }
        case AST_UNARY_OPERATION: {
            double val;
            if (es_ir_evaluate_numeric_constant(expr->data.unary_op.operand, &val)) {
                switch (expr->data.unary_op.operator) {
                    case TOKEN_MINUS: *out_value = -val; return 1;
                    case TOKEN_NOT: *out_value = (val == 0) ? 1 : 0; return 1;
                    default: return 0;
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

static bool try_extract_constant(ASTNode* node, double* value) {
    if (!node || !value) {
        return false;
    }
    if (node->type == AST_NUMBER) {
        *value = node->data.number_value;
        return true;
    }
    return false;
}

static bool is_float_expression(EsIRBuilder* builder, ASTNode* expr) {
    if (!expr) return false;
    if (expr->type == AST_NUMBER) return true;
    if (expr->type == AST_IDENTIFIER) {
        
        if (builder->type_context && builder->type_context->current_scope) {
            TypeCheckSymbolTable* scope = builder->type_context->current_scope;
            while (scope) {
                TypeCheckSymbol* symbol = type_check_symbol_table_lookup(scope, expr->data.identifier_name);
                if (symbol && symbol->type) {
                    return symbol->type->kind == TYPE_FLOAT64 || symbol->type->kind == TYPE_FLOAT32;
                }
                scope = scope->parent;
            }
        }
        return false;
    }
    if (expr->type == AST_CALL) {
        if (expr->data.call.name && strcmp(expr->data.call.name, "timer_elapsed") == 0) {
            return true;
        }
    }
    return false;
}

static EsIRValue convert_to_string_if_needed(EsIRBuilder* builder, ASTNode* node, EsIRValue value) {
    if (is_string_expression(builder, node)) {
        return value;
    }
    if (is_float_expression(builder, node)) {
#ifdef DEBUG
        
#endif
        return es_ir_double_to_string(builder, value);
    }
#ifdef DEBUG
    
#endif
    
    return es_ir_int_to_string(builder, value);
}

static EsIRValue es_ir_generate_expression(EsIRBuilder* builder, ASTNode* expr) {
    if (!expr) {
        EsIRValue void_val = {0};
        void_val.type = ES_IR_VALUE_VOID;
        return void_val;
    }

    
    double const_val;
    if (es_ir_evaluate_numeric_constant(expr, &const_val)) {
        return es_ir_imm(builder, const_val);
    }

    switch (expr->type) {
        case AST_IDENTIFIER: {
            const char* var_name = expr->data.identifier_name;
            if (builder->current_function && builder->current_function->params) {
                for (int i = 0; i < builder->current_function->param_count; i++) {
                    if (strcmp(builder->current_function->params[i].name, var_name) == 0) {
                        return es_ir_arg(builder, i);
                    }
                }
            }
            const char* current_class = es_ir_current_class_context(builder);
            if (current_class && var_name) {
                char* mangled = es_ir_mangle_static_member(current_class, var_name);
                if (mangled) {
                    EsIRGlobal* global = es_ir_module_find_global(builder->module, mangled);
                    if (global) {
                        EsIRValue v = es_ir_load(builder, mangled);
                        ES_FREE(mangled);
                        return v;
                    }
                    ES_FREE(mangled);
                }
            }
            return es_ir_load(builder, var_name);
        }

        case AST_NUMBER: {
            return es_ir_imm(builder, expr->data.number_value);
        }

        case AST_BOOLEAN: {
            return es_ir_imm(builder, expr->data.boolean_value ? 1 : 0);
        }

        case AST_STRING: {
            return es_ir_string_const(builder, expr->data.string_value);
        }

        case AST_NEW_EXPRESSION: {
            const char* class_name = expr->data.new_expr.class_name;
            int layout_size = es_ir_layout_get_size(builder, class_name);
            double alloc_size = (double)layout_size;
            EsIRValue* args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
            if (!args) {
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }
            args[0] = es_ir_imm(builder, alloc_size);
            EsIRValue obj = es_ir_call(builder, "es_malloc", args, 1);
            ES_FREE(args);
            char* ctor_name = es_ir_mangle_static_member(class_name, "constructor");
            if (ctor_name) {
                bool exists = false;
                EsIRFunction* f = builder->module->functions;
                while (f) {
                    if (strcmp(f->name, ctor_name) == 0) {
                        exists = true;
                        break;
                    }
                    f = f->next;
                }
                if (!exists) {
                    es_ir_function_create(builder, ctor_name, NULL, -1, TOKEN_VOID);
                }
                int ac = expr->data.new_expr.argument_count;
                EsIRValue* cargs = (EsIRValue*)ES_MALLOC((ac + 1) * sizeof(EsIRValue));
                if (cargs) {
                    cargs[0] = obj;
                    for (int i = 0; i < ac; i++) {
                        cargs[i + 1] = es_ir_generate_expression(builder, expr->data.new_expr.arguments[i]);
                    }
                    es_ir_call(builder, ctor_name, cargs, ac + 1);
                    ES_FREE(cargs);
                }
                ES_FREE(ctor_name);
            }
            return obj;
        }
        case AST_BINARY_OPERATION: {
            EsIRValue lhs = es_ir_generate_expression(builder, expr->data.binary_op.left);
            EsIRValue rhs = es_ir_generate_expression(builder, expr->data.binary_op.right);
            switch (expr->data.binary_op.operator) {
                case TOKEN_ASSIGN: {
                    if (expr->data.binary_op.left->type == AST_IDENTIFIER) {
                        const char* name = expr->data.binary_op.left->data.identifier_name;
                        es_ir_store(builder, name, rhs);
                        return rhs;
                    }
                    if (expr->data.binary_op.left->type == AST_MEMBER_ACCESS) {
                        ASTNode* objexpr = expr->data.binary_op.left->data.member_access.object;
                        const char* member_name = expr->data.binary_op.left->data.member_access.member_name;
                        if (objexpr->type == AST_THIS) {
                            const char* current_class = es_ir_current_class_context(builder);
                            int offset = es_ir_layout_get_offset(builder, current_class, member_name);
                            EsIRValue this_val = es_ir_arg(builder, 0);
                            es_ir_store_ptr(builder, this_val, offset >= 0 ? offset : 0, rhs);
                            return rhs;
                        }
                        if (objexpr->type == AST_IDENTIFIER) {
                            const char* class_name = objexpr->data.identifier_name;
                            char* mangled = es_ir_mangle_static_member(class_name, member_name);
                            if (mangled) {
                                es_ir_store(builder, mangled, rhs);
                                ES_FREE(mangled);
                                return rhs;
                            }
                        }
                        if (expr->data.binary_op.left->data.member_access.resolved_class_name) {
                            int offset = es_ir_layout_get_offset(builder, expr->data.binary_op.left->data.member_access.resolved_class_name, member_name);
                            EsIRValue base = es_ir_generate_expression(builder, objexpr);
                            es_ir_store_ptr(builder, base, offset >= 0 ? offset : 0, rhs);
                            return rhs;
                        }
                    }
                    return rhs;
                }
                case TOKEN_PLUS: {
                    bool left_is_string = is_string_expression(builder, expr->data.binary_op.left);
                    bool right_is_string = is_string_expression(builder, expr->data.binary_op.right);
                    if (left_is_string || right_is_string) {
                        EsIRValue string_lhs = convert_to_string_if_needed(builder, expr->data.binary_op.left, lhs);
                        EsIRValue string_rhs = convert_to_string_if_needed(builder, expr->data.binary_op.right, rhs);
                        return es_ir_strcat(builder, string_lhs, string_rhs);
                    }
                    return es_ir_add(builder, lhs, rhs);
                }
                case TOKEN_MINUS:
                    return es_ir_sub(builder, lhs, rhs);
                case TOKEN_MULTIPLY:
                    return es_ir_mul(builder, lhs, rhs);
                case TOKEN_DIVIDE:
                    return es_ir_div(builder, lhs, rhs);
                case TOKEN_MODULO:
                    return es_ir_mod(builder, lhs, rhs);
                case TOKEN_BITWISE_AND:
                    return es_ir_and(builder, lhs, rhs);
                case TOKEN_BITWISE_OR:
                    return es_ir_or(builder, lhs, rhs);
                case TOKEN_BITWISE_XOR:
                    return es_ir_xor(builder, lhs, rhs);
                case TOKEN_LSHIFT:
                    return es_ir_lshift(builder, lhs, rhs);
                case TOKEN_RSHIFT:
                    return es_ir_rshift(builder, lhs, rhs);
                case TOKEN_POWER:
                    return es_ir_pow(builder, lhs, rhs);
                case TOKEN_LESS:
                    return es_ir_compare(builder, ES_IR_LT, lhs, rhs);
                case TOKEN_GREATER:
                    return es_ir_compare(builder, ES_IR_GT, lhs, rhs);
                case TOKEN_EQUAL:
                    return es_ir_compare(builder, ES_IR_EQ, lhs, rhs);
                case TOKEN_LESS_EQUAL:
                    return es_ir_compare(builder, ES_IR_LE, lhs, rhs);
                case TOKEN_GREATER_EQUAL:
                    return es_ir_compare(builder, ES_IR_GE, lhs, rhs);
                case TOKEN_NOT_EQUAL:
                    return es_ir_compare(builder, ES_IR_NE, lhs, rhs);
                default: {
                    EsIRValue void_val = {0};
                    void_val.type = ES_IR_VALUE_VOID;
                    return void_val;
                }
            }
        }
        case AST_UNARY_OPERATION: {
            ASTNode* operand_node = expr->data.unary_op.operand;
            EsIRValue operand_value = es_ir_generate_expression(builder, operand_node);
            switch (expr->data.unary_op.operator) {
                case TOKEN_MINUS: {
                    EsIRValue zero = es_ir_imm(builder, 0);
                    return es_ir_sub(builder, zero, operand_value);
                }
                case TOKEN_NOT: {
                    EsIRValue zero = es_ir_imm(builder, 0);
                    return es_ir_compare(builder, ES_IR_EQ, operand_value, zero);
                }
                case TOKEN_INCREMENT:
                case TOKEN_DECREMENT: {
                    if (!operand_node || operand_node->type != AST_IDENTIFIER) {
                        EsIRValue void_val = {0};
                        void_val.type = ES_IR_VALUE_VOID;
                        return void_val;
                    }
                    const char* target_name = operand_node->data.identifier_name;
                    EsIRValue one = es_ir_imm(builder, 1);
                    EsIRValue updated = (expr->data.unary_op.operator == TOKEN_INCREMENT)
                        ? es_ir_add(builder, operand_value, one)
                        : es_ir_sub(builder, operand_value, one);
                    es_ir_store(builder, target_name, updated);
                    if (expr->data.unary_op.is_postfix) {
                        return operand_value;
                    }
                    return updated;
                }
                case TOKEN_INT32:
                case TOKEN_INT64:
                case TOKEN_INT16:
                case TOKEN_INT8:
                case TOKEN_UINT32:
                case TOKEN_UINT64:
                case TOKEN_UINT16:
                case TOKEN_UINT8:
                case TOKEN_FLOAT32:
                case TOKEN_FLOAT64:
                case TOKEN_BOOL:
                    return es_ir_cast(builder, operand_value, expr->data.unary_op.operator);
                default: {
                    EsIRValue void_val = {0};
                    void_val.type = ES_IR_VALUE_VOID;
                    return void_val;
                }
            }
        }
        case AST_ARRAY_ACCESS: {
            EsIRValue array_expr = es_ir_generate_expression(builder, expr->data.array_access.array);
            EsIRValue index_expr = es_ir_generate_expression(builder, expr->data.array_access.index);
            
            
            int element_size = 8; 
            
            
            

































            
            EsIRValue element_size_value = es_ir_imm(builder, element_size);
            EsIRValue byte_offset = es_ir_mul(builder, index_expr, element_size_value);
            EsIRValue base_addr;
            if (expr->data.array_access.array->type == AST_IDENTIFIER) {
                base_addr = es_ir_load(builder, expr->data.array_access.array->data.identifier_name);
            } else {
                base_addr = array_expr;
            }
            EsIRValue element_addr = es_ir_add(builder, base_addr, byte_offset);
            EsIRValue result = es_ir_load_ptr(builder, element_addr, 0);
            return result;
        }
        case AST_STATIC_METHOD_CALL: {
            const char* class_name = expr->data.static_call.class_name;
            const char* method_name = expr->data.static_call.method_name;

            
            if (class_name && strcmp(class_name, "Console") == 0 &&
                method_name && (strcmp(method_name, "WriteLine") == 0 || strcmp(method_name, "Write") == 0)) {
                
                EsIRValue final_str;
                bool first = true;
                
                if (expr->data.static_call.argument_count == 0) {
                    
                    if (strcmp(method_name, "WriteLine") == 0) {
                        final_str.type = ES_IR_VALUE_STRING_CONST;
                        final_str.data.string_const_id = -1; 
                        
                        char* mangled_name = es_ir_mangle_static_member(class_name, method_name);
                        EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        call_args[0] = final_str;
                        EsIRValue result = es_ir_call(builder, mangled_name, call_args, 1);
                        ES_FREE(call_args);
                        ES_FREE(mangled_name);
                        return result;
                    }
                } else {
                    if (expr->data.static_call.argument_count == 1 &&
                        !is_string_expression(builder, expr->data.static_call.arguments[0]) &&
                        !is_float_expression(builder, expr->data.static_call.arguments[0])) {
                        EsIRValue ival = es_ir_generate_expression(builder, expr->data.static_call.arguments[0]);
                        char* mangled_name = es_ir_mangle_static_member(class_name, strcmp(method_name, "WriteLine") == 0 ? "WriteLineInt" : "WriteInt");
                        EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        call_args[0] = ival;
                        EsIRValue result = es_ir_call(builder, mangled_name, call_args, 1);
                        ES_FREE(call_args);
                        ES_FREE(mangled_name);
                        return result;
                    } else {
                        for (int i = 0; i < expr->data.static_call.argument_count; i++) {
                            EsIRValue val = es_ir_generate_expression(builder, expr->data.static_call.arguments[i]);
                            EsIRValue str_val = convert_to_string_if_needed(builder, expr->data.static_call.arguments[i], val);
                            if (first) {
                                final_str = str_val;
                                first = false;
                            } else {
                                final_str = es_ir_strcat(builder, final_str, str_val);
                            }
                        }
                        char* mangled_name = es_ir_mangle_static_member(class_name, method_name);
                        EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        call_args[0] = final_str;
                        EsIRValue result = es_ir_call(builder, mangled_name, call_args, 1);
                        ES_FREE(call_args);
                        ES_FREE(mangled_name);
                        return result;
                    }
                }
            }

            if (method_name && strcmp(method_name, "delete") == 0) {
                char* dtor_name = es_ir_mangle_static_member(class_name, "destructor");
                if (dtor_name) {
                    bool exists = false;
                    EsIRFunction* f = builder->module->functions;
                    while (f) {
                        if (strcmp(f->name, dtor_name) == 0) {
                            exists = true;
                            break;
                        }
                        f = f->next;
                    }
                    if (!exists) {
                        es_ir_function_create(builder, dtor_name, NULL, -1, TOKEN_VOID);
                    }
                    if (expr->data.static_call.argument_count > 0) {
                        EsIRValue obj = es_ir_generate_expression(builder, expr->data.static_call.arguments[0]);
                        EsIRValue* dargs = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        if (dargs) {
                            dargs[0] = obj;
                            es_ir_call(builder, dtor_name, dargs, 1);
                            ES_FREE(dargs);
                        }
                        EsIRValue* fargs = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        if (fargs) {
                            fargs[0] = obj;
                            es_ir_call(builder, "es_free", fargs, 1);
                            ES_FREE(fargs);
                        }
                    }
                    ES_FREE(dtor_name);
                }
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }
            char* mangled_name = es_ir_mangle_static_member(class_name, method_name);
            if (!mangled_name) {
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }
            bool function_exists = false;
            EsIRFunction* iter = builder->module->functions;
            while (iter) {
                if (strcmp(iter->name, mangled_name) == 0) {
                    function_exists = true;
                    break;
                }
                iter = iter->next;
            }
            if (!function_exists) {
                es_ir_function_create(builder, mangled_name, NULL, -1, TOKEN_VOID);
                ES_WARNING("Static method '%s::%s' called before definition - creating forward reference",
                           class_name ? class_name : "<unknown>",
                           method_name ? method_name : "<unknown>");
            }

            EsIRValue* args = NULL;
            if (expr->data.static_call.argument_count > 0) {
                args = (EsIRValue*)ES_MALLOC(expr->data.static_call.argument_count * sizeof(EsIRValue));
                if (!args) {
                    ES_FREE(mangled_name);
                    EsIRValue void_val = {0};
                    void_val.type = ES_IR_VALUE_VOID;
                    return void_val;
                }
                for (int i = 0; i < expr->data.static_call.argument_count; i++) {
                    args[i] = es_ir_generate_expression(builder, expr->data.static_call.arguments[i]);
                }
            }

            EsIRValue result = es_ir_call(builder, mangled_name, args, expr->data.static_call.argument_count);

            if (args) {
                ES_FREE(args);
            }
            ES_FREE(mangled_name);
            return result;
        }

        case AST_MEMBER_ACCESS: {
            if (expr->data.member_access.object &&
                expr->data.member_access.object->type == AST_IDENTIFIER) {
                const char* class_name = expr->data.member_access.object->data.identifier_name;
                const char* member_name = expr->data.member_access.member_name;
                char* mangled_name = es_ir_mangle_static_member(class_name, member_name);
                if (mangled_name) {
                    EsIRGlobal* global = es_ir_module_find_global(builder->module, mangled_name);
                    if (global) {
                        EsIRValue value = es_ir_load(builder, mangled_name);
                        ES_FREE(mangled_name);
                        return value;
                    }
                    ES_FREE(mangled_name);
                }
            }

            if (expr->data.member_access.object &&
                expr->data.member_access.object->type == AST_THIS) {
                const char* current_class = es_ir_current_class_context(builder);
                const char* field_name = expr->data.member_access.member_name;
                int offset = es_ir_layout_get_offset(builder, current_class, field_name);
                EsIRValue this_val = es_ir_arg(builder, 0);
                return es_ir_load_ptr(builder, this_val, offset >= 0 ? offset : 0);
            }

            if (expr->data.member_access.resolved_class_name) {
                const char* field_name = expr->data.member_access.member_name;
                int offset = es_ir_layout_get_offset(builder, expr->data.member_access.resolved_class_name, field_name);
                EsIRValue base = es_ir_generate_expression(builder, expr->data.member_access.object);
                return es_ir_load_ptr(builder, base, offset >= 0 ? offset : 0);
            }

            EsIRValue void_val = {0};
            void_val.type = ES_IR_VALUE_VOID;
            return void_val;
        }

        case AST_CALL: {
            const char* func_name = expr->data.call.name;

            
            if (func_name && strcmp(func_name, "print") == 0 && expr->data.call.argument_count == 1) {
                ASTNode* arg = expr->data.call.arguments[0];
                const char* actual_func = "print_int";
                
                if (arg->type == AST_STRING || is_string_expression(builder, arg)) {
                    actual_func = "print_string";
                } else if (arg->type == AST_NUMBER) {
                    actual_func = "print_float";
                } else if (arg->type == AST_IDENTIFIER) {
                    
                    if (builder->type_context && builder->type_context->current_scope) {
                        TypeCheckSymbolTable* scope = builder->type_context->current_scope;
                        while (scope) {
                            TypeCheckSymbol* symbol = type_check_symbol_table_lookup(scope, arg->data.identifier_name);
                            if (symbol) {
                                if (symbol->type) {
                                    if (symbol->type->kind == TYPE_FLOAT64 || symbol->type->kind == TYPE_FLOAT32) {
                                        actual_func = "print_float";
                                    } else if (symbol->type->kind == TYPE_INT64 || symbol->type->kind == TYPE_UINT64) {
                                        actual_func = "print_int64";
                                    }
                                }
                                break;
                            }
                            scope = scope->parent;
                        }
                    }
                } else if (arg->type == AST_BINARY_OPERATION) {
                    ASTNode* left = arg->data.binary_op.left;
                    ASTNode* right = arg->data.binary_op.right;
                    if ((left && left->type == AST_NUMBER) || (right && right->type == AST_NUMBER)) {
                        actual_func = "print_float";
                    }
                } else if (arg->type == AST_UNARY_OPERATION) {
                    if (arg->data.unary_op.operator == TOKEN_MINUS || arg->data.unary_op.operator == TOKEN_PLUS) {
                        ASTNode* operand = arg->data.unary_op.operand;
                        if (operand && operand->type == AST_NUMBER) {
                            actual_func = "print_float";
                        }
                    }
                }
                
                EsIRValue arg_val = es_ir_generate_expression(builder, arg);
                EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                if (call_args) {
                    call_args[0] = arg_val;
                    es_ir_call(builder, actual_func, call_args, 1);
                    ES_FREE(call_args);
                }
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }

            
            if (func_name && (strcmp(func_name, "Console__WriteLine") == 0 || strcmp(func_name, "Console__Write") == 0)) {
                EsIRValue final_str;
                bool first = true;
                
                if (expr->data.call.argument_count == 0) {
                    if (strcmp(func_name, "Console__WriteLine") == 0) {
                        final_str.type = ES_IR_VALUE_STRING_CONST;
                        final_str.data.string_const_id = -1;
                        
                        EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        call_args[0] = final_str;
                        EsIRValue result = es_ir_call(builder, (char*)func_name, call_args, 1);
                        ES_FREE(call_args);
                        return result;
                    }
                } else {
                    if (expr->data.call.argument_count == 1 &&
                        !is_string_expression(builder, expr->data.call.arguments[0]) &&
                        !is_float_expression(builder, expr->data.call.arguments[0])) {
                        EsIRValue ival = es_ir_generate_expression(builder, expr->data.call.arguments[0]);
                        const char* alt_name = strcmp(func_name, "Console__WriteLine") == 0 ? "Console__WriteLineInt" : "Console__WriteInt";
                        EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        call_args[0] = ival;
                        EsIRValue result = es_ir_call(builder, alt_name, call_args, 1);
                        ES_FREE(call_args);
                        return result;
                    } else {
                        for (int i = 0; i < expr->data.call.argument_count; i++) {
                            EsIRValue val = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                            EsIRValue str_val = convert_to_string_if_needed(builder, expr->data.call.arguments[i], val);
                            
                            if (first) {
                                final_str = str_val;
                                first = false;
                            } else {
                                final_str = es_ir_strcat(builder, final_str, str_val);
                            }
                        }
                        
                        EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        call_args[0] = final_str;
                        EsIRValue result = es_ir_call(builder, (char*)func_name, call_args, 1);
                        ES_FREE(call_args);
                        return result;
                    }
                }
            }

            const char* alt_mangled = NULL;
            if (expr->data.call.object && expr->data.call.object->type == AST_IDENTIFIER && strcmp(expr->data.call.name, "delete") == 0) {
                const char* class_name = expr->data.call.object->data.identifier_name;
                char* dtor_name = es_ir_mangle_static_member(class_name, "destructor");
                if (dtor_name) {
                    bool exists = false;
                    EsIRFunction* f = builder->module->functions;
                    while (f) {
                        if (strcmp(f->name, dtor_name) == 0) {
                            exists = true;
                            break;
                        }
                        f = f->next;
                    }
                    if (!exists) {
                        es_ir_function_create(builder, dtor_name, NULL, -1, TOKEN_VOID);
                    }
                    if (expr->data.call.argument_count > 0) {
                        EsIRValue obj = es_ir_generate_expression(builder, expr->data.call.arguments[0]);
                        EsIRValue* dargs = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        if (dargs) {
                            dargs[0] = obj;
                            es_ir_call(builder, dtor_name, dargs, 1);
                            ES_FREE(dargs);
                        }
                        EsIRValue* fargs = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        if (fargs) {
                            fargs[0] = obj;
                            es_ir_call(builder, "es_free", fargs, 1);
                            ES_FREE(fargs);
                        }
                    }
                    ES_FREE(dtor_name);
                }
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }
            if (expr->data.call.object && expr->data.call.object->type == AST_IDENTIFIER) {
                if (expr->data.call.resolved_class_name) {
                    
                    if (strstr(expr->data.call.name, "__") != NULL) {
                        
                        EsIRValue* iargs = (EsIRValue*)ES_MALLOC(expr->data.call.argument_count * sizeof(EsIRValue));
                        if (iargs) {
                            for (int i = 0; i < expr->data.call.argument_count; i++) {
                                iargs[i] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                            }
                            EsIRValue result = es_ir_call(builder, expr->data.call.name, iargs, expr->data.call.argument_count);
                            ES_FREE(iargs);
                            return result;
                        }
                    } else {
                        char* mangled = es_ir_mangle_static_member(expr->data.call.resolved_class_name, expr->data.call.name);
                        if (mangled) {
                            
                            EsIRValue* iargs = (EsIRValue*)ES_MALLOC(expr->data.call.argument_count * sizeof(EsIRValue));
                            if (iargs) {
                                for (int i = 0; i < expr->data.call.argument_count; i++) {
                                    iargs[i] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                                }
                                EsIRValue result = es_ir_call(builder, mangled, iargs, expr->data.call.argument_count);
                                ES_FREE(iargs);
                                ES_FREE(mangled);
                                return result;
                            }
                            ES_FREE(mangled);
                        }
                    }
                } else {
                    const char* class_name = expr->data.call.object->data.identifier_name;
                    
                    
                    if (strstr(func_name, "__") != NULL) {
                        
                    } else {
                        char* mangled = es_ir_mangle_static_member(class_name, func_name);
                        if (mangled) {
                            alt_mangled = mangled;
                            func_name = mangled;
                        }
                    }
                }
            } else {
                const char* current_class = es_ir_current_class_context(builder);
                if (current_class) {
                    char* mangled = es_ir_mangle_static_member(current_class, func_name);
                    if (mangled) {
                        alt_mangled = mangled;
                        func_name = mangled;
                    }
                }
            }

            bool function_exists = false;
            EsIRFunction* func_iter = builder->module->functions;
            while (func_iter) {
                if (strcmp(func_iter->name, func_name) == 0) {
                    function_exists = true;
                    break;
                }
                func_iter = func_iter->next;
            }

            
            if (func_name && (strcmp(func_name, "writeline") == 0 || strcmp(func_name, "write") == 0)) {
                const char* console_func = strcmp(func_name, "writeline") == 0 ? "Console__WriteLine" : "Console__Write";
                EsIRValue final_str;
                bool first = true;
                if (expr->data.call.argument_count == 0) {
                    if (strcmp(func_name, "writeline") == 0) {
                        final_str.type = ES_IR_VALUE_STRING_CONST;
                        final_str.data.string_const_id = -1;
                        EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        call_args[0] = final_str;
                        EsIRValue result = es_ir_call(builder, (char*)console_func, call_args, 1);
                        ES_FREE(call_args);
                        if (alt_mangled) {
                            ES_FREE((char*)alt_mangled);
                        }
                        return result;
                    }
                } else {
                    for (int i = 0; i < expr->data.call.argument_count; i++) {
                        EsIRValue val = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                        EsIRValue str_val = convert_to_string_if_needed(builder, expr->data.call.arguments[i], val);
                        if (first) {
                            final_str = str_val;
                            first = false;
                        } else {
                            final_str = es_ir_strcat(builder, final_str, str_val);
                        }
                    }
                    EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                    call_args[0] = final_str;
                    EsIRValue result = es_ir_call(builder, (char*)console_func, call_args, 1);
                    ES_FREE(call_args);
                    if (alt_mangled) {
                        ES_FREE((char*)alt_mangled);
                    }
                    return result;
                }
            }

            if (func_name && (strcmp(func_name, "Console__WriteLine") == 0 || strcmp(func_name, "Console__Write") == 0)) {
                EsIRValue final_str;
                bool first = true;
                if (expr->data.call.argument_count == 0) {
                    if (strcmp(func_name, "Console__WriteLine") == 0) {
                        final_str.type = ES_IR_VALUE_STRING_CONST;
                        final_str.data.string_const_id = -1;
                        EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                        call_args[0] = final_str;
                        EsIRValue result = es_ir_call(builder, (char*)func_name, call_args, 1);
                        ES_FREE(call_args);
                        if (alt_mangled) {
                            ES_FREE((char*)alt_mangled);
                        }
                        return result;
                    }
                } else {
                    for (int i = 0; i < expr->data.call.argument_count; i++) {
                        EsIRValue val = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                        EsIRValue str_val = convert_to_string_if_needed(builder, expr->data.call.arguments[i], val);
                        if (first) {
                            final_str = str_val;
                            first = false;
                        } else {
                            final_str = es_ir_strcat(builder, final_str, str_val);
                        }
                    }
                    EsIRValue* call_args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                    call_args[0] = final_str;
                    EsIRValue result = es_ir_call(builder, (char*)func_name, call_args, 1);
                    ES_FREE(call_args);
                    if (alt_mangled) {
                        ES_FREE((char*)alt_mangled);
                    }
                    return result;
                }
            }

            
            bool is_runtime_func = 
                strcmp(func_name, "timer_start") == 0 || 
                strcmp(func_name, "timer_elapsed") == 0 ||
                strcmp(func_name, "timer_current") == 0 ||
                strcmp(func_name, "timer_start_int") == 0 ||
                strcmp(func_name, "timer_elapsed_int") == 0 ||
                strcmp(func_name, "timer_current_int") == 0 ||
                strcmp(func_name, "print") == 0 ||
                strcmp(func_name, "println") == 0 ||
                strcmp(func_name, "write") == 0 ||
                strcmp(func_name, "writeline") == 0 ||
                strcmp(func_name, "print_number") == 0 ||
                strcmp(func_name, "print_string") == 0 ||
                strcmp(func_name, "println_string") == 0 ||
                strcmp(func_name, "print_int") == 0 ||
                strcmp(func_name, "print_int64") == 0 ||
                strcmp(func_name, "print_float") == 0 ||
                strncmp(func_name, "Console__", 9) == 0 ||
                strncmp(func_name, "_es_", 4) == 0;

            if (!function_exists && !is_runtime_func) {
                
                
                char lambda_name[256];
                snprintf(lambda_name, sizeof(lambda_name), "__lambda_%s", func_name);
                
                EsIRFunction* lambda_func = builder->module->functions;
                bool lambda_found = false;
                while (lambda_func) {
                    if (strncmp(lambda_func->name, "__lambda_", 9) == 0) {
                        lambda_found = true;
                        break;
                    }
                    lambda_func = lambda_func->next;
                }
                
                if (!lambda_found) {
                    es_ir_function_create(builder, func_name, NULL, -1, TOKEN_VOID);
                    ES_WARNING("Function '%s' is used before its definition. Consider moving the function definition before its first use, or adding a function declaration.", func_name);
                }
            }

            EsIRValue* args = NULL;
            if (expr->data.call.argument_count > 0) {
                if (func_name && (strcmp(func_name, "Console__WriteLine") == 0 || strcmp(func_name, "Console__Write") == 0)) {
                    EsIRValue final_str;
                    bool first = true;
                    for (int i = 0; i < expr->data.call.argument_count; i++) {
                        EsIRValue val = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                        EsIRValue str_val = convert_to_string_if_needed(builder, expr->data.call.arguments[i], val);
                        if (first) {
                            final_str = str_val;
                            first = false;
                        } else {
                            final_str = es_ir_strcat(builder, final_str, str_val);
                        }
                    }
                    args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                    if (!args) {
                        EsIRValue void_val = {0};
                        void_val.type = ES_IR_VALUE_VOID;
                        return void_val;
                    }
                    args[0] = final_str;
                    EsIRValue result = es_ir_call(builder, func_name, args, 1);
                    ES_FREE(args);
                    if (alt_mangled) {
                        ES_FREE((char*)alt_mangled);
                    }
                    return result;
                } else {
                    args = (EsIRValue*)ES_MALLOC(expr->data.call.argument_count * sizeof(EsIRValue));
                    if (!args) {
                        EsIRValue void_val = {0};
                        void_val.type = ES_IR_VALUE_VOID;
                        return void_val;
                    }
                    for (int i = 0; i < expr->data.call.argument_count; i++) {
                        args[i] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                    }
                }
            }

            if (expr->data.call.object && expr->data.call.object->type == AST_THIS) {
                const char* current_class = es_ir_current_class_context(builder);
                if (current_class) {
                    char* mangled = es_ir_mangle_static_member(current_class, expr->data.call.name);
                    if (mangled) {
                        EsIRValue* iargs = (EsIRValue*)ES_MALLOC((expr->data.call.argument_count + 1) * sizeof(EsIRValue));
                        if (iargs) {
                            iargs[0] = es_ir_arg(builder, 0);
                            for (int i = 0; i < expr->data.call.argument_count; i++) {
                                iargs[i + 1] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                            }
                            EsIRValue result = es_ir_call(builder, mangled, iargs, expr->data.call.argument_count + 1);
                            ES_FREE(iargs);
                            ES_FREE(mangled);
                            if (alt_mangled) {
                                ES_FREE((char*)alt_mangled);
                            }
                            return result;
                        }
                        ES_FREE(mangled);
                    }
                }
            }

            if (expr->data.call.resolved_class_name && expr->data.call.object) {
                
                if (strstr(expr->data.call.name, "__") != NULL) {
                    
                    EsIRValue* iargs = (EsIRValue*)ES_MALLOC(expr->data.call.argument_count * sizeof(EsIRValue));
                    if (iargs) {
                        for (int i = 0; i < expr->data.call.argument_count; i++) {
                            iargs[i] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                        }
                        EsIRValue result = es_ir_call(builder, expr->data.call.name, iargs, expr->data.call.argument_count);
                        ES_FREE(iargs);
                        return result;
                    }
                } else {
                    char* mangled = es_ir_mangle_static_member(expr->data.call.resolved_class_name, expr->data.call.name);
                    if (mangled) {
                        
                        EsIRValue* iargs = (EsIRValue*)ES_MALLOC(expr->data.call.argument_count * sizeof(EsIRValue));
                        if (iargs) {
                            for (int i = 0; i < expr->data.call.argument_count; i++) {
                                iargs[i] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                            }
                            EsIRValue result = es_ir_call(builder, mangled, iargs, expr->data.call.argument_count);
                            ES_FREE(iargs);
                            ES_FREE(mangled);
                            return result;
                        }
                        ES_FREE(mangled);
                    }
                }
            }

            EsIRValue result = es_ir_call(builder, func_name, args, expr->data.call.argument_count);
            if (alt_mangled) {
                ES_FREE((char*)alt_mangled);
            }
            if (args) {
                ES_FREE(args);
            }
            return result;
        }

        case AST_TERNARY_OPERATION: {
            EsIRValue cond = es_ir_generate_expression(builder, expr->data.ternary_op.condition);

            char true_label[32], false_label[32], end_label[32];
            int true_label_id = builder->label_counter++;
            int false_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;

            snprintf(true_label, sizeof(true_label), "if_true_%d", true_label_id);
            snprintf(false_label, sizeof(false_label), "if_false_%d", false_label_id);
            snprintf(end_label, sizeof(end_label), "if_end_%d", end_label_id);

            char result_var[32];
            snprintf(result_var, sizeof(result_var), "result_%d", builder->temp_counter++);

            
            EsIRBasicBlock* true_block = es_ir_block_create(builder, true_label);
            EsIRBasicBlock* false_block = es_ir_block_create(builder, false_label);
            EsIRBasicBlock* end_block = es_ir_block_create(builder, end_label);
            
            
            EsIRBasicBlock* saved_current_block = builder->current_block;

            
            es_ir_branch(builder, cond, true_block, false_block);

            
            if (saved_current_block) {
                saved_current_block->next = true_block;
            }
            true_block->next = false_block;
            false_block->next = end_block;

            
            es_ir_block_set_current(builder, true_block);
            EsIRValue true_value = es_ir_generate_expression(builder, expr->data.ternary_op.true_value);
            es_ir_store(builder, result_var, true_value);
            es_ir_jump(builder, end_block);

            
            es_ir_block_set_current(builder, false_block);
            EsIRValue false_value = es_ir_generate_expression(builder, expr->data.ternary_op.false_value);
            es_ir_store(builder, result_var, false_value);
            es_ir_jump(builder, end_block);

            
            es_ir_block_set_current(builder, end_block);

            return es_ir_load(builder, result_var);
        }

        case AST_ARRAY_LITERAL: {
            int element_count = expr->data.array_literal.element_count;

            if (element_count == 0) {
                EsIRValue* args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                if (args) {
                    args[0] = es_ir_imm(builder, 8);
                    EsIRValue array_ptr = es_ir_call(builder, "es_malloc", args, 1);
                    ES_FREE(args);
                    return array_ptr;
                }
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }

            
            int element_size = 8; 
            
            
            

































            
            EsIRValue element_size_value = es_ir_imm(builder, element_size);
            EsIRValue count_val = es_ir_imm(builder, element_count);
            EsIRValue total_size = es_ir_mul(builder, count_val, element_size_value);

            EsIRValue* args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
            if (args) {
                args[0] = total_size;
                EsIRValue array_ptr = es_ir_call(builder, "es_malloc", args, 1);
                ES_FREE(args);

                for (int i = 0; i < element_count; i++) {
                    EsIRValue element_val = es_ir_generate_expression(builder, expr->data.array_literal.elements[i]);
                    EsIRValue index_val = es_ir_imm(builder, i);
                    es_ir_array_store(builder, array_ptr, index_val, element_val);
                }

                return array_ptr;
            }
            EsIRValue void_val = {0};
            void_val.type = ES_IR_VALUE_VOID;
            return void_val;
        }

        
        case AST_LAMBDA_EXPRESSION: {
            
            
            static int lambda_counter = 0;
            char lambda_name[256];
            snprintf(lambda_name, sizeof(lambda_name), "__lambda_%d", lambda_counter++);

            
            int param_count = expr->data.lambda_expr.parameter_count;

            EsIRParam* params = NULL;
            if (param_count > 0) {
                params = (EsIRParam*)ES_MALLOC(param_count * sizeof(EsIRParam));
                if (!params) {
                    EsIRValue void_val = {0};
                    void_val.type = ES_IR_VALUE_VOID;
                    return void_val;
                }
                for (int i = 0; i < param_count; i++) {
                    params[i].name = ES_STRDUP(expr->data.lambda_expr.parameters[i]);
                    params[i].type = TOKEN_INT32; 
                }
            }

            
            EsIRFunction* lambda_func = es_ir_function_create(builder, lambda_name,
                params, param_count, TOKEN_INT32); 

            
            if (params) {
                for (int i = 0; i < param_count; i++) {
                    ES_FREE(params[i].name);
                }
                ES_FREE(params);
            }

            if (!lambda_func) {
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }

            
            EsIRFunction* saved_func = builder->current_function;
            EsIRBasicBlock* saved_block = builder->current_block;

            
            builder->current_function = lambda_func;
            EsIRBasicBlock* lambda_entry = es_ir_block_create(builder, "entry");
            es_ir_block_set_current(builder, lambda_entry);

            
            if (expr->data.lambda_expr.expression) {
                
                EsIRValue result = es_ir_generate_expression(builder, expr->data.lambda_expr.expression);
                es_ir_return(builder, result);
            } else if (expr->data.lambda_expr.body) {
                
                es_ir_generate_statement(builder, expr->data.lambda_expr.body);
                
                if (!es_ir_check_has_return(expr->data.lambda_expr.body)) {
                    EsIRValue zero = es_ir_imm(builder, 0);
                    es_ir_return(builder, zero);
                }
            }

            
            builder->current_function = saved_func;
            builder->current_block = saved_block;

            
            
            
            EsIRValue lambda_val = es_ir_imm(builder, 0);

            return lambda_val;
        }

        case AST_LINQ_QUERY: {
            
            
            
            
            
            
            EsIRValue void_val = {0};
            void_val.type = ES_IR_VALUE_VOID;
            return void_val;
        }

        default: {
            EsIRValue void_val = {0};
            void_val.type = ES_IR_VALUE_VOID;
            return void_val;
        }
    }
}


static void es_ir_generate_statement(EsIRBuilder* builder, ASTNode* stmt) {
    if (!stmt) {
        return;
    }

    switch (stmt->type) {
        case AST_ASSIGNMENT: {
            EsIRValue value = es_ir_generate_expression(builder, stmt->data.assignment.value);
            const char* target_name = stmt->data.assignment.name;
            const char* current_class = es_ir_current_class_context(builder);
            int stored = 0;
            if (current_class && target_name) {
                char* mangled = es_ir_mangle_static_member(current_class, target_name);
                if (mangled) {
                    EsIRGlobal* global = es_ir_module_find_global(builder->module, mangled);
                    if (global) {
                        es_ir_store(builder, mangled, value);
                        stored = 1;
                    }
                    ES_FREE(mangled);
                }
            }
            if (!stored) {
                es_ir_store(builder, target_name, value);
            }
            break;
        }

        case AST_COMPOUND_ASSIGNMENT: {
            const char* target_name = stmt->data.compound_assignment.name;
            EsIRValue current_val = es_ir_load(builder, target_name);
            EsIRValue rhs = es_ir_generate_expression(builder, stmt->data.compound_assignment.value);
            EsIRValue result;
            
            switch (stmt->data.compound_assignment.operator) {
                case TOKEN_PLUS_ASSIGN: result = es_ir_add(builder, current_val, rhs); break;
                case TOKEN_MINUS_ASSIGN: result = es_ir_sub(builder, current_val, rhs); break;
                case TOKEN_MUL_ASSIGN: result = es_ir_mul(builder, current_val, rhs); break;
                case TOKEN_DIV_ASSIGN: result = es_ir_div(builder, current_val, rhs); break;
                default: result = rhs; break;
            }
            
            es_ir_store(builder, target_name, result);
            break;
        }

        case AST_ARRAY_ASSIGNMENT: {
            EsIRValue array_value = es_ir_generate_expression(builder, stmt->data.array_assignment.array);
            EsIRValue index_value = es_ir_generate_expression(builder, stmt->data.array_assignment.index);
            EsIRValue value = es_ir_generate_expression(builder, stmt->data.array_assignment.value);
            es_ir_array_store(builder, array_value, index_value, value);
            break;
        }

        case AST_ARRAY_COMPOUND_ASSIGNMENT: {
            EsIRValue array_value = es_ir_generate_expression(builder, stmt->data.array_compound_assignment.array);
            EsIRValue index_value = es_ir_generate_expression(builder, stmt->data.array_compound_assignment.index);
            
            
            int element_size = 8;
            EsIRValue element_size_value = es_ir_imm(builder, element_size);
            EsIRValue byte_offset = es_ir_mul(builder, index_value, element_size_value);
            EsIRValue element_addr = es_ir_add(builder, array_value, byte_offset);
            EsIRValue current_val = es_ir_load_ptr(builder, element_addr, 0);
            
            EsIRValue rhs = es_ir_generate_expression(builder, stmt->data.array_compound_assignment.value);
            EsIRValue result;
            
            switch (stmt->data.array_compound_assignment.operator) {
                case TOKEN_PLUS_ASSIGN: result = es_ir_add(builder, current_val, rhs); break;
                case TOKEN_MINUS_ASSIGN: result = es_ir_sub(builder, current_val, rhs); break;
                case TOKEN_MUL_ASSIGN: result = es_ir_mul(builder, current_val, rhs); break;
                case TOKEN_DIV_ASSIGN: result = es_ir_div(builder, current_val, rhs); break;
                default: result = rhs; break;
            }
            
            es_ir_array_store(builder, array_value, index_value, result);
            break;
        }

        case AST_RETURN_STATEMENT: {
            EsIRValue value = es_ir_generate_expression(builder, stmt->data.return_stmt.value);
            es_ir_return(builder, value);
            break;
        }

        case AST_IF_STATEMENT: {

            EsIRValue cond = es_ir_generate_expression(builder, stmt->data.if_stmt.condition);


            char true_label[32], false_label[32], end_label[32];
            int true_label_id = builder->label_counter++;
            int false_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;

            snprintf(true_label, sizeof(true_label), "if_true_%d", true_label_id);
            snprintf(false_label, sizeof(false_label), "if_false_%d", false_label_id);
            snprintf(end_label, sizeof(end_label), "if_end_%d", end_label_id);

            EsIRBasicBlock* true_block = es_ir_block_create(builder, true_label);
            EsIRBasicBlock* false_block = es_ir_block_create(builder, false_label);
            EsIRBasicBlock* end_block = es_ir_block_create(builder, end_label);


            es_ir_branch(builder, cond, true_block, false_block);


            es_ir_block_set_current(builder, true_block);
            es_ir_generate_statement(builder, stmt->data.if_stmt.then_branch);

            int then_has_return = es_ir_check_has_return(stmt->data.if_stmt.then_branch);
            if (!then_has_return) {
                es_ir_jump(builder, end_block);
            }


            es_ir_block_set_current(builder, false_block);
            if (stmt->data.if_stmt.else_branch) {
                es_ir_generate_statement(builder, stmt->data.if_stmt.else_branch);

                int else_has_return = es_ir_check_has_return(stmt->data.if_stmt.else_branch);
                if (!else_has_return) {
                    es_ir_jump(builder, end_block);
                }
            } else {
                es_ir_jump(builder, end_block);
            }


            es_ir_block_set_current(builder, end_block);
            break;
        }

        case AST_WHILE_STATEMENT: {
            char cond_label[32], body_label[32], end_label[32];
            int cond_label_id = builder->label_counter++;
            int body_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;
            snprintf(cond_label, sizeof(cond_label), "while_cond_%d", cond_label_id);
            snprintf(body_label, sizeof(body_label), "while_body_%d", body_label_id);
            snprintf(end_label, sizeof(end_label), "while_end_%d", end_label_id);
            EsIRBasicBlock* cond_block = es_ir_block_create(builder, cond_label);
            EsIRBasicBlock* body_block = es_ir_block_create(builder, body_label);
            EsIRBasicBlock* end_block = es_ir_block_create(builder, end_label);
            es_ir_push_loop_context(builder, cond_block, end_block);
            es_ir_jump(builder, cond_block);
            es_ir_block_set_current(builder, cond_block);
            EsIRValue cond = es_ir_generate_expression(builder, stmt->data.while_stmt.condition);
            es_ir_branch(builder, cond, body_block, end_block);
            es_ir_block_set_current(builder, body_block);
            es_ir_generate_statement(builder, stmt->data.while_stmt.body);
            es_ir_jump(builder, cond_block);
            es_ir_block_set_current(builder, end_block);
            es_ir_pop_loop_context(builder);
            break;
        }

        case AST_FOR_STATEMENT: {
            if (stmt->data.for_stmt.init) {
                if (stmt->data.for_stmt.init->type == AST_ASSIGNMENT ||
                    stmt->data.for_stmt.init->type == AST_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.init->type == AST_ARRAY_ASSIGNMENT ||
                    stmt->data.for_stmt.init->type == AST_ARRAY_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.init->type == AST_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.init->type == AST_STATIC_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.init->type == AST_BLOCK) {
                    es_ir_generate_statement(builder, stmt->data.for_stmt.init);
                } else {
                    es_ir_generate_expression(builder, stmt->data.for_stmt.init);
                }
            }
            char cond_label[32], body_label[32], incr_label[32], end_label[32];
            int cond_label_id = builder->label_counter++;
            int body_label_id = builder->label_counter++;
            int incr_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;
            snprintf(cond_label, sizeof(cond_label), "for_cond_%d", cond_label_id);
            snprintf(body_label, sizeof(body_label), "for_body_%d", body_label_id);
            snprintf(incr_label, sizeof(incr_label), "for_incr_%d", incr_label_id);
            snprintf(end_label, sizeof(end_label), "for_end_%d", end_label_id);
            EsIRBasicBlock* cond_block = es_ir_block_create(builder, cond_label);
            EsIRBasicBlock* body_block = es_ir_block_create(builder, body_label);
            EsIRBasicBlock* incr_block = es_ir_block_create(builder, incr_label);
            EsIRBasicBlock* end_block = es_ir_block_create(builder, end_label);
            es_ir_push_loop_context(builder, incr_block, end_block);
            es_ir_jump(builder, cond_block);
            es_ir_block_set_current(builder, cond_block);
            EsIRValue cond_value;
            if (stmt->data.for_stmt.condition) {
                cond_value = es_ir_generate_expression(builder, stmt->data.for_stmt.condition);
                if (cond_value.type == ES_IR_VALUE_VOID) {
                    cond_value = es_ir_imm(builder, 1);
                }
            } else {
                cond_value = es_ir_imm(builder, 1);
            }
            es_ir_branch(builder, cond_value, body_block, end_block);
            es_ir_block_set_current(builder, body_block);
            es_ir_generate_statement(builder, stmt->data.for_stmt.body);
            es_ir_jump(builder, incr_block);
            es_ir_block_set_current(builder, incr_block);
            if (stmt->data.for_stmt.increment) {
                if (stmt->data.for_stmt.increment->type == AST_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_ARRAY_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_ARRAY_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.increment->type == AST_STATIC_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.increment->type == AST_BLOCK) {
                    es_ir_generate_statement(builder, stmt->data.for_stmt.increment);
                } else {
                    es_ir_generate_expression(builder, stmt->data.for_stmt.increment);
                }
            }
            es_ir_jump(builder, cond_block);
            es_ir_block_set_current(builder, end_block);
            es_ir_pop_loop_context(builder);
            break;
        }
        case AST_FOREACH_STATEMENT: {
            char* var_name = stmt->data.foreach_stmt.var_name;
            ASTNode* iterable = stmt->data.foreach_stmt.iterable;
            ASTNode* body = stmt->data.foreach_stmt.body;
            
            EsIRValue iterable_value = es_ir_generate_expression(builder, iterable);
            
            char index_var_name[256];
            snprintf(index_var_name, sizeof(index_var_name), "%s__idx", var_name);
            es_ir_alloc(builder, index_var_name);
            es_ir_store(builder, index_var_name, es_ir_imm(builder, 0));
            
            char loop_label[32], body_label[32], end_label[32];
            int loop_label_id = builder->label_counter++;
            int body_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;
            snprintf(loop_label, sizeof(loop_label), "foreach_cond_%d", loop_label_id);
            snprintf(body_label, sizeof(body_label), "foreach_body_%d", body_label_id);
            snprintf(end_label, sizeof(end_label), "foreach_end_%d", end_label_id);
            
            EsIRBasicBlock* loop_block = es_ir_block_create(builder, loop_label);
            EsIRBasicBlock* body_block = es_ir_block_create(builder, body_label);
            EsIRBasicBlock* end_block = es_ir_block_create(builder, end_label);
            
            es_ir_push_loop_context(builder, end_block, end_block);
            es_ir_jump(builder, loop_block);
            
            es_ir_block_set_current(builder, loop_block);
            
            EsIRValue index_value = es_ir_load(builder, index_var_name);
            EsIRValue array_len = es_ir_call(builder, "array_size", &iterable_value, 1);
            EsIRValue cond = es_ir_compare(builder, ES_IR_LT, index_value, array_len);
            es_ir_branch(builder, cond, body_block, end_block);
            
            es_ir_block_set_current(builder, body_block);
            
            EsIRValue current_index = es_ir_load(builder, index_var_name);
            EsIRValue* array_args = (EsIRValue*)ES_MALLOC(2 * sizeof(EsIRValue));
            array_args[0] = iterable_value;
            array_args[1] = current_index;
            EsIRValue element_ptr = es_ir_call(builder, "array_get", array_args, 2);
            ES_FREE(array_args);
            EsIRValue element_value = es_ir_load_ptr(builder, element_ptr, 0);
            
            es_ir_alloc(builder, var_name);
            es_ir_store(builder, var_name, element_value);
            
            TypeCheckSymbolTable* foreach_scope = NULL;
            TypeCheckSymbolTable* saved_scope = NULL;
            if (builder->type_context) {
                saved_scope = builder->type_context->current_scope;
                foreach_scope = type_check_symbol_table_create(saved_scope);
                builder->type_context->current_scope = foreach_scope;
                
                TypeCheckSymbol symbol = {0};
                symbol.name = var_name;
                symbol.type = type_create_basic(TYPE_INT64);
                symbol.is_constant = 0;
                symbol.line_number = 0;
                symbol.owns_name = 0;
                type_check_symbol_table_add(builder->type_context->current_scope, symbol);
            }
            
            es_ir_generate_statement(builder, body);
            
            EsIRValue new_index = es_ir_add(builder, es_ir_load(builder, index_var_name), es_ir_imm(builder, 1));
            es_ir_store(builder, index_var_name, new_index);
            
            es_ir_jump(builder, loop_block);
            
            es_ir_block_set_current(builder, end_block);
            
            if (builder->type_context) {
                builder->type_context->current_scope = saved_scope;
                type_check_symbol_table_unref(foreach_scope);
            }
            
            es_ir_pop_loop_context(builder);
            break;
        }
        case AST_PRINT_STATEMENT: {
            
            EsIRValue final_str;
            bool first = true;
            
            for (int i = 0; i < stmt->data.print_stmt.value_count; i++) {
                EsIRValue val = es_ir_generate_expression(builder, stmt->data.print_stmt.values[i]);
                EsIRValue str_val = convert_to_string_if_needed(builder, stmt->data.print_stmt.values[i], val);
                
                if (first) {
                    final_str = str_val;
                    first = false;
                } else {
                    final_str = es_ir_strcat(builder, final_str, str_val);
                }
            }
            
            
            EsIRValue* args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
            if (args) {
                args[0] = final_str;
                es_ir_call(builder, "Console__WriteLine", args, 1);
                ES_FREE(args);
            }
            break;
        }

        case AST_BLOCK: {
            es_ir_generate_block(builder, stmt);
            break;
        }

        case AST_SWITCH_STATEMENT: {
            EsIRValue cond = es_ir_generate_expression(builder, stmt->data.switch_stmt.expression);
            char end_label[32];
            int end_label_id = builder->label_counter++;
            snprintf(end_label, sizeof(end_label), "end_%d", end_label_id);
            EsIRBasicBlock* end_block = es_ir_block_create(builder, end_label);
            es_ir_push_loop_context(builder, NULL, end_block);
            char** case_labels = (char**)ES_MALLOC(stmt->data.switch_stmt.case_count * sizeof(char*));
            EsIRBasicBlock** case_blocks = (EsIRBasicBlock**)ES_MALLOC(stmt->data.switch_stmt.case_count * sizeof(EsIRBasicBlock*));
            if (case_labels && case_blocks) {
                for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                    case_labels[i] = (char*)ES_MALLOC(32);
                    if (case_labels[i]) {
                        int case_label_id = builder->label_counter++;
                        snprintf(case_labels[i], 32, "case_%d_%d", case_label_id, i + 1);
                        case_blocks[i] = es_ir_block_create(builder, case_labels[i]);
                    }
                }
            }
            EsIRBasicBlock* default_block = NULL;
            char* default_label = NULL;
            if (stmt->data.switch_stmt.default_case) {
                default_label = (char*)ES_MALLOC(32);
                if (default_label) {
                    int default_label_id = builder->label_counter++;
                    snprintf(default_label, 32, "default_%d", default_label_id);
                    default_block = es_ir_block_create(builder, default_label);
                }
            }
            for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                EsIRValue case_value = es_ir_generate_expression(builder, stmt->data.switch_stmt.cases[i]->data.case_clause.value);
                EsIRValue cmp_result = es_ir_compare(builder, ES_IR_EQ, cond, case_value);
                es_ir_branch(builder, cmp_result, case_blocks[i],
                            (i < stmt->data.switch_stmt.case_count - 1) ? case_blocks[i + 1] :
                            (default_block ? default_block : end_block));
            }
            if (stmt->data.switch_stmt.default_case) {
                es_ir_jump(builder, default_block);
            } else if (stmt->data.switch_stmt.case_count > 0) {
                es_ir_jump(builder, end_block);
            }
            for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                es_ir_block_set_current(builder, case_blocks[i]);
                es_ir_generate_statement(builder, stmt->data.switch_stmt.cases[i]);
                es_ir_jump(builder, end_block);
            }
            if (stmt->data.switch_stmt.default_case) {
                es_ir_block_set_current(builder, default_block);
                es_ir_generate_statement(builder, stmt->data.switch_stmt.default_case);
                es_ir_jump(builder, end_block);
            }
            es_ir_block_set_current(builder, end_block);
            es_ir_nop(builder);
            es_ir_pop_loop_context(builder);
            if (case_labels) {
                for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                    if (case_labels[i]) {
                        ES_FREE(case_labels[i]);
                    }
                }
                ES_FREE(case_labels);
            }
            if (case_blocks) {
                ES_FREE(case_blocks);
            }
            if (default_label) {
                ES_FREE(default_label);
            }
            break;
        }
        case AST_CASE_CLAUSE: {
            for (int i = 0; i < stmt->data.case_clause.statement_count; i++) {
                es_ir_generate_statement(builder, stmt->data.case_clause.statements[i]);
            }
            break;
        }
        case AST_DEFAULT_CLAUSE: {
            for (int i = 0; i < stmt->data.default_clause.statement_count; i++) {
                es_ir_generate_statement(builder, stmt->data.default_clause.statements[i]);
            }
            break;
        }
        case AST_BREAK_STATEMENT: {
            EsIRBasicBlock* break_block = es_ir_get_current_break_block(builder);
            if (break_block) {
                es_ir_jump(builder, break_block);
            }
            break;
        }
        case AST_CONTINUE_STATEMENT: {
            EsIRBasicBlock* continue_block = es_ir_get_current_continue_block(builder);
            if (continue_block) {
                es_ir_jump(builder, continue_block);
            }
            break;
        }
        case AST_DELETE_STATEMENT: {
            EsIRValue obj = es_ir_generate_expression(builder, stmt->data.delete_stmt.value);
            const char* class_name = stmt->data.delete_stmt.resolved_class_name;
            if (class_name) {
                char* dtor_name = es_ir_mangle_static_member(class_name, "destructor");
                if (dtor_name) {
                    bool exists = false;
                    EsIRFunction* f = builder->module->functions;
                    while (f) {
                        if (strcmp(f->name, dtor_name) == 0) {
                            exists = true;
                            break;
                        }
                        f = f->next;
                    }
                    if (!exists) {
                        es_ir_function_create(builder, dtor_name, NULL, -1, TOKEN_VOID);
                    }
                    EsIRValue* dargs = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                    if (dargs) {
                        dargs[0] = obj;
                        es_ir_call(builder, dtor_name, dargs, 1);
                        ES_FREE(dargs);
                    }
                    EsIRValue* fargs = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                    if (fargs) {
                        fargs[0] = obj;
                        es_ir_call(builder, "es_free", fargs, 1);
                        ES_FREE(fargs);
                    }
                    ES_FREE(dtor_name);
                }
            }
            break;
        }

        case AST_VARIABLE_DECLARATION: {
            if (builder->type_context && builder->type_context->current_scope) {
                TypeCheckSymbol symbol = {0};
                symbol.name = (char*)stmt->data.variable_decl.name;
                symbol.type = type_create_from_token(stmt->data.variable_decl.type);
                symbol.is_constant = 0;
                symbol.line_number = 0;
                symbol.owns_name = 0;
                if (symbol.type) {
                    type_check_symbol_table_add(builder->type_context->current_scope, symbol);
                }
            }
            bool is_c_style_array = (stmt->data.variable_decl.array_size != NULL);
            bool is_array_literal = false;
            if (stmt->data.variable_decl.value &&
                stmt->data.variable_decl.value->type == AST_ARRAY_LITERAL) {
                is_array_literal = true;
            }
            if (is_c_style_array) {
                int array_size = 10;
                if (stmt->data.variable_decl.array_size) {
                    double size_value;
                    if (try_extract_constant(stmt->data.variable_decl.array_size, &size_value)) {
                        array_size = (int)size_value;
                        if (array_size <= 0) {
                            array_size = 10;
                        }
                    }
                }
                
                
                int element_size = 8; 
                
                
                

































                
                int total_bytes = array_size * element_size;
                es_ir_alloc(builder, stmt->data.variable_decl.name);
                EsIRValue* args = (EsIRValue*)ES_MALLOC(sizeof(EsIRValue));
                if (args) {
                    args[0] = es_ir_imm(builder, total_bytes);
                    EsIRValue array_ptr = es_ir_call(builder, "es_malloc", args, 1);
                    ES_FREE(args);
                    es_ir_store(builder, stmt->data.variable_decl.name, array_ptr);
                }
            } else if (is_array_literal) {
                es_ir_alloc(builder, stmt->data.variable_decl.name);
                if (stmt->data.variable_decl.value) {
                    EsIRValue init_value = es_ir_generate_expression(builder, stmt->data.variable_decl.value);
                    es_ir_store(builder, stmt->data.variable_decl.name, init_value);
                }
            } else {
                es_ir_alloc(builder, stmt->data.variable_decl.name);
                if (stmt->data.variable_decl.value) {
                    EsIRValue init_value = es_ir_generate_expression(builder, stmt->data.variable_decl.value);
                    es_ir_store(builder, stmt->data.variable_decl.name, init_value);
                }
            }
            break;
        }
        case AST_STATIC_VARIABLE_DECLARATION: {
            const char* base_name = stmt->data.static_variable_decl.name;
            if (!base_name) {
                break;
            }
            const char* current_class = es_ir_current_class_context(builder);
            char* mangled_name = NULL;
            const char* global_name = base_name;
            if (current_class) {
                mangled_name = es_ir_mangle_static_member(current_class, base_name);
                if (mangled_name) {
                    global_name = mangled_name;
                }
            }
            EsTokenType declared_type = stmt->data.static_variable_decl.type;
            if (declared_type == TOKEN_UNKNOWN) {
                declared_type = TOKEN_INT32;
            }
            EsIRGlobal* global = es_ir_module_add_global(builder, global_name, declared_type);
            if (!global) {
                if (mangled_name) {
                    ES_FREE(mangled_name);
                }
                break;
            }
            if (stmt->data.static_variable_decl.value) {
                double init_value = 0;
                if (es_ir_evaluate_numeric_constant(stmt->data.static_variable_decl.value, &init_value)) {
                    es_ir_module_set_global_number_initializer(global, init_value);
                } else {
                    EsIRFunction* saved_function = builder->current_function;
                    EsIRBasicBlock* saved_block = builder->current_block;
                    es_ir_ensure_main_entry(builder);
                    EsIRValue value = es_ir_generate_expression(builder, stmt->data.static_variable_decl.value);
                    es_ir_store(builder, global_name, value);
                    builder->current_function = saved_function;
                    builder->current_block = saved_block;
                }
            }
            if (mangled_name) {
                ES_FREE(mangled_name);
            }
            break;
        }
        case AST_FUNCTION_DECLARATION: {
            EsIRParam* params = NULL;
            if (stmt->data.function_decl.parameter_count > 0) {
                params = (EsIRParam*)ES_MALLOC(stmt->data.function_decl.parameter_count * sizeof(EsIRParam));
                if (params) {
                    for (int i = 0; i < stmt->data.function_decl.parameter_count; i++) {
                        params[i].name = stmt->data.function_decl.parameters[i];
                        params[i].type = stmt->data.function_decl.parameter_types[i];
                    }
                }
            }
            EsIRFunction* func = es_ir_function_create(builder, stmt->data.function_decl.name, params, stmt->data.function_decl.parameter_count, stmt->data.function_decl.return_type);
            EsIRFunction* saved_function = builder->current_function;
            builder->current_function = func;
            TypeCheckSymbolTable* saved_scope = NULL;
            if (builder->type_context) {
                saved_scope = builder->type_context->current_scope;
                TypeCheckSymbol* func_symbol = type_check_symbol_table_lookup(saved_scope, stmt->data.function_decl.name);
                if (func_symbol && func_symbol->type && func_symbol->type->kind == TYPE_FUNCTION && func_symbol->type->data.function.function_scope) {
                    builder->type_context->current_scope = func_symbol->type->data.function.function_scope;
                }
            }
            
            es_ir_function_set_entry(builder, func);
            if (params) {
                ES_FREE(params);
            }
            
            EsIRBasicBlock* entry_block = func->entry_block;
            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                entry_block = es_ir_block_create(builder, "entry");
                es_ir_block_set_current(builder, entry_block);
            } else {
                builder->current_block = entry_block;
            }
            es_ir_generate_block(builder, stmt->data.function_decl.body);
            int has_return = es_ir_check_has_return(stmt->data.function_decl.body);
            if (!has_return) {
                es_ir_return(builder, es_ir_imm(builder, 0));
            }
            builder->current_function = saved_function;
            if (builder->type_context) {
                builder->type_context->current_scope = saved_scope;
            }
            break;
        }

        case AST_CLASS_DECLARATION: {
            if (!stmt->data.class_decl.name) {
                break;
            }
            es_ir_push_class_context(builder, stmt->data.class_decl.name);
            ASTNode* body = stmt->data.class_decl.body;
            if (body) {
                if (body->type == AST_BLOCK) {
                    for (int i = 0; i < body->data.block.statement_count; i++) {
                        ASTNode* member = body->data.block.statements[i];
                        if (!member) continue;
                        if (member->type == AST_ACCESS_MODIFIER) {
                            member = member->data.access_modifier.member;
                            if (!member) continue;
                        }
                        if (member->type == AST_STATIC_FUNCTION_DECLARATION) {
                            es_ir_generate_statement(builder, member);
                        } else if (member->type == AST_CONSTRUCTOR_DECLARATION) {
                            EsIRParam* params = NULL;
                            int pc = member->data.constructor_decl.parameter_count;
                            if (pc >= 0) {
                                params = (EsIRParam*)ES_MALLOC((pc + 1) * sizeof(EsIRParam));
                                if (params) {
                                    params[0].name = "this";
                                    params[0].type = TOKEN_UINT64;
                                    for (int i = 0; i < pc; i++) {
                                        params[i + 1].name = member->data.constructor_decl.parameters[i];
                                        params[i + 1].type = member->data.constructor_decl.parameter_types[i];
                                    }
                                }
                            }
                            char* mangled = es_ir_mangle_static_member(stmt->data.class_decl.name, "constructor");
                            EsIRFunction* func = es_ir_function_create(builder, mangled, params, pc + 1, TOKEN_VOID);
                            es_ir_function_set_entry(builder, func);
                            if (params) {
                                ES_FREE(params);
                            }
                            EsIRBasicBlock* entry_block = func->entry_block;
                            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                                entry_block = es_ir_block_create(builder, "entry");
                                es_ir_block_set_current(builder, entry_block);
                            } else {
                                builder->current_block = entry_block;
                            }
                            es_ir_generate_block(builder, member->data.constructor_decl.body);
                            es_ir_return(builder, es_ir_imm(builder, 0));
                            ES_FREE(mangled);
                        } else if (member->type == AST_DESTRUCTOR_DECLARATION) {
                            EsIRParam* params = (EsIRParam*)ES_MALLOC(sizeof(EsIRParam));
                            if (params) {
                                params[0].name = "this";
                                params[0].type = TOKEN_UINT64;
                            }
                            char* mangled = es_ir_mangle_static_member(stmt->data.class_decl.name, "destructor");
                            EsIRFunction* func = es_ir_function_create(builder, mangled, params, 1, TOKEN_VOID);
                            es_ir_function_set_entry(builder, func);
                            if (params) {
                                ES_FREE(params);
                            }
                            EsIRBasicBlock* entry_block = func->entry_block;
                            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                                entry_block = es_ir_block_create(builder, "entry");
                                es_ir_block_set_current(builder, entry_block);
                            } else {
                                builder->current_block = entry_block;
                            }
                            es_ir_generate_block(builder, member->data.destructor_decl.body);
                            es_ir_return(builder, es_ir_imm(builder, 0));
                            ES_FREE(mangled);
                        } else if (member->type == AST_FUNCTION_DECLARATION) {
                            EsIRParam* params = NULL;
                            int pc = member->data.function_decl.parameter_count;
                            if (pc >= 0) {
                                params = (EsIRParam*)ES_MALLOC((pc + 1) * sizeof(EsIRParam));
                                if (params) {
                                    params[0].name = "this";
                                    params[0].type = TOKEN_UINT64;
                                    for (int i = 0; i < pc; i++) {
                                        params[i + 1].name = member->data.function_decl.parameters[i];
                                        params[i + 1].type = member->data.function_decl.parameter_types[i];
                                    }
                                }
                            }
                            const char* base_name = member->data.function_decl.name;
                            char* mangled = es_ir_mangle_static_member(stmt->data.class_decl.name, base_name);
                            EsIRFunction* func = es_ir_function_create(builder, mangled, params, pc + 1, member->data.function_decl.return_type);
                            es_ir_function_set_entry(builder, func);
                            if (params) {
                                ES_FREE(params);
                            }
                            EsIRBasicBlock* entry_block = func->entry_block;
                            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                                entry_block = es_ir_block_create(builder, "entry");
                                es_ir_block_set_current(builder, entry_block);
                            } else {
                                builder->current_block = entry_block;
                            }
                            es_ir_generate_block(builder, member->data.function_decl.body);
                            int has_return = es_ir_check_has_return(member->data.function_decl.body);
                            if (!has_return) {
                                es_ir_return(builder, es_ir_imm(builder, 0));
                            }
                            ES_FREE(mangled);
                        } else if (member->type == AST_STATIC_VARIABLE_DECLARATION) {
                            es_ir_generate_statement(builder, member);
                        } else if (member->type == AST_CLASS_DECLARATION) {
                            es_ir_generate_statement(builder, member);
                        }
                    }
                } else {
                    ASTNode* member = body;
                    if (member->type == AST_ACCESS_MODIFIER) {
                        member = member->data.access_modifier.member;
                    }
                    if (member && member->type == AST_STATIC_FUNCTION_DECLARATION) {
                        es_ir_generate_statement(builder, member);
                    } else if (member && member->type == AST_FUNCTION_DECLARATION) {
                        
                    } else if (member && member->type == AST_STATIC_VARIABLE_DECLARATION) {
                        es_ir_generate_statement(builder, member);
                    }
                }
            }
            es_ir_register_class_layout(builder, stmt->data.class_decl.name, stmt->data.class_decl.body);
            es_ir_pop_class_context(builder);
            break;
        }
        case AST_CONSTRUCTOR_DECLARATION:
        case AST_DESTRUCTOR_DECLARATION:
            break;
        case AST_STATIC_FUNCTION_DECLARATION: {
            EsIRParam* params = NULL;
            if (stmt->data.static_function_decl.parameter_count > 0) {
                params = (EsIRParam*)ES_MALLOC(stmt->data.static_function_decl.parameter_count * sizeof(EsIRParam));
                if (params) {
                    for (int i = 0; i < stmt->data.static_function_decl.parameter_count; i++) {
                        params[i].name = stmt->data.static_function_decl.parameters[i];
                        params[i].type = stmt->data.static_function_decl.parameter_types[i];
                    }
                }
            }
            const char* current_class = es_ir_current_class_context(builder);
            const char* base_name = stmt->data.static_function_decl.name;
            char* mangled_name = NULL;
            const char* function_name = base_name;
            if (current_class) {
                mangled_name = es_ir_mangle_static_member(current_class, base_name);
                if (mangled_name) {
                    function_name = mangled_name;
                }
            }
            EsIRFunction* func = es_ir_function_create(builder, function_name, params,
                                                       stmt->data.static_function_decl.parameter_count,
                                                       stmt->data.static_function_decl.return_type);
            es_ir_function_set_entry(builder, func);
            if (params) {
                ES_FREE(params);
            }
            EsIRBasicBlock* entry_block = func->entry_block;
            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                entry_block = es_ir_block_create(builder, "entry");
                es_ir_block_set_current(builder, entry_block);
            } else {
                builder->current_block = entry_block;
            }
            es_ir_generate_block(builder, stmt->data.static_function_decl.body);
            int has_return = es_ir_check_has_return(stmt->data.static_function_decl.body);
            if (!has_return) {
                es_ir_return(builder, es_ir_imm(builder, 0));
            }
            if (mangled_name) {
                ES_FREE(mangled_name);
            }
            break;
        }

        default:
#ifdef DEBUG
            
#endif
            es_ir_generate_expression(builder, stmt);
            return;
    }
}

static int es_ir_check_has_return(ASTNode* node) {
    if (!node) return 0;
    switch (node->type) {
        case AST_RETURN_STATEMENT: return 1;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.statement_count; i++) {
                if (es_ir_check_has_return(node->data.block.statements[i])) return 1;
            }
            return 0;
        case AST_IF_STATEMENT: {
            int then_has = es_ir_check_has_return(node->data.if_stmt.then_branch);
            int else_has = node->data.if_stmt.else_branch && es_ir_check_has_return(node->data.if_stmt.else_branch);
            return then_has && else_has;
        }
        case AST_WHILE_STATEMENT:
        case AST_FOR_STATEMENT:
        case AST_FOREACH_STATEMENT: return 0;
        default: return 0;
    }
}
static void es_ir_generate_block(EsIRBuilder* builder, ASTNode* block) {
    if (!block || block->type != AST_BLOCK) {
        return;
    }
    for (int i = 0; i < block->data.block.statement_count; i++) {
        es_ir_generate_statement(builder, block->data.block.statements[i]);
    }
}
void es_ir_generate_from_ast(EsIRBuilder* builder, ASTNode* ast, TypeCheckContext* type_context) {
    if (!builder || !ast) {
        return;
    }
    builder->type_context = type_context;
    if (!builder->module->main_function) {
        EsIRFunction* main_func = es_ir_function_create(builder, "main", NULL, 0, TOKEN_VOID);
        builder->module->main_function = main_func;
        es_ir_function_set_entry(builder, main_func);
    }
    if (ast->type == AST_BLOCK || ast->type == AST_PROGRAM) {
        
        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            if (stmt->type == AST_FUNCTION_DECLARATION ||
                stmt->type == AST_STATIC_FUNCTION_DECLARATION ||
                stmt->type == AST_CLASS_DECLARATION) {
                es_ir_generate_statement(builder, stmt);
            }
        }

        
        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            if (stmt->type != AST_FUNCTION_DECLARATION &&
                stmt->type != AST_STATIC_FUNCTION_DECLARATION &&
                stmt->type != AST_CLASS_DECLARATION) {
                es_ir_ensure_main_entry(builder);
                es_ir_generate_statement(builder, stmt);
            }
        }
    } else {
        es_ir_ensure_main_entry(builder);
        es_ir_generate_statement(builder, ast);
    }
    if (builder->current_function) {
        int has_main_program_code = 0;
        if (ast->type == AST_BLOCK || ast->type == AST_PROGRAM) {
            for (int i = 0; i < ast->data.block.statement_count; i++) {
                ASTNode* stmt = ast->data.block.statements[i];
                if (stmt->type != AST_FUNCTION_DECLARATION &&
                    stmt->type != AST_STATIC_FUNCTION_DECLARATION &&
                    stmt->type != AST_CLASS_DECLARATION) {
                    has_main_program_code = 1;
                    break;
                }
            }
        } else {
            has_main_program_code = 1;
        }
        if (has_main_program_code) {
            if (ast->type == AST_BLOCK || ast->type == AST_PROGRAM) {
                int main_has_return = 0;
                for (int i = 0; i < ast->data.block.statement_count; i++) {
                    ASTNode* stmt = ast->data.block.statements[i];
                    if (stmt->type != AST_FUNCTION_DECLARATION &&
                        stmt->type != AST_STATIC_FUNCTION_DECLARATION &&
                        stmt->type != AST_CLASS_DECLARATION) {
                        if (es_ir_check_has_return(stmt)) {
                            main_has_return = 1;
                            break;
                        }
                    }
                }
                if (!main_has_return) {
                    es_ir_return(builder, es_ir_imm(builder, 0));
                }
            } else if (ast->type != AST_RETURN_STATEMENT) {
                es_ir_return(builder, es_ir_imm(builder, 0));
            }
        }
    }
}
