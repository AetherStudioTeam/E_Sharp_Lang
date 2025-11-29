#include "semantic_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../common/output_cache.h"
#include "../common/es_common.h"


SemanticAnalyzer* semantic_analyzer_create(void) {
    SemanticAnalyzer* analyzer = malloc(sizeof(SemanticAnalyzer));
    if (!analyzer) return NULL;

    analyzer->symbol_table = symbol_table_create();
    analyzer->ir_builder = NULL;
    analyzer->error_count = 0;
    analyzer->warning_count = 0;
    analyzer->has_entry_point = false;
    analyzer->entry_point_name = NULL;


    analyzer->class_stack_size = 0;
    analyzer->class_stack_capacity = 8;
    analyzer->class_name_stack = malloc(sizeof(char*) * analyzer->class_stack_capacity);
    if (!analyzer->class_name_stack) {
        free(analyzer);
        return NULL;
    }

    return analyzer;
}


void semantic_analyzer_destroy(SemanticAnalyzer* analyzer) {
    if (!analyzer) return;

    if (analyzer->symbol_table) {
        symbol_table_destroy(analyzer->symbol_table);
    }


    if (analyzer->class_name_stack) {
        for (int i = 0; i < analyzer->class_stack_size; i++) {
            free(analyzer->class_name_stack[i]);
        }
        free(analyzer->class_name_stack);
    }

    free(analyzer->entry_point_name);
    free(analyzer);
}

void semantic_analyzer_push_class_context(SemanticAnalyzer* analyzer, const char* class_name) {
    if (!analyzer || !class_name) return;


    if (analyzer->class_stack_size >= analyzer->class_stack_capacity) {
        int new_capacity = analyzer->class_stack_capacity * 2;
        char** new_stack = realloc(analyzer->class_name_stack, sizeof(char*) * new_capacity);
        if (!new_stack) return;
        analyzer->class_name_stack = new_stack;
        analyzer->class_stack_capacity = new_capacity;
    }


    analyzer->class_name_stack[analyzer->class_stack_size] = strdup(class_name);
    analyzer->class_stack_size++;
}

void semantic_analyzer_pop_class_context(SemanticAnalyzer* analyzer) {
    if (!analyzer || analyzer->class_stack_size <= 0) return;

    analyzer->class_stack_size--;
    free(analyzer->class_name_stack[analyzer->class_stack_size]);
    analyzer->class_name_stack[analyzer->class_stack_size] = NULL;
}

const char* semantic_analyzer_get_current_class_context(SemanticAnalyzer* analyzer) {
    if (!analyzer || analyzer->class_stack_size <= 0) return NULL;
    return analyzer->class_name_stack[analyzer->class_stack_size - 1];
}


void semantic_analyzer_add_error(SemanticAnalyzer* analyzer, const char* format, ...) {
    if (!analyzer) return;

    analyzer->error_count++;

    va_list args;
    va_start(args, format);
    es_output_cache_add_error("Error: ");
    es_output_cache_add_errorv(format, args);
    es_output_cache_add_error("\n");
    va_end(args);
}


void semantic_analyzer_add_warning(SemanticAnalyzer* analyzer, const char* format, ...) {
    if (!analyzer) return;

    analyzer->warning_count++;

    va_list args;
    va_start(args, format);
    es_output_cache_add("Warning: ");
    es_output_cache_addv(format, args);
    es_output_cache_add("\n");
    va_end(args);
}


bool semantic_analyzer_analyze_expression(SemanticAnalyzer* analyzer, ASTNode* expr) {
    if (!analyzer || !expr) return false;

    switch (expr->type) {
        case AST_NUMBER:
        case AST_BOOLEAN:
            return true;

        case AST_IDENTIFIER: {
            const char* var_name = expr->data.identifier_name;
            SymbolEntry* symbol = symbol_table_lookup(analyzer->symbol_table, var_name);
            if (!symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined variable: %s", var_name);
                return false;
            }
            return true;
        }

        case AST_BINARY_OPERATION: {
            bool left_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.binary_op.left);
            bool right_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.binary_op.right);
            return left_ok && right_ok;
        }

        case AST_UNARY_OPERATION:
            return semantic_analyzer_analyze_expression(analyzer, expr->data.unary_op.operand);

        case AST_CALL:
            return semantic_analyzer_analyze_function_call(analyzer, expr);

        case AST_TERNARY_OPERATION: {
            bool cond_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.ternary_op.condition);
            bool true_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.ternary_op.true_value);
            bool false_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.ternary_op.false_value);
            return cond_ok && true_ok && false_ok;
        }

        case AST_NEW_EXPRESSION: {
            const char* class_name = expr->data.new_expr.class_name;
            SymbolEntry* class_symbol = symbol_table_lookup(analyzer->symbol_table, class_name);

            if (!class_symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined class: %s", class_name);
                return false;
            }

            if (class_symbol->type != SYMBOL_CLASS) {
                semantic_analyzer_add_error(analyzer, "%s is not a class", class_name);
                return false;
            }

            for (int i = 0; i < expr->data.new_expr.argument_count; i++) {
                if (!semantic_analyzer_analyze_expression(analyzer, expr->data.new_expr.arguments[i])) {
                    return false;
                }
            }

            return true;
        }

        case AST_STATIC_METHOD_CALL: {
            const char* class_name = expr->data.static_call.class_name;
            const char* method_name = expr->data.static_call.method_name;

#ifdef DEBUG
            ES_COMPILER_DEBUG("Looking for static method: %s::%s", class_name, method_name);
#endif
#ifdef DEBUG
            ES_DEBUG_LOG("SEMANTIC", "Looking for static method: %s::%s", class_name, method_name);
#endif


            SymbolEntry* class_symbol = symbol_table_lookup(analyzer->symbol_table, class_name);
#ifdef DEBUG
            ES_COMPILER_DEBUG("Class symbol lookup for '%s': %s", class_name, class_symbol ? "found" : "not found");
#endif
            if (class_symbol) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Class symbol type: %d", class_symbol->type);
#endif
                if (class_symbol->nested_table) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Class has nested table");
#endif
                } else {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Class has no nested table");
#endif
                }
            }
            if (!class_symbol || class_symbol->type != SYMBOL_CLASS) {
                semantic_analyzer_add_error(analyzer, "Undefined class: %s", class_name);
                return false;
            }

#ifdef DEBUG
            ES_DEBUG_LOG("SEMANTIC", "Found class symbol: %s", class_name);
#endif


            SymbolEntry* method_symbol = NULL;
            size_t class_len = strlen(class_name);
            size_t method_len = strlen(method_name);
            size_t mangled_len = class_len + method_len + 3;
            char* mangled_name = ES_MALLOC(mangled_len);
            if (!mangled_name) {
                semantic_analyzer_add_error(analyzer, "Memory allocation failed");
                return false;
            }
            snprintf(mangled_name, mangled_len, "%s__%s", class_name, method_name);

#ifdef DEBUG
            ES_DEBUG_LOG("SEMANTIC", "Looking for mangled name: %s", mangled_name);
#endif

            if (class_symbol->nested_table) {
                method_symbol = symbol_table_lookup(class_symbol->nested_table, mangled_name);
#ifdef DEBUG
                ES_DEBUG_LOG("SEMANTIC", "Lookup in nested table %s", method_symbol ? "found" : "not found");
#endif
            }
            ES_FREE(mangled_name);

            if (!method_symbol) {

#ifdef DEBUG
                ES_DEBUG_LOG("SEMANTIC", "Looking for original name: %s", method_name);
#endif
                if (class_symbol->nested_table) {
                    method_symbol = symbol_table_lookup(class_symbol->nested_table, method_name);
                    #ifdef DEBUG
                ES_DEBUG_LOG("SEMANTIC", "Lookup with original name: %s", method_symbol ? "found" : "not found");
#endif
                }
            }

            if (!method_symbol) {

#ifdef DEBUG
                ES_DEBUG_LOG("SEMANTIC", "Trying namespace lookup");
#endif
                method_symbol = symbol_table_lookup_in_namespace(analyzer->symbol_table, class_name, method_name);
                if (!method_symbol) {
                    semantic_analyzer_add_error(analyzer, "Undefined static method: %s::%s", class_name, method_name);
                    return false;
                }
            }

            if (method_symbol->type != SYMBOL_FUNCTION) {
                semantic_analyzer_add_error(analyzer, "%s::%s is not a static method", class_name, method_name);
                return false;
            }

            for (int i = 0; i < expr->data.static_call.argument_count; i++) {
                if (!semantic_analyzer_analyze_expression(analyzer, expr->data.static_call.arguments[i])) {
                    return false;
                }
            }

            return true;
        }

        case AST_MEMBER_ACCESS:

            return semantic_analyzer_analyze_expression(analyzer, expr->data.member_access.object);

        case AST_ARRAY_ACCESS: {
            bool array_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.array_access.array);
            bool index_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.array_access.index);
            return array_ok && index_ok;
        }

        default:
            return true;
    }
}


bool semantic_analyzer_analyze_function_call(SemanticAnalyzer* analyzer, ASTNode* call_expr) {
    if (!analyzer || !call_expr || call_expr->type != AST_CALL) return false;

    const char* func_name = call_expr->data.call.name;
    SymbolEntry* symbol = symbol_table_lookup(analyzer->symbol_table, func_name);

    if (!symbol) {

        symbol = symbol_table_declare(analyzer->symbol_table, func_name, SYMBOL_FUNCTION, 0);
        if (symbol) {
            symbol->state = SYMBOL_FORWARD_REF;
        }
        return true;
    }

    if (symbol->type != SYMBOL_FUNCTION) {
        semantic_analyzer_add_error(analyzer, "%s is not a function", func_name);
        return false;
    }


    for (int i = 0; i < call_expr->data.call.argument_count; i++) {
        if (!semantic_analyzer_analyze_expression(analyzer, call_expr->data.call.arguments[i])) {
            return false;
        }
    }

    return true;
}


bool semantic_analyzer_analyze_variable_decl(SemanticAnalyzer* analyzer, ASTNode* var_decl) {
    if (!analyzer || !var_decl) return false;

    const char* var_name = var_decl->data.variable_decl.name;


    SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, var_name);
    if (existing) {
        semantic_analyzer_add_error(analyzer, "Variable %s already declared", var_name);
        return false;
    }


    SymbolEntry* symbol = symbol_table_define(analyzer->symbol_table, var_name,
                                             SYMBOL_VARIABLE, 0, NULL);
    if (!symbol) {
        semantic_analyzer_add_error(analyzer, "Failed to declare variable %s", var_name);
        return false;
    }


    if (var_decl->data.variable_decl.value) {
        return semantic_analyzer_analyze_expression(analyzer, var_decl->data.variable_decl.value);
    }

    return true;
}


bool semantic_analyzer_analyze_static_variable_decl(SemanticAnalyzer* analyzer, ASTNode* var_decl) {
    if (!analyzer || !var_decl) return false;

    const char* var_name = var_decl->data.static_variable_decl.name;

    SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, var_name);
    if (existing) {
        semantic_analyzer_add_error(analyzer, "Static variable %s already declared", var_name);
        return false;
    }

    SymbolEntry* symbol = symbol_table_define(analyzer->symbol_table, var_name,
                                              SYMBOL_STATIC_FIELD, 0, NULL);
    if (!symbol) {
        semantic_analyzer_add_error(analyzer, "Failed to declare static variable %s", var_name);
        return false;
    }

    if (var_decl->data.static_variable_decl.value) {
        return semantic_analyzer_analyze_expression(analyzer, var_decl->data.static_variable_decl.value);
    }

    return true;
}


bool semantic_analyzer_analyze_statement(SemanticAnalyzer* analyzer, ASTNode* stmt) {
    if (!analyzer || !stmt) return false;

    switch (stmt->type) {
        case AST_VARIABLE_DECLARATION:
            return semantic_analyzer_analyze_variable_decl(analyzer, stmt);

        case AST_STATIC_VARIABLE_DECLARATION:
            return semantic_analyzer_analyze_static_variable_decl(analyzer, stmt);

        case AST_ASSIGNMENT: {
            const char* var_name = stmt->data.assignment.name;
            SymbolEntry* symbol = symbol_table_lookup(analyzer->symbol_table, var_name);
            if (!symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined variable: %s", var_name);
                return false;
            }
            return semantic_analyzer_analyze_expression(analyzer, stmt->data.assignment.value);
        }

        case AST_RETURN_STATEMENT:
            if (stmt->data.return_stmt.value) {
                return semantic_analyzer_analyze_expression(analyzer, stmt->data.return_stmt.value);
            }
            return true;

        case AST_IF_STATEMENT: {
            bool cond_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.if_stmt.condition);
            bool then_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.if_stmt.then_branch);
            bool else_ok = true;
            if (stmt->data.if_stmt.else_branch) {
                else_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.if_stmt.else_branch);
            }
            return cond_ok && then_ok && else_ok;
        }

        case AST_WHILE_STATEMENT: {
            bool cond_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.while_stmt.condition);
            bool body_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.while_stmt.body);
            return cond_ok && body_ok;
        }

        case AST_CLASS_DECLARATION: {
            const char* class_name = stmt->data.class_decl.name;
#ifdef DEBUG
            ES_COMPILER_DEBUG("Processing class declaration: %s", class_name);
#endif

            SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, class_name);
            if (existing) {
                semantic_analyzer_add_error(analyzer, "Class %s already declared", class_name);
                return false;
            }


            SymbolEntry* class_sym = symbol_table_define(analyzer->symbol_table, class_name, SYMBOL_CLASS, 0, NULL);
            if (!class_sym) {
                semantic_analyzer_add_error(analyzer, "Failed to define class %s", class_name);
                return false;
            }
            if (!class_sym->nested_table) {
                class_sym->nested_table = symbol_table_create();
            }

#ifdef DEBUG
            ES_COMPILER_DEBUG("Defined class symbol: %s with nested table", class_name);
#endif

            SymbolTable* prev_table = analyzer->symbol_table;
            analyzer->symbol_table = class_sym->nested_table;


            if (!analyzer->symbol_table->current_scope) {
                symbol_table_push_scope(analyzer->symbol_table);
            }


            semantic_analyzer_push_class_context(analyzer, class_name);

            bool body_ok = true;
            ASTNode* body_node = stmt->data.class_decl.body;
#ifdef DEBUG
            ES_COMPILER_DEBUG("Class body node type: %d", body_node ? body_node->type : -1);
#endif
            if (body_node && body_node->type == AST_BLOCK) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Class has %d statements in body", body_node->data.block.statement_count);
#endif
                for (int i = 0; i < body_node->data.block.statement_count && body_ok; i++) {
                    ASTNode* stmt = body_node->data.block.statements[i];
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Processing class statement %d, type: %d", i, stmt ? stmt->type : -1);
#endif
#ifdef DEBUG
                    ES_COMPILER_DEBUG("AST_STATIC_FUNCTION_DECLARATION = %d", AST_STATIC_FUNCTION_DECLARATION);
#endif
            if (stmt && stmt->type == AST_ACCESS_MODIFIER) {
#ifdef DEBUG
                    ES_COMPILER_DEBUG("Found access modifier, member type: %d", stmt->data.access_modifier.member ? stmt->data.access_modifier.member->type : -1);
#endif
            }
                    body_ok = semantic_analyzer_analyze_statement(analyzer, stmt);
                }
            } else if (body_node) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Processing single class body statement");
#endif
                body_ok = semantic_analyzer_analyze_statement(analyzer, body_node);
            }


            semantic_analyzer_pop_class_context(analyzer);

            analyzer->symbol_table = prev_table;

            return body_ok;
        }

        case AST_SWITCH_STATEMENT: {
            bool cond_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.switch_stmt.expression);
            bool cases_ok = true;

            for (int i = 0; i < stmt->data.switch_stmt.case_count && cases_ok; i++) {
                cases_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.switch_stmt.cases[i]);
            }

            bool default_ok = true;
            if (stmt->data.switch_stmt.default_case) {
                default_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.switch_stmt.default_case);
            }

            return cond_ok && cases_ok && default_ok;
        }

        case AST_CASE_CLAUSE: {
            bool value_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.case_clause.value);
            bool stmts_ok = true;

            for (int i = 0; i < stmt->data.case_clause.statement_count && stmts_ok; i++) {
                stmts_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.case_clause.statements[i]);
            }

            return value_ok && stmts_ok;
        }

        case AST_DEFAULT_CLAUSE: {
            bool stmts_ok = true;

            for (int i = 0; i < stmt->data.default_clause.statement_count && stmts_ok; i++) {
                stmts_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.default_clause.statements[i]);
            }

            return stmts_ok;
        }

        case AST_BREAK_STATEMENT:
        case AST_CONTINUE_STATEMENT:
            return true;

        case AST_ACCESS_MODIFIER: {

            if (stmt->data.access_modifier.member) {
                return semantic_analyzer_analyze_statement(analyzer, stmt->data.access_modifier.member);
            }
            return true;
        }

        case AST_STATIC_FUNCTION_DECLARATION: {
            const char* method_name = stmt->data.static_function_decl.name;


            const char* current_class = semantic_analyzer_get_current_class_context(analyzer);

            char* mangled_name = NULL;
            const char* function_name = method_name;
            if (current_class) {

                size_t class_len = strlen(current_class);
                size_t method_len = strlen(method_name);
                size_t mangled_len = class_len + method_len + 3;
                mangled_name = ES_MALLOC(mangled_len);
                if (mangled_name) {
                    snprintf(mangled_name, mangled_len, "%s__%s", current_class, method_name);
                    function_name = mangled_name;
                }
            }

#ifdef DEBUG
            ES_COMPILER_DEBUG("Defining static method '%s' with name '%s'", method_name, function_name);
#endif
#ifdef DEBUG
            ES_DEBUG_LOG("SEMANTIC", "Defining static method '%s' with name '%s'", method_name, function_name);
#endif

            SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, function_name);
            if (existing && existing->state == SYMBOL_DEFINED) {
                semantic_analyzer_add_error(analyzer, "Static method %s already defined", method_name);
                if (mangled_name) ES_FREE(mangled_name);
                return false;
            }


            SymbolEntry* func_sym = symbol_table_define(analyzer->symbol_table, function_name, SYMBOL_FUNCTION, 0, NULL);
            if (!func_sym) {
                semantic_analyzer_add_error(analyzer, "Failed to define static method %s", method_name);
                if (mangled_name) ES_FREE(mangled_name);
                return false;
            }
            func_sym->state = SYMBOL_DEFINED;

#ifdef DEBUG
            ES_COMPILER_DEBUG("Successfully defined static method '%s'", method_name);
#endif
#ifdef DEBUG
            ES_DEBUG_LOG("SEMANTIC", "Successfully defined static method '%s'", function_name);
#endif


            symbol_table_push_scope(analyzer->symbol_table);
            for (int i = 0; i < stmt->data.static_function_decl.parameter_count; i++) {
                symbol_table_define(analyzer->symbol_table, stmt->data.static_function_decl.parameters[i], SYMBOL_VARIABLE, 0, NULL);
            }
            bool body_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.static_function_decl.body);
            symbol_table_pop_scope(analyzer->symbol_table);

            if (mangled_name) ES_FREE(mangled_name);
            return body_ok;
        }

        case AST_BLOCK: {
            symbol_table_push_scope(analyzer->symbol_table);
            bool result = true;
            for (int i = 0; i < stmt->data.block.statement_count && result; i++) {
                result = semantic_analyzer_analyze_statement(analyzer, stmt->data.block.statements[i]);
            }
            symbol_table_pop_scope(analyzer->symbol_table);
            return result;
        }

        case AST_BINARY_OPERATION:
        case AST_UNARY_OPERATION:
        case AST_TERNARY_OPERATION:
        case AST_CALL:
        case AST_IDENTIFIER:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:

            return semantic_analyzer_analyze_expression(analyzer, stmt);

        default:
            return true;
    }
}


FunctionAnalysisResult semantic_analyzer_analyze_function(SemanticAnalyzer* analyzer, ASTNode* function_node) {
    FunctionAnalysisResult result = {0};
    result.success = false;

    if (!analyzer || !function_node || function_node->type != AST_FUNCTION_DECLARATION) {
        return result;
    }

    const char* func_name = function_node->data.function_decl.name;


    SymbolEntry* existing = symbol_table_lookup(analyzer->symbol_table, func_name);
    if (existing && existing->state == SYMBOL_DEFINED) {
        semantic_analyzer_add_error(analyzer, "Function %s already defined", func_name);
        return result;
    }


    symbol_table_push_scope(analyzer->symbol_table);


    int param_count = function_node->data.function_decl.parameter_count;
    for (int i = 0; i < param_count; i++) {
        symbol_table_define(analyzer->symbol_table, function_node->data.function_decl.parameters[i],
                           SYMBOL_VARIABLE, 0, NULL);
    }


    SymbolEntry* func_symbol = symbol_table_define(analyzer->symbol_table, func_name,
                                                  SYMBOL_FUNCTION, 0, NULL);
    if (!func_symbol) {
        semantic_analyzer_add_error(analyzer, "Failed to define function %s", func_name);
        symbol_table_pop_scope(analyzer->symbol_table);
        return result;
    }

    func_symbol->state = SYMBOL_DEFINED;


    if (strcmp(func_name, "main") == 0) {
        if (analyzer->has_entry_point) {
            semantic_analyzer_add_error(analyzer, "Multiple entry points (main functions) defined");
        } else {
            analyzer->has_entry_point = true;
            analyzer->entry_point_name = strdup(func_name);
            func_symbol->is_entry_point = true;
        }
    }


    bool body_ok = semantic_analyzer_analyze_statement(analyzer, function_node->data.function_decl.body);

    symbol_table_pop_scope(analyzer->symbol_table);

    result.success = body_ok;
    result.param_count = param_count;
    result.function_symbol = func_symbol;
    result.has_return_statement = true;

    return result;
}


SemanticAnalysisResult* semantic_analyzer_analyze(SemanticAnalyzer* analyzer, ASTNode* ast) {
    if (!analyzer || !ast) return NULL;



    SemanticAnalysisResult* result = malloc(sizeof(SemanticAnalysisResult));
    if (!result) return NULL;

    result->success = true;
    result->error_count = 0;
    result->warning_count = 0;
    result->symbol_table = analyzer->symbol_table;
    result->error_messages = NULL;


    analyzer->error_count = 0;
    analyzer->warning_count = 0;

#ifdef DEBUG
    ES_COMPILER_DEBUG("AST type is %d", ast->type);
#endif
    fflush(stderr);

    if (ast->type == AST_PROGRAM) {



#ifdef DEBUG
        ES_DEBUG_LOG("SEMANTIC", "Analyzing program with %d top-level declarations", ast->data.block.statement_count);
#endif
#ifdef DEBUG
        ES_COMPILER_DEBUG("Analyzing program with %d top-level declarations", ast->data.block.statement_count);
#endif

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* decl = ast->data.block.statements[i];
#ifdef DEBUG
            ES_COMPILER_DEBUG("Processing top-level declaration %d, type: %d", i, decl ? decl->type : -1);
#endif
            if (decl && decl->type == AST_CLASS_DECLARATION) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Found class declaration, analyzing...");
#endif
                if (!semantic_analyzer_analyze_statement(analyzer, decl)) {
                    result->success = false;
                }
            }
        }


        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* decl = ast->data.block.statements[i];
#ifdef DEBUG
            ES_COMPILER_DEBUG("Processing function declaration %d, type: %d", i, decl ? decl->type : -1);
#endif
            if (decl && decl->type == AST_FUNCTION_DECLARATION) {
#ifdef DEBUG
                ES_COMPILER_DEBUG("Found function declaration, analyzing...");
#endif
                FunctionAnalysisResult func_result = semantic_analyzer_analyze_function(analyzer, decl);
                if (!func_result.success) {
                    result->success = false;
                }
            }
        }
    } else if (ast->type == AST_FUNCTION_DECLARATION) {

        FunctionAnalysisResult func_result = semantic_analyzer_analyze_function(analyzer, ast);
        if (!func_result.success) {
            result->success = false;
        }
    } else if (ast->type == AST_BLOCK) {

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            if (stmt && !semantic_analyzer_analyze_statement(analyzer, stmt)) {
                result->success = false;
            }
        }
    } else {

        if (!semantic_analyzer_analyze_statement(analyzer, ast)) {
            result->success = false;
        }
    }


    symbol_table_check_undefined(analyzer->symbol_table);


    if (!analyzer->has_entry_point) {
        semantic_analyzer_add_warning(analyzer, "No entry point (main function) defined");
    }

    result->error_count = analyzer->error_count;
    result->warning_count = analyzer->warning_count;

    if (result->error_count > 0) {
        result->success = false;
    }

    return result;
}

