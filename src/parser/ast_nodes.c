#define _GNU_SOURCE
#include "ast_nodes.h"
#include "accelerator.h"
#include "../common/es_common.h"
#include "../common/output_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ASTNode* ast_create_node(ASTNodeType type) {
    ASTNode* node = ES_MALLOC(sizeof(ASTNode));
    if (!node) return NULL;

    node->type = type;
    memset(&node->data, 0, sizeof(node->data));

    return node;
}

void ast_destroy(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_STRING:
            if (node->data.string_value) ES_FREE(node->data.string_value);
            break;
        case AST_IDENTIFIER:
            if (node->data.identifier_name) ES_FREE(node->data.identifier_name);
            break;
        case AST_FUNCTION_DECLARATION:
            if (node->data.function_decl.name) ES_FREE(node->data.function_decl.name);
            if (node->data.function_decl.parameters) {
                for (int i = 0; i < node->data.function_decl.parameter_count; i++) {
                    ES_FREE(node->data.function_decl.parameters[i]);
                }
                ES_FREE(node->data.function_decl.parameters);
            }
            if (node->data.function_decl.parameter_types) {
                ES_FREE(node->data.function_decl.parameter_types);
            }
            ast_destroy(node->data.function_decl.body);
            break;
        case AST_STATIC_FUNCTION_DECLARATION:
            if (node->data.static_function_decl.name) ES_FREE(node->data.static_function_decl.name);
            if (node->data.static_function_decl.parameters) {
                for (int i = 0; i < node->data.static_function_decl.parameter_count; i++) {
                    ES_FREE(node->data.static_function_decl.parameters[i]);
                }
                ES_FREE(node->data.static_function_decl.parameters);
            }
            if (node->data.static_function_decl.parameter_types) {
                ES_FREE(node->data.static_function_decl.parameter_types);
            }
            ast_destroy(node->data.static_function_decl.body);
            break;
        case AST_VARIABLE_DECLARATION:
            if (node->data.variable_decl.name) ES_FREE(node->data.variable_decl.name);
            if (node->data.variable_decl.value) ast_destroy(node->data.variable_decl.value);
            if (node->data.variable_decl.array_size) ast_destroy(node->data.variable_decl.array_size);
            break;
        case AST_STATIC_VARIABLE_DECLARATION:
            if (node->data.static_variable_decl.name) ES_FREE(node->data.static_variable_decl.name);
            if (node->data.static_variable_decl.value) ast_destroy(node->data.static_variable_decl.value);
            break;
        case AST_ASSIGNMENT:
            if (node->data.assignment.name) ES_FREE(node->data.assignment.name);
            ast_destroy(node->data.assignment.value);
            break;
        case AST_ARRAY_ASSIGNMENT:
            ast_destroy(node->data.array_assignment.array);
            ast_destroy(node->data.array_assignment.index);
            ast_destroy(node->data.array_assignment.value);
            break;
        case AST_IF_STATEMENT:
            ast_destroy(node->data.if_stmt.condition);
            ast_destroy(node->data.if_stmt.then_branch);
            ast_destroy(node->data.if_stmt.else_branch);
            break;
        case AST_WHILE_STATEMENT:
            ast_destroy(node->data.while_stmt.condition);
            ast_destroy(node->data.while_stmt.body);
            break;
        case AST_FOR_STATEMENT:
            ast_destroy(node->data.for_stmt.init);
            ast_destroy(node->data.for_stmt.condition);
            ast_destroy(node->data.for_stmt.increment);
            ast_destroy(node->data.for_stmt.body);
            break;
        case AST_RETURN_STATEMENT:
            ast_destroy(node->data.return_stmt.value);
            break;
        case AST_PRINT_STATEMENT:
            if (node->data.print_stmt.values) {
                for (int i = 0; i < node->data.print_stmt.value_count; i++) {
                    ast_destroy(node->data.print_stmt.values[i]);
                }
                ES_FREE(node->data.print_stmt.values);
            }
            break;
        case AST_BINARY_OPERATION:
            ast_destroy(node->data.binary_op.left);
            ast_destroy(node->data.binary_op.right);
            break;
        case AST_UNARY_OPERATION:
            ast_destroy(node->data.unary_op.operand);
            break;
        case AST_CALL:
            if (node->data.call.name) ES_FREE(node->data.call.name);
            if (node->data.call.arguments) {
                for (int i = 0; i < node->data.call.argument_count; i++) {
                    ast_destroy(node->data.call.arguments[i]);
                }
                ES_FREE(node->data.call.arguments);
            }
            if (node->data.call.argument_names) {
                for (int i = 0; i < node->data.call.argument_count; i++) {
                    if (node->data.call.argument_names[i]) {
                        ES_FREE(node->data.call.argument_names[i]);
                    }
                }
                ES_FREE(node->data.call.argument_names);
            }
            if (node->data.call.object) {
                ast_destroy(node->data.call.object);
            }
            break;
        case AST_BLOCK:
            if (node->data.block.statements) {
                for (int i = 0; i < node->data.block.statement_count; i++) {
                    ast_destroy(node->data.block.statements[i]);
                }
                ES_FREE(node->data.block.statements);
            }
            break;
        case AST_ARRAY_LITERAL:
            if (node->data.array_literal.elements) {
                for (int i = 0; i < node->data.array_literal.element_count; i++) {
                    ast_destroy(node->data.array_literal.elements[i]);
                }
                ES_FREE(node->data.array_literal.elements);
            }
            break;
        case AST_THIS:

            break;
        case AST_MEMBER_ACCESS:
            ast_destroy(node->data.member_access.object);
            ES_FREE(node->data.member_access.member_name);
            break;
        case AST_ACCESS_MODIFIER:
            ast_destroy(node->data.access_modifier.member);
            break;
        case AST_CONSTRUCTOR_DECLARATION:
            if (node->data.constructor_decl.parameters) {
                for (int i = 0; i < node->data.constructor_decl.parameter_count; i++) {
                    ES_FREE(node->data.constructor_decl.parameters[i]);
                }
                ES_FREE(node->data.constructor_decl.parameters);
            }
            if (node->data.constructor_decl.parameter_types) {
                ES_FREE(node->data.constructor_decl.parameter_types);
            }
            ast_destroy(node->data.constructor_decl.body);
            break;
        case AST_DESTRUCTOR_DECLARATION:
            if (node->data.destructor_decl.class_name) ES_FREE(node->data.destructor_decl.class_name);
            ast_destroy(node->data.destructor_decl.body);
            break;

        case AST_TRY_STATEMENT:
            ast_destroy(node->data.try_stmt.try_block);
            if (node->data.try_stmt.catch_clauses) {
                for (int i = 0; i < node->data.try_stmt.catch_clause_count; i++) {
                    ast_destroy(node->data.try_stmt.catch_clauses[i]);
                }
                ES_FREE(node->data.try_stmt.catch_clauses);
            }
            ast_destroy(node->data.try_stmt.finally_clause);
            break;
        case AST_CATCH_CLAUSE:
            if (node->data.catch_clause.exception_type) ES_FREE(node->data.catch_clause.exception_type);
            if (node->data.catch_clause.exception_var) ES_FREE(node->data.catch_clause.exception_var);
            ast_destroy(node->data.catch_clause.catch_block);
            break;
        case AST_FINALLY_CLAUSE:
            ast_destroy(node->data.finally_clause.finally_block);
            break;
        case AST_THROW_STATEMENT:
            ast_destroy(node->data.throw_stmt.exception_expr);
            break;

        case AST_TEMPLATE_DECLARATION:
            if (node->data.template_decl.parameters) {
                for (int i = 0; i < node->data.template_decl.parameter_count; i++) {
                    ast_destroy(node->data.template_decl.parameters[i]);
                }
                ES_FREE(node->data.template_decl.parameters);
            }
            ast_destroy(node->data.template_decl.declaration);
            break;
        case AST_TEMPLATE_PARAMETER:
            if (node->data.template_param.param_name) ES_FREE(node->data.template_param.param_name);
            break;
        case AST_GENERIC_TYPE:
            if (node->data.generic_type.type_name) ES_FREE(node->data.generic_type.type_name);
            break;
        default:
            break;
    }

    ES_FREE(node);
}

static void ast_print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print(ASTNode* node, int indent) {
    if (!node) return;

    ast_print_indent(indent);

    switch (node->type) {
        case AST_PROGRAM:
            printf("PROGRAM\n");
            break;
        case AST_FUNCTION_DECLARATION:
            printf("FUNCTION_DECLARATION: %s, return type: %s\n", node->data.function_decl.name, token_type_to_string(node->data.function_decl.return_type));
            printf("  Parameters:");
            for (int i = 0; i < node->data.function_decl.parameter_count; i++) {
                printf(" %s", node->data.function_decl.parameters[i]);
            }
            printf("\n");
            ast_print(node->data.function_decl.body, indent + 1);
            break;
        case AST_STATIC_FUNCTION_DECLARATION:
            printf("STATIC_FUNCTION_DECLARATION: %s, return type: %s\n", node->data.static_function_decl.name, token_type_to_string(node->data.static_function_decl.return_type));
            printf("  Parameters:");
            for (int i = 0; i < node->data.static_function_decl.parameter_count; i++) {
                printf(" %s", node->data.static_function_decl.parameters[i]);
            }
            printf("\n");
            ast_print(node->data.static_function_decl.body, indent + 1);
            break;
        case AST_VARIABLE_DECLARATION:
            if (node->data.variable_decl.is_array) {
                printf("ARRAY_DECLARATION: %s", node->data.variable_decl.name);
                if (node->data.variable_decl.array_size) {
                    printf("[");
                    ast_print(node->data.variable_decl.array_size, 0);
                    printf("]");
                }
                printf("\n");
            } else {
                printf("VARIABLE_DECLARATION: %s\n", node->data.variable_decl.name);
            }
            if (node->data.variable_decl.value) {
                ast_print(node->data.variable_decl.value, indent + 1);
            }
            break;
        case AST_STATIC_VARIABLE_DECLARATION:
            printf("STATIC_VARIABLE_DECLARATION: %s\n", node->data.static_variable_decl.name);
            if (node->data.static_variable_decl.value) {
                ast_print(node->data.static_variable_decl.value, indent + 1);
            }
            break;
        case AST_ASSIGNMENT:
            printf("ASSIGNMENT: %s = ", node->data.assignment.name);
            ast_print(node->data.assignment.value, 0);
            printf("\n");
            break;
        case AST_ARRAY_ASSIGNMENT:
            printf("ARRAY_ASSIGNMENT: ");
            ast_print(node->data.array_assignment.array, 0);
            printf("[");
            ast_print(node->data.array_assignment.index, 0);
            printf("] = ");
            ast_print(node->data.array_assignment.value, 0);
            printf("\n");
            break;
        case AST_IF_STATEMENT:
            printf("IF_STATEMENT\n");
            ast_print_indent(indent + 1);
            printf("Condition: ");
            ast_print(node->data.if_stmt.condition, 0);
            printf("\n");
            ast_print_indent(indent + 1);
            printf("Then: \n");
            ast_print(node->data.if_stmt.then_branch, indent + 2);
            if (node->data.if_stmt.else_branch) {
                ast_print_indent(indent + 1);
                printf("Else: \n");
                ast_print(node->data.if_stmt.else_branch, indent + 2);
            }
            break;
        case AST_WHILE_STATEMENT:
            printf("WHILE_STATEMENT\n");
            ast_print_indent(indent + 1);
            printf("Condition: ");
            ast_print(node->data.while_stmt.condition, 0);
            printf("\n");
            ast_print_indent(indent + 1);
            printf("Body: \n");
            ast_print(node->data.while_stmt.body, indent + 2);
            break;
        case AST_FOR_STATEMENT:
            printf("FOR_STATEMENT\n");
            break;
        case AST_RETURN_STATEMENT:
            printf("RETURN_STATEMENT: ");
            ast_print(node->data.return_stmt.value, 0);
            printf("\n");
            break;
        case AST_PRINT_STATEMENT:
            printf("PRINT_STATEMENT: ");
            for (int i = 0; i < node->data.print_stmt.value_count; i++) {
                if (i > 0) printf(", ");
                ast_print(node->data.print_stmt.values[i], 0);
            }
            printf("\n");
            break;
        case AST_BINARY_OPERATION:
            printf("BINARY_OPERATION: %s\n", token_type_to_string(node->data.binary_op.operator));
            ast_print_indent(indent + 1);
            printf("Left: ");
            ast_print(node->data.binary_op.left, 0);
            printf("\n");
            ast_print_indent(indent + 1);
            printf("Right: ");
            ast_print(node->data.binary_op.right, 0);
            printf("\n");
            break;
        case AST_UNARY_OPERATION:
            printf("UNARY_OPERATION: %s\n", token_type_to_string(node->data.unary_op.operator));
            ast_print_indent(indent + 1);
            printf("Operand: ");
            ast_print(node->data.unary_op.operand, 0);
            printf("\n");
            break;
        case AST_TERNARY_OPERATION:
            printf("TERNARY_OPERATION\n");
            ast_print_indent(indent + 1);
            printf("Condition: ");
            ast_print(node->data.ternary_op.condition, 0);
            printf("\n");
            ast_print_indent(indent + 1);
            printf("True Value: ");
            ast_print(node->data.ternary_op.true_value, 0);
            printf("\n");
            ast_print_indent(indent + 1);
            printf("False Value: ");
            ast_print(node->data.ternary_op.false_value, 0);
            printf("\n");
            break;
        case AST_IDENTIFIER:
            printf("IDENTIFIER: %s\n", node->data.identifier_name);
            break;
        case AST_NUMBER:
            printf("NUMBER: %f\n", node->data.number_value);
            break;
        case AST_STRING:
            printf("STRING: \"%s\"\n", node->data.string_value);
            break;
        case AST_BOOLEAN:
            printf("BOOLEAN: %s\n", node->data.boolean_value ? "true" : "false");
            break;
        case AST_CALL:
            printf("CALL: %s\n", node->data.call.name);
            break;
        case AST_ARRAY_LITERAL:
            printf("ARRAY_LITERAL\n");
            for (int i = 0; i < node->data.array_literal.element_count; i++) {
                ast_print_indent(indent + 1);
                printf("Element %d: ", i);
                ast_print(node->data.array_literal.elements[i], 0);
                printf("\n");
            }
            break;
        case AST_CLASS_DECLARATION:
            printf("CLASS_DECLARATION: %s", node->data.class_decl.name);
            if (node->data.class_decl.base_class) {
                printf(" extends ");
                ast_print(node->data.class_decl.base_class, 0);
            }
            printf("\n");
            ast_print(node->data.class_decl.body, indent + 1);
            break;
        case AST_THIS:
            printf("THIS\n");
            break;
        case AST_MEMBER_ACCESS:
            printf("MEMBER_ACCESS: ");
            ast_print(node->data.member_access.object, 0);
            printf(".%s\n", node->data.member_access.member_name);
            break;
        case AST_ACCESS_MODIFIER:
            printf("ACCESS_MODIFIER: %s\n", token_type_to_string(node->data.access_modifier.access_modifier));
            ast_print(node->data.access_modifier.member, indent + 1);
            break;
        case AST_CONSTRUCTOR_DECLARATION:
            printf("CONSTRUCTOR_DECLARATION\n");
            printf("  Parameters:");
            for (int i = 0; i < node->data.constructor_decl.parameter_count; i++) {
                printf(" %s", node->data.constructor_decl.parameters[i]);
            }
            printf("\n");
            ast_print(node->data.constructor_decl.body, indent + 1);
            break;
        case AST_DESTRUCTOR_DECLARATION:
            printf("DESTRUCTOR_DECLARATION");
            if (node->data.destructor_decl.class_name) {
                printf(": %s", node->data.destructor_decl.class_name);
            }
            printf("\n");
            ast_print(node->data.destructor_decl.body, indent + 1);
            break;

        case AST_TRY_STATEMENT:
            printf("TRY_STATEMENT\n");
            ast_print_indent(indent + 1);
            printf("Try Block:\n");
            ast_print(node->data.try_stmt.try_block, indent + 2);
            if (node->data.try_stmt.catch_clauses && node->data.try_stmt.catch_clause_count > 0) {
                for (int i = 0; i < node->data.try_stmt.catch_clause_count; i++) {
                    ast_print_indent(indent + 1);
                    printf("Catch Clause %d:\n", i);
                    ast_print(node->data.try_stmt.catch_clauses[i], indent + 2);
                }
            }
            if (node->data.try_stmt.finally_clause) {
                ast_print_indent(indent + 1);
                printf("Finally Block:\n");
                ast_print(node->data.try_stmt.finally_clause, indent + 2);
            }
            break;
        case AST_CATCH_CLAUSE:
            printf("CATCH_CLAUSE");
            if (node->data.catch_clause.exception_type) {
                printf(" (%s", node->data.catch_clause.exception_type);
                if (node->data.catch_clause.exception_var) {
                    printf(" %s", node->data.catch_clause.exception_var);
                }
                printf(")");
            }
            printf("\n");
            ast_print(node->data.catch_clause.catch_block, indent + 1);
            break;
        case AST_FINALLY_CLAUSE:
            printf("FINALLY_CLAUSE\n");
            ast_print(node->data.finally_clause.finally_block, indent + 1);
            break;
        case AST_THROW_STATEMENT:
            printf("THROW_STATEMENT");
            if (node->data.throw_stmt.exception_expr) {
                printf(": ");
                ast_print(node->data.throw_stmt.exception_expr, 0);
            }
            printf("\n");
            break;

        case AST_TEMPLATE_DECLARATION:
            printf("TEMPLATE_DECLARATION\n");
            ast_print_indent(indent + 1);
            printf("Parameters:");
            for (int i = 0; i < node->data.template_decl.parameter_count; i++) {
                printf(" ");
                ast_print(node->data.template_decl.parameters[i], 0);
            }
            printf("\n");
            ast_print(node->data.template_decl.declaration, indent + 1);
            break;
        case AST_TEMPLATE_PARAMETER:
            printf("TEMPLATE_PARAMETER: %s\n", node->data.template_param.param_name);
            break;
        case AST_GENERIC_TYPE:
            printf("GENERIC_TYPE: %s\n", node->data.generic_type.type_name);
            break;
        default:
            printf("UNKNOWN_NODE\n");
            break;
    }
}