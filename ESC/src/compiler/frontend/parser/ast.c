#include "ast.h"
#include "../../../accelerator.h"
#include "../../../core/utils/es_common.h"
#include "../../../core/utils/output_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


extern void* memset(void* s, int c, size_t n);

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
            if (node->data.variable_decl.template_instantiation_type) ES_FREE(node->data.variable_decl.template_instantiation_type);
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
        case AST_COMPOUND_ASSIGNMENT:
            if (node->data.compound_assignment.name) ES_FREE(node->data.compound_assignment.name);
            ast_destroy(node->data.compound_assignment.value);
            break;
        case AST_ARRAY_COMPOUND_ASSIGNMENT:
            ast_destroy(node->data.array_compound_assignment.array);
            ast_destroy(node->data.array_compound_assignment.index);
            ast_destroy(node->data.array_compound_assignment.value);
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
        case AST_FOREACH_STATEMENT:
            if (node->data.foreach_stmt.var_name) ES_FREE(node->data.foreach_stmt.var_name);
            ast_destroy(node->data.foreach_stmt.iterable);
            ast_destroy(node->data.foreach_stmt.body);
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
        case AST_ARRAY_ACCESS:
            ast_destroy(node->data.array_access.array);
            ast_destroy(node->data.array_access.index);
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
        case AST_CLASS_DECLARATION:
            if (node->data.class_decl.name) ES_FREE(node->data.class_decl.name);
            ast_destroy(node->data.class_decl.body);
            if (node->data.class_decl.base_class) ast_destroy(node->data.class_decl.base_class);
            if (node->data.class_decl.template_params) {
                for (int i = 0; i < node->data.class_decl.template_param_count; i++) {
                    if (node->data.class_decl.template_params[i]) {
                        ES_FREE(node->data.class_decl.template_params[i]);
                    }
                }
                ES_FREE(node->data.class_decl.template_params);
            }
            
            if (node->data.class_decl.constraints) {
                for (int i = 0; i < node->data.class_decl.constraint_count; i++) {
                    ast_destroy(node->data.class_decl.constraints[i]);
                }
                ES_FREE(node->data.class_decl.constraints);
            }
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
            
            if (node->data.template_decl.constraints) {
                for (int i = 0; i < node->data.template_decl.constraint_count; i++) {
                    ast_destroy(node->data.template_decl.constraints[i]);
                }
                ES_FREE(node->data.template_decl.constraints);
            }
            ast_destroy(node->data.template_decl.declaration);
            break;
        case AST_TEMPLATE_PARAMETER:
            if (node->data.template_param.param_name) ES_FREE(node->data.template_param.param_name);
            break;
        case AST_GENERIC_TYPE:
            if (node->data.generic_type.type_name) ES_FREE(node->data.generic_type.type_name);
            break;
        case AST_GENERIC_CONSTRAINT:
            if (node->data.generic_constraint.param_name) ES_FREE(node->data.generic_constraint.param_name);
            if (node->data.generic_constraint.constraint_type) ES_FREE(node->data.generic_constraint.constraint_type);
            if (node->data.generic_constraint.interface_constraint) ast_destroy(node->data.generic_constraint.interface_constraint);
            break;
        case AST_USING_STATEMENT:
            if (node->data.using_stmt.resource) ast_destroy(node->data.using_stmt.resource);
            if (node->data.using_stmt.body) ast_destroy(node->data.using_stmt.body);
            break;
        case AST_NAMESPACE_IMPORT:
            if (node->data.namespace_import.namespace_name) ES_FREE(node->data.namespace_import.namespace_name);
            break;
        
        case AST_PROPERTY_DECLARATION:
            if (node->data.property_decl.name) ES_FREE(node->data.property_decl.name);
            if (node->data.property_decl.getter) ast_destroy(node->data.property_decl.getter);
            if (node->data.property_decl.setter) ast_destroy(node->data.property_decl.setter);
            if (node->data.property_decl.initial_value) ast_destroy(node->data.property_decl.initial_value);
            if (node->data.property_decl.attributes) {
                for (int i = 0; i < node->data.property_decl.attribute_count; i++) {
                    ast_destroy(node->data.property_decl.attributes[i]);
                }
                ES_FREE(node->data.property_decl.attributes);
            }
            break;
        case AST_PROPERTY_GETTER:
            if (node->data.property_getter.body) ast_destroy(node->data.property_getter.body);
            break;
        case AST_PROPERTY_SETTER:
            if (node->data.property_setter.value_param_name) ES_FREE(node->data.property_setter.value_param_name);
            if (node->data.property_setter.body) ast_destroy(node->data.property_setter.body);
            break;
        case AST_LAMBDA_EXPRESSION:
            if (node->data.lambda_expr.parameters) {
                for (int i = 0; i < node->data.lambda_expr.parameter_count; i++) {
                    ES_FREE(node->data.lambda_expr.parameters[i]);
                }
                ES_FREE(node->data.lambda_expr.parameters);
            }
            if (node->data.lambda_expr.body) ast_destroy(node->data.lambda_expr.body);
            if (node->data.lambda_expr.expression) ast_destroy(node->data.lambda_expr.expression);
            break;
        case AST_LINQ_QUERY:
            if (node->data.linq_query.from_clause) ast_destroy(node->data.linq_query.from_clause);
            if (node->data.linq_query.clauses) {
                for (int i = 0; i < node->data.linq_query.clause_count; i++) {
                    ast_destroy(node->data.linq_query.clauses[i]);
                }
                ES_FREE(node->data.linq_query.clauses);
            }
            if (node->data.linq_query.select_clause) ast_destroy(node->data.linq_query.select_clause);
            break;
        case AST_LINQ_FROM:
            if (node->data.linq_from.var_name) ES_FREE(node->data.linq_from.var_name);
            if (node->data.linq_from.source) ast_destroy(node->data.linq_from.source);
            if (node->data.linq_from.type) ast_destroy(node->data.linq_from.type);
            break;
        case AST_LINQ_WHERE:
            if (node->data.linq_where.condition) ast_destroy(node->data.linq_where.condition);
            break;
        case AST_LINQ_SELECT:
            if (node->data.linq_select.expression) ast_destroy(node->data.linq_select.expression);
            if (node->data.linq_select.key_selector) ast_destroy(node->data.linq_select.key_selector);
            break;
        case AST_LINQ_ORDERBY:
            if (node->data.linq_orderby.expression) ast_destroy(node->data.linq_orderby.expression);
            break;
        case AST_LINQ_JOIN:
            if (node->data.linq_join.var_name) ES_FREE(node->data.linq_join.var_name);
            if (node->data.linq_join.source) ast_destroy(node->data.linq_join.source);
            if (node->data.linq_join.join_var_name) ES_FREE(node->data.linq_join.join_var_name);
            if (node->data.linq_join.join_source) ast_destroy(node->data.linq_join.join_source);
            if (node->data.linq_join.left_key) ast_destroy(node->data.linq_join.left_key);
            if (node->data.linq_join.right_key) ast_destroy(node->data.linq_join.right_key);
            if (node->data.linq_join.into_var_name) ES_FREE(node->data.linq_join.into_var_name);
            break;
        case AST_ATTRIBUTE:
            if (node->data.attribute.name) ES_FREE(node->data.attribute.name);
            if (node->data.attribute.arguments) {
                for (int i = 0; i < node->data.attribute.argument_count; i++) {
                    ast_destroy(node->data.attribute.arguments[i]);
                }
                ES_FREE(node->data.attribute.arguments);
            }
            if (node->data.attribute.named_arguments) ast_destroy(node->data.attribute.named_arguments);
            break;
        case AST_ATTRIBUTE_LIST:
            if (node->data.attribute_list.attributes) {
                for (int i = 0; i < node->data.attribute_list.attribute_count; i++) {
                    ast_destroy(node->data.attribute_list.attributes[i]);
                }
                ES_FREE(node->data.attribute_list.attributes);
            }
            if (node->data.attribute_list.target) ast_destroy(node->data.attribute_list.target);
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
        case AST_FOREACH_STATEMENT:
            printf("FOREACH_STATEMENT: var=%s\n", node->data.foreach_stmt.var_name);
            ast_print_indent(indent + 1);
            printf("Iterable: ");
            ast_print(node->data.foreach_stmt.iterable, 0);
            printf("\n");
            ast_print_indent(indent + 1);
            printf("Body: \n");
            ast_print(node->data.foreach_stmt.body, indent + 2);
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
        case AST_USING_STATEMENT:
            printf("USING_STATEMENT\n");
            ast_print(node->data.using_stmt.resource, indent + 1);
            ast_print(node->data.using_stmt.body, indent + 1);
            break;
        case AST_NAMESPACE_IMPORT:
            printf("NAMESPACE_IMPORT: %s\n", node->data.namespace_import.namespace_name);
            break;
        
        case AST_PROPERTY_DECLARATION:
            printf("PROPERTY_DECLARATION: %s (type: %s)\n", 
                   node->data.property_decl.name, 
                   token_type_to_string(node->data.property_decl.type));
            if (node->data.property_decl.initial_value) {
                ast_print_indent(indent + 1);
                printf("INITIAL_VALUE:\n");
                ast_print(node->data.property_decl.initial_value, indent + 2);
            }
            if (node->data.property_decl.getter) {
                ast_print(node->data.property_decl.getter, indent + 1);
            }
            if (node->data.property_decl.setter) {
                ast_print(node->data.property_decl.setter, indent + 1);
            }
            break;
        case AST_PROPERTY_GETTER:
            printf("GETTER:\n");
            if (node->data.property_getter.body) {
                ast_print(node->data.property_getter.body, indent + 1);
            }
            break;
        case AST_PROPERTY_SETTER:
            printf("SETTER (value param: %s):\n", 
                   node->data.property_setter.value_param_name ? node->data.property_setter.value_param_name : "value");
            if (node->data.property_setter.body) {
                ast_print(node->data.property_setter.body, indent + 1);
            }
            break;
        case AST_LAMBDA_EXPRESSION:
            printf("LAMBDA_EXPRESSION (%d params):\n", node->data.lambda_expr.parameter_count);
            for (int i = 0; i < node->data.lambda_expr.parameter_count; i++) {
                ast_print_indent(indent + 1);
                printf("PARAM: %s\n", node->data.lambda_expr.parameters[i]);
            }
            if (node->data.lambda_expr.expression) {
                ast_print_indent(indent + 1);
                printf("EXPRESSION:\n");
                ast_print(node->data.lambda_expr.expression, indent + 2);
            } else if (node->data.lambda_expr.body) {
                ast_print(node->data.lambda_expr.body, indent + 1);
            }
            break;
        case AST_LINQ_QUERY:
            printf("LINQ_QUERY:\n");
            if (node->data.linq_query.from_clause) {
                ast_print(node->data.linq_query.from_clause, indent + 1);
            }
            for (int i = 0; i < node->data.linq_query.clause_count; i++) {
                ast_print(node->data.linq_query.clauses[i], indent + 1);
            }
            if (node->data.linq_query.select_clause) {
                ast_print(node->data.linq_query.select_clause, indent + 1);
            }
            break;
        case AST_LINQ_FROM:
            printf("FROM: %s in ", node->data.linq_from.var_name);
            ast_print(node->data.linq_from.source, 0);
            break;
        case AST_LINQ_WHERE:
            printf("WHERE:\n");
            ast_print(node->data.linq_where.condition, indent + 1);
            break;
        case AST_LINQ_SELECT:
            printf("SELECT:\n");
            ast_print(node->data.linq_select.expression, indent + 1);
            break;
        case AST_LINQ_ORDERBY:
            printf("ORDERBY (%s):\n", node->data.linq_orderby.ascending ? "ascending" : "descending");
            ast_print(node->data.linq_orderby.expression, indent + 1);
            break;
        case AST_LINQ_JOIN:
            printf("JOIN: %s in ", node->data.linq_join.var_name);
            ast_print(node->data.linq_join.source, 0);
            printf(" on ");
            ast_print(node->data.linq_join.left_key, 0);
            printf(" equals ");
            ast_print(node->data.linq_join.right_key, 0);
            if (node->data.linq_join.into_var_name) {
                printf(" into %s", node->data.linq_join.into_var_name);
            }
            printf("\n");
            break;
        case AST_ATTRIBUTE:
            printf("ATTRIBUTE: %s\n", node->data.attribute.name);
            for (int i = 0; i < node->data.attribute.argument_count; i++) {
                ast_print(node->data.attribute.arguments[i], indent + 1);
            }
            break;
        case AST_ATTRIBUTE_LIST:
            printf("ATTRIBUTE_LIST (%d attributes):\n", node->data.attribute_list.attribute_count);
            for (int i = 0; i < node->data.attribute_list.attribute_count; i++) {
                ast_print(node->data.attribute_list.attributes[i], indent + 1);
            }
            if (node->data.attribute_list.target) {
                ast_print_indent(indent + 1);
                printf("TARGET:\n");
                ast_print(node->data.attribute_list.target, indent + 2);
            }
            break;
    }
}