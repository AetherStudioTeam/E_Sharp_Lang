#include "type_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/es_common.h"
#include "../compiler/symbol_table.h"


Type* infer_return_type_from_statement(TypeCheckContext* context, ASTNode* statement);


Type* infer_function_return_type(TypeCheckContext* context, ASTNode* function_body);


static int type_check_class_body(TypeCheckContext* context, ClassInfo* class_info, ASTNode* body, AccessModifier default_access);


static int type_check_add_class_member(TypeCheckContext* context, ClassInfo* class_info,
                                       ASTNode* member, AccessModifier access, int is_static);


static TypeCheckSymbolTable* type_check_symbol_table_create(TypeCheckSymbolTable* parent);
static void type_check_symbol_table_destroy(TypeCheckSymbolTable* table);
static void type_check_symbol_table_ref(TypeCheckSymbolTable* table);
static void type_check_symbol_table_unref(TypeCheckSymbolTable* table);
static int type_check_symbol_table_add(TypeCheckSymbolTable* table, TypeCheckSymbol symbol);
static TypeCheckSymbol* type_check_symbol_table_lookup(TypeCheckSymbolTable* table, const char* name);


Type* type_create_basic(TypeKind kind) {
    Type* type = (Type*)ES_MALLOC(sizeof(Type));
    if (!type) {
        return NULL;
    }
    type->kind = kind;
    return type;
}


Type* type_create_pointer(Type* base_type) {
    Type* type = (Type*)ES_MALLOC(sizeof(Type));
    if (!type) {
        return NULL;
    }
    type->kind = TYPE_POINTER;
    type->data.pointer_to = base_type;
    return type;
}


Type* type_create_array(Type* element_type, int size) {
#ifdef DEBUG
    ES_COMPILER_DEBUG("type_create_array called with element_type: %p, size: %d", (void*)element_type, size);
#endif
    Type* type = (Type*)ES_MALLOC(sizeof(Type));
    if (!type) {
        return NULL;
    }
    type->kind = TYPE_ARRAY;
    type->data.array.element_type = element_type;
    type->data.array.size = size;
#ifdef DEBUG
    ES_COMPILER_DEBUG("type_create_array completed, array type: %p, element_type: %p", (void*)type, (void*)type->data.array.element_type);
#endif
    return type;
}


Type* type_create_function(Type* return_type, Type** param_types, int param_count) {
    Type* type = (Type*)ES_MALLOC(sizeof(Type));
    if (!type) {
        return NULL;
    }
    type->kind = TYPE_FUNCTION;
    type->data.function.return_type = return_type;
    type->data.function.parameter_types = param_types;
    type->data.function.parameter_count = param_count;
    return type;
}


Type* type_create_class(const char* class_name) {
    Type* type = (Type*)ES_MALLOC(sizeof(Type));
    if (!type) {
        return NULL;
    }
    type->kind = TYPE_CLASS;
    type->data.class.class_name = strdup(class_name);
    type->data.class.field_types = NULL;
    type->data.class.field_names = NULL;
    type->data.class.field_count = 0;
    type->data.class.class_info = NULL;
    return type;
}


ClassInfo* class_info_create(const char* class_name) {
    ClassInfo* info = (ClassInfo*)ES_MALLOC(sizeof(ClassInfo));
    if (!info) {
        return NULL;
    }
    info->class_name = strdup(class_name);
    info->base_class_name = NULL;
    info->members = NULL;
    info->member_count = 0;
    info->member_capacity = 0;
    info->member_scope = type_check_symbol_table_create(NULL);
    return info;
}


void class_info_destroy(ClassInfo* class_info) {
    if (!class_info) return;

    ES_FREE(class_info->class_name);
    if (class_info->base_class_name) {
        ES_FREE(class_info->base_class_name);
    }


    if (class_info->member_scope) {
        type_check_symbol_table_unref(class_info->member_scope);
        class_info->member_scope = NULL;
    }


    for (int i = 0; i < class_info->member_count; i++) {
        if (class_info->members[i].name) {
            ES_FREE(class_info->members[i].name);
        }
        if (class_info->members[i].type) {
            type_destroy(class_info->members[i].type);
        }
    }

    if (class_info->members) {
        ES_FREE(class_info->members);
    }

    ES_FREE(class_info);
}


void class_info_add_member(ClassInfo* class_info, ClassMember member) {
    if (!class_info) return;


    if (class_info->member_count >= class_info->member_capacity) {
        int new_capacity = class_info->member_capacity == 0 ? 16 : class_info->member_capacity * 2;
        ClassMember* new_members = (ClassMember*)realloc(class_info->members,
                                                         new_capacity * sizeof(ClassMember));
        if (!new_members) {
            return;
        }
        class_info->members = new_members;
        class_info->member_capacity = new_capacity;
    }


    class_info->members[class_info->member_count] = member;
    class_info->member_count++;



}


ClassMember* class_info_find_member(ClassInfo* class_info, const char* member_name) {
    if (!class_info || !member_name) return NULL;

    for (int i = 0; i < class_info->member_count; i++) {
        if (strcmp(class_info->members[i].name, member_name) == 0) {
            return &class_info->members[i];
        }
    }

    return NULL;
}


AccessModifier token_to_access_modifier(EsTokenType token) {
    switch (token) {
        case TOKEN_PUBLIC:
            return ACCESS_PUBLIC;
        case TOKEN_PRIVATE:
            return ACCESS_PRIVATE;
        case TOKEN_PROTECTED:
            return ACCESS_PROTECTED;
        default:
            return ACCESS_PUBLIC;
    }
}


void type_function_add_parameter(Type* func_type, Type* param_type) {
    if (!func_type || func_type->kind != TYPE_FUNCTION || !param_type) {
        return;
    }


    int new_count = func_type->data.function.parameter_count + 1;
    Type** new_params = (Type**)realloc(func_type->data.function.parameter_types,
                                       new_count * sizeof(Type*));
    if (!new_params) {
        return;
    }

    func_type->data.function.parameter_types = new_params;
    func_type->data.function.parameter_types[func_type->data.function.parameter_count] = param_type;
    func_type->data.function.parameter_count = new_count;
}


void type_destroy(Type* type) {
    if (!type) return;

#ifdef DEBUG
    ES_COMPILER_DEBUG("type_destroy called for type %p, kind: %d", (void*)type, type->kind);
#endif
    if (type->kind == TYPE_ARRAY) {
#ifdef DEBUG
        ES_COMPILER_DEBUG("About to destroy array element type: %p", (void*)type->data.array.element_type);
#endif
    }

    switch (type->kind) {
        case TYPE_POINTER:
            type_destroy(type->data.pointer_to);
            break;
        case TYPE_ARRAY:
            type_destroy(type->data.array.element_type);
            break;
        case TYPE_FUNCTION:
            type_destroy(type->data.function.return_type);
            for (int i = 0; i < type->data.function.parameter_count; i++) {
                type_destroy(type->data.function.parameter_types[i]);
            }
            ES_FREE(type->data.function.parameter_types);
            break;
        case TYPE_CLASS:
            if (type->data.class.class_name) {
                ES_FREE(type->data.class.class_name);
                type->data.class.class_name = NULL;
            }
            for (int i = 0; i < type->data.class.field_count; i++) {
                if (type->data.class.field_types[i]) {
                    type_destroy(type->data.class.field_types[i]);
                    type->data.class.field_types[i] = NULL;
                }
                if (type->data.class.field_names[i]) {
                    ES_FREE(type->data.class.field_names[i]);
                    type->data.class.field_names[i] = NULL;
                }
            }
            ES_FREE(type->data.class.field_types);
            type->data.class.field_types = NULL;
            ES_FREE(type->data.class.field_names);
            type->data.class.field_names = NULL;

            break;
        default:
            break;
    }

    ES_FREE(type);
}


Type* type_copy(Type* type) {
    if (!type) return NULL;

    Type* new_type = ES_MALLOC(sizeof(Type));
    if (!new_type) return NULL;


    new_type->kind = type->kind;


    switch (type->kind) {
        case TYPE_POINTER:
            new_type->data.pointer_to = type_copy(type->data.pointer_to);
            break;

        case TYPE_ARRAY:
#ifdef DEBUG
            ES_COMPILER_DEBUG("type_copy - copying array, element_type: %p", (void*)type->data.array.element_type);
#endif
            new_type->data.array.element_type = type_copy(type->data.array.element_type);
            new_type->data.array.size = type->data.array.size;
#ifdef DEBUG
            ES_COMPILER_DEBUG("type_copy - copied array, new element_type: %p", (void*)new_type->data.array.element_type);
#endif
            break;

        case TYPE_FUNCTION:

            new_type->data.function.return_type = type_copy(type->data.function.return_type);


            if (type->data.function.parameter_count > 0) {
                new_type->data.function.parameter_types = ES_MALLOC(sizeof(Type*) * type->data.function.parameter_count);
                if (!new_type->data.function.parameter_types) {
                    type_destroy(new_type);
                    return NULL;
                }

                for (int i = 0; i < type->data.function.parameter_count; i++) {
                    new_type->data.function.parameter_types[i] = type_copy(type->data.function.parameter_types[i]);
                }
                new_type->data.function.parameter_count = type->data.function.parameter_count;
            } else {
                new_type->data.function.parameter_types = NULL;
                new_type->data.function.parameter_count = 0;
            }
            break;

        case TYPE_CLASS:

            if (type->data.class.class_name) {
                new_type->data.class.class_name = strdup(type->data.class.class_name);
            } else {
                new_type->data.class.class_name = NULL;
            }


            if (type->data.class.field_count > 0) {
                new_type->data.class.field_types = ES_MALLOC(sizeof(Type*) * type->data.class.field_count);
                new_type->data.class.field_names = ES_MALLOC(sizeof(char*) * type->data.class.field_count);

                if (!new_type->data.class.field_types || !new_type->data.class.field_names) {
                    if (new_type->data.class.field_types) ES_FREE(new_type->data.class.field_types);
                    if (new_type->data.class.field_names) ES_FREE(new_type->data.class.field_names);
                    type_destroy(new_type);
                    return NULL;
                }

                for (int i = 0; i < type->data.class.field_count; i++) {
                    new_type->data.class.field_types[i] = type_copy(type->data.class.field_types[i]);
                    if (type->data.class.field_names[i]) {
                        new_type->data.class.field_names[i] = strdup(type->data.class.field_names[i]);
                    } else {
                        new_type->data.class.field_names[i] = NULL;
                    }
                }
                new_type->data.class.field_count = type->data.class.field_count;
            } else {
                new_type->data.class.field_types = NULL;
                new_type->data.class.field_names = NULL;
                new_type->data.class.field_count = 0;
            }



            new_type->data.class.class_info = type->data.class.class_info;
            break;

        default:

            break;
    }

    return new_type;
}


static TypeCheckSymbolTable* type_check_symbol_table_create(TypeCheckSymbolTable* parent) {
    TypeCheckSymbolTable* table = (TypeCheckSymbolTable*)ES_MALLOC(sizeof(TypeCheckSymbolTable));
    if (!table) {
        return NULL;
    }

    table->symbols = NULL;
    table->symbol_count = 0;
    table->capacity = 0;
    table->parent = parent;
    table->ref_count = 1;


    if (parent) {
        type_check_symbol_table_ref(parent);
    }

    return table;
}

static void type_check_symbol_table_ref(TypeCheckSymbolTable* table) {
    if (table) {
        table->ref_count++;
    }
}

static void type_check_symbol_table_unref(TypeCheckSymbolTable* table) {
    if (!table) return;

    table->ref_count--;
    if (table->ref_count <= 0) {
        type_check_symbol_table_destroy(table);
    }
}


static void type_check_symbol_table_destroy(TypeCheckSymbolTable* table) {
    if (!table) return;

    ES_DEBUG("=== TYPE_CHECK: Destroying symbol table at %p, symbols=%p, count=%d, ref_count=%d ===",
           (void*)table, (void*)table->symbols, table->symbol_count, table->ref_count);


    if (table->symbols && (uintptr_t)table->symbols >= 0x1000) {

        if (table->symbol_count > 0 && table->symbol_count <= table->capacity) {
            for (int i = 0; i < table->symbol_count; i++) {
                ES_DEBUG("=== TYPE_CHECK: Processing symbol %d: name=%p, owns_name=%d ===",
                       i, (void*)table->symbols[i].name, table->symbols[i].owns_name);


                if (table->symbols[i].name && (uintptr_t)table->symbols[i].name >= 0x1000 &&
                    table->symbols[i].owns_name) {
                    ES_DEBUG("=== TYPE_CHECK: Freeing name at %p ===", (void*)table->symbols[i].name);
                    ES_FREE(table->symbols[i].name);
                    table->symbols[i].name = NULL;
                }

                if (table->symbols[i].type) {
                    ES_DEBUG("=== TYPE_CHECK: Destroying type at %p ===", (void*)table->symbols[i].type);
                    type_destroy(table->symbols[i].type);
                    table->symbols[i].type = NULL;
                }
            }
        }
        ES_DEBUG("=== TYPE_CHECK: Freeing symbols array at %p ===", (void*)table->symbols);
        ES_FREE(table->symbols);
        table->symbols = NULL;
        table->symbol_count = 0;
        table->capacity = 0;
    }




    ES_DEBUG("=== TYPE_CHECK: Freeing table at %p ===", (void*)table);
    ES_FREE(table);
}


static int type_check_symbol_table_add(TypeCheckSymbolTable* table, TypeCheckSymbol symbol) {
    if (!table) return 0;


    for (int i = 0; i < table->symbol_count; i++) {
        if (strcmp(table->symbols[i].name, symbol.name) == 0) {
            return 0;
        }
    }


    if (table->symbol_count >= table->capacity) {
        int new_capacity = table->capacity == 0 ? 16 : table->capacity * 2;
        TypeCheckSymbol* new_symbols = (TypeCheckSymbol*)realloc(table->symbols, new_capacity * sizeof(TypeCheckSymbol));
        if (!new_symbols) {
            return 0;
        }
        table->symbols = new_symbols;
        table->capacity = new_capacity;
    }


    table->symbols[table->symbol_count] = symbol;
    table->symbol_count++;

    return 1;
}


static TypeCheckSymbol* type_check_symbol_table_lookup(TypeCheckSymbolTable* table, const char* name) {
    TypeCheckSymbolTable* current = table;

    while (current) {
        for (int i = 0; i < current->symbol_count; i++) {
            if (strcmp(current->symbols[i].name, name) == 0) {
                return &current->symbols[i];
            }
        }
        current = current->parent;
    }

    return NULL;
}


TypeCheckContext* type_check_context_create(void) {
    TypeCheckContext* context = (TypeCheckContext*)ES_MALLOC(sizeof(TypeCheckContext));
    if (!context) {
        return NULL;
    }

    context->current_scope = type_check_symbol_table_create(NULL);
    context->error_count = 0;
    context->warning_count = 0;
    context->scope_ownership = 1;


    type_check_init_builtin_types(context);
    type_check_init_builtin_functions(context);

    return context;
}


void type_check_context_destroy(TypeCheckContext* context) {
    if (!context) return;


    if (context->scope_ownership && context->current_scope) {

        type_check_symbol_table_unref(context->current_scope);
    }

    ES_FREE(context);
}


void type_error(TypeCheckContext* context, const char* message, int line) {
    (void)line;
    if (!context) return;


    if (context->suppress_errors) {
        context->error_count++;
        return;
    }

    context->error_count++;
    ES_ERROR("Type error: %s", message);
}


void type_warning(TypeCheckContext* context, const char* message, int line) {
    (void)message;
    (void)line;
    if (!context) return;

    context->warning_count++;
}


int type_is_compatible(Type* type1, Type* type2) {
    if (!type1 || !type2) return 0;



    if (type1->kind == TYPE_UNKNOWN || type2->kind == TYPE_UNKNOWN) {
        return 1;
    }


    if (type1->kind == type2->kind) {
        return 1;
    }


    if ((type1->kind >= TYPE_INT8 && type1->kind <= TYPE_FLOAT64) &&
        (type2->kind >= TYPE_INT8 && type2->kind <= TYPE_FLOAT64)) {
        return 1;
    }


    if (type1->kind == TYPE_POINTER && type2->kind == TYPE_POINTER) {
        return type_is_compatible(type1->data.pointer_to, type2->data.pointer_to);
    }


    if (type1->kind == TYPE_STRING && type2->kind == TYPE_ARRAY &&
        type2->data.array.element_type->kind == TYPE_INT8) {
        return 1;
    }

    if (type2->kind == TYPE_STRING && type1->kind == TYPE_ARRAY &&
        type1->data.array.element_type->kind == TYPE_INT8) {
        return 1;
    }


    if (type1->kind == TYPE_POINTER && type2->kind == TYPE_VOID) {
        return 1;
    }

    if (type2->kind == TYPE_POINTER && type1->kind == TYPE_VOID) {
        return 1;
    }


    if (type1->kind == TYPE_BOOL && type2->kind == TYPE_BOOL) {
        return 1;
    }

    return 0;
}


const char* type_get_name(Type* type) {
    if (!type) return "unknown";

    switch (type->kind) {
        case TYPE_VOID: return "void";
        case TYPE_INT8: return "int8";
        case TYPE_INT16: return "int16";
        case TYPE_INT32: return "int32";
        case TYPE_INT64: return "int64";
        case TYPE_UINT8: return "uint8";
        case TYPE_UINT16: return "uint16";
        case TYPE_UINT32: return "uint32";
        case TYPE_UINT64: return "uint64";
        case TYPE_FLOAT32: return "float32";
        case TYPE_FLOAT64: return "float64";
        case TYPE_BOOL: return "bool";
        case TYPE_STRING: return "string";
        case TYPE_POINTER: return "pointer";
        case TYPE_ARRAY: return "array";
        case TYPE_FUNCTION: return "function";
        case TYPE_CLASS: return "class";
        case TYPE_UNKNOWN: return "unknown";
        default: return "unknown";
    }
}


int type_is_assignable(Type* target, Type* source) {
    if (!target || !source) return 0;


    if (target->kind == source->kind) {
        return 1;
    }


    if ((target->kind >= TYPE_INT8 && target->kind <= TYPE_FLOAT64) &&
        (source->kind >= TYPE_INT8 && source->kind <= TYPE_FLOAT64)) {

        if (target->kind == TYPE_FLOAT64 || target->kind == TYPE_FLOAT32) {

            return 1;
        } else if (target->kind >= TYPE_INT32 && source->kind <= TYPE_INT32) {

            return 1;
        } else if (target->kind >= TYPE_UINT32 && source->kind <= TYPE_UINT32) {

            return 1;
        }

        return 0;
    }


    if (target->kind == TYPE_POINTER && source->kind == TYPE_POINTER) {
        return type_is_compatible(target->data.pointer_to, source->data.pointer_to);
    }


    if (target->kind == TYPE_POINTER && source->kind == TYPE_VOID) {
        return 1;
    }


    if (target->kind == TYPE_VOID && source->kind == TYPE_POINTER) {
        return 1;
    }


    if (target->kind == TYPE_POINTER && source->kind == TYPE_CLASS) {
        return 1;
    }


    if (target->kind == TYPE_CLASS && source->kind == TYPE_POINTER) {
        if (source->data.pointer_to && source->data.pointer_to->kind == TYPE_CLASS) {
            return 1;
        }
    }


    if (target->kind == TYPE_STRING && source->kind == TYPE_ARRAY &&
        source->data.array.element_type->kind == TYPE_INT8) {
        return 1;
    }

    if (target->kind == TYPE_ARRAY && source->kind == TYPE_STRING &&
        target->data.array.element_type->kind == TYPE_INT8) {
        return 1;
    }


    if (target->kind == TYPE_ARRAY && source->kind == TYPE_ARRAY) {

        Type* target_element = target->data.array.element_type;
        Type* source_element = source->data.array.element_type;
        return type_is_assignable(target_element, source_element);
    }

    return 0;
}


static Type* type_create_from_token(EsTokenType token_type) {
    switch (token_type) {
        case TOKEN_INT8: return type_create_basic(TYPE_INT8);
        case TOKEN_INT16: return type_create_basic(TYPE_INT16);
        case TOKEN_INT32: return type_create_basic(TYPE_INT32);
        case TOKEN_INT64: return type_create_basic(TYPE_INT64);
        case TOKEN_UINT8: return type_create_basic(TYPE_UINT8);
        case TOKEN_UINT16: return type_create_basic(TYPE_UINT16);
        case TOKEN_UINT32: return type_create_basic(TYPE_UINT32);
        case TOKEN_UINT64: return type_create_basic(TYPE_UINT64);
        case TOKEN_FLOAT32: return type_create_basic(TYPE_FLOAT32);
        case TOKEN_FLOAT64: return type_create_basic(TYPE_FLOAT64);
        case TOKEN_BOOL: return type_create_basic(TYPE_BOOL);
        case TOKEN_STRING: return type_create_basic(TYPE_STRING);
        case TOKEN_VOID: return type_create_basic(TYPE_VOID);
        default: return type_create_basic(TYPE_UNKNOWN);
    }
}


static Type* type_from_string(TypeCheckContext* context, const char* type_name) {
    if (!type_name) return type_create_basic(TYPE_UNKNOWN);

    if (strcmp(type_name, "int") == 0) return type_create_basic(TYPE_INT32);
    if (strcmp(type_name, "string") == 0) return type_create_basic(TYPE_STRING);
    if (strcmp(type_name, "bool") == 0) return type_create_basic(TYPE_BOOL);
    if (strcmp(type_name, "float") == 0) return type_create_basic(TYPE_FLOAT32);
    if (strcmp(type_name, "double") == 0) return type_create_basic(TYPE_FLOAT64);
    if (strcmp(type_name, "void") == 0) return type_create_basic(TYPE_VOID);


    TypeCheckSymbol* symbol = type_check_symbol_table_lookup(context->current_scope, type_name);
    if (symbol && symbol->type && symbol->type->kind == TYPE_CLASS) {
        return type_create_class(type_name);
    }

    return NULL;
}

static int type_check_handle_function_decl(TypeCheckContext* context,
                                           const char* function_name,
                                           char** parameters,
                                           int parameter_count,
                                           EsTokenType* parameter_types,
                                           ASTNode* body,
                                           EsTokenType return_type_token) {

    #ifdef DEBUG
    ES_COMPILER_DEBUG("Processing function declaration '%s'", function_name);
    #endif


    TypeCheckSymbol* existing_symbol = type_check_symbol_table_lookup(context->current_scope, function_name);
    if (existing_symbol && existing_symbol->type && existing_symbol->type->kind == TYPE_FUNCTION) {
#ifdef DEBUG
        ES_COMPILER_DEBUG("Function '%s' already pre-declared, processing body only", function_name);
#endif


        if (!body) {
#ifdef DEBUG
            ES_COMPILER_DEBUG("No body to process for pre-declared function '%s'", function_name);
#endif
            return 1;
        }


        TypeCheckSymbolTable* old_scope = context->current_scope;
        TypeCheckSymbolTable* new_scope = type_check_symbol_table_create(old_scope);
        if (!new_scope) {
            type_error(context, "Failed to create scope for function", 0);
            return 0;
        }
        context->current_scope = new_scope;


        for (int i = 0; i < parameter_count; i++) {
            EsTokenType token = parameter_types ? parameter_types[i] : TOKEN_UNKNOWN;
            Type* param_type = type_create_from_token(token);
            if (!param_type) {
                type_error(context, "Failed to create function parameter type", 0);
                context->scope_ownership = 0;
                context->current_scope = old_scope;
                return 0;
            }

            TypeCheckSymbol param_symbol = {
                .name = strdup(parameters[i]),
                .type = param_type,
                .is_constant = 0,
                .line_number = 0,
                .owns_name = 1
            };

            if (!type_check_symbol_table_add(context->current_scope, param_symbol)) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg),
                         "Parameter '%s' already declared in function '%s'",
                         parameters[i], function_name);
                type_error(context, error_msg, 0);
                type_destroy(param_type);
                ES_FREE(param_symbol.name);
                context->scope_ownership = 0;
                context->current_scope = old_scope;
                return 0;
            }
        }


        if (!type_check_statement(context, body)) {
            type_error(context, "Type error in function body", 0);
            context->scope_ownership = 0;
            context->current_scope = old_scope;
            return 0;
        }


        #ifdef DEBUG
        ES_COMPILER_DEBUG("Pre-declared function '%s' processed successfully", function_name);
        #endif

        context->scope_ownership = 0;
        context->current_scope = old_scope;
        return 1;
    }


    #ifdef DEBUG
    ES_COMPILER_DEBUG("Creating new function '%s'", function_name);
    #endif

    Type* func_type = type_create_function(NULL, NULL, 0);
    if (!func_type) {
        type_error(context, "Failed to create function type", 0);
        return 0;
    }

    for (int i = 0; i < parameter_count; i++) {
        EsTokenType token = parameter_types ? parameter_types[i] : TOKEN_UNKNOWN;
        Type* param_type = type_create_from_token(token);
        if (!param_type) {
            type_error(context, "Failed to create function parameter type", 0);
            type_destroy(func_type);
            return 0;
        }
        type_function_add_parameter(func_type, param_type);
    }


    if (return_type_token != TOKEN_UNKNOWN) {
        Type* explicit_return_type = type_create_from_token(return_type_token);
        if (explicit_return_type) {
            func_type->data.function.return_type = explicit_return_type;
            #ifdef DEBUG
            ES_COMPILER_DEBUG("Function '%s' has explicit return type: %s",
                    function_name, type_get_name(explicit_return_type));
            #endif
        } else {
            func_type->data.function.return_type = type_create_basic(TYPE_VOID);
            #ifdef DEBUG
            ES_COMPILER_DEBUG("Function '%s' has invalid return type token %d, defaulting to void",
                    function_name, return_type_token);
            #endif
        }
    } else {
        func_type->data.function.return_type = type_create_basic(TYPE_VOID);
        #ifdef DEBUG
        ES_COMPILER_DEBUG("Function '%s' has no explicit return type, defaulting to void", function_name);
        #endif
    }

    TypeCheckSymbol symbol = {
        .name = strdup(function_name),
        .type = func_type,
        .is_constant = 1,
        .line_number = 0,
        .owns_name = 1
    };

    if (!type_check_symbol_table_add(context->current_scope, symbol)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                 "Function '%s' already declared in this scope",
                 function_name);
        type_error(context, error_msg, 0);
        type_destroy(func_type);
        ES_FREE(symbol.name);
        return 0;
    }


    if (!body) {
        #ifdef DEBUG
        ES_COMPILER_DEBUG("Pre-declaration only for function '%s', skipping body processing", function_name);
        #endif
        return 1;
    }

    TypeCheckSymbolTable* old_scope = context->current_scope;
    TypeCheckSymbolTable* new_scope = type_check_symbol_table_create(old_scope);
    if (!new_scope) {
        type_error(context, "Failed to create scope for function", 0);
        context->current_scope = old_scope;
        return 0;
    }
    context->current_scope = new_scope;

    for (int i = 0; i < parameter_count; i++) {
        EsTokenType token = parameter_types ? parameter_types[i] : TOKEN_UNKNOWN;
        Type* param_type = type_create_from_token(token);
        if (!param_type) {
            type_error(context, "Failed to create function parameter type", 0);
            context->scope_ownership = 0;
            context->current_scope = old_scope;
            return 0;
        }

        TypeCheckSymbol param_symbol = {
            .name = strdup(parameters[i]),
            .type = param_type,
            .is_constant = 0,
            .line_number = 0,
            .owns_name = 1
        };

        if (!type_check_symbol_table_add(context->current_scope, param_symbol)) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg),
                     "Parameter '%s' already declared in function '%s'",
                     parameters[i], function_name);
            type_error(context, error_msg, 0);
            type_destroy(param_type);
            ES_FREE(param_symbol.name);
            context->scope_ownership = 0;
            context->current_scope = old_scope;
            return 0;
        }
    }

    if (!type_check_statement(context, body)) {
        type_error(context, "Type error in function body", 0);
        context->scope_ownership = 0;
        context->current_scope = old_scope;
        return 0;
    }



    #ifdef DEBUG
    ES_COMPILER_DEBUG("Checking if return type inference is needed for function '%s'", function_name);
    ES_COMPILER_DEBUG("return_type_token = %d, TOKEN_UNKNOWN = %d", return_type_token, TOKEN_UNKNOWN);
    #endif
    if (return_type_token == TOKEN_UNKNOWN) {
        #ifdef DEBUG
        ES_COMPILER_DEBUG("No explicit return type declared, inferring from function body");
        #endif
        Type* inferred_return_type = infer_function_return_type(context, body);
        if (inferred_return_type && inferred_return_type->kind != TYPE_VOID) {
            #ifdef DEBUG
            ES_COMPILER_DEBUG("Inferred return type: %s", type_get_name(inferred_return_type));
            #endif
            type_destroy(func_type->data.function.return_type);
            func_type->data.function.return_type = inferred_return_type;
        } else if (inferred_return_type) {
            #ifdef DEBUG
            ES_COMPILER_DEBUG("Inferred void return type, keeping declared type");
            #endif
            type_destroy(inferred_return_type);
        }
    } else {
        #ifdef DEBUG
        ES_COMPILER_DEBUG("Explicit return type declared (%d), skipping inference", return_type_token);
        ES_COMPILER_DEBUG("Function '%s' will keep declared return type: %s",
                function_name, type_get_name(func_type->data.function.return_type));
        #endif
    }

    context->scope_ownership = 0;
    context->current_scope = old_scope;

    return 1;
}


int type_check_program(TypeCheckContext* context, ASTNode* program) {
    #ifdef DEBUG
    ES_COMPILER_DEBUG("type_check_program called with program type: %d", program->type);
    #endif
    if (!context || !program) return 0;


    #ifdef DEBUG
    if (program->type == AST_PROGRAM) {
        ES_COMPILER_DEBUG("Program has %d statements:", program->data.block.statement_count);
        for (int i = 0; i < program->data.block.statement_count; i++) {
            if (program->data.block.statements[i]) {
                        if (program->data.block.statements[i]->type == AST_CLASS_DECLARATION) {
                    ES_COMPILER_DEBUG("Statement %d: type=%d (CLASS_DECLARATION)", i, program->data.block.statements[i]->type);
                } else if (program->data.block.statements[i]->type == AST_FUNCTION_DECLARATION) {
                    ES_COMPILER_DEBUG("Statement %d: type=%d (FUNCTION_DECLARATION)", i, program->data.block.statements[i]->type);
                } else if (program->data.block.statements[i]->type == AST_STATIC_FUNCTION_DECLARATION) {
                    ES_COMPILER_DEBUG("Statement %d: type=%d (STATIC_FUNCTION_DECLARATION)", i, program->data.block.statements[i]->type);
                } else {
                    ES_COMPILER_DEBUG("Statement %d: type=%d", i, program->data.block.statements[i]->type);
                }
            }
        }
    }
    #endif

    context->error_count = 0;
    context->warning_count = 0;

    if (program->type != AST_PROGRAM) {
        type_error(context, "Expected program node", 0);
        return 0;
    }


    #ifdef DEBUG
    ES_COMPILER_DEBUG("Phase 1 - Pre-declaring functions and classes");
    #endif
    for (int i = 0; i < program->data.block.statement_count; i++) {
        ASTNode* statement = program->data.block.statements[i];
        if (!statement) continue;

        #ifdef DEBUG
        ES_COMPILER_DEBUG("Pre-declaring statement %d (type: %d)", i, statement->type);
        #endif

        if (statement->type == AST_FUNCTION_DECLARATION) {
            #ifdef DEBUG
            ES_COMPILER_DEBUG("Pre-declaring function '%s'", statement->data.function_decl.name);
            #endif
            if (!type_check_handle_function_decl(context,
                                                 statement->data.function_decl.name,
                                                 statement->data.function_decl.parameters,
                                                 statement->data.function_decl.parameter_count,
                                                 statement->data.function_decl.parameter_types,
                                                 NULL,
                                                 statement->data.function_decl.return_type)) {
                #ifdef DEBUG
                ES_COMPILER_DEBUG("Failed to pre-declare function '%s'", statement->data.function_decl.name);
                #endif
                continue;
            }
        } else if (statement->type == AST_STATIC_FUNCTION_DECLARATION) {
            #ifdef DEBUG
            ES_COMPILER_DEBUG("Pre-declaring static function '%s'", statement->data.static_function_decl.name);
            #endif
            if (!type_check_handle_function_decl(context,
                                                 statement->data.static_function_decl.name,
                                                 statement->data.static_function_decl.parameters,
                                                 statement->data.static_function_decl.parameter_count,
                                                 statement->data.static_function_decl.parameter_types,
                                                 NULL,
                                                 statement->data.static_function_decl.return_type)) {
                #ifdef DEBUG
                ES_COMPILER_DEBUG("Failed to pre-declare static function '%s'", statement->data.static_function_decl.name);
                #endif
                continue;
            }
        }

    }


    #ifdef DEBUG
    ES_COMPILER_DEBUG("Phase 2 - Processing all statements");
    #endif
    for (int i = 0; i < program->data.block.statement_count; i++) {
        #ifdef DEBUG
        ES_COMPILER_DEBUG("Processing statement %d", i);
        #endif
        if (!type_check_statement(context, program->data.block.statements[i])) {
            continue;
        }
    }

    return context->error_count == 0;
}


int type_check_statement(TypeCheckContext* context, ASTNode* statement) {
    if (!context || !statement) return 0;

    #ifdef DEBUG
    ES_COMPILER_DEBUG("Processing statement type: %d", statement->type);
    #endif

    switch (statement->type) {
        case AST_VARIABLE_DECLARATION:

            {
                #ifdef DEBUG
                ES_COMPILER_DEBUG("Processing variable declaration '%s'", statement->data.variable_decl.name);
                #endif


                if (statement->data.variable_decl.is_array) {
                    #ifdef DEBUG
                    ES_COMPILER_DEBUG("Processing array declaration");
                    #endif


                    int array_size = 0;
                    if (statement->data.variable_decl.array_size) {
#ifdef DEBUG
                        ES_COMPILER_DEBUG("Processing array size expression");
#endif
                        Type* size_type = type_check_expression(context, statement->data.variable_decl.array_size);
                        if (!size_type) {
                            type_error(context, "Cannot determine type of array size expression", 0);
                            return 0;
                        }

                        if (size_type->kind != TYPE_INT32) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Array size must be int32, got %s",
                                     type_get_name(size_type));
                            type_error(context, error_msg, 0);
                            type_destroy(size_type);
                            return 0;
                        }



                        array_size = 10;
                        #ifdef DEBUG
                        ES_COMPILER_DEBUG("Array size set to %d", array_size);
                        #endif
                        type_destroy(size_type);
                    }


                    Type* initializer_type = NULL;
                    if (statement->data.variable_decl.value) {
#ifdef DEBUG
                        ES_COMPILER_DEBUG("Processing array initializer");
#endif
                        initializer_type = type_check_expression(context, statement->data.variable_decl.value);
                        if (!initializer_type) {
                            type_error(context, "Cannot determine type of array initializer", 0);
                            return 0;
                        }

                        if (initializer_type->kind != TYPE_ARRAY) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Array declaration requires array initializer, got %s",
                                     type_get_name(initializer_type));
                            type_error(context, error_msg, 0);
                            type_destroy(initializer_type);
                            return 0;
                        }


                        #ifdef DEBUG
                        ES_COMPILER_DEBUG("Creating symbol from initializer type");
                        #endif
                        Type* symbol_type = type_copy(initializer_type);
                        if (!symbol_type) {
                            type_error(context, "Failed to copy array type for variable", 0);
                            type_destroy(initializer_type);
                            return 0;
                        }

                        #ifdef DEBUG
                        ES_COMPILER_DEBUG("Creating symbol structure");
                        #endif
                        TypeCheckSymbol symbol = {
                            .name = strdup(statement->data.variable_decl.name),
                            .type = symbol_type,
                            .is_constant = 0,
                            .line_number = 0,
                            .owns_name = 1
                        };

                        #ifdef DEBUG
                        ES_COMPILER_DEBUG("Adding symbol to table");
                        #endif
                        if (!type_check_symbol_table_add(context->current_scope, symbol)) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Array variable '%s' already declared in this scope",
                                     statement->data.variable_decl.name);
                            type_error(context, error_msg, 0);
                            type_destroy(symbol_type);
                            ES_FREE(symbol.name);
                            type_destroy(initializer_type);
                            return 0;
                        }

                        #ifdef DEBUG
                        ES_COMPILER_DEBUG("Array with initializer completed successfully");
                        #endif
                        type_destroy(initializer_type);
                        return 1;
                    }

                    #ifdef DEBUG
                    ES_COMPILER_DEBUG("Creating array type without initializer");
                    #endif

                    Type* element_type = type_create_basic(TYPE_INT32);
                    #ifdef DEBUG
                    ES_COMPILER_DEBUG("Created element type: %p", (void*)element_type);
                    ES_COMPILER_DEBUG("About to call type_create_array with element_type: %p, size: %d", (void*)element_type, array_size > 0 ? array_size : 10);
                    #endif
                    Type* array_type = type_create_array(element_type, array_size > 0 ? array_size : 10);
                    fflush(stderr);
                    #ifdef DEBUG
                    ES_COMPILER_DEBUG("Created array type: %p, element_type: %p", (void*)array_type, (void*)(array_type ? array_type->data.array.element_type : NULL));

                    ES_COMPILER_DEBUG("After array creation, element_type still: %p", (void*)element_type);
                    #endif

                    if (!array_type) {
                        type_error(context, "Failed to create array type for variable", 0);
                        return 0;
                    }

                    #ifdef DEBUG
                    ES_COMPILER_DEBUG("Creating symbol structure for array");
                    #endif
                    TypeCheckSymbol symbol = {
                        .name = strdup(statement->data.variable_decl.name),
                        .type = array_type,
                        .is_constant = 0,
                        .line_number = 0,
                        .owns_name = 1
                    };

                    #ifdef DEBUG
                    ES_COMPILER_DEBUG("Adding array symbol to table");
                    #endif
                    if (!type_check_symbol_table_add(context->current_scope, symbol)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Array variable '%s' already declared in this scope",
                                 statement->data.variable_decl.name);
                        type_error(context, error_msg, 0);
                        type_destroy(array_type);
                        ES_FREE(symbol.name);
                        return 0;
                    }

                    #ifdef DEBUG
                    ES_COMPILER_DEBUG("Array declaration completed successfully");
                    #endif
                    return 1;
                }


                Type* initializer_type = NULL;
                if (statement->data.variable_decl.value) {
                    initializer_type = type_check_expression(context, statement->data.variable_decl.value);
                    if (!initializer_type) {
                        type_error(context, "Cannot determine type of variable initializer", 0);
                        return 0;
                    }
                    #ifdef DEBUG
                    ES_COMPILER_DEBUG("Initializer type: %s", type_get_name(initializer_type));
                    #endif
                }

                EsTokenType declared_token = statement->data.variable_decl.type;
                Type* declared_type = NULL;
                if (declared_token != TOKEN_UNKNOWN) {
                    switch (declared_token) {
                        case TOKEN_INT8: declared_type = type_create_basic(TYPE_INT8); break;
                        case TOKEN_INT16: declared_type = type_create_basic(TYPE_INT16); break;
                        case TOKEN_INT32: declared_type = type_create_basic(TYPE_INT32); break;
                        case TOKEN_INT64: declared_type = type_create_basic(TYPE_INT64); break;
                        case TOKEN_UINT8: declared_type = type_create_basic(TYPE_UINT8); break;
                        case TOKEN_UINT16: declared_type = type_create_basic(TYPE_UINT16); break;
                        case TOKEN_UINT32: declared_type = type_create_basic(TYPE_UINT32); break;
                        case TOKEN_UINT64: declared_type = type_create_basic(TYPE_UINT64); break;
                        case TOKEN_FLOAT32: declared_type = type_create_basic(TYPE_FLOAT32); break;
                        case TOKEN_FLOAT64: declared_type = type_create_basic(TYPE_FLOAT64); break;
                        case TOKEN_BOOL: declared_type = type_create_basic(TYPE_BOOL); break;
                        case TOKEN_STRING: declared_type = type_create_basic(TYPE_STRING); break;
                        case TOKEN_VOID: declared_type = type_create_basic(TYPE_VOID); break;
                        case TOKEN_VAR:

                            declared_type = NULL;
                            break;
                        default:

                            declared_type = NULL;
                            break;
                    }
                }

                Type* symbol_type = NULL;
                if (declared_type && initializer_type) {
                    if (!type_is_assignable(declared_type, initializer_type)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Type mismatch in variable declaration: cannot assign %s to %s",
                                 type_get_name(initializer_type),
                                 type_get_name(declared_type));
                        type_error(context, error_msg, 0);
                        type_destroy(initializer_type);
                        type_destroy(declared_type);
                        return 0;
                    }
                    type_destroy(initializer_type);
                    symbol_type = declared_type;
                } else if (declared_type) {
                    symbol_type = declared_type;
                    if (initializer_type) {
                        type_destroy(initializer_type);
                    }
                } else if (initializer_type) {
                    symbol_type = type_copy(initializer_type);
                    if (!symbol_type) {
                        type_destroy(initializer_type);
                        type_error(context, "Failed to copy type for variable", 0);
                        return 0;
                    }
                    type_destroy(initializer_type);
                } else {
                    symbol_type = type_create_basic(TYPE_UNKNOWN);
                    if (!symbol_type) {
                        type_error(context, "Failed to create type for variable", 0);
                        return 0;
                    }
                }

                TypeCheckSymbol symbol = {
                    .name = strdup(statement->data.variable_decl.name),
                    .type = symbol_type,
                    .is_constant = 0,
                    .line_number = 0,
                    .owns_name = 1
                };

#ifdef DEBUG
                ES_COMPILER_DEBUG("Adding symbol '%s' with type '%s' to symbol table",
                        symbol.name, type_get_name(symbol_type));
#endif

#ifdef DEBUG
                ES_COMPILER_DEBUG("About to call type_check_symbol_table_add");
#endif
                int add_result = type_check_symbol_table_add(context->current_scope, symbol);
#ifdef DEBUG
                ES_COMPILER_DEBUG("type_check_symbol_table_add returned %d", add_result);
#endif

                if (!add_result) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Variable '%s' already declared in this scope",
                             statement->data.variable_decl.name);
                    type_error(context, error_msg, 0);
                    type_destroy(symbol_type);
                    ES_FREE(symbol.name);
                    return 0;
                }
#ifdef DEBUG
                ES_COMPILER_DEBUG("Copied symbol type: %p, kind: %d", (void*)symbol_type, symbol_type->kind);
#endif
                if (symbol_type->kind == TYPE_ARRAY) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Array element type: %p, size: %d",
                            (void*)symbol_type->data.array.element_type, symbol_type->data.array.size);
#endif
                }

#ifdef DEBUG
                ES_COMPILER_DEBUG("Variable declaration completed successfully");
#endif
            }
            break;

        case AST_STATIC_VARIABLE_DECLARATION:

            {
                Type* initializer_type = NULL;
                if (statement->data.static_variable_decl.value) {
                    initializer_type = type_check_expression(context, statement->data.static_variable_decl.value);
                    if (!initializer_type) {
                        type_error(context, "Cannot determine type of static variable initializer", 0);
                        return 0;
                    }
                }

                EsTokenType declared_token = statement->data.static_variable_decl.type;
                Type* declared_type = NULL;
                if (declared_token != TOKEN_UNKNOWN) {
                    declared_type = type_create_from_token(declared_token);
                    if (!declared_type) {
                        if (initializer_type) {
                            type_destroy(initializer_type);
                        }
                        type_error(context, "Failed to create declared type for static variable", 0);
                        return 0;
                    }
                }

                Type* symbol_type = NULL;
                if (declared_type && initializer_type) {
                    if (!type_is_assignable(declared_type, initializer_type)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Type mismatch in static variable declaration: cannot assign %s to %s",
                                 type_get_name(initializer_type),
                                 type_get_name(declared_type));
                        type_error(context, error_msg, 0);
                        type_destroy(initializer_type);
                        type_destroy(declared_type);
                        return 0;
                    }
                    type_destroy(initializer_type);
                    symbol_type = declared_type;
                } else if (declared_type) {
                    symbol_type = declared_type;
                    if (initializer_type) {
                        type_destroy(initializer_type);
                    }
                } else if (initializer_type) {
                    symbol_type = type_copy(initializer_type);
                    if (!symbol_type) {
                        type_destroy(initializer_type);
                        type_error(context, "Failed to copy type for static variable", 0);
                        return 0;
                    }
                    type_destroy(initializer_type);
                } else {
                    symbol_type = type_create_basic(TYPE_UNKNOWN);
                    if (!symbol_type) {
                        type_error(context, "Failed to create type for static variable", 0);
                        return 0;
                    }
                }

                TypeCheckSymbol symbol = {
                    .name = strdup(statement->data.static_variable_decl.name),
                    .type = symbol_type,
                    .is_constant = 0,
                    .line_number = 0,
                    .owns_name = 1
                };

                if (!type_check_symbol_table_add(context->current_scope, symbol)) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Static variable '%s' already declared in this scope",
                             statement->data.static_variable_decl.name);
                    type_error(context, error_msg, 0);
                    type_destroy(symbol_type);
                    ES_FREE(symbol.name);
                    return 0;
                }
            }
            break;

        case AST_ASSIGNMENT:

            {

                TypeCheckSymbol* left_symbol = type_check_symbol_table_lookup(context->current_scope, statement->data.assignment.name);
                if (!left_symbol) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Undefined variable '%s' in assignment",
                             statement->data.assignment.name);
                    type_error(context, error_msg, 0);
                    return 0;
                }

                if (statement->data.assignment.value) {
                    Type* value_type = type_check_expression(context, statement->data.assignment.value);
                    if (!value_type) {
                        type_error(context, "Cannot determine type of assignment value", 0);
                        return 0;
                    }


                    if (!type_is_assignable(left_symbol->type, value_type)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Type mismatch in assignment: cannot assign %s to %s",
                                 type_get_name(value_type), type_get_name(left_symbol->type));
                        type_error(context, error_msg, 0);
                        type_destroy(value_type);
                        return 0;
                    }

                    type_destroy(value_type);
                }
            }
            break;

        case AST_ARRAY_ASSIGNMENT:
            {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Processing AST_ARRAY_ASSIGNMENT");
#endif

                Type* array_type = type_check_expression(context, statement->data.array_assignment.array);
                if (!array_type) {
                    type_error(context, "Cannot determine type of array in array assignment", 0);
                    return 0;
                }


                if (array_type->kind != TYPE_ARRAY) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Array assignment target must be an array, got %s",
                             type_get_name(array_type));
                    type_error(context, error_msg, 0);
                    type_destroy(array_type);
                    return 0;
                }


                Type* index_type = type_check_expression(context, statement->data.array_assignment.index);
                if (!index_type) {
                    type_error(context, "Cannot determine type of array index", 0);
                    type_destroy(array_type);
                    return 0;
                }


                if (index_type->kind != TYPE_INT32) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Array index must be int32, got %s",
                             type_get_name(index_type));
                    type_error(context, error_msg, 0);
                    type_destroy(array_type);
                    type_destroy(index_type);
                    return 0;
                }
                type_destroy(index_type);


                Type* value_type = type_check_expression(context, statement->data.array_assignment.value);
                if (!value_type) {
                    type_error(context, "Cannot determine type of assignment value", 0);
                    type_destroy(array_type);
                    return 0;
                }


                Type* element_type = array_type->data.array.element_type;
                if (!type_is_assignable(element_type, value_type)) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Type mismatch in array assignment: cannot assign %s to array element of type %s",
                             type_get_name(value_type), type_get_name(element_type));
                    type_error(context, error_msg, 0);
                    type_destroy(array_type);
                    type_destroy(value_type);
                    return 0;
                }

                type_destroy(array_type);
                type_destroy(value_type);
            }
            break;

        case AST_RETURN_STATEMENT:

            {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Processing return statement");
#endif

                if (statement->data.return_stmt.value) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Processing return value expression");
#endif
                    Type* return_type = type_check_expression(context, statement->data.return_stmt.value);
                    if (!return_type) {
                        type_error(context, "Cannot determine type of return value", 0);
                        return 0;
                    }
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Return value type checked successfully");
#endif
                    type_destroy(return_type);
                }
#ifdef DEBUG
                ES_COMPILER_DEBUG("Return statement completed successfully");
#endif
            }
            break;

        case AST_IF_STATEMENT:

            {

                Type* condition_type = type_check_expression(context, statement->data.if_stmt.condition);
                if (!condition_type) {
                    type_error(context, "Cannot determine type of if condition", 0);
                    return 0;
                }


                if (condition_type->kind != TYPE_BOOL) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Condition expression in if statement must be boolean, got %s",
                             type_get_name(condition_type));
                    type_error(context, error_msg, 0);
                    type_destroy(condition_type);
                    return 0;
                }
                type_destroy(condition_type);


                if (!type_check_statement(context, statement->data.if_stmt.then_branch)) {
                    type_error(context, "Type error in if statement then branch", 0);
                    return 0;
                }


                if (statement->data.if_stmt.else_branch) {
                    if (!type_check_statement(context, statement->data.if_stmt.else_branch)) {
                        type_error(context, "Type error in if statement else branch", 0);
                        return 0;
                    }
                }
            }
            break;

        case AST_WHILE_STATEMENT:

            {

                Type* condition_type = type_check_expression(context, statement->data.while_stmt.condition);
                if (!condition_type) {
                    type_error(context, "Cannot determine type of while condition", 0);
                    return 0;
                }


                if (condition_type->kind != TYPE_BOOL) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Condition expression in while statement must be boolean, got %s",
                             type_get_name(condition_type));
                    type_error(context, error_msg, 0);
                    type_destroy(condition_type);
                    return 0;
                }
                type_destroy(condition_type);


                if (!type_check_statement(context, statement->data.while_stmt.body)) {
                    type_error(context, "Type error in while statement body", 0);
                    return 0;
                }
            }
            break;

        case AST_FOR_STATEMENT:

            {

                if (statement->data.for_stmt.init) {
                    if (!type_check_statement(context, statement->data.for_stmt.init)) {
                        type_error(context, "Type error in for statement initialization", 0);
                        return 0;
                    }
                }


                if (statement->data.for_stmt.condition) {
                    Type* condition_type = type_check_expression(context, statement->data.for_stmt.condition);
                    if (!condition_type) {
                        type_error(context, "Cannot determine type of for condition", 0);
                        return 0;
                    }


                    if (condition_type->kind != TYPE_BOOL) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Condition expression in for statement must be boolean, got %s",
                                 type_get_name(condition_type));
                        type_error(context, error_msg, 0);
                        type_destroy(condition_type);
                        return 0;
                    }
                    type_destroy(condition_type);
                }


                if (statement->data.for_stmt.increment) {
                    Type* increment_type = type_check_expression(context, statement->data.for_stmt.increment);
                    if (!increment_type) {
                        type_error(context, "Cannot determine type of for increment expression", 0);
                        return 0;
                    }
                    type_destroy(increment_type);
                }


                if (!type_check_statement(context, statement->data.for_stmt.body)) {
                    type_error(context, "Type error in for statement body", 0);
                    return 0;
                }
            }
            break;

        case AST_BLOCK:

            {

                TypeCheckSymbolTable* old_scope = context->current_scope;
                context->current_scope = type_check_symbol_table_create(old_scope);


                for (int i = 0; i < statement->data.block.statement_count; i++) {
                    if (!type_check_statement(context, statement->data.block.statements[i])) {

                        continue;
                    }
                }


                type_check_symbol_table_destroy(context->current_scope);
                context->current_scope = old_scope;
            }
            break;

        case AST_PRINT_STATEMENT:

            {
                for (int i = 0; i < statement->data.print_stmt.value_count; i++) {
                    Type* value_type = type_check_expression(context, statement->data.print_stmt.values[i]);
                    if (!value_type) {
                        type_error(context, "Cannot determine type of print value", 0);
                        return 0;
                    }
                    type_destroy(value_type);
                }
            }
            break;

        case AST_FUNCTION_DECLARATION:
            {

                TypeCheckSymbol* existing_symbol = type_check_symbol_table_lookup(context->current_scope, statement->data.function_decl.name);
                if (existing_symbol && existing_symbol->type && existing_symbol->type->kind == TYPE_FUNCTION) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Function '%s' already pre-declared, processing body now", statement->data.function_decl.name);
#endif

                    if (!type_check_handle_function_decl(context,
                                                         statement->data.function_decl.name,
                                                         statement->data.function_decl.parameters,
                                                         statement->data.function_decl.parameter_count,
                                                         statement->data.function_decl.parameter_types,
                                                         statement->data.function_decl.body,
                                                         statement->data.function_decl.return_type)) {
                        return 0;
                    }
                } else {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Function '%s' not pre-declared, processing normally", statement->data.function_decl.name);
#endif

                    if (!type_check_handle_function_decl(context,
                                                         statement->data.function_decl.name,
                                                         statement->data.function_decl.parameters,
                                                         statement->data.function_decl.parameter_count,
                                                         statement->data.function_decl.parameter_types,
                                                         statement->data.function_decl.body,
                                                         statement->data.function_decl.return_type)) {
                        return 0;
                    }
                }
            }
            break;

        case AST_STATIC_FUNCTION_DECLARATION:
            {

                TypeCheckSymbol* existing_symbol = type_check_symbol_table_lookup(context->current_scope, statement->data.static_function_decl.name);
                if (existing_symbol && existing_symbol->type && existing_symbol->type->kind == TYPE_FUNCTION) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Static function '%s' already pre-declared, processing body now", statement->data.static_function_decl.name);
#endif

                    if (!type_check_handle_function_decl(context,
                                                         statement->data.static_function_decl.name,
                                                         statement->data.static_function_decl.parameters,
                                                         statement->data.static_function_decl.parameter_count,
                                                         statement->data.static_function_decl.parameter_types,
                                                         statement->data.static_function_decl.body,
                                                         statement->data.static_function_decl.return_type)) {
                        return 0;
                    }
                } else {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Static function '%s' not pre-declared, processing normally", statement->data.static_function_decl.name);
#endif

                    if (!type_check_handle_function_decl(context,
                                                         statement->data.static_function_decl.name,
                                                         statement->data.static_function_decl.parameters,
                                                         statement->data.static_function_decl.parameter_count,
                                                         statement->data.static_function_decl.parameter_types,
                                                         statement->data.static_function_decl.body,
                                                         statement->data.static_function_decl.return_type)) {
                        return 0;
                    }
                }
            }
            break;

        case AST_TRY_STATEMENT:

            {

                if (!type_check_statement(context, statement->data.try_stmt.try_block)) {
                    type_error(context, "Type error in try block", 0);
                    return 0;
                }


                for (int i = 0; i < statement->data.try_stmt.catch_clause_count; i++) {
                    if (!type_check_statement(context, statement->data.try_stmt.catch_clauses[i])) {
                        type_error(context, "Type error in catch clause", 0);
                        return 0;
                    }
                }


                if (statement->data.try_stmt.finally_clause) {
                    if (!type_check_statement(context, statement->data.try_stmt.finally_clause)) {
                        type_error(context, "Type error in finally clause", 0);
                        return 0;
                    }
                }
            }
            break;

        case AST_THROW_STATEMENT:
            {
                if (statement->data.throw_stmt.exception_expr) {
                    Type* value_type = type_check_expression(context, statement->data.throw_stmt.exception_expr);
                    if (value_type) {
                        type_destroy(value_type);
                    }
                }
            }
            break;

        case AST_CATCH_CLAUSE:

            {

                TypeCheckSymbolTable* old_scope = context->current_scope;
                context->current_scope = type_check_symbol_table_create(old_scope);


                if (statement->data.catch_clause.exception_var) {
                    Type* exception_type = NULL;


                    if (statement->data.catch_clause.exception_type) {
                        exception_type = type_from_string(context, statement->data.catch_clause.exception_type);
                        if (!exception_type) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Unknown exception type '%s' in catch clause",
                                     statement->data.catch_clause.exception_type);
                            type_error(context, error_msg, 0);
                            type_check_symbol_table_destroy(context->current_scope);
                            context->current_scope = old_scope;
                            return 0;
                        }
                    } else {

                        exception_type = type_create_basic(TYPE_STRING);
                    }

                    TypeCheckSymbol exception_symbol = {
                        .name = strdup(statement->data.catch_clause.exception_var),
                        .type = exception_type,
                        .is_constant = 0,
                        .line_number = 0,
                        .owns_name = 1
                    };

                    if (!type_check_symbol_table_add(context->current_scope, exception_symbol)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Exception variable '%s' already declared in catch clause",
                                 statement->data.catch_clause.exception_var);
                        type_error(context, error_msg, 0);
                        type_destroy(exception_type);
                        ES_FREE(exception_symbol.name);
                        type_check_symbol_table_destroy(context->current_scope);
                        context->current_scope = old_scope;
                        return 0;
                    }
                }


                if (!type_check_statement(context, statement->data.catch_clause.catch_block)) {
                    type_error(context, "Type error in catch block", 0);
                    type_check_symbol_table_destroy(context->current_scope);
                    context->current_scope = old_scope;
                    return 0;
                }


                type_check_symbol_table_destroy(context->current_scope);
                context->current_scope = old_scope;
            }
            break;

        case AST_FINALLY_CLAUSE:

            {
                if (!type_check_statement(context, statement->data.finally_clause.finally_block)) {
                    type_error(context, "Type error in finally block", 0);
                    return 0;
                }
            }
            break;

        case AST_NAMESPACE_DECLARATION:

            {

                if (type_check_symbol_table_lookup(context->current_scope, statement->data.namespace_decl.name)) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Namespace '%s' already declared in this scope",
                             statement->data.namespace_decl.name);
                    type_error(context, error_msg, 0);
                    return 0;
                }


                TypeCheckSymbol ns_symbol = {
                    .name = strdup(statement->data.namespace_decl.name),
                    .type = NULL,
                    .is_constant = 1,
                    .line_number = 0,
                    .owns_name = 1
                };
                type_check_symbol_table_add(context->current_scope, ns_symbol);


                TypeCheckSymbolTable* old_scope = context->current_scope;
                context->current_scope = type_check_symbol_table_create(old_scope);

                int body_ok = 1;
                if (statement->data.namespace_decl.body) {
                    body_ok = type_check_statement(context, statement->data.namespace_decl.body);
                }


                type_check_symbol_table_destroy(context->current_scope);
                context->current_scope = old_scope;

                if (!body_ok) {
                    type_error(context, "Type error in namespace body", 0);
                    return 0;
                }
            }
            break;

        case AST_CLASS_DECLARATION:

            {
#ifdef DEBUG
                ES_COMPILER_DEBUG("AST_CLASS_DECLARATION case reached!");
#endif
                const char* class_name = statement->data.class_decl.name;
#ifdef DEBUG
                ES_COMPILER_DEBUG("Processing class declaration '%s'", class_name);
#endif


                TypeCheckSymbol* existing_symbol = type_check_symbol_table_lookup(context->current_scope, class_name);
                if (existing_symbol) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Class '%s' already declared in this scope", class_name);
                    type_error(context, error_msg, 0);
                    return 0;
                }


                ClassInfo* class_info = class_info_create(class_name);
                if (!class_info) {
                    type_error(context, "Failed to create class info", 0);
                    return 0;
                }


                if (statement->data.class_decl.base_class) {


                    if (statement->data.class_decl.base_class->type == AST_IDENTIFIER) {
                        class_info->base_class_name = strdup(statement->data.class_decl.base_class->data.identifier_name);
                    }
                }


                Type* class_type = type_create_class(class_name);
                if (!class_type) {
                    class_info_destroy(class_info);
                    type_error(context, "Failed to create class type", 0);
                    return 0;
                }


                class_type->data.class.class_info = class_info;


                TypeCheckSymbol class_symbol = {
                    .name = strdup(class_name),
                    .type = class_type,
                    .is_constant = 1,
                    .line_number = 0,
                    .owns_name = 1
                };

                if (!type_check_symbol_table_add(context->current_scope, class_symbol)) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Failed to add class '%s' to symbol table", class_name);
                    type_error(context, error_msg, 0);
                    type_destroy(class_type);
                    ES_FREE(class_symbol.name);
                    return 0;
                }


                char* old_class = context->current_class;
                ClassInfo* old_class_info = context->current_class_info;


                context->current_class = strdup(class_name);
                context->current_class_info = class_info;


                if (statement->data.class_decl.body) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Processing class body for '%s'", class_name);
#endif
                    AccessModifier default_access = ACCESS_PUBLIC;
#ifdef DEBUG
                    ES_COMPILER_DEBUG("About to call type_check_class_body for '%s'", class_name);
#endif
                    if (!type_check_class_body(context, class_info, statement->data.class_decl.body, default_access)) {
                        ES_FREE(context->current_class);
                        context->current_class = old_class;
                        context->current_class_info = old_class_info;
                        type_error(context, "Type error in class body", 0);
                        return 0;
                    }
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Completed type_check_class_body for '%s'", class_name);
#endif
                }


                ES_FREE(context->current_class);
                context->current_class = old_class;
                context->current_class_info = old_class_info;
            }
            break;

        case AST_CONSTRUCTOR_DECLARATION:

            {
                if (!context->current_class_info) {
                    type_error(context, "Constructor must be declared inside a class", 0);
                    return 0;
                }


                ClassMember constructor_member = {
                    .name = strdup(context->current_class),
                    .member_type = MEMBER_CONSTRUCTOR,
                    .access = ACCESS_PUBLIC,
                    .is_static = 0,
                    .type = NULL
                };


                Type* constructor_type = type_create_function(type_create_basic(TYPE_VOID), NULL, 0);
                if (!constructor_type) {
                    type_error(context, "Failed to create constructor type", 0);
                    ES_FREE(constructor_member.name);
                    return 0;
                }


                for (int i = 0; i < statement->data.constructor_decl.parameter_count; i++) {
                    EsTokenType token = statement->data.constructor_decl.parameter_types[i];
                    Type* param_type = NULL;
                    switch (token) {
                        case TOKEN_INT8: param_type = type_create_basic(TYPE_INT8); break;
                        case TOKEN_INT16: param_type = type_create_basic(TYPE_INT16); break;
                        case TOKEN_INT32: param_type = type_create_basic(TYPE_INT32); break;
                        case TOKEN_INT64: param_type = type_create_basic(TYPE_INT64); break;
                        case TOKEN_UINT8: param_type = type_create_basic(TYPE_UINT8); break;
                        case TOKEN_UINT16: param_type = type_create_basic(TYPE_UINT16); break;
                        case TOKEN_UINT32: param_type = type_create_basic(TYPE_UINT32); break;
                        case TOKEN_UINT64: param_type = type_create_basic(TYPE_UINT64); break;
                        case TOKEN_FLOAT32: param_type = type_create_basic(TYPE_FLOAT32); break;
                        case TOKEN_FLOAT64: param_type = type_create_basic(TYPE_FLOAT64); break;
                        case TOKEN_BOOL: param_type = type_create_basic(TYPE_BOOL); break;
                        case TOKEN_STRING: param_type = type_create_basic(TYPE_STRING); break;
                        default: param_type = type_create_basic(TYPE_UNKNOWN); break;
                    }
                    type_function_add_parameter(constructor_type, param_type);
                }

                constructor_member.type = constructor_type;


                class_info_add_member(context->current_class_info, constructor_member);


                TypeCheckSymbolTable* old_scope = context->current_scope;
                context->current_scope = type_check_symbol_table_create(old_scope);


                for (int i = 0; i < statement->data.constructor_decl.parameter_count; i++) {
                    Type* param_type = constructor_type->data.function.parameter_types[i];
                    TypeCheckSymbol param_symbol = {
                        .name = strdup(statement->data.constructor_decl.parameters[i]),
                        .type = type_copy(param_type),
                        .is_constant = 0,
                        .line_number = 0,
                        .owns_name = 1
                    };
                    type_check_symbol_table_add(context->current_scope, param_symbol);
                }


                if (statement->data.constructor_decl.body) {
                    if (!type_check_statement(context, statement->data.constructor_decl.body)) {
                        type_check_symbol_table_destroy(context->current_scope);
                        context->current_scope = old_scope;
                        type_error(context, "Type error in constructor body", 0);
                        return 0;
                    }
                }

                type_check_symbol_table_destroy(context->current_scope);
                context->current_scope = old_scope;
            }
            break;

        case AST_DESTRUCTOR_DECLARATION:

            {
                if (!context->current_class_info) {
                    type_error(context, "Destructor must be declared inside a class", 0);
                    return 0;
                }


                char destructor_name[256];
                snprintf(destructor_name, sizeof(destructor_name), "~%s", context->current_class);

                ClassMember destructor_member = {
                    .name = strdup(destructor_name),
                    .member_type = MEMBER_DESTRUCTOR,
                    .access = ACCESS_PUBLIC,
                    .is_static = 0,
                    .type = type_create_function(type_create_basic(TYPE_VOID), NULL, 0)
                };


                class_info_add_member(context->current_class_info, destructor_member);


                TypeCheckSymbolTable* old_scope = context->current_scope;
                context->current_scope = type_check_symbol_table_create(old_scope);


                if (statement->data.destructor_decl.body) {
                    if (!type_check_statement(context, statement->data.destructor_decl.body)) {
                        type_check_symbol_table_destroy(context->current_scope);
                        context->current_scope = old_scope;
                        type_error(context, "Type error in destructor body", 0);
                        return 0;
                    }
                }

                type_check_symbol_table_destroy(context->current_scope);
                context->current_scope = old_scope;
            }
            break;

        case AST_ACCESS_MODIFIER:

            {
                if (!context->current_class_info) {
                    type_error(context, "Access modifier can only be used inside a class", 0);
                    return 0;
                }


                AccessModifier access = token_to_access_modifier(statement->data.access_modifier.access_modifier);


                ASTNode* member = statement->data.access_modifier.member;
                if (!member) {
                    return 1;
                }



                return type_check_statement(context, member);
            }
            break;

        default:
            break;
    }
    return 1;
}


Type* type_check_expression(TypeCheckContext* context, ASTNode* expression) {
    if (!context || !expression) return NULL;

    switch (expression->type) {
        case AST_NUMBER:

            return type_create_basic(TYPE_INT32);

        case AST_STRING:

            return type_create_basic(TYPE_STRING);

        case AST_BOOLEAN:

            return type_create_basic(TYPE_BOOL);

        case AST_IDENTIFIER:

            {
                TypeCheckSymbol* symbol = type_check_symbol_table_lookup(context->current_scope, expression->data.identifier_name);
                if (!symbol) {
                    type_error(context, "Undefined identifier", 0);
                    return NULL;
                }


                Type* copied_type = type_copy(symbol->type);
                if (!copied_type) {
                    type_error(context, "Failed to copy identifier type", 0);
                }
                return copied_type;
            }

        case AST_BINARY_OPERATION:

            {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Processing binary operation");
#endif
                Type* left_type = type_check_expression(context, expression->data.binary_op.left);
#ifdef DEBUG
                ES_COMPILER_DEBUG("Left operand type checked");
#endif
                Type* right_type = type_check_expression(context, expression->data.binary_op.right);
#ifdef DEBUG
                ES_COMPILER_DEBUG("Right operand type checked");
#endif

                if (!left_type || !right_type) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Binary operation failed - missing operand types");
#endif
                    return NULL;
                }

                Type* result_type = NULL;
                switch (expression->data.binary_op.operator) {
                    case TOKEN_PLUS:

                        if (left_type->kind == TYPE_STRING && right_type->kind == TYPE_STRING) {
                            result_type = type_create_basic(TYPE_STRING);
                        } else if (left_type->kind == TYPE_STRING && (right_type->kind >= TYPE_INT8 && right_type->kind <= TYPE_FLOAT64)) {

                            result_type = type_create_basic(TYPE_STRING);
                        } else if ((left_type->kind >= TYPE_INT8 && left_type->kind <= TYPE_FLOAT64) && right_type->kind == TYPE_STRING) {

                            result_type = type_create_basic(TYPE_STRING);
                        } else if (!type_is_compatible(left_type, right_type)) {

                            if (left_type->kind == TYPE_UNKNOWN || right_type->kind == TYPE_UNKNOWN) {
                                result_type = type_create_basic(TYPE_INT32);
                            } else {
                                char error_msg[256];
                                snprintf(error_msg, sizeof(error_msg),
                                         "Incompatible types in binary operation: %s and %s",
                                         type_get_name(left_type), type_get_name(right_type));
                                type_error(context, error_msg, 0);
                                type_destroy(left_type);
                                type_destroy(right_type);
                                return NULL;
                            }
                        } else if ((left_type->kind >= TYPE_INT8 && left_type->kind <= TYPE_FLOAT64) &&
                                 (right_type->kind >= TYPE_INT8 && right_type->kind <= TYPE_FLOAT64)) {
                            if (left_type->kind == TYPE_FLOAT64 || right_type->kind == TYPE_FLOAT64) {
                                result_type = type_create_basic(TYPE_FLOAT64);
                            } else if (left_type->kind == TYPE_FLOAT32 || right_type->kind == TYPE_FLOAT32) {
                                result_type = type_create_basic(TYPE_FLOAT32);
                            } else {
                                result_type = type_create_basic(TYPE_INT32);
                            }
                        } else if (left_type->kind == TYPE_UNKNOWN || right_type->kind == TYPE_UNKNOWN) {
                            result_type = type_create_basic(TYPE_INT32);
                        } else {
                            type_error(context, "Invalid operands for addition operator", 0);
                        }
                        break;

                    case TOKEN_MINUS:
                    case TOKEN_MULTIPLY:
                    case TOKEN_DIVIDE:

                        if ((left_type->kind >= TYPE_INT8 && left_type->kind <= TYPE_FLOAT64) &&
                            (right_type->kind >= TYPE_INT8 && right_type->kind <= TYPE_FLOAT64)) {

                            if (left_type->kind == TYPE_FLOAT64 || right_type->kind == TYPE_FLOAT64) {
                                result_type = type_create_basic(TYPE_FLOAT64);
                            } else if (left_type->kind == TYPE_FLOAT32 || right_type->kind == TYPE_FLOAT32) {
                                result_type = type_create_basic(TYPE_FLOAT32);
                            } else {
                                result_type = type_create_basic(TYPE_INT32);
                            }
                        } else {
                            type_error(context, "Invalid operands for arithmetic operator", 0);
                        }
                        break;

                    case TOKEN_EQUAL:
                    case TOKEN_NOT_EQUAL:
                    case TOKEN_LESS:
                    case TOKEN_GREATER:
                    case TOKEN_LESS_EQUAL:
                    case TOKEN_GREATER_EQUAL:

                        result_type = type_create_basic(TYPE_BOOL);
                        break;

                    default:
                        type_error(context, "Unsupported binary operator", 0);
                        break;
                }

                type_destroy(left_type);
                type_destroy(right_type);
                return result_type;
            }

        case AST_TERNARY_OPERATION:

            {

                Type* condition_type = type_check_expression(context, expression->data.ternary_op.condition);
                if (!condition_type) {
                    return NULL;
                }


                if (condition_type->kind != TYPE_BOOL) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Condition expression in ternary operator must be boolean, got %s",
                             type_get_name(condition_type));
                    type_error(context, error_msg, 0);
                    type_destroy(condition_type);
                    return NULL;
                }
                type_destroy(condition_type);


                Type* true_type = type_check_expression(context, expression->data.ternary_op.true_value);
                if (!true_type) {
                    return NULL;
                }


                Type* false_type = type_check_expression(context, expression->data.ternary_op.false_value);
                if (!false_type) {
                    type_destroy(true_type);
                    return NULL;
                }


                if (!type_is_compatible(true_type, false_type)) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Incompatible types in ternary operator branches: %s and %s",
                             type_get_name(true_type), type_get_name(false_type));
                    type_error(context, error_msg, 0);
                    type_destroy(true_type);
                    type_destroy(false_type);
                    return NULL;
                }


                type_destroy(false_type);
                return true_type;
            }

        case AST_UNARY_OPERATION:

            {
                Type* operand_type = type_check_expression(context, expression->data.unary_op.operand);
                if (!operand_type) {
                    return NULL;
                }

                Type* result_type = NULL;

                switch (expression->data.unary_op.operator) {
                    case TOKEN_MINUS:

                        if (operand_type->kind >= TYPE_INT8 && operand_type->kind <= TYPE_FLOAT64) {
                            result_type = type_create_basic(operand_type->kind);
                        } else {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Unary minus operator requires numeric operand, got %s",
                                     type_get_name(operand_type));
                            type_error(context, error_msg, 0);
                        }
                        break;

                    case TOKEN_NOT:

                        if (operand_type->kind == TYPE_BOOL) {
                            result_type = type_create_basic(TYPE_BOOL);
                        } else {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Logical NOT operator requires boolean operand, got %s",
                                     type_get_name(operand_type));
                            type_error(context, error_msg, 0);
                        }
                        break;

                    case TOKEN_INCREMENT:
                    case TOKEN_DECREMENT:

                        if (operand_type->kind >= TYPE_INT8 && operand_type->kind <= TYPE_FLOAT64) {
                            result_type = type_create_basic(operand_type->kind);


                            if (expression->data.unary_op.operand->type != AST_IDENTIFIER) {
                                type_error(context, "Increment/decrement operator requires lvalue operand", 0);
                            }
                        } else {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Increment/decrement operator requires numeric operand, got %s",
                                     type_get_name(operand_type));
                            type_error(context, error_msg, 0);
                        }
                        break;


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

                        TypeKind target_kind = TYPE_UNKNOWN;
                        switch (expression->data.unary_op.operator) {
                            case TOKEN_INT32: target_kind = TYPE_INT32; break;
                            case TOKEN_INT64: target_kind = TYPE_INT64; break;
                            case TOKEN_INT16: target_kind = TYPE_INT16; break;
                            case TOKEN_INT8: target_kind = TYPE_INT8; break;
                            case TOKEN_UINT32: target_kind = TYPE_UINT32; break;
                            case TOKEN_UINT64: target_kind = TYPE_UINT64; break;
                            case TOKEN_UINT16: target_kind = TYPE_UINT16; break;
                            case TOKEN_UINT8: target_kind = TYPE_UINT8; break;
                            case TOKEN_FLOAT32: target_kind = TYPE_FLOAT32; break;
                            case TOKEN_FLOAT64: target_kind = TYPE_FLOAT64; break;
                            case TOKEN_BOOL: target_kind = TYPE_BOOL; break;
                            default: break;
                        }

                        if (target_kind != TYPE_UNKNOWN) {

                            if (operand_type->kind >= TYPE_INT8 && operand_type->kind <= TYPE_FLOAT64) {
                                result_type = type_create_basic(target_kind);
                            } else {
                                char error_msg[256];
                                snprintf(error_msg, sizeof(error_msg),
                                         "Cannot cast type %s to %s",
                                         type_get_name(operand_type), type_get_name(type_create_basic(target_kind)));
                                type_error(context, error_msg, 0);
                            }
                        }
                        break;
                    }

                    default:
                        type_error(context, "Unsupported unary operator", 0);
                        break;
                }

                type_destroy(operand_type);
                return result_type;
            }

        case AST_ARRAY_LITERAL:

            {
                if (expression->data.array_literal.element_count == 0) {

                    Type* unknown_type = type_create_basic(TYPE_UNKNOWN);
                    Type* array_type = type_create_array(unknown_type, 0);
                    return array_type;
                }


                Type* first_element_type = type_check_expression(context, expression->data.array_literal.elements[0]);
                if (!first_element_type) {
                    return NULL;
                }


                for (int i = 1; i < expression->data.array_literal.element_count; i++) {
                    Type* element_type = type_check_expression(context, expression->data.array_literal.elements[i]);
                    if (!element_type) {
                        type_destroy(first_element_type);
                        return NULL;
                    }

                    if (!type_is_compatible(first_element_type, element_type)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Inconsistent element types in array literal: element %d has type %s but expected %s",
                                 i + 1, type_get_name(element_type), type_get_name(first_element_type));
                        type_error(context, error_msg, 0);
                        type_destroy(element_type);
                        type_destroy(first_element_type);
                        return NULL;
                    }

                    type_destroy(element_type);
                }


                Type* array_type = type_create_array(first_element_type, expression->data.array_literal.element_count);
                return array_type;
            }

        case AST_MEMBER_ACCESS:

            {

                Type* object_type = type_check_expression(context, expression->data.member_access.object);
                if (!object_type) {
                    return NULL;
                }


                const char* member_name = expression->data.member_access.member_name;


                Type* member_type = NULL;

                if (object_type->kind == TYPE_POINTER && object_type->data.pointer_to && object_type->data.pointer_to->kind == TYPE_CLASS) {
                    object_type = object_type->data.pointer_to;
                }
                if (object_type->kind == TYPE_STRUCT || object_type->kind == TYPE_CLASS) {

                    ClassInfo* class_info = object_type->data.class.class_info;

                    if (class_info) {

                        ClassMember* member = class_info_find_member(class_info, member_name);
                        if (member) {

                            if (member->access == ACCESS_PRIVATE) {

                                if (!context->current_class || strcmp(context->current_class, class_info->class_name) != 0) {
                                    char error_msg[256];
                                    snprintf(error_msg, sizeof(error_msg),
                                             "Member '%s' is private and cannot be accessed outside class '%s'",
                                             member_name, class_info->class_name);
                                    type_error(context, error_msg, 0);
                                }
                            } else if (member->access == ACCESS_PROTECTED) {


                                if (!context->current_class) {
                                    char error_msg[256];
                                    snprintf(error_msg, sizeof(error_msg),
                                             "Member '%s' is protected and cannot be accessed outside class hierarchy",
                                             member_name);
                                    type_error(context, error_msg, 0);
                                }
                            }

                            int accessing_via_class = 0;
                            if (expression->data.member_access.object->type == AST_IDENTIFIER) {
                                const char* obj_name = expression->data.member_access.object->data.identifier_name;
                                TypeCheckSymbol* obj_sym = type_check_symbol_table_lookup(context->current_scope, obj_name);
                                if (obj_sym && obj_sym->type && obj_sym->type->kind == TYPE_CLASS) {
                                    accessing_via_class = 1;
                                }
                            }
                            if (accessing_via_class && !member->is_static) {
                                char error_msg[256];
                                snprintf(error_msg, sizeof(error_msg),
                                         "Cannot access non-static member '%s' via class '%s'",
                                         member_name, class_info->class_name);
                                type_error(context, error_msg, 0);
                            }


                            member_type = type_copy(member->type);
                            if (!accessing_via_class) {
                                if (expression->data.member_access.resolved_class_name) {
                                    ES_FREE(expression->data.member_access.resolved_class_name);
                                }
                                expression->data.member_access.resolved_class_name = strdup(class_info->class_name);
                            }
                        } else {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Class '%s' does not have member '%s'",
                                     class_info->class_name, member_name);
                            type_error(context, error_msg, 0);
                            member_type = type_create_basic(TYPE_UNKNOWN);
                        }
                    } else {

                        for (int i = 0; i < object_type->data.class.field_count; i++) {
                            if (strcmp(object_type->data.class.field_names[i], member_name) == 0) {
                                member_type = type_copy(object_type->data.class.field_types[i]);
                                if (expression->data.member_access.resolved_class_name) {
                                    ES_FREE(expression->data.member_access.resolved_class_name);
                                }
                                expression->data.member_access.resolved_class_name = strdup(object_type->data.class.class_name);
                                break;
                        }
                    }

                    if (!member_type) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Type %s does not have member '%s'",
                                 type_get_name(object_type), member_name);
                        type_error(context, error_msg, 0);
                        member_type = type_create_basic(TYPE_UNKNOWN);
                        }
                    }
                } else if (object_type->kind == TYPE_ARRAY) {

                    if (strcmp(member_name, "length") == 0) {
                        member_type = type_create_basic(TYPE_INT32);
                    } else {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Array type does not have member '%s'", member_name);
                        type_error(context, error_msg, 0);
                        member_type = type_create_basic(TYPE_UNKNOWN);
                    }
                } else {

                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Type %s does not support member access", type_get_name(object_type));
                    type_error(context, error_msg, 0);
                    member_type = type_create_basic(TYPE_UNKNOWN);
                }

                type_destroy(object_type);
                return member_type;
            }

        case AST_NEW_EXPRESSION:

            {
                const char* class_name = expression->data.new_expr.class_name;


                TypeCheckSymbol* class_symbol = type_check_symbol_table_lookup(context->current_scope, class_name);
                if (!class_symbol || class_symbol->type->kind != TYPE_CLASS) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Undefined class '%s'", class_name);
                    type_error(context, error_msg, 0);
                    return NULL;
                }


                Type* class_type = type_create_class(class_name);


                if (expression->data.new_expr.arguments) {

                    if (class_symbol->type->data.class.field_count > 0) {

                        if (expression->data.new_expr.argument_count != class_symbol->type->data.class.field_count) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Class '%s' constructor expects %d arguments but got %d",
                                     class_name, class_symbol->type->data.class.field_count, expression->data.new_expr.argument_count);
                            type_error(context, error_msg, 0);
                            type_destroy(class_type);
                            return NULL;
                        }


                        for (int i = 0; i < expression->data.new_expr.argument_count; i++) {
                            Type* arg_type = type_check_expression(context, expression->data.new_expr.arguments[i]);
                            if (!arg_type) {
                                type_destroy(class_type);
                                return NULL;
                            }


                            Type* expected_type = class_symbol->type->data.class.field_types[i];
                            if (!type_is_compatible(expected_type, arg_type)) {
                                char error_msg[256];
                                snprintf(error_msg, sizeof(error_msg),
                                         "Constructor argument %d type mismatch: expected %s but got %s",
                                         i + 1, type_get_name(expected_type), type_get_name(arg_type));
                                type_error(context, error_msg, 0);
                            }

                            type_destroy(arg_type);
                        }
                    } else {

                        if (expression->data.new_expr.argument_count > 0) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Class '%s' has no fields but %d arguments provided",
                                     class_name, expression->data.new_expr.argument_count);
                            type_error(context, error_msg, 0);
                        }
                    }
                }

                return type_create_pointer(class_type);
            }

        case AST_THIS:

            {

                if (!context->current_class) {
                    type_error(context, "'this' keyword can only be used within a class or struct", 0);
                    return NULL;
                }

                if (context->current_function_is_static) {
                    type_error(context, "'this' cannot be used in static method", 0);
                    return NULL;
                }


                Type* this_type = type_create_class(context->current_class);

                return this_type;
            }

        case AST_STATIC_METHOD_CALL:

            {
                const char* class_name = expression->data.static_call.class_name;
                const char* method_name = expression->data.static_call.method_name;
#ifdef DEBUG
                ES_COMPILER_DEBUG("Processing static call %s::%s", class_name, method_name);
#endif
                if (method_name && strcmp(method_name, "delete") == 0) {
                    TypeCheckSymbol* class_symbol = type_check_symbol_table_lookup(context->current_scope, class_name);
                    if (!class_symbol || class_symbol->type->kind != TYPE_CLASS) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Undefined class '%s'", class_name);
                        type_error(context, error_msg, 0);
                        return NULL;
                    }
                    ClassInfo* class_info = class_symbol->type->data.class.class_info;
                    if (!class_info) {
                        type_warning(context, "Class information unavailable for delete call", 0);
                        return type_create_basic(TYPE_VOID);
                    }
                    char dname[256];
                    snprintf(dname, sizeof(dname), "~%s", class_name);
                    ClassMember* dmember = class_info_find_member(class_info, dname);
                    if (!dmember || dmember->member_type != MEMBER_DESTRUCTOR) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Class '%s' does not have destructor", class_name);
                        type_error(context, error_msg, 0);
                        return NULL;
                    }
                    int actual_param_count = expression->data.static_call.argument_count;
                    if (actual_param_count != 1) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Delete '%s.delete' expects 1 argument but got %d",
                                 class_name, actual_param_count);
                        type_error(context, error_msg, 0);
                    }
                    return type_create_basic(TYPE_VOID);
                }


                TypeCheckSymbol* class_symbol = type_check_symbol_table_lookup(context->current_scope, class_name);
                if (!class_symbol || class_symbol->type->kind != TYPE_CLASS) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Undefined class '%s'", class_name);
                    type_error(context, error_msg, 0);
                    return NULL;
                }

                ClassInfo* class_info = class_symbol->type->data.class.class_info;
                if (!class_info) {

                    type_warning(context, "Class information unavailable for static method call", 0);
                    return type_create_basic(TYPE_UNKNOWN);
                }

                ClassMember* member = class_info_find_member(class_info, method_name);
                if (!member) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Class '%s' does not have static method '%s'", class_name, method_name);
                    type_error(context, error_msg, 0);
                    return NULL;
                }

#ifdef DEBUG
                ES_COMPILER_DEBUG("Found member '%s', member_type=%d, is_static=%d, type_kind=%d",
                        method_name, member->member_type, member->is_static, member->type->kind);
#endif
                if (member->type->kind == TYPE_FUNCTION) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Member function return type: %s",
                            type_get_name(member->type->data.function.return_type));
#endif
                }

                if (member->member_type != MEMBER_METHOD || !member->is_static) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "'%s::%s' is not a static method", class_name, method_name);
                    type_error(context, error_msg, 0);
                    return NULL;
                }

                Type* func_type = member->type;
#ifdef DEBUG
                ES_COMPILER_DEBUG("Found static method %s::%s, return type: %s",
                        class_name, method_name, type_get_name(func_type->data.function.return_type));
#endif
                if (!func_type || func_type->kind != TYPE_FUNCTION) {
                    type_error(context, "Static method does not have valid function type", 0);
                    return NULL;
                }


                int expected_param_count = func_type->data.function.parameter_count;
                int actual_param_count = expression->data.static_call.argument_count;

                if (expected_param_count != actual_param_count) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Static method '%s::%s' expects %d arguments but got %d",
                             class_name, method_name, expected_param_count, actual_param_count);
                    type_error(context, error_msg, 0);
                }

                int param_to_check = actual_param_count < expected_param_count ? actual_param_count : expected_param_count;
                for (int i = 0; i < param_to_check; i++) {
                    Type* arg_type = type_check_expression(context, expression->data.static_call.arguments[i]);
                    if (!arg_type) {
                        return NULL;
                    }
                    Type* expected_type = func_type->data.function.parameter_types[i];
                    if (!type_is_compatible(expected_type, arg_type)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Argument %d type mismatch in call to %s::%s: expected %s but got %s",
                                 i + 1, class_name, method_name,
                                 type_get_name(expected_type), type_get_name(arg_type));
                        type_error(context, error_msg, 0);
                    }
                    type_destroy(arg_type);
                }


                return type_copy(func_type->data.function.return_type);
            }

            if (expression->data.call.object && expression->data.call.object->type == AST_THIS) {
                if (!context->current_class_info) {
                    type_error(context, "'this' used outside of class", 0);
                    return NULL;
                }
                ClassMember* member = class_info_find_member(context->current_class_info, expression->data.call.name);
                if (!member) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Class '%s' does not have method '%s'",
                             context->current_class ? context->current_class : "", expression->data.call.name);
                    type_error(context, error_msg, 0);
                    return NULL;
                }
                if (member->member_type != MEMBER_METHOD || member->is_static) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "'%s.%s' is not an instance method",
                             context->current_class ? context->current_class : "", expression->data.call.name);
                    type_error(context, error_msg, 0);
                    return NULL;
                }
                Type* func_type = member->type;
                int expected_param_count = func_type->data.function.parameter_count;
                int actual_param_count = expression->data.call.argument_count;
                if (expected_param_count != actual_param_count) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Method '%s.%s' expects %d arguments but got %d",
                             context->current_class ? context->current_class : "", expression->data.call.name,
                             expected_param_count, actual_param_count);
                    type_error(context, error_msg, 0);
                }
                int param_to_check = actual_param_count < expected_param_count ? actual_param_count : expected_param_count;
                for (int i = 0; i < param_to_check; i++) {
                    Type* arg_type = type_check_expression(context, expression->data.call.arguments[i]);
                    if (!arg_type) {
                        return NULL;
                    }
                    Type* expected_type = func_type->data.function.parameter_types[i];
                    if (!type_is_compatible(expected_type, arg_type)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Argument %d type mismatch in call to %s.%s: expected %s but got %s",
                                 i + 1,
                                 context->current_class ? context->current_class : "", expression->data.call.name,
                                 type_get_name(expected_type), type_get_name(arg_type));
                        type_error(context, error_msg, 0);
                    }
                    type_destroy(arg_type);
                }
                return type_copy(func_type->data.function.return_type);
            }

        case AST_CALL:

            {
                if (expression->data.call.object) {
                    Type* obj_type = type_check_expression(context, expression->data.call.object);
                    int is_class_constant = 0;
                    if (expression->data.call.object->type == AST_IDENTIFIER) {
                        TypeCheckSymbol* sym = type_check_symbol_table_lookup(context->current_scope, expression->data.call.object->data.identifier_name);
                        if (sym && sym->type && sym->type->kind == TYPE_CLASS && sym->is_constant) {
                            is_class_constant = 1;
                        }
                    }
                    const char* class_name_inst = NULL;
                    if (obj_type && obj_type->kind == TYPE_POINTER && obj_type->data.pointer_to && obj_type->data.pointer_to->kind == TYPE_CLASS) {
                        class_name_inst = obj_type->data.pointer_to->data.class.class_name;
                    } else if (obj_type && obj_type->kind == TYPE_CLASS && !is_class_constant) {
                        class_name_inst = obj_type->data.class.class_name;
                    }
                    if (class_name_inst) {
                        TypeCheckSymbol* class_symbol = type_check_symbol_table_lookup(context->current_scope, class_name_inst);
                        if (!class_symbol || class_symbol->type->kind != TYPE_CLASS) {
                            if (obj_type) type_destroy(obj_type);
                            return NULL;
                        }
                        ClassInfo* class_info = class_symbol->type->data.class.class_info;
                        if (!class_info) {
                            if (obj_type) type_destroy(obj_type);
                            return type_create_basic(TYPE_UNKNOWN);
                        }
                        ClassMember* member = class_info_find_member(class_info, expression->data.call.name);
                        if (!member) {
                            if (obj_type) type_destroy(obj_type);
                            return NULL;
                        }
                        if (member->member_type != MEMBER_METHOD || member->is_static) {
                            if (obj_type) type_destroy(obj_type);
                            return NULL;
                        }
                        if (expression->data.call.resolved_class_name) {
                            ES_FREE(expression->data.call.resolved_class_name);
                        }
                        expression->data.call.resolved_class_name = strdup(class_name_inst);
                        Type* func_type = member->type;
                        int expected_param_count = func_type->data.function.parameter_count;
                        int actual_param_count = expression->data.call.argument_count;
                        int param_to_check = actual_param_count < expected_param_count ? actual_param_count : expected_param_count;
                        for (int i = 0; i < param_to_check; i++) {
                            Type* arg_type = type_check_expression(context, expression->data.call.arguments[i]);
                            if (!arg_type) {
                                if (obj_type) type_destroy(obj_type);
                                return NULL;
                            }
                            Type* expected_type = func_type->data.function.parameter_types[i];
                            if (!type_is_compatible(expected_type, arg_type)) {
                            }
                            type_destroy(arg_type);
                        }
                        if (obj_type) type_destroy(obj_type);
                        return type_copy(func_type->data.function.return_type);
                    }
                    if (obj_type) type_destroy(obj_type);
                }
                if (expression->data.call.object && expression->data.call.object->type == AST_IDENTIFIER) {
                    const char* obj_name = expression->data.call.object->data.identifier_name;
                    TypeCheckSymbol* obj_symbol = type_check_symbol_table_lookup(context->current_scope, obj_name);
                    if (obj_symbol && obj_symbol->type && obj_symbol->type->kind == TYPE_CLASS && obj_symbol->is_constant == 0) {
                        ClassInfo* class_info = obj_symbol->type->data.class.class_info;
                        if (!class_info) {
                            type_warning(context, "Class information unavailable for instance method call", 0);
                            return type_create_basic(TYPE_UNKNOWN);
                        }
                        ClassMember* member = class_info_find_member(class_info, expression->data.call.name);
                        if (!member) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Class '%s' does not have method '%s'",
                                     class_info->class_name, expression->data.call.name);
                            type_error(context, error_msg, 0);
                            return NULL;
                        }
                        if (member->member_type != MEMBER_METHOD || member->is_static) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "'%s.%s' is not an instance method",
                                     class_info->class_name, expression->data.call.name);
                            type_error(context, error_msg, 0);
                            return NULL;
                        }
                        if (expression->data.call.resolved_class_name) {
                            ES_FREE(expression->data.call.resolved_class_name);
                        }
                        expression->data.call.resolved_class_name = strdup(class_info->class_name);
                        Type* func_type = member->type;
                        int expected_param_count = func_type->data.function.parameter_count;
                        int actual_param_count = expression->data.call.argument_count;
                        if (expected_param_count != actual_param_count) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Method '%s.%s' expects %d arguments but got %d",
                                     class_info->class_name, expression->data.call.name,
                                     expected_param_count, actual_param_count);
                            type_error(context, error_msg, 0);
                        }
                        int param_to_check = actual_param_count < expected_param_count ? actual_param_count : expected_param_count;
                        for (int i = 0; i < param_to_check; i++) {
                            Type* arg_type = type_check_expression(context, expression->data.call.arguments[i]);
                            if (!arg_type) {
                                return NULL;
                            }
                            Type* expected_type = func_type->data.function.parameter_types[i];
                            if (!type_is_compatible(expected_type, arg_type)) {
                                char error_msg[256];
                                snprintf(error_msg, sizeof(error_msg),
                                         "Argument %d type mismatch in call to %s.%s: expected %s but got %s",
                                         i + 1, class_info->class_name, expression->data.call.name,
                                         type_get_name(expected_type), type_get_name(arg_type));
                                type_error(context, error_msg, 0);
                            }
                            type_destroy(arg_type);
                        }
                        return type_copy(func_type->data.function.return_type);
                    }
                    const char* class_name = obj_name;
                    if (strcmp(expression->data.call.name, "delete") == 0) {
                        TypeCheckSymbol* class_symbol = type_check_symbol_table_lookup(context->current_scope, class_name);
                        if (!class_symbol || class_symbol->type->kind != TYPE_CLASS) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Undefined class '%s'", class_name);
                            type_error(context, error_msg, 0);
                            return NULL;
                        }
                        ClassInfo* class_info = class_symbol->type->data.class.class_info;
                        if (!class_info) {
                            type_warning(context, "Class information unavailable for delete call", 0);
                            return type_create_basic(TYPE_VOID);
                        }
                        char dname[256];
                        snprintf(dname, sizeof(dname), "~%s", class_name);
                        ClassMember* dmember = class_info_find_member(class_info, dname);
                        if (!dmember || dmember->member_type != MEMBER_DESTRUCTOR) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Class '%s' does not have destructor", class_name);
                            type_error(context, error_msg, 0);
                            return NULL;
                        }
                        int actual_param_count = expression->data.call.argument_count;
                        if (actual_param_count != 1) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Delete '%s.delete' expects 1 argument but got %d",
                                     class_name, actual_param_count);
                            type_error(context, error_msg, 0);
                        }
                        return type_create_basic(TYPE_VOID);
                    }
                    TypeCheckSymbol* class_symbol = type_check_symbol_table_lookup(context->current_scope, class_name);
                    if (!class_symbol || class_symbol->type->kind != TYPE_CLASS) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Undefined class '%s'", class_name);
                        type_error(context, error_msg, 0);
                        return NULL;
                    }
                    ClassInfo* class_info = class_symbol->type->data.class.class_info;
                    if (!class_info) {
                        type_warning(context, "Class information unavailable for method call", 0);
                        return type_create_basic(TYPE_UNKNOWN);
                    }
                    ClassMember* member = class_info_find_member(class_info, expression->data.call.name);
                    if (!member) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Class '%s' does not have method '%s'",
                                 class_name, expression->data.call.name);
                        type_error(context, error_msg, 0);
                        return NULL;
                    }
                    if (member->member_type != MEMBER_METHOD || !member->is_static) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "'%s.%s' is not a static method",
                                 class_name, expression->data.call.name);
                        type_error(context, error_msg, 0);
                        return NULL;
                    }
                    Type* func_type = member->type;
                    int expected_param_count = func_type->data.function.parameter_count;
                    int actual_param_count = expression->data.call.argument_count;
                    if (expected_param_count != actual_param_count) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Static method '%s.%s' expects %d arguments but got %d",
                                 class_name, expression->data.call.name, expected_param_count, actual_param_count);
                        type_error(context, error_msg, 0);
                    }
                    int param_to_check = actual_param_count < expected_param_count ? actual_param_count : expected_param_count;
                    for (int i = 0; i < param_to_check; i++) {
                        Type* arg_type = type_check_expression(context, expression->data.call.arguments[i]);
                        if (!arg_type) {
                            return NULL;
                        }
                        Type* expected_type = func_type->data.function.parameter_types[i];
                        if (!type_is_compatible(expected_type, arg_type)) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Argument %d type mismatch in call to %s.%s: expected %s but got %s",
                                     i + 1, class_name, expression->data.call.name,
                                     type_get_name(expected_type), type_get_name(arg_type));
                            type_error(context, error_msg, 0);
                        }
                        type_destroy(arg_type);
                    }
                    return type_copy(func_type->data.function.return_type);
                }

                TypeCheckSymbol* func_symbol = type_check_symbol_table_lookup(context->current_scope, expression->data.call.name);
                if (!func_symbol) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Undefined function '%s'",
                             expression->data.call.name);
                    type_error(context, error_msg, 0);
                    return NULL;
                }

                if (func_symbol->type->kind != TYPE_FUNCTION) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "'%s' is not a function",
                             expression->data.call.name);
                    type_error(context, error_msg, 0);
                    return NULL;
                }

                int expected_param_count = func_symbol->type->data.function.parameter_count;
                int actual_param_count = expression->data.call.argument_count;
                if (expected_param_count != actual_param_count) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Function '%s' expects %d arguments but got %d",
                             expression->data.call.name, expected_param_count, actual_param_count);
                    type_error(context, error_msg, 0);
                    return func_symbol->type->data.function.return_type;
                }
                for (int i = 0; i < actual_param_count; i++) {
                    Type* arg_type = type_check_expression(context, expression->data.call.arguments[i]);
                    Type* expected_type = func_symbol->type->data.function.parameter_types[i];
                    if (!arg_type) {
                        type_error(context, "Cannot determine type of function argument", 0);
                        continue;
                    }
                    if (!type_is_compatible(expected_type, arg_type)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Argument %d type mismatch: expected %s but got %s",
                                 i + 1, type_get_name(expected_type), type_get_name(arg_type));
                        type_error(context, error_msg, 0);
                    }
                    type_destroy(arg_type);
                }
                Type* return_type_copy = type_copy(func_symbol->type->data.function.return_type);
                if (!return_type_copy) {
                    type_error(context, "Failed to copy function return type", 0);
                    return NULL;
                }
                return return_type_copy;
            }

        case AST_ARRAY_ACCESS:
            {

#ifdef DEBUG
                ES_COMPILER_DEBUG("Processing array access");
#endif
                Type* array_type = type_check_expression(context, expression->data.array_access.array);
                if (!array_type) {
                    type_error(context, "Cannot determine type of array expression", 0);
                    return NULL;
                }
#ifdef DEBUG
                ES_COMPILER_DEBUG("Array type: %s", type_get_name(array_type));
#endif


                if (array_type->kind != TYPE_ARRAY) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Array access requires array type, got %s",
                             type_get_name(array_type));
                    type_error(context, error_msg, 0);
                    type_destroy(array_type);
                    return NULL;
                }


                Type* index_type = type_check_expression(context, expression->data.array_access.index);
                if (!index_type) {
                    type_error(context, "Cannot determine type of array index", 0);
                    type_destroy(array_type);
                    return NULL;
                }


                if (index_type->kind < TYPE_INT8 || index_type->kind > TYPE_UINT64) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Array index must be an integer type, got %s",
                             type_get_name(index_type));
                    type_error(context, error_msg, 0);
                    type_destroy(array_type);
                    type_destroy(index_type);
                    return NULL;
                }


#ifdef DEBUG
                ES_COMPILER_DEBUG("Array type kind: %d, element_type pointer: %p",
                        array_type->kind, (void*)array_type->data.array.element_type);
#endif

                if (!array_type->data.array.element_type) {
                    type_error(context, "Array element type is NULL - this indicates a bug in array type creation", 0);
                    type_destroy(array_type);
                    type_destroy(index_type);
                    return NULL;
                }

                Type* element_type = type_copy(array_type->data.array.element_type);
                type_destroy(array_type);
                type_destroy(index_type);

                if (!element_type) {
                    type_error(context, "Failed to copy array element type", 0);
                    return NULL;
                }

                return element_type;
            }

        default:
            type_error(context, "Unsupported expression type", 0);
            return NULL;
    }
}


void type_check_init_builtin_types(TypeCheckContext* context) {
    if (!context) return;


    TypeCheckSymbol int8_symbol = {"int8", type_create_basic(TYPE_INT8), 1, 0, 0};
    TypeCheckSymbol int16_symbol = {"int16", type_create_basic(TYPE_INT16), 1, 0, 0};
    TypeCheckSymbol int32_symbol = {"int32", type_create_basic(TYPE_INT32), 1, 0, 0};
    TypeCheckSymbol int64_symbol = {"int64", type_create_basic(TYPE_INT64), 1, 0, 0};
    TypeCheckSymbol uint8_symbol = {"uint8", type_create_basic(TYPE_UINT8), 1, 0, 0};
    TypeCheckSymbol uint16_symbol = {"uint16", type_create_basic(TYPE_UINT16), 1, 0, 0};
    TypeCheckSymbol uint32_symbol = {"uint32", type_create_basic(TYPE_UINT32), 1, 0, 0};
    TypeCheckSymbol uint64_symbol = {"uint64", type_create_basic(TYPE_UINT64), 1, 0, 0};
    TypeCheckSymbol float32_symbol = {"float32", type_create_basic(TYPE_FLOAT32), 1, 0, 0};
    TypeCheckSymbol float64_symbol = {"float64", type_create_basic(TYPE_FLOAT64), 1, 0, 0};
    TypeCheckSymbol bool_symbol = {"bool", type_create_basic(TYPE_BOOL), 1, 0, 0};
    TypeCheckSymbol string_symbol = {"string", type_create_basic(TYPE_STRING), 1, 0, 0};
    TypeCheckSymbol void_symbol = {"void", type_create_basic(TYPE_VOID), 1, 0, 0};

    type_check_symbol_table_add(context->current_scope, int8_symbol);
    type_check_symbol_table_add(context->current_scope, int16_symbol);
    type_check_symbol_table_add(context->current_scope, int32_symbol);
    type_check_symbol_table_add(context->current_scope, int64_symbol);
    type_check_symbol_table_add(context->current_scope, uint8_symbol);
    type_check_symbol_table_add(context->current_scope, uint16_symbol);
    type_check_symbol_table_add(context->current_scope, uint32_symbol);
    type_check_symbol_table_add(context->current_scope, uint64_symbol);
    type_check_symbol_table_add(context->current_scope, float32_symbol);
    type_check_symbol_table_add(context->current_scope, float64_symbol);
    type_check_symbol_table_add(context->current_scope, bool_symbol);
    type_check_symbol_table_add(context->current_scope, string_symbol);
    type_check_symbol_table_add(context->current_scope, void_symbol);
}

void type_check_init_builtin_functions(TypeCheckContext* context) {
    if (!context) return;

    Type* void_type = type_create_basic(TYPE_VOID);
    Type* int32_type = type_create_basic(TYPE_INT32);
    Type* float64_type = type_create_basic(TYPE_FLOAT64);
    Type* string_type = type_create_basic(TYPE_STRING);

    Type** print_number_params = (Type**)ES_MALLOC(sizeof(Type*) * 1);
    print_number_params[0] = float64_type;
    Type* print_number_type = type_create_function(void_type, print_number_params, 1);
    TypeCheckSymbol print_number_symbol = {"print_number", print_number_type, 1, 0, 0};
    type_check_symbol_table_add(context->current_scope, print_number_symbol);

    Type** print_int_params = (Type**)ES_MALLOC(sizeof(Type*) * 1);
    print_int_params[0] = int32_type;
    Type* print_int_type = type_create_function(void_type, print_int_params, 1);
    TypeCheckSymbol print_int_symbol = {"print_int", print_int_type, 1, 0, 0};
    type_check_symbol_table_add(context->current_scope, print_int_symbol);

    Type** print_string_params = (Type**)ES_MALLOC(sizeof(Type*) * 1);
    print_string_params[0] = string_type;
    Type* print_string_type = type_create_function(void_type, print_string_params, 1);
    TypeCheckSymbol print_string_symbol = {"print_string", print_string_type, 1, 0, 0};
    type_check_symbol_table_add(context->current_scope, print_string_symbol);

    Type** print_params = (Type**)ES_MALLOC(sizeof(Type*) * 1);
    print_params[0] = string_type;
    Type* print_type = type_create_function(void_type, print_params, 1);
    TypeCheckSymbol print_symbol = {"print", print_type, 1, 0, 0};
    type_check_symbol_table_add(context->current_scope, print_symbol);

    Type** malloc_params = (Type**)ES_MALLOC(sizeof(Type*) * 1);
    malloc_params[0] = int32_type;
    Type* pointer_type = type_create_pointer(void_type);
    Type* malloc_type = type_create_function(pointer_type, malloc_params, 1);
    TypeCheckSymbol malloc_symbol = {"es_malloc", malloc_type, 1, 0, 0};
    type_check_symbol_table_add(context->current_scope, malloc_symbol);

    Type** free_params = (Type**)ES_MALLOC(sizeof(Type*) * 1);
    free_params[0] = pointer_type;
    Type* free_type = type_create_function(void_type, free_params, 1);
    TypeCheckSymbol free_symbol = {"es_free", free_type, 1, 0, 0};
    type_check_symbol_table_add(context->current_scope, free_symbol);
}


Type* infer_function_return_type(TypeCheckContext* context, ASTNode* function_body) {
    if (!function_body) {
        return type_create_basic(TYPE_VOID);
    }


    return infer_return_type_from_statement(context, function_body);
}


Type* infer_return_type_from_statement(TypeCheckContext* context, ASTNode* statement) {
    if (!statement) {
        return NULL;
    }

#ifdef DEBUG
    ES_COMPILER_DEBUG("infer_return_type_from_statement called with statement type=%d", statement->type);
#endif

    switch (statement->type) {
        case AST_RETURN_STATEMENT:
#ifdef DEBUG
            ES_COMPILER_DEBUG("Found return statement");
#endif

            if (statement->data.return_stmt.value) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Return statement has value, expression type=%d", statement->data.return_stmt.value->type);
#endif

                TypeCheckContext silent_context = *context;
                silent_context.error_count = 0;
                silent_context.suppress_errors = 1;

                Type* return_type = type_check_expression(&silent_context, statement->data.return_stmt.value);
#ifdef DEBUG
                ES_COMPILER_DEBUG("Return statement expression type: %s",
                        return_type ? type_get_name(return_type) : "NULL");
#endif


                if (return_type) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Successfully inferred return type: %s", type_get_name(return_type));
#endif
                    return return_type;
                } else {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Expression check returned NULL, trying direct type check");
#endif

                    Type* direct_type = type_check_expression(context, statement->data.return_stmt.value);
                    if (direct_type) {
#ifdef DEBUG
                        ES_COMPILER_DEBUG("Direct type check succeeded: %s", type_get_name(direct_type));
#endif
                        return direct_type;
                    }
                }
            } else {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Return statement has no value, returning void");
#endif
                return type_create_basic(TYPE_VOID);
            }
#ifdef DEBUG
            ES_COMPILER_DEBUG("Return statement could not infer type, returning NULL");
#endif
            return NULL;

        case AST_IF_STATEMENT:
            {

                Type* then_type = infer_return_type_from_statement(context, statement->data.if_stmt.then_branch);
                Type* else_type = NULL;

                if (statement->data.if_stmt.else_branch) {
                    else_type = infer_return_type_from_statement(context, statement->data.if_stmt.else_branch);
                }


                if (then_type && else_type) {

                    type_destroy(else_type);
                    return then_type;
                } else if (then_type) {
                    return then_type;
                } else if (else_type) {
                    return else_type;
                }
                return NULL;
            }

        case AST_WHILE_STATEMENT:
        case AST_FOR_STATEMENT:

            return infer_return_type_from_statement(context, statement->data.while_stmt.body);

        case AST_BLOCK:
#ifdef DEBUG
            ES_COMPILER_DEBUG("Found block statement with %d statements",
                    statement->data.block.statement_count);
#endif

            for (int i = 0; i < statement->data.block.statement_count; i++) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Checking block statement %d", i);
#endif
                Type* stmt_type = infer_return_type_from_statement(context, statement->data.block.statements[i]);
                if (stmt_type) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Found return type in block statement %d: %s",
                            i, type_get_name(stmt_type));
#endif
                    return stmt_type;
                }
            }
#ifdef DEBUG
            ES_COMPILER_DEBUG("No return type found in block");
#endif
            return NULL;

        default:

            {
                TypeCheckContext silent_context = *context;
                silent_context.error_count = 0;
                silent_context.suppress_errors = 1;

                Type* expr_type = type_check_expression(&silent_context, statement);
                if (expr_type) {
                    return expr_type;
                }
            }
            return NULL;
    }
}


static int type_check_class_body(TypeCheckContext* context, ClassInfo* class_info,
                                 ASTNode* body, AccessModifier default_access) {
    if (!body || body->type != AST_BLOCK) {
        return 1;
    }


#ifdef DEBUG
    ES_COMPILER_DEBUG("Processing class body with %d statements", body->data.block.statement_count);
#endif


    for (int i = 0; i < body->data.block.statement_count; i++) {
        ASTNode* statement = body->data.block.statements[i];
        AccessModifier member_access = default_access;
        int is_static = 0;


#ifdef DEBUG
        ES_COMPILER_DEBUG("Processing class statement %d: type=%d", i, statement->type);
#endif


        if (statement->type == AST_ACCESS_MODIFIER) {
            member_access = token_to_access_modifier(statement->data.access_modifier.access_modifier);
            statement = statement->data.access_modifier.member;
            if (!statement) continue;
        }


        if (statement->type == AST_STATIC_VARIABLE_DECLARATION ||
            statement->type == AST_STATIC_FUNCTION_DECLARATION) {
            is_static = 1;
        }


        switch (statement->type) {
            case AST_VARIABLE_DECLARATION:
            case AST_STATIC_VARIABLE_DECLARATION:

                if (!type_check_add_class_member(context, class_info, statement, member_access, is_static)) {
                    return 0;
                }
                break;

            case AST_FUNCTION_DECLARATION:
            case AST_STATIC_FUNCTION_DECLARATION:

                if (!type_check_add_class_member(context, class_info, statement, member_access, is_static)) {
                    return 0;
                }
                break;

            case AST_CONSTRUCTOR_DECLARATION:
            case AST_DESTRUCTOR_DECLARATION:

                if (!type_check_statement(context, statement)) {
                    return 0;
                }
                break;

            default:

                type_error(context, "Invalid statement in class body", 0);
                return 0;
        }
    }

    return 1;
}


static int type_check_add_class_member(TypeCheckContext* context, ClassInfo* class_info,
                                       ASTNode* member, AccessModifier access, int is_static) {
    if (!class_info || !member) return 0;

    switch (member->type) {
        case AST_VARIABLE_DECLARATION:
        case AST_STATIC_VARIABLE_DECLARATION: {

            const char* field_name = member->type == AST_VARIABLE_DECLARATION
                ? member->data.variable_decl.name
                : member->data.static_variable_decl.name;


            ClassMember* existing = class_info_find_member(class_info, field_name);
            if (existing) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg),
                         "Member '%s' already declared in class '%s'",
                         field_name, class_info->class_name);
                type_error(context, error_msg, 0);
                return 0;
            }


            Type* field_type = NULL;
            if (member->type == AST_VARIABLE_DECLARATION) {
                EsTokenType type_token = member->data.variable_decl.type;
                switch (type_token) {
                    case TOKEN_INT8: field_type = type_create_basic(TYPE_INT8); break;
                    case TOKEN_INT16: field_type = type_create_basic(TYPE_INT16); break;
                    case TOKEN_INT32: field_type = type_create_basic(TYPE_INT32); break;
                    case TOKEN_INT64: field_type = type_create_basic(TYPE_INT64); break;
                    case TOKEN_UINT8: field_type = type_create_basic(TYPE_UINT8); break;
                    case TOKEN_UINT16: field_type = type_create_basic(TYPE_UINT16); break;
                    case TOKEN_UINT32: field_type = type_create_basic(TYPE_UINT32); break;
                    case TOKEN_UINT64: field_type = type_create_basic(TYPE_UINT64); break;
                    case TOKEN_FLOAT32: field_type = type_create_basic(TYPE_FLOAT32); break;
                    case TOKEN_FLOAT64: field_type = type_create_basic(TYPE_FLOAT64); break;
                    case TOKEN_BOOL: field_type = type_create_basic(TYPE_BOOL); break;
                    case TOKEN_STRING: field_type = type_create_basic(TYPE_STRING); break;
                    default: field_type = type_create_basic(TYPE_UNKNOWN); break;
                }
            } else {

                if (member->data.static_variable_decl.value) {
                    field_type = type_check_expression(context, member->data.static_variable_decl.value);
                } else {
                    field_type = type_create_basic(TYPE_UNKNOWN);
                }
            }

            if (!field_type) {
                type_error(context, "Failed to determine field type", 0);
                return 0;
            }


            ClassMember class_member = {
                .name = strdup(field_name),
                .member_type = MEMBER_FIELD,
                .access = access,
                .is_static = is_static,
                .type = field_type
            };


            class_info_add_member(class_info, class_member);


            if (class_info->member_scope) {
                TypeCheckSymbol field_sym = {
                    .name = strdup(field_name),
                    .type = type_copy(field_type),
                    .is_constant = 0,
                    .line_number = 0
                };
                type_check_symbol_table_add(class_info->member_scope, field_sym);
            }
            break;
        }

        case AST_FUNCTION_DECLARATION:
        case AST_STATIC_FUNCTION_DECLARATION: {

            const char* method_name = member->type == AST_FUNCTION_DECLARATION
                ? member->data.function_decl.name
                : member->data.static_function_decl.name;


#ifdef DEBUG
            ES_COMPILER_DEBUG("Processing class member method '%s' (type=%d, is_static=%d)",
                   method_name, member->type, is_static);
#endif
#ifdef DEBUG
            ES_COMPILER_DEBUG("Class name: '%s'", class_info->class_name);
#endif


            ClassMember* existing = class_info_find_member(class_info, method_name);
            if (existing) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg),
                         "Member '%s' already declared in class '%s'",
                         method_name, class_info->class_name);
                type_error(context, error_msg, 0);
                return 0;
            }


            Type* method_type = type_create_function(NULL, NULL, 0);
            if (!method_type) {
                type_error(context, "Failed to create method type", 0);
                return 0;
            }


            int param_count = member->type == AST_FUNCTION_DECLARATION
                ? member->data.function_decl.parameter_count
                : member->data.static_function_decl.parameter_count;
            EsTokenType* param_types = member->type == AST_FUNCTION_DECLARATION
                ? member->data.function_decl.parameter_types
                : member->data.static_function_decl.parameter_types;

            for (int i = 0; i < param_count; i++) {
                Type* param_type = NULL;
                switch (param_types[i]) {
                    case TOKEN_INT8: param_type = type_create_basic(TYPE_INT8); break;
                    case TOKEN_INT16: param_type = type_create_basic(TYPE_INT16); break;
                    case TOKEN_INT32: param_type = type_create_basic(TYPE_INT32); break;
                    case TOKEN_INT64: param_type = type_create_basic(TYPE_INT64); break;
                    case TOKEN_UINT8: param_type = type_create_basic(TYPE_UINT8); break;
                    case TOKEN_UINT16: param_type = type_create_basic(TYPE_UINT16); break;
                    case TOKEN_UINT32: param_type = type_create_basic(TYPE_UINT32); break;
                    case TOKEN_UINT64: param_type = type_create_basic(TYPE_UINT64); break;
                    case TOKEN_FLOAT32: param_type = type_create_basic(TYPE_FLOAT32); break;
                    case TOKEN_FLOAT64: param_type = type_create_basic(TYPE_FLOAT64); break;
                    case TOKEN_BOOL: param_type = type_create_basic(TYPE_BOOL); break;
                    case TOKEN_STRING: param_type = type_create_basic(TYPE_STRING); break;
                    default: param_type = type_create_basic(TYPE_UNKNOWN); break;
                }
                type_function_add_parameter(method_type, param_type);
            }


            EsTokenType return_type_token;
            if (member->type == AST_FUNCTION_DECLARATION) {
                return_type_token = member->data.function_decl.return_type;
#ifdef DEBUG
                ES_COMPILER_DEBUG("Using function_decl.return_type = %d", return_type_token);
#endif
            } else {
                return_type_token = member->data.static_function_decl.return_type;
#ifdef DEBUG
                ES_COMPILER_DEBUG("Using static_function_decl.return_type = %d", return_type_token);
#endif
            }


#ifdef DEBUG
            ES_COMPILER_DEBUG("Processing class method '%s' (is_static=%d), return_type_token = %d",
                   method_name, is_static, return_type_token);
#endif
#ifdef DEBUG
            ES_COMPILER_DEBUG("AST_FUNCTION_DECLARATION=%d, AST_STATIC_FUNCTION_DECLARATION=%d",
                    AST_FUNCTION_DECLARATION, AST_STATIC_FUNCTION_DECLARATION);
#endif
#ifdef DEBUG
            ES_COMPILER_DEBUG("member->type = %d", member->type);
#endif
#ifdef DEBUG
            ES_COMPILER_DEBUG("TOKEN_INT32=%d, TOKEN_VOID=%d, TOKEN_UNKNOWN=%d",
                    TOKEN_INT32, TOKEN_VOID, TOKEN_UNKNOWN);
#endif

            switch (return_type_token) {
                case TOKEN_INT8: method_type->data.function.return_type = type_create_basic(TYPE_INT8); break;
                case TOKEN_INT16: method_type->data.function.return_type = type_create_basic(TYPE_INT16); break;
                case TOKEN_INT32: method_type->data.function.return_type = type_create_basic(TYPE_INT32); break;
                case TOKEN_INT64: method_type->data.function.return_type = type_create_basic(TYPE_INT64); break;
                case TOKEN_UINT8: method_type->data.function.return_type = type_create_basic(TYPE_UINT8); break;
                case TOKEN_UINT16: method_type->data.function.return_type = type_create_basic(TYPE_UINT16); break;
                case TOKEN_UINT32: method_type->data.function.return_type = type_create_basic(TYPE_UINT32); break;
                case TOKEN_UINT64: method_type->data.function.return_type = type_create_basic(TYPE_UINT64); break;
                case TOKEN_FLOAT32: method_type->data.function.return_type = type_create_basic(TYPE_FLOAT32); break;
                case TOKEN_FLOAT64: method_type->data.function.return_type = type_create_basic(TYPE_FLOAT64); break;
                case TOKEN_BOOL: method_type->data.function.return_type = type_create_basic(TYPE_BOOL); break;
                case TOKEN_STRING: method_type->data.function.return_type = type_create_basic(TYPE_STRING); break;
                case TOKEN_VOID: method_type->data.function.return_type = type_create_basic(TYPE_VOID); break;
                default: method_type->data.function.return_type = type_create_basic(TYPE_UNKNOWN); break;
            }

#ifdef DEBUG
            ES_COMPILER_DEBUG("Set return type for method '%s' to %s (return_type_token was %d)",
                    method_name, type_get_name(method_type->data.function.return_type), return_type_token);
#endif

#ifdef DEBUG
            ES_COMPILER_DEBUG("Function type created - param_count=%d, return_type=%p, return_type_name=%s",
                    method_type->data.function.parameter_count,
                    method_type->data.function.return_type,
                    type_get_name(method_type->data.function.return_type));
#endif

            ASTNode* body = member->type == AST_FUNCTION_DECLARATION
                ? member->data.function_decl.body
                : member->data.static_function_decl.body;


            ClassMember class_member = {
                .name = strdup(method_name),
                .member_type = MEMBER_METHOD,
                .access = access,
                .is_static = is_static,
                .type = method_type
            };


            class_info_add_member(class_info, class_member);


            if (class_info->member_scope) {
                TypeCheckSymbol method_sym = {
                    .name = strdup(method_name),
                    .type = type_copy(method_type),
                    .is_constant = 1,
                    .line_number = 0
                };
#ifdef DEBUG
                ES_COMPILER_DEBUG("Registering method '%s' in class scope, return type: %s",
                        method_name, type_get_name(method_sym.type->data.function.return_type));
#endif
                type_check_symbol_table_add(class_info->member_scope, method_sym);
            }

            TypeCheckSymbolTable* old_scope = context->current_scope;
            context->current_scope = class_info->member_scope;


            TypeCheckSymbolTable* method_scope = type_check_symbol_table_create(context->current_scope);
            context->current_scope = method_scope;

            int old_is_static = context->current_function_is_static;
            context->current_function_is_static = is_static;


            char** params = member->type == AST_FUNCTION_DECLARATION
                ? member->data.function_decl.parameters
                : member->data.static_function_decl.parameters;
            for (int i = 0; i < param_count; i++) {
                TypeCheckSymbol param_symbol = {
                    .name = strdup(params[i]),
                    .type = type_copy(method_type->data.function.parameter_types[i]),
                    .is_constant = 0,
                    .line_number = 0
                };
                type_check_symbol_table_add(context->current_scope, param_symbol);
            }



            if (return_type_token == TOKEN_UNKNOWN) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("No explicit return type for method '%s', inferring from body (body type=%d)", method_name, body->type);
#endif
                Type* inferred_return_type = infer_function_return_type(context, body);
                if (inferred_return_type) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Inferred return type for method '%s': %s (kind=%d)",
                            method_name, type_get_name(inferred_return_type), inferred_return_type->kind);
#endif
                    if (inferred_return_type->kind != TYPE_VOID) {
#ifdef DEBUG
                        ES_COMPILER_DEBUG("Updating return type for method '%s' from %s to %s",
                                method_name, type_get_name(method_type->data.function.return_type),
                                type_get_name(inferred_return_type));
#endif
                        type_destroy(method_type->data.function.return_type);
                        method_type->data.function.return_type = inferred_return_type;


                        if (class_info->member_scope) {
                            TypeCheckSymbol* method_sym = type_check_symbol_table_lookup(class_info->member_scope, method_name);
                            if (method_sym && method_sym->type->kind == TYPE_FUNCTION) {
                                type_destroy(method_sym->type->data.function.return_type);
                                method_sym->type->data.function.return_type = type_copy(inferred_return_type);
                            }
                        }
                    } else {
#ifdef DEBUG
                        ES_COMPILER_DEBUG("Inferred void return type for method '%s', keeping existing type: %s",
                                method_name, type_get_name(method_type->data.function.return_type));
#endif
                        type_destroy(inferred_return_type);
                    }
                } else {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Failed to infer return type for method '%s', keeping existing type: %s",
                            method_name, type_get_name(method_type->data.function.return_type));
#endif
                }
            } else {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Method '%s' has explicit return type, skipping inference", method_name);
#endif
            }

            if (!type_check_statement(context, body)) {
                type_check_symbol_table_destroy(context->current_scope);
                context->current_scope = old_scope;
                context->current_function_is_static = old_is_static;
                return 0;
            }

            type_check_symbol_table_destroy(context->current_scope);
            context->current_scope = old_scope;
            context->current_function_is_static = old_is_static;
            break;
        }

        default:
            return 0;
    }

    return 1;
}