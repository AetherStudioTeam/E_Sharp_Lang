#include "interpreter.h"
#include "accelerator.h"
#include "../common/es_common.h"
#include "../common/output_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


#define STRING_POOL_SIZE 256
typedef struct StringPool {
    char* strings[STRING_POOL_SIZE];
    int count;
} StringPool;

static StringPool string_pool = {0};


static char* string_pool_get(const char* str) {

    for (int i = 0; i < string_pool.count; i++) {
        if (strcmp(string_pool.strings[i], str) == 0) {
            return string_pool.strings[i];
        }
    }

    if (string_pool.count < STRING_POOL_SIZE) {
        char* new_str = accelerator_strdup(str);
        string_pool.strings[string_pool.count++] = new_str;
        return new_str;
    }
    return accelerator_strdup(str);
}


static void string_pool_cleanup() {
    for (int i = 0; i < string_pool.count; i++) {
        ES_FREE(string_pool.strings[i]);
    }
    string_pool.count = 0;
}

static Scope* scope_create(Scope* parent) {
    Scope* scope = ES_MALLOC(sizeof(Scope));
    if (!scope) return NULL;

    scope->variables = NULL;
    scope->variable_count = 0;
    scope->parent = parent;
    scope->scope_type = 0;
    scope->scope_name = NULL;

    return scope;
}

static void scope_destroy(Scope* scope) {
    if (!scope) return;


    for (int i = 0; i < scope->variable_count; i++) {
        ES_FREE(scope->variables[i].name);
        ES_FREE(scope->variables[i].string_value);


        if (scope->variables[i].parameters) {
            for (int j = 0; j < scope->variables[i].parameter_count; j++) {
                ES_FREE(scope->variables[i].parameters[j]);
            }
            ES_FREE(scope->variables[i].parameters);
        }
    }
    ES_FREE(scope->variables);
    ES_FREE(scope->scope_name);
    ES_FREE(scope);
}

static Variable* scope_find_variable(Scope* scope, const char* name) {
    while (scope) {
        for (int i = 0; i < scope->variable_count; i++) {
            if (strcmp(scope->variables[i].name, name) == 0) {
                return &scope->variables[i];
            }
        }
        scope = scope->parent;
    }
    return NULL;
}


static double scope_get_variable(Scope* scope, const char* name) {
    Variable* var = scope_find_variable(scope, name);
    if (var) {
        return var->number_value;
    }
    return 0;
}

static void scope_set_variable(Scope* scope, const char* name, double number_value,
                              const char* string_value, int boolean_value, int type) {
    Variable* var = scope_find_variable(scope, name);
    if (var) {
        if (var->type == 1 && var->string_value) {
            ES_FREE(var->string_value);
        }
        var->number_value = number_value;
        var->string_value = string_value ? accelerator_strdup(string_value) : NULL;
        var->boolean_value = boolean_value;
        var->type = type;
        var->is_function = 0;
        var->access_modifier = TOKEN_PUBLIC;
        var->is_static = 0;
        return;
    }

    Variable* new_variables = ES_REALLOC(scope->variables,
                              (scope->variable_count + 1) * sizeof(Variable));
    if (!new_variables) {
        return;
    }
    scope->variables = new_variables;
    Variable* new_var = &scope->variables[scope->variable_count];
    new_var->name = string_pool_get(name);
    new_var->type = type;
    new_var->number_value = number_value;
    new_var->string_value = string_value ? accelerator_strdup(string_value) : NULL;
    new_var->boolean_value = boolean_value;
    new_var->is_function = 0;
    new_var->parameters = NULL;
    new_var->parameter_count = 0;
    new_var->body = NULL;
    new_var->access_modifier = TOKEN_PUBLIC;
    new_var->is_static = 0;


    scope->variable_count++;
}

static int can_access_member(Variable* member, Scope* current_scope, Scope* member_scope) {
    if (!member || !current_scope || !member_scope) return 1;


    if (member->access_modifier == TOKEN_PUBLIC) return 1;


    if (member->access_modifier == TOKEN_PRIVATE) {
        return (current_scope == member_scope);
    }


    if (member->access_modifier == TOKEN_PROTECTED) {

        Scope* scope = current_scope;
        while (scope) {
            if (scope == member_scope) return 1;
            scope = scope->parent;
        }
        return 0;
    }

    return 1;
}

static Variable* scope_find_member(Scope* scope, const char* name, int check_access) {
    while (scope) {
        for (int i = 0; i < scope->variable_count; i++) {
            if (strcmp(scope->variables[i].name, name) == 0) {
                if (!check_access || can_access_member(&scope->variables[i], scope, scope)) {
                    return &scope->variables[i];
                }
                return NULL;
            }
        }
        scope = scope->parent;
    }
    return NULL;
}

static void scope_set_function(Scope* scope, const char* name, char** parameters,
                              int parameter_count, ASTNode* body) {
    Variable* var = scope_find_variable(scope, name);
    if (var) {

        if (var->parameters) {
                for (int i = 0; i < var->parameter_count; i++) {
                    ES_FREE(var->parameters[i]);
                }
                ES_FREE(var->parameters);
            }
    } else {
        Variable* new_variables = ES_REALLOC(scope->variables,
                                  (scope->variable_count + 1) * sizeof(Variable));
        if (!new_variables) {
            return;
        }
        scope->variables = new_variables;
        var = &scope->variables[scope->variable_count];
        var->name = accelerator_strdup(name);
        scope->variable_count++;
    }

    var->type = 3;
    var->is_function = 1;
    var->parameters = parameters;
    var->parameter_count = parameter_count;
    var->body = body;
    var->access_modifier = TOKEN_PUBLIC;
    var->is_static = 0;
}


static double get_current_time() {
    return (double)clock() / CLOCKS_PER_SEC;
}

Interpreter* interpreter_create() {
    Interpreter* interpreter = (Interpreter*)ES_MALLOC(sizeof(Interpreter));
    if (!interpreter) return NULL;

    interpreter->global_scope = scope_create(NULL);
    interpreter->current_scope = interpreter->global_scope;
    interpreter->recursion_depth = 0;
    interpreter->start_time = get_current_time();
    interpreter->loop_iterations = 0;
    interpreter->error_occurred = 0;
    interpreter->error_message[0] = '\0';

    return interpreter;
}

void interpreter_destroy(Interpreter* interpreter) {
    if (!interpreter) return;

    scope_destroy(interpreter->global_scope);


    string_pool_cleanup();

    ES_FREE(interpreter);
}


static double interpreter_evaluate_expression(Interpreter* interpreter, ASTNode* expr);
static void interpreter_execute_statement(Interpreter* interpreter, ASTNode* stmt);
static void interpreter_execute_block(Interpreter* interpreter, ASTNode* block);


static int interpreter_check_limits(Interpreter* interpreter) {
    if (interpreter->error_occurred) return 0;


    double current_time = get_current_time();
    if (current_time - interpreter->start_time > MAX_EXECUTION_TIME) {
        snprintf(interpreter->error_message, sizeof(interpreter->error_message),
                "执行超时: %.2f秒 > %.2f秒", current_time - interpreter->start_time, MAX_EXECUTION_TIME);
        interpreter->error_occurred = 1;
        return 0;
    }


    if (interpreter->recursion_depth > MAX_RECURSION_DEPTH) {
        snprintf(interpreter->error_message, sizeof(interpreter->error_message),
                "递归深度超限: %d > %d", interpreter->recursion_depth, MAX_RECURSION_DEPTH);
        interpreter->error_occurred = 1;
        return 0;
    }

    return 1;
}

static double interpreter_evaluate_binary_operation(Interpreter* interpreter, ASTNode* node) {
    double left = interpreter_evaluate_expression(interpreter, node->data.binary_op.left);
    double right = interpreter_evaluate_expression(interpreter, node->data.binary_op.right);

    switch (node->data.binary_op.operator) {
        case TOKEN_PLUS:

            if (left > 1000000 || right > 1000000) {

                char left_buf[64], right_buf[64];
                char* left_str;
                char* right_str;
                size_t left_len, right_len;

                if (left > 1000000) {
                    left_str = (char*)(uintptr_t)left;
                    left_len = strlen(left_str);
                } else {
                    snprintf(left_buf, sizeof(left_buf), "%f", left);
                    left_str = left_buf;
                    left_len = strlen(left_buf);
                }

                if (right > 1000000) {
                    right_str = (char*)(uintptr_t)right;
                    right_len = strlen(right_str);
                } else {
                    snprintf(right_buf, sizeof(right_buf), "%f", right);
                    right_str = right_buf;
                    right_len = strlen(right_buf);
                }


                char* result_str = ES_MALLOC(left_len + right_len + 1);
                if (result_str) {
                    memcpy(result_str, left_str, left_len);
                    memcpy(result_str + left_len, right_str, right_len + 1);
                }
                return (double)(uintptr_t)result_str;
            }
            return left + right;
        case TOKEN_MINUS: return left - right;
        case TOKEN_MULTIPLY: return left * right;
        case TOKEN_DIVIDE:
            if (right == 0) {
                fprintf(stderr, "Division by zero\n");
                fflush(stderr);
                return 0;
            }
            return left / right;
        case TOKEN_MODULO:
            if (right == 0) {
                fprintf(stderr, "Modulo by zero\n");
                fflush(stderr);
                return 0;
            }
            return fmod(left, right);
        case TOKEN_EQUAL: return left == right;
        case TOKEN_NOT_EQUAL: return left != right;
        case TOKEN_LESS: return left < right;
        case TOKEN_GREATER: return left > right;
        case TOKEN_LESS_EQUAL: return left <= right;
        case TOKEN_GREATER_EQUAL: return left >= right;
        default: return 0;
    }
}

static double interpreter_evaluate_expression(Interpreter* interpreter, ASTNode* expr) {
    if (!expr) return 0;

    switch (expr->type) {
        case AST_NUMBER:
            return expr->data.number_value;
        case AST_STRING: {

            return (double)(uintptr_t)expr->data.string_value;
        }
        case AST_BOOLEAN:
            return expr->data.boolean_value;
        case AST_IDENTIFIER: {
            Variable* var = scope_find_variable(interpreter->current_scope, expr->data.identifier_name);
            if (!var) {
                fprintf(stderr, "Undefined variable: %s\n", expr->data.identifier_name);
                fflush(stderr);
                return 0;
            }
            if (var->type == 0) {
                return var->number_value;
            } else if (var->type == 1) {

                return (double)(uintptr_t)var->string_value;
            } else if (var->type == 2) {
                return var->boolean_value;
            }
            return 0;
        }
        case AST_BINARY_OPERATION:
            return interpreter_evaluate_binary_operation(interpreter, expr);
        case AST_UNARY_OPERATION: {
            double operand_value = interpreter_evaluate_expression(interpreter, expr->data.unary_op.operand);
            if (expr->data.unary_op.operator == TOKEN_INCREMENT) {

                if (expr->data.unary_op.operand->type == AST_IDENTIFIER) {
                    char* var_name = expr->data.unary_op.operand->data.identifier_name;
                    double new_value = operand_value + 1;
                    scope_set_variable(interpreter->current_scope, var_name, new_value, NULL, 0, 0);
                    return operand_value;
                }
            }
            return operand_value;
        }
        case AST_CALL: {
            if (!interpreter_check_limits(interpreter)) return 0;

            if (strcmp(expr->data.call.name, "print") == 0) {
                for (int i = 0; i < expr->data.call.argument_count; i++) {
                    ASTNode* arg = expr->data.call.arguments[i];
                    if (arg->type == AST_STRING) {
                    } else {
                        double value = interpreter_evaluate_expression(interpreter, arg);
                        (void)value;
                    }
                    if (i < expr->data.call.argument_count - 1) {
                    }
                }
                return 0;
            } else if (strcmp(expr->data.call.name, "time") == 0) {
                return get_current_time();
            } else {

                interpreter->recursion_depth++;
                if (interpreter->recursion_depth > MAX_RECURSION_DEPTH) {
                    interpreter->recursion_depth--;
                    snprintf(interpreter->error_message, sizeof(interpreter->error_message),
                            "递归深度超限: %d > %d", interpreter->recursion_depth, MAX_RECURSION_DEPTH);
                    interpreter->error_occurred = 1;
                    return 0;
                }


                Variable* func = scope_find_variable(interpreter->current_scope, expr->data.call.name);
                if (!func || !func->is_function) {
                    fprintf(stderr, "Undefined function: %s\n", expr->data.call.name);
                    fflush(stderr);
                    interpreter->recursion_depth--;
                    return 0;
                }


                int has_named_params = 0;
                for (int i = 0; i < expr->data.call.argument_count; i++) {
                    if (expr->data.call.argument_names && expr->data.call.argument_names[i] != NULL) {
                        has_named_params = 1;
                        break;
                    }
                }

                if (!has_named_params && expr->data.call.argument_count != func->parameter_count) {
                    fprintf(stderr, "Function %s expects %d arguments, got %d\n",
                           expr->data.call.name, func->parameter_count, expr->data.call.argument_count);
                    interpreter->recursion_depth--;
                    return 0;
                }


                Scope* new_scope = scope_create(interpreter->current_scope);


                if (has_named_params) {


                    for (int i = 0; i < func->parameter_count; i++) {
                        scope_set_variable(new_scope, func->parameters[i], 0, NULL, 0, 0);
                    }


                    for (int i = 0; i < expr->data.call.argument_count; i++) {
                        double arg_value = interpreter_evaluate_expression(interpreter, expr->data.call.arguments[i]);

                        if (expr->data.call.argument_names && expr->data.call.argument_names[i] != NULL) {

                            char* param_name = expr->data.call.argument_names[i];
                            int found = 0;
                            for (int j = 0; j < func->parameter_count; j++) {
                                if (strcmp(func->parameters[j], param_name) == 0) {
                                    scope_set_variable(new_scope, param_name, arg_value, NULL, 0, 0);
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found) {
                                fprintf(stderr, "Function %s has no parameter named '%s'\n",
                                       expr->data.call.name, param_name);
                                fflush(stderr);
                                scope_destroy(new_scope);
                                interpreter->recursion_depth--;
                                return 0;
                            }
                        } else {

                            if (i >= func->parameter_count) {
                                fprintf(stderr, "Function %s expects %d arguments, got %d\n",
                                       expr->data.call.name, func->parameter_count, expr->data.call.argument_count);
                                scope_destroy(new_scope);
                                interpreter->recursion_depth--;
                                return 0;
                            }
                            scope_set_variable(new_scope, func->parameters[i], arg_value, NULL, 0, 0);
                        }
                    }
                } else {

                    for (int i = 0; i < func->parameter_count; i++) {
                        double arg_value = interpreter_evaluate_expression(interpreter, expr->data.call.arguments[i]);
                        scope_set_variable(new_scope, func->parameters[i], arg_value, NULL, 0, 0);
                    }
                }


                Scope* old_scope = interpreter->current_scope;
                interpreter->current_scope = new_scope;


                double result = 0;
                interpreter->has_return = 0;
                if (func->body->type == AST_BLOCK) {
                    for (int i = 0; i < func->body->data.block.statement_count; i++) {
                        interpreter_execute_statement(interpreter, func->body->data.block.statements[i]);
                        if (interpreter->has_return) {
                            result = interpreter->return_value;
                            break;
                        }
                    }
                }


                interpreter->current_scope = old_scope;
                scope_destroy(new_scope);

                interpreter->recursion_depth--;
                return result;
            }
        }
        case AST_ARRAY_LITERAL: {



            return 2000000;
        }
        case AST_NEW_EXPRESSION: {

            char* class_name = expr->data.new_expr.class_name;


            char constructor_name[256];
            snprintf(constructor_name, sizeof(constructor_name), "%s", class_name);

            Variable* func = scope_find_variable(interpreter->current_scope, constructor_name);
            if (!func || !func->is_function) {
                fprintf(stderr, "Undefined constructor: %s\n", class_name);
                return 0;
            }


            interpreter->recursion_depth++;
            if (interpreter->recursion_depth > MAX_RECURSION_DEPTH) {
                interpreter->recursion_depth--;
                snprintf(interpreter->error_message, sizeof(interpreter->error_message),
                        "递归深度超限: %d > %d", interpreter->recursion_depth, MAX_RECURSION_DEPTH);
                interpreter->error_occurred = 1;
                return 0;
            }


            int has_named_params = 0;
            for (int i = 0; i < expr->data.new_expr.argument_count; i++) {
                if (expr->data.new_expr.argument_names && expr->data.new_expr.argument_names[i] != NULL) {
                    has_named_params = 1;
                    break;
                }
            }

            if (!has_named_params && expr->data.new_expr.argument_count != func->parameter_count) {
                fprintf(stderr, "Constructor %s expects %d arguments, got %d\n",
                       class_name, func->parameter_count, expr->data.new_expr.argument_count);
                interpreter->recursion_depth--;
                return 0;
            }


            Scope* new_scope = scope_create(interpreter->current_scope);


            if (has_named_params) {


                for (int i = 0; i < func->parameter_count; i++) {
                    scope_set_variable(new_scope, func->parameters[i], 0, NULL, 0, 0);
                }


                for (int i = 0; i < expr->data.new_expr.argument_count; i++) {
                    double arg_value = interpreter_evaluate_expression(interpreter, expr->data.new_expr.arguments[i]);

                    if (expr->data.new_expr.argument_names && expr->data.new_expr.argument_names[i] != NULL) {

                        char* param_name = expr->data.new_expr.argument_names[i];
                        int found = 0;
                        for (int j = 0; j < func->parameter_count; j++) {
                            if (strcmp(func->parameters[j], param_name) == 0) {
                                scope_set_variable(new_scope, param_name, arg_value, NULL, 0, 0);
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            fprintf(stderr, "Constructor %s has no parameter named '%s'\n",
                                   class_name, param_name);
                            scope_destroy(new_scope);
                            interpreter->recursion_depth--;
                            return 0;
                        }
                    } else {

                        if (i >= func->parameter_count) {
                            fprintf(stderr, "Constructor %s expects %d arguments, got %d\n",
                                   class_name, func->parameter_count, expr->data.new_expr.argument_count);
                            scope_destroy(new_scope);
                            interpreter->recursion_depth--;
                            return 0;
                        }
                        scope_set_variable(new_scope, func->parameters[i], arg_value, NULL, 0, 0);
                    }
                }
            } else {

                for (int i = 0; i < func->parameter_count; i++) {
                    double arg_value = interpreter_evaluate_expression(interpreter, expr->data.new_expr.arguments[i]);
                    scope_set_variable(new_scope, func->parameters[i], arg_value, NULL, 0, 0);
                }
            }


            Scope* old_scope = interpreter->current_scope;
            interpreter->current_scope = new_scope;


            double result = 0;
            interpreter->has_return = 0;
            if (func->body->type == AST_BLOCK) {
                for (int i = 0; i < func->body->data.block.statement_count; i++) {
                    interpreter_execute_statement(interpreter, func->body->data.block.statements[i]);
                    if (interpreter->has_return) {
                        result = interpreter->return_value;
                        break;
                    }
                }
            }


            interpreter->current_scope = old_scope;


            double object_id = (double)(uintptr_t)new_scope;

            interpreter->recursion_depth--;
            return object_id;
        }
        default: return 0;
    }
}

static void interpreter_execute_statement(Interpreter* interpreter, ASTNode* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case AST_VARIABLE_DECLARATION: {
            double value = 0;
            if (stmt->data.variable_decl.value) {
                value = interpreter_evaluate_expression(interpreter, stmt->data.variable_decl.value);
            }

            if (stmt->data.variable_decl.value &&
                stmt->data.variable_decl.value->type == AST_STRING) {
                scope_set_variable(interpreter->current_scope,
                                 stmt->data.variable_decl.name,
                                 0,
                                 stmt->data.variable_decl.value->data.string_value,
                                 0, 1);
            } else {
                scope_set_variable(interpreter->current_scope,
                                 stmt->data.variable_decl.name,
                                 value,
                                 NULL,
                                 0, 0);
            }
            break;
        }
        case AST_ASSIGNMENT: {
            double value = interpreter_evaluate_expression(interpreter, stmt->data.assignment.value);

            if (stmt->data.assignment.value &&
                stmt->data.assignment.value->type == AST_STRING) {
                scope_set_variable(interpreter->current_scope,
                                 stmt->data.assignment.name,
                                 0,
                                 stmt->data.assignment.value->data.string_value,
                                 0, 1);
            } else {
                scope_set_variable(interpreter->current_scope,
                                 stmt->data.assignment.name,
                                 value,
                                 NULL,
                                 0, 0);
            }
            break;
        }
        case AST_ARRAY_ASSIGNMENT: {

            double array_val = interpreter_evaluate_expression(interpreter, stmt->data.array_assignment.array);
            double index_val = interpreter_evaluate_expression(interpreter, stmt->data.array_assignment.index);
            double value_val = interpreter_evaluate_expression(interpreter, stmt->data.array_assignment.value);



            char array_assign_key[256];
            snprintf(array_assign_key, sizeof(array_assign_key), "array_%d_%d", (int)array_val, (int)index_val);
            scope_set_variable(interpreter->current_scope, array_assign_key, value_val, NULL, 0, 0);
            break;
        }
        case AST_IF_STATEMENT: {
            double condition = interpreter_evaluate_expression(interpreter, stmt->data.if_stmt.condition);
            if (condition) {
                interpreter_execute_statement(interpreter, stmt->data.if_stmt.then_branch);
            } else if (stmt->data.if_stmt.else_branch) {
                interpreter_execute_statement(interpreter, stmt->data.if_stmt.else_branch);
            }
            break;
        }
        case AST_WHILE_STATEMENT: {
            int iteration_count = 0;
            while (1) {
                if (!interpreter_check_limits(interpreter)) break;

                iteration_count++;
                if (iteration_count > MAX_LOOP_ITERATIONS) {
                    snprintf(interpreter->error_message, sizeof(interpreter->error_message),
                            "循环次数超限: %d > %d", iteration_count, MAX_LOOP_ITERATIONS);
                    interpreter->error_occurred = 1;
                    break;
                }

                double condition = interpreter_evaluate_expression(interpreter, stmt->data.while_stmt.condition);
                if (!condition) break;
                interpreter_execute_statement(interpreter, stmt->data.while_stmt.body);
            }
            break;
        }
        case AST_FOR_STATEMENT: {

            if (stmt->data.for_stmt.init) {
                interpreter_execute_statement(interpreter, stmt->data.for_stmt.init);
            }

            int iteration_count = 0;


            char* increment_var_name = NULL;
            ASTNode* increment_expr = NULL;

            if (stmt->data.for_stmt.increment) {
                ASTNode* increment = stmt->data.for_stmt.increment;
                if (increment->type == AST_BINARY_OPERATION &&
                    increment->data.binary_op.operator == TOKEN_ASSIGN) {
                    if (increment->data.binary_op.left->type == AST_IDENTIFIER) {
                        increment_var_name = increment->data.binary_op.left->data.identifier_name;
                        increment_expr = increment->data.binary_op.right;
                    }
                }
            }

            while (1) {
                if (!interpreter_check_limits(interpreter)) break;

                iteration_count++;
                if (iteration_count > MAX_LOOP_ITERATIONS) {
                    snprintf(interpreter->error_message, sizeof(interpreter->error_message),
                            "循环次数超限: %d > %d", iteration_count, MAX_LOOP_ITERATIONS);
                    interpreter->error_occurred = 1;
                    break;
                }


                if (stmt->data.for_stmt.condition) {
                    double condition = interpreter_evaluate_expression(interpreter, stmt->data.for_stmt.condition);
                    if (!condition) break;
                } else {

                    break;
                }


                interpreter_execute_statement(interpreter, stmt->data.for_stmt.body);


                if (increment_var_name && increment_expr) {

                    double increment_value = interpreter_evaluate_expression(interpreter, increment_expr);
                    scope_set_variable(interpreter->current_scope, increment_var_name, increment_value, NULL, 0, 0);
                } else if (stmt->data.for_stmt.increment) {

                    interpreter_evaluate_expression(interpreter, stmt->data.for_stmt.increment);
                }
            }
            break;
        }
        case AST_FUNCTION_DECLARATION: {
            scope_set_function(interpreter->current_scope,
                           stmt->data.function_decl.name,
                           stmt->data.function_decl.parameters,
                           stmt->data.function_decl.parameter_count,
                           stmt->data.function_decl.body);
            break;
        }
        case AST_RETURN_STATEMENT: {
            if (stmt->data.return_stmt.value) {
                double return_val = interpreter_evaluate_expression(interpreter, stmt->data.return_stmt.value);
                interpreter->return_value = return_val;
                interpreter->has_return = 1;
            } else {
                interpreter->return_value = 0;
                interpreter->has_return = 1;
            }
            break;
        }
        case AST_PRINT_STATEMENT: {
            for (int i = 0; i < stmt->data.print_stmt.value_count; i++) {
                if (i > 0) printf(" ");

                double value = interpreter_evaluate_expression(interpreter, stmt->data.print_stmt.values[i]);
                if (stmt->data.print_stmt.values[i]->type == AST_STRING) {
                    printf("%s", stmt->data.print_stmt.values[i]->data.string_value);
                } else {

                    printf("%f", value);
                }
            }
            printf("\n");
            break;
        }
        case AST_BLOCK: {
            interpreter_execute_block(interpreter, stmt);
            break;
        }
        case AST_NAMESPACE_DECLARATION: {

            Scope* namespace_scope = scope_create(interpreter->current_scope);
            namespace_scope->scope_type = 2;
            namespace_scope->scope_name = accelerator_strdup(stmt->data.namespace_decl.name);


            Scope* old_scope = interpreter->current_scope;
            interpreter->current_scope = namespace_scope;


            interpreter_execute_statement(interpreter, stmt->data.namespace_decl.body);


            interpreter->current_scope = old_scope;


            scope_set_variable(interpreter->current_scope, stmt->data.namespace_decl.name, 0, NULL, 0, 4);

            scope_destroy(namespace_scope);
            break;
        }
        case AST_ACCESS_MODIFIER: {

            ASTNode* member = stmt->data.access_modifier.member;


            Scope* old_scope = interpreter->current_scope;


            interpreter_execute_statement(interpreter, member);


            if (old_scope->variable_count > 0) {
                Variable* last_var = &old_scope->variables[old_scope->variable_count - 1];
                last_var->access_modifier = stmt->data.access_modifier.access_modifier;
            }

            break;
        }
        case AST_DESTRUCTOR_DECLARATION: {

            interpreter_execute_statement(interpreter, stmt->data.destructor_decl.body);
            break;
        }
        case AST_STATIC_FUNCTION_DECLARATION: {

            scope_set_function(interpreter->global_scope,
                             stmt->data.static_function_decl.name,
                             stmt->data.static_function_decl.parameters,
                             stmt->data.static_function_decl.parameter_count,
                             stmt->data.static_function_decl.body);


            Variable* var = scope_find_variable(interpreter->global_scope, stmt->data.static_function_decl.name);
            if (var) {
                var->access_modifier = TOKEN_PUBLIC;
                var->is_static = 1;
            }

            break;
        }
        case AST_STATIC_VARIABLE_DECLARATION: {

            double value = 0;
            if (stmt->data.static_variable_decl.value) {
                value = interpreter_evaluate_expression(interpreter, stmt->data.static_variable_decl.value);
            }


            scope_set_variable(interpreter->global_scope, stmt->data.static_variable_decl.name,
                             value, NULL, 0, 1);


            Variable* var = scope_find_variable(interpreter->global_scope, stmt->data.static_variable_decl.name);
            if (var) {
                var->access_modifier = TOKEN_PUBLIC;
                var->is_static = 1;
            }

            break;
        }
        case AST_CLASS_DECLARATION: {

            Scope* class_scope = scope_create(interpreter->current_scope);
            class_scope->scope_type = 1;
            class_scope->scope_name = accelerator_strdup(stmt->data.class_decl.name);


            Scope* old_scope = interpreter->current_scope;
            interpreter->current_scope = class_scope;


            interpreter_execute_statement(interpreter, stmt->data.class_decl.body);


            interpreter->current_scope = old_scope;


            scope_set_variable(interpreter->current_scope, stmt->data.class_decl.name, 0, NULL, 0, 5);

            scope_destroy(class_scope);
            break;
        }
        default:

            interpreter_evaluate_expression(interpreter, stmt);
            break;
    }
}

static void interpreter_execute_block(Interpreter* interpreter, ASTNode* block) {
    if (!block || block->type != AST_BLOCK) return;

    for (int i = 0; i < block->data.block.statement_count; i++) {
        interpreter_execute_statement(interpreter, block->data.block.statements[i]);
    }
}

void interpreter_run(Interpreter* interpreter, ASTNode* program) {
    if (!interpreter || !program) return;


    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    #endif


    interpreter->start_time = get_current_time();
    interpreter->recursion_depth = 0;
    interpreter->loop_iterations = 0;
    interpreter->error_occurred = 0;
    strcpy(interpreter->error_message, "");

    interpreter_execute_block(interpreter, program);


    if (interpreter->error_occurred) {
        fprintf(stderr, "\n[看门狗报告] %s\n", interpreter->error_message);
        fprintf(stderr, "执行时间: %.2f秒\n", get_current_time() - interpreter->start_time);
        fprintf(stderr, "递归深度: %d\n", interpreter->recursion_depth);
    }


    fflush(stdout);
    fflush(stderr);
}