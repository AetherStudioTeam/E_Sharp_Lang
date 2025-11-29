#include "ir.h"
#include "compiler.h"
#include "../common/output_cache.h"
#include "../type_check/type_check.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>


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
    if (!expr) return false;

    if (expr->type == AST_STRING) {
        return true;
    }


    if (expr->type == AST_IDENTIFIER && builder->current_function) {
        const char* var_name = expr->data.identifier_name;


        if (builder->current_function->params) {
            for (int i = 0; i < builder->current_function->param_count; i++) {
                if (strcmp(builder->current_function->params[i].name, var_name) == 0) {
                    return builder->current_function->params[i].type == TOKEN_STRING;
                }
            }
        }


        if (builder->type_context) {

            TypeCheckSymbolTable* scope = builder->type_context->current_scope;
            while (scope) {
                for (int i = 0; i < scope->symbol_count; i++) {
                    if (strcmp(scope->symbols[i].name, var_name) == 0) {

                        if (scope->symbols[i].type) {
                            return scope->symbols[i].type->kind == TYPE_STRING;
                        }
                    }
                }
                scope = scope->parent;
            }
        }


        return false;
    }

    if (expr->type == AST_BINARY_OPERATION && expr->data.binary_op.operator == TOKEN_PLUS) {
        return is_string_expression(builder, expr->data.binary_op.left) ||
               is_string_expression(builder, expr->data.binary_op.right);
    }

    return false;
}

static void es_ir_ensure_main_entry(EsIRBuilder* builder) {
    if (!builder) return;

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
        builder->current_block = func->entry_block;
        while (builder->current_block->next) {
            builder->current_block = builder->current_block->next;
        }
    }
}

static void es_ir_push_class_context(EsIRBuilder* builder, const char* class_name) {
    if (!builder || !class_name) return;

    if (builder->class_stack_size >= builder->class_stack_capacity) {
        builder->class_stack_capacity *= 2;
        builder->class_name_stack = ES_REALLOC(builder->class_name_stack,
                                               builder->class_stack_capacity * sizeof(char*));
    }

    builder->class_name_stack[builder->class_stack_size] = ES_STRDUP(class_name);
    builder->class_stack_size++;
}

static void es_ir_pop_class_context(EsIRBuilder* builder) {
    if (!builder || builder->class_stack_size <= 0) return;

    builder->class_stack_size--;
    ES_FREE(builder->class_name_stack[builder->class_stack_size]);
    builder->class_name_stack[builder->class_stack_size] = NULL;
}

static const char* es_ir_current_class_context(EsIRBuilder* builder) {
    if (!builder || builder->class_stack_size <= 0) return NULL;
    return builder->class_name_stack[builder->class_stack_size - 1];
}

static char* es_ir_mangle_static_member(const char* class_name, const char* member_name) {
    if (!class_name || !member_name) return NULL;

    size_t class_len = strlen(class_name);
    size_t member_len = strlen(member_name);
    size_t total_len = class_len + member_len + 3;

    char* mangled = ES_MALLOC(total_len);
    if (!mangled) return NULL;

    snprintf(mangled, total_len, "%s__%s", class_name, member_name);
    return mangled;
}

static int es_ir_evaluate_numeric_constant(ASTNode* expr, double* out_value) {
    if (!expr || !out_value) return 0;

    switch (expr->type) {
        case AST_NUMBER:
            *out_value = expr->data.number_value;
            return 1;
        case AST_BOOLEAN:
            *out_value = expr->data.boolean_value ? 1 : 0;
            return 1;
        default:
            return 0;
    }
}


static bool try_extract_constant(ASTNode* node, double* value) {
    if (!node || !value) return false;

    if (node->type == AST_NUMBER) {
        *value = node->data.number_value;
        return true;
    }

    return false;
}

static EsIRValue es_ir_generate_expression(EsIRBuilder* builder, ASTNode* expr) {
    if (!expr) {
        EsIRValue void_val = {0};
        void_val.type = ES_IR_VALUE_VOID;
        return void_val;
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
            EsIRValue* args = ES_MALLOC(sizeof(EsIRValue));
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
                while (f) { if (strcmp(f->name, ctor_name) == 0) { exists = true; break; } f = f->next; }
                if (!exists) { es_ir_function_create(builder, ctor_name, NULL, -1, TOKEN_VOID); }
                int ac = expr->data.new_expr.argument_count;
                EsIRValue* cargs = ES_MALLOC((ac + 1) * sizeof(EsIRValue));
                cargs[0] = obj;
                for (int i = 0; i < ac; i++) { cargs[i + 1] = es_ir_generate_expression(builder, expr->data.new_expr.arguments[i]); }
                es_ir_call(builder, ctor_name, cargs, ac + 1);
                ES_FREE(cargs);
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
                            if (mangled) { es_ir_store(builder, mangled, rhs); ES_FREE(mangled); return rhs; }
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

                        EsIRValue string_lhs = lhs;
                        EsIRValue string_rhs = rhs;


                        if (left_is_string && !right_is_string) {

                            string_rhs = es_ir_call(builder, "_es_int_to_string_value", &rhs, 1);
                        }

                        else if (!left_is_string && right_is_string) {

                            string_lhs = es_ir_call(builder, "_es_int_to_string_value", &lhs, 1);
                        }

                        return es_ir_strcat(builder, string_lhs, string_rhs);
                    } else {
                        return es_ir_add(builder, lhs, rhs);
                    }
                }
                case TOKEN_MINUS: return es_ir_sub(builder, lhs, rhs);
                case TOKEN_MULTIPLY: return es_ir_mul(builder, lhs, rhs);
                case TOKEN_DIVIDE: return es_ir_div(builder, lhs, rhs);
                case TOKEN_LESS: return es_ir_compare(builder, ES_IR_LT, lhs, rhs);
                case TOKEN_GREATER: return es_ir_compare(builder, ES_IR_GT, lhs, rhs);
                case TOKEN_EQUAL: return es_ir_compare(builder, ES_IR_EQ, lhs, rhs);
                case TOKEN_LESS_EQUAL: return es_ir_compare(builder, ES_IR_LE, lhs, rhs);
                case TOKEN_GREATER_EQUAL: return es_ir_compare(builder, ES_IR_GE, lhs, rhs);
                case TOKEN_NOT_EQUAL: return es_ir_compare(builder, ES_IR_NE, lhs, rhs);
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
                case TOKEN_BOOL: {

                    return es_ir_cast(builder, operand_value, expr->data.unary_op.operator);
                }

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
            if (method_name && strcmp(method_name, "delete") == 0) {
                char* dtor_name = es_ir_mangle_static_member(class_name, "destructor");
                if (dtor_name) {
                    bool exists = false;
                    EsIRFunction* f = builder->module->functions;
                    while (f) { if (strcmp(f->name, dtor_name) == 0) { exists = true; break; } f = f->next; }
                    if (!exists) { es_ir_function_create(builder, dtor_name, NULL, -1, TOKEN_VOID); }
                    if (expr->data.static_call.argument_count > 0) {
                        EsIRValue obj = es_ir_generate_expression(builder, expr->data.static_call.arguments[0]);
                        EsIRValue* dargs = ES_MALLOC(sizeof(EsIRValue));
                        dargs[0] = obj;
                        es_ir_call(builder, dtor_name, dargs, 1);
                        ES_FREE(dargs);
                        EsIRValue* fargs = ES_MALLOC(sizeof(EsIRValue));
                        fargs[0] = obj;
                        es_ir_call(builder, "es_free", fargs, 1);
                        ES_FREE(fargs);
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
                args = ES_MALLOC(expr->data.static_call.argument_count * sizeof(EsIRValue));
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
            const char* alt_mangled = NULL;
            if (expr->data.call.object && expr->data.call.object->type == AST_IDENTIFIER && strcmp(expr->data.call.name, "delete") == 0) {
                const char* class_name = expr->data.call.object->data.identifier_name;
                char* dtor_name = es_ir_mangle_static_member(class_name, "destructor");
                if (dtor_name) {
                    bool exists = false;
                    EsIRFunction* f = builder->module->functions;
                    while (f) { if (strcmp(f->name, dtor_name) == 0) { exists = true; break; } f = f->next; }
                    if (!exists) { es_ir_function_create(builder, dtor_name, NULL, -1, TOKEN_VOID); }
                    if (expr->data.call.argument_count > 0) {
                        EsIRValue obj = es_ir_generate_expression(builder, expr->data.call.arguments[0]);
                        EsIRValue* dargs = ES_MALLOC(sizeof(EsIRValue));
                        dargs[0] = obj;
                        es_ir_call(builder, dtor_name, dargs, 1);
                        ES_FREE(dargs);
                        EsIRValue* fargs = ES_MALLOC(sizeof(EsIRValue));
                        fargs[0] = obj;
                        es_ir_call(builder, "es_free", fargs, 1);
                        ES_FREE(fargs);
                    }
                    ES_FREE(dtor_name);
                }
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }
            if (expr->data.call.object && expr->data.call.object->type == AST_IDENTIFIER) {
                if (expr->data.call.resolved_class_name) {
                    char* mangled = es_ir_mangle_static_member(expr->data.call.resolved_class_name, expr->data.call.name);
                    if (mangled) {
                        EsIRValue* iargs = ES_MALLOC((expr->data.call.argument_count + 1) * sizeof(EsIRValue));
                        iargs[0] = es_ir_generate_expression(builder, expr->data.call.object);
                        for (int i = 0; i < expr->data.call.argument_count; i++) {
                            iargs[i + 1] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                        }
                        EsIRValue result = es_ir_call(builder, mangled, iargs, expr->data.call.argument_count + 1);
                        ES_FREE(iargs);
                        ES_FREE(mangled);
                        return result;
                    }
                } else {
                    const char* class_name = expr->data.call.object->data.identifier_name;
                    char* mangled = es_ir_mangle_static_member(class_name, func_name);
                    if (mangled) {
                        alt_mangled = mangled;
                        func_name = mangled;
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
            EsIRFunction* func = builder->module->functions;
            while (func) {
                if (strcmp(func->name, func_name) == 0) {
                    function_exists = true;
                    break;
                }
                func = func->next;
            }


            if (!function_exists) {
                es_ir_function_create(builder, func_name, NULL, -1, TOKEN_VOID);
                ES_WARNING("Function '%s' called before definition - creating forward reference", func_name);
            }


            EsIRValue* args = ES_MALLOC(expr->data.call.argument_count * sizeof(EsIRValue));
            if (!args) {
                EsIRValue void_val = {0};
                void_val.type = ES_IR_VALUE_VOID;
                return void_val;
            }

            for (int i = 0; i < expr->data.call.argument_count; i++) {
                args[i] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
            }


            if (expr->data.call.object && expr->data.call.object->type == AST_THIS) {
                const char* current_class = es_ir_current_class_context(builder);
                if (current_class) {
                    char* mangled = es_ir_mangle_static_member(current_class, expr->data.call.name);
                    if (mangled) {
                        EsIRValue* iargs = ES_MALLOC((expr->data.call.argument_count + 1) * sizeof(EsIRValue));
                        iargs[0] = es_ir_arg(builder, 0);
                        for (int i = 0; i < expr->data.call.argument_count; i++) {
                            iargs[i + 1] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                        }
                        EsIRValue result = es_ir_call(builder, mangled, iargs, expr->data.call.argument_count + 1);
                        ES_FREE(iargs);
                        ES_FREE(mangled);
                        if (alt_mangled) { ES_FREE((char*)alt_mangled); }
                        return result;
                    }
                }
            }

            if (expr->data.call.resolved_class_name && expr->data.call.object) {
                char* mangled = es_ir_mangle_static_member(expr->data.call.resolved_class_name, expr->data.call.name);
                if (mangled) {
                    EsIRValue* iargs = ES_MALLOC((expr->data.call.argument_count + 1) * sizeof(EsIRValue));
                    iargs[0] = es_ir_generate_expression(builder, expr->data.call.object);
                    for (int i = 0; i < expr->data.call.argument_count; i++) {
                        iargs[i + 1] = es_ir_generate_expression(builder, expr->data.call.arguments[i]);
                    }
                    EsIRValue result = es_ir_call(builder, mangled, iargs, expr->data.call.argument_count + 1);
                    ES_FREE(iargs);
                    ES_FREE(mangled);
                    return result;
                }
            }

            EsIRValue result = es_ir_call(builder, func_name, args, expr->data.call.argument_count);
            if (alt_mangled) {
                ES_FREE((char*)alt_mangled);
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

            EsIRBasicBlock* true_block = es_ir_block_create(builder, true_label);
            EsIRBasicBlock* false_block = es_ir_block_create(builder, false_label);
            EsIRBasicBlock* end_block = es_ir_block_create(builder, end_label);


            char result_var[32];
            snprintf(result_var, sizeof(result_var), "result_%d", builder->temp_counter++);


            es_ir_branch(builder, cond, true_block, false_block);


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
            #ifdef DEBUG
            ES_COMPILER_DEBUG("IR_GEN: Generating array literal with %d elements", element_count);
            #endif

            if (element_count == 0) {

                #ifdef DEBUG
                ES_COMPILER_DEBUG("IR_GEN: Empty array literal");
                #endif
                EsIRValue* args = ES_MALLOC(sizeof(EsIRValue));
                args[0] = es_ir_imm(builder, 8);
                EsIRValue array_ptr = es_ir_call(builder, "es_malloc", args, 1);
                ES_FREE(args);
#ifdef DEBUG
                ES_COMPILER_DEBUG("IR_GEN: Empty array literal returning temp value type %d", array_ptr.type);
#endif
                return array_ptr;
            }


            EsIRValue element_size = es_ir_imm(builder, 8);
            EsIRValue count_val = es_ir_imm(builder, element_count);
            EsIRValue total_size = es_ir_mul(builder, count_val, element_size);


            EsIRValue* args = ES_MALLOC(sizeof(EsIRValue));
            args[0] = total_size;
            #ifdef DEBUG
            ES_COMPILER_DEBUG("IR_GEN: About to call es_malloc for array literal");
            #endif
            EsIRValue array_ptr = es_ir_call(builder, "es_malloc", args, 1);
            ES_FREE(args);
            #ifdef DEBUG
            ES_COMPILER_DEBUG("IR_GEN: es_malloc returned temp value type %d", array_ptr.type);
            #endif


            for (int i = 0; i < element_count; i++) {
                #ifdef DEBUG
                ES_COMPILER_DEBUG("IR_GEN: Initializing array element %d", i);
                #endif
                EsIRValue element_val = es_ir_generate_expression(builder, expr->data.array_literal.elements[i]);
                EsIRValue index_val = es_ir_imm(builder, i);
                es_ir_array_store(builder, array_ptr, index_val, element_val);
            }

            #ifdef DEBUG
            ES_COMPILER_DEBUG("IR_GEN: Array literal returning temp value type %d", array_ptr.type);
            #endif
            return array_ptr;
        }

        default: {
            EsIRValue void_val = {0};
            void_val.type = ES_IR_VALUE_VOID;
            return void_val;
        }
    }
}


static void es_ir_generate_statement(EsIRBuilder* builder, ASTNode* stmt) {
    if (!stmt) return;

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

        case AST_ARRAY_ASSIGNMENT: {

            EsIRValue array_value = es_ir_generate_expression(builder, stmt->data.array_assignment.array);


            EsIRValue index_value = es_ir_generate_expression(builder, stmt->data.array_assignment.index);


            EsIRValue value = es_ir_generate_expression(builder, stmt->data.array_assignment.value);


            es_ir_array_store(builder, array_value, index_value, value);
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

        case AST_PRINT_STATEMENT: {
            #ifdef DEBUG
            ES_DEBUG_IR("Generating print statement with %d values", stmt->data.print_stmt.value_count);
            #endif
            for (int i = 0; i < stmt->data.print_stmt.value_count; i++) {
                #ifdef DEBUG
                ES_DEBUG_IR("About to generate expression for print value %d, AST node type: %d", i, stmt->data.print_stmt.values[i]->type);
                #endif
                EsIRValue value = es_ir_generate_expression(builder, stmt->data.print_stmt.values[i]);
                #ifdef DEBUG
                ES_DEBUG_IR("Generated expression for print value %d, type: %d", i, value.type);
                if (value.type == ES_IR_VALUE_VOID) {
                    ES_DEBUG_IR("Skipping print value %d because it's VOID (unsupported expression type)", i);
                    continue;
                }
                #endif

                EsIRValue* args = ES_MALLOC(sizeof(EsIRValue));
                if (!args) {
                    break;
                }
                args[0] = value;

                ASTNode* expr = stmt->data.print_stmt.values[i];
            const char* func_name = "print_number";
            if (expr->type == AST_STRING || is_string_expression(builder, expr)) {
                func_name = "print_string";
            }

            #ifdef DEBUG
            ES_DEBUG_IR("Calling %s", func_name);
            #endif
            EsIRValue result = es_ir_call(builder, func_name, args, 1);
            #ifdef DEBUG
            ES_DEBUG_IR("%s call returned, type: %d", func_name, result.type);
            #endif
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

            #ifdef DEBUG
            ES_DEBUG_IR("Switch statement creating end block with label: %s", end_label);
            #endif


            es_ir_push_loop_context(builder, NULL, end_block);

            char** case_labels = malloc(stmt->data.switch_stmt.case_count * sizeof(char*));
            EsIRBasicBlock** case_blocks = malloc(stmt->data.switch_stmt.case_count * sizeof(EsIRBasicBlock*));

            for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                case_labels[i] = malloc(32);
                int case_label_id = builder->label_counter++;
                snprintf(case_labels[i], 32, "case_%d_%d", case_label_id, i + 1);
                case_blocks[i] = es_ir_block_create(builder, case_labels[i]);
            }

            EsIRBasicBlock* default_block = NULL;
            char* default_label = NULL;
            if (stmt->data.switch_stmt.default_case) {
                default_label = malloc(32);
                int default_label_id = builder->label_counter++;
                snprintf(default_label, 32, "default_%d", default_label_id);
                default_block = es_ir_block_create(builder, default_label);
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

            for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                free(case_labels[i]);
            }
            free(case_labels);
            free(case_blocks);
            free(default_label);
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
            #ifdef DEBUG
            if (break_block) {
                ES_DEBUG_IR("Break statement jumping to block with label: %s", break_block->label);
            } else {
                ES_DEBUG_IR("Break statement found no break block");
            }
            #endif
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
                    while (f) { if (strcmp(f->name, dtor_name) == 0) { exists = true; break; } f = f->next; }
                    if (!exists) { es_ir_function_create(builder, dtor_name, NULL, -1, TOKEN_VOID); }
                    EsIRValue* dargs = ES_MALLOC(sizeof(EsIRValue));
                    dargs[0] = obj;
                    es_ir_call(builder, dtor_name, dargs, 1);
                    ES_FREE(dargs);
                    EsIRValue* fargs = ES_MALLOC(sizeof(EsIRValue));
                    fargs[0] = obj;
                    es_ir_call(builder, "es_free", fargs, 1);
                    ES_FREE(fargs);
                    ES_FREE(dtor_name);
                }
            }
            break;
        }

        case AST_VARIABLE_DECLARATION: {
#ifdef DEBUG
            ES_COMPILER_DEBUG("IR generation for variable declaration: %s", stmt->data.variable_decl.name);
#endif


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
                        if (array_size <= 0) array_size = 10;
                    }
                }


                int total_bytes = array_size * 8;


                es_ir_alloc(builder, stmt->data.variable_decl.name);
#ifdef DEBUG
                ES_COMPILER_DEBUG("Generated ES_IR_ALLOC for C-style array variable %s", stmt->data.variable_decl.name);
#endif


                EsIRValue* args = ES_MALLOC(sizeof(EsIRValue));
                args[0] = es_ir_imm(builder, total_bytes);
                EsIRValue array_ptr = es_ir_call(builder, "es_malloc", args, 1);
                ES_FREE(args);


                es_ir_store(builder, stmt->data.variable_decl.name, array_ptr);

            } else if (is_array_literal) {

#ifdef DEBUG
                ES_COMPILER_DEBUG("IR_GEN: About to generate ES_IR_ALLOC for array literal variable %s", stmt->data.variable_decl.name);
#endif
                es_ir_alloc(builder, stmt->data.variable_decl.name);
#ifdef DEBUG
                ES_COMPILER_DEBUG("Generated ES_IR_ALLOC for array literal variable %s", stmt->data.variable_decl.name);
#endif

                if (stmt->data.variable_decl.value) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("IR_GEN: About to generate expression for array literal");
#endif
                    EsIRValue init_value = es_ir_generate_expression(builder, stmt->data.variable_decl.value);
#ifdef DEBUG
                    ES_COMPILER_DEBUG("IR_GEN: Generated expression for array literal, type: %d", init_value.type);
#endif
#ifdef DEBUG
                    ES_COMPILER_DEBUG("IR_GEN: About to generate ES_IR_STORE to variable %s", stmt->data.variable_decl.name);
#endif
#ifdef DEBUG
                    ES_COMPILER_DEBUG("IR_GEN: init_value type: %d, temp index: %d", init_value.type,
                            init_value.type == ES_IR_VALUE_TEMP ? init_value.data.index : -1);
#endif
                    es_ir_store(builder, stmt->data.variable_decl.name, init_value);
#ifdef DEBUG
                    ES_COMPILER_DEBUG("IR_GEN: Generated ES_IR_STORE to variable %s", stmt->data.variable_decl.name);
#endif
                }
            } else {

                es_ir_alloc(builder, stmt->data.variable_decl.name);
#ifdef DEBUG
                ES_COMPILER_DEBUG("Generated ES_IR_ALLOC for regular variable %s", stmt->data.variable_decl.name);
#endif

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
                params = malloc(stmt->data.function_decl.parameter_count * sizeof(EsIRParam));
                for (int i = 0; i < stmt->data.function_decl.parameter_count; i++) {
                    params[i].name = stmt->data.function_decl.parameters[i];
                    params[i].type = stmt->data.function_decl.parameter_types[i];
                }
            }

            EsIRFunction* func = es_ir_function_create(builder, stmt->data.function_decl.name, params, stmt->data.function_decl.parameter_count, stmt->data.function_decl.return_type);
            es_ir_function_set_entry(builder, func);


            if (params) {
                free(params);
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
                                params = ES_MALLOC((pc + 1) * sizeof(EsIRParam));
                                params[0].name = "this";
                                params[0].type = TOKEN_UINT64;
                                for (int i = 0; i < pc; i++) {
                                    params[i + 1].name = member->data.constructor_decl.parameters[i];
                                    params[i + 1].type = member->data.constructor_decl.parameter_types[i];
                                }
                            }
                            char* mangled = es_ir_mangle_static_member(stmt->data.class_decl.name, "constructor");
                            EsIRFunction* func = es_ir_function_create(builder, mangled, params, pc + 1, TOKEN_VOID);
                            es_ir_function_set_entry(builder, func);
                            if (params) { ES_FREE(params); }
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
                            EsIRParam* params = ES_MALLOC(sizeof(EsIRParam));
                            params[0].name = "this";
                            params[0].type = TOKEN_UINT64;
                            char* mangled = es_ir_mangle_static_member(stmt->data.class_decl.name, "destructor");
                            EsIRFunction* func = es_ir_function_create(builder, mangled, params, 1, TOKEN_VOID);
                            es_ir_function_set_entry(builder, func);
                            if (params) { ES_FREE(params); }
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
                                params = ES_MALLOC((pc + 1) * sizeof(EsIRParam));
                                params[0].name = "this";
                                params[0].type = TOKEN_UINT64;
                                for (int i = 0; i < pc; i++) {
                                    params[i + 1].name = member->data.function_decl.parameters[i];
                                    params[i + 1].type = member->data.function_decl.parameter_types[i];
                                }
                            }
                            const char* base_name = member->data.function_decl.name;
                            char* mangled = es_ir_mangle_static_member(stmt->data.class_decl.name, base_name);
                            EsIRFunction* func = es_ir_function_create(builder, mangled, params, pc + 1, member->data.function_decl.return_type);
                            es_ir_function_set_entry(builder, func);
                            if (params) { ES_FREE(params); }
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
                params = malloc(stmt->data.static_function_decl.parameter_count * sizeof(EsIRParam));
                for (int i = 0; i < stmt->data.static_function_decl.parameter_count; i++) {
                    params[i].name = stmt->data.static_function_decl.parameters[i];
                    params[i].type = stmt->data.static_function_decl.parameter_types[i];
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

            EsIRFunction* func = es_ir_function_create(builder, function_name,
                                                       params,
                                                       stmt->data.static_function_decl.parameter_count,
                                                       stmt->data.static_function_decl.return_type);
            es_ir_function_set_entry(builder, func);


            if (params) {
                free(params);
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

            es_ir_generate_expression(builder, stmt);
            return;
    }
}


static int es_ir_check_has_return(ASTNode* node) {
    if (!node) return 0;

    switch (node->type) {
        case AST_RETURN_STATEMENT:
            return 1;

        case AST_BLOCK:

            for (int i = 0; i < node->data.block.statement_count; i++) {
                if (es_ir_check_has_return(node->data.block.statements[i])) {
                    return 1;
                }
            }
            return 0;

        case AST_IF_STATEMENT:

            int then_has = es_ir_check_has_return(node->data.if_stmt.then_branch);
            int else_has = node->data.if_stmt.else_branch && es_ir_check_has_return(node->data.if_stmt.else_branch);

            return then_has && else_has;

        case AST_WHILE_STATEMENT:
        case AST_FOR_STATEMENT:

            return 0;

        default:
            return 0;
    }
}


static void es_ir_generate_block(EsIRBuilder* builder, ASTNode* block) {
    if (!block || block->type != AST_BLOCK) return;

    for (int i = 0; i < block->data.block.statement_count; i++) {
        es_ir_generate_statement(builder, block->data.block.statements[i]);
    }
}


void es_ir_generate_from_ast(EsIRBuilder* builder, ASTNode* ast, TypeCheckContext* type_context) {
    if (!builder || !ast) return;


    builder->type_context = type_context;

    #ifdef DEBUG
    ES_DEBUG_IR("Starting IR generation from AST");
    #endif


    if (!builder->module->main_function) {
        #ifdef DEBUG
        ES_DEBUG_IR("Creating main function");
        #endif
        EsIRFunction* main_func = es_ir_function_create(builder, "main", NULL, 0, TOKEN_VOID);
        builder->module->main_function = main_func;
        es_ir_function_set_entry(builder, main_func);
    }


    if (ast->type == AST_BLOCK || ast->type == AST_PROGRAM) {
        #ifdef DEBUG
        ES_DEBUG_IR("Processing AST node type %d with %d statements", ast->type, ast->data.block.statement_count);
        #endif


        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            #ifdef DEBUG
            ES_DEBUG_IR("Statement %d type: %d", i, stmt->type);
            #endif
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
                #ifdef DEBUG
                ES_DEBUG_IR("Generating non-function statement type: %d", stmt->type);
                #endif
                es_ir_generate_statement(builder, stmt);
            }
        }
    } else {

        #ifdef DEBUG
        ES_DEBUG_IR("Generating single statement type: %d", ast->type);
        #endif
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