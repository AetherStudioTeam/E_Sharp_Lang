#ifndef AST_NODES_H
#define AST_NODES_H

#include "../lexer/lexer.h"

#ifdef _WIN32
#define strdup _strdup
#endif

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION_DECLARATION,
    AST_VARIABLE_DECLARATION,
    AST_STATIC_VARIABLE_DECLARATION,
    AST_ASSIGNMENT,
    AST_ARRAY_ASSIGNMENT,
    AST_IF_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_FOR_STATEMENT,
    AST_RETURN_STATEMENT,
    AST_PRINT_STATEMENT,
    AST_BINARY_OPERATION,
    AST_UNARY_OPERATION,
    AST_TERNARY_OPERATION,
    AST_IDENTIFIER,
    AST_NUMBER,
    AST_STRING,
    AST_BOOLEAN,
    AST_CALL,
    AST_BLOCK,
    AST_ARRAY_LITERAL,
    AST_NEW_EXPRESSION,
    AST_NAMESPACE_DECLARATION,
    AST_CLASS_DECLARATION,
    AST_THIS,
    AST_MEMBER_ACCESS,
    AST_ACCESS_MODIFIER,
    AST_CONSTRUCTOR_DECLARATION,
    AST_DESTRUCTOR_DECLARATION,
    AST_STATIC_FUNCTION_DECLARATION,

    AST_TRY_STATEMENT,
    AST_CATCH_CLAUSE,
    AST_FINALLY_CLAUSE,
    AST_THROW_STATEMENT,

    AST_TEMPLATE_DECLARATION,
    AST_TEMPLATE_PARAMETER,
    AST_GENERIC_TYPE,

    AST_STATIC_METHOD_CALL,
    AST_ARRAY_ACCESS,

    AST_SWITCH_STATEMENT,
    AST_CASE_CLAUSE,
    AST_DEFAULT_CLAUSE,
    AST_BREAK_STATEMENT,
    AST_CONTINUE_STATEMENT,
    AST_DELETE_STATEMENT
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    union {
        double number_value;
        char* string_value;
        char* identifier_name;
        int boolean_value;
        struct {
            char* name;
            char** parameters;
            int parameter_count;
            EsTokenType* parameter_types;
            struct ASTNode* body;
            EsTokenType return_type;
        } function_decl;
        struct {
            char* name;
            char** parameters;
            int parameter_count;
            EsTokenType* parameter_types;
            struct ASTNode* body;
            EsTokenType return_type;
        } static_function_decl;
        struct {
            char* name;
            struct ASTNode* value;
            EsTokenType type;
            struct ASTNode* array_size;
            int is_array;
        } variable_decl;
        struct {
            char* name;
            struct ASTNode* value;
            EsTokenType type;
        } static_variable_decl;
        struct {
            char* name;
            struct ASTNode* value;
        } assignment;
        struct {
            struct ASTNode* array;
            struct ASTNode* index;
            struct ASTNode* value;
        } array_assignment;
        struct {
            struct ASTNode* condition;
            struct ASTNode* then_branch;
            struct ASTNode* else_branch;
        } if_stmt;
        struct {
            struct ASTNode* condition;
            struct ASTNode* body;
        } while_stmt;
        struct {
            struct ASTNode* init;
            struct ASTNode* condition;
            struct ASTNode* increment;
            struct ASTNode* body;
        } for_stmt;
        struct {
            struct ASTNode* value;
        } return_stmt;
        struct {
            struct ASTNode** values;
            int value_count;
        } print_stmt;
        struct {
            struct ASTNode* left;
            EsTokenType operator;
            struct ASTNode* right;
        } binary_op;
        struct {
            EsTokenType operator;
            struct ASTNode* operand;
            int is_postfix;
        } unary_op;
        struct {
            struct ASTNode* condition;
            struct ASTNode* true_value;
            struct ASTNode* false_value;
        } ternary_op;
        struct {
            char* name;
            struct ASTNode** arguments;
            int argument_count;
            char** argument_names;
            struct ASTNode* object;
            char* resolved_class_name;
        } call;
        struct {
            struct ASTNode** statements;
            int statement_count;
        } block;
        struct {
            char* class_name;
            char* method_name;
            struct ASTNode** arguments;
            int argument_count;
        } static_call;
        struct {
            struct ASTNode* array;
            struct ASTNode* index;
        } array_access;

        struct {
            struct ASTNode** elements;
            int element_count;
        } array_literal;
        struct {
            char* class_name;
            struct ASTNode** arguments;
            int argument_count;
            char** argument_names;
        } new_expr;
        struct {
            char* name;
            struct ASTNode* body;
        } namespace_decl;
        struct {
            char* name;
            struct ASTNode* body;
            struct ASTNode* base_class;
        } class_decl;
        struct {
            struct ASTNode* object;
            char* member_name;
            char* resolved_class_name;
        } member_access;
        struct {
            EsTokenType access_modifier;
            struct ASTNode* member;
        } access_modifier;
        struct {
            char** parameters;
            int parameter_count;
            EsTokenType* parameter_types;
            struct ASTNode* body;
        } constructor_decl;
        struct {
            char* class_name;
            struct ASTNode* body;
        } destructor_decl;

        struct {
            struct ASTNode* try_block;
            struct ASTNode** catch_clauses;
            int catch_clause_count;
            struct ASTNode* finally_clause;
        } try_stmt;
        struct {
            char* exception_type;
            char* exception_var;
            struct ASTNode* catch_block;
        } catch_clause;
        struct {
            struct ASTNode* finally_block;
        } finally_clause;
        struct {
            struct ASTNode* exception_expr;
        } throw_stmt;

        struct {
            struct ASTNode** parameters;
            int parameter_count;
            struct ASTNode* declaration;
        } template_decl;
        struct {
            char* param_name;
        } template_param;
        struct {
            char* type_name;
        } generic_type;

        struct {
            struct ASTNode* expression;
            struct ASTNode** cases;
            int case_count;
            struct ASTNode* default_case;
        } switch_stmt;
        struct {
            struct ASTNode* value;
            struct ASTNode** statements;
            int statement_count;
        } case_clause;
        struct {
            struct ASTNode** statements;
            int statement_count;
        } default_clause;
        struct {
            struct ASTNode* value;
            char* resolved_class_name;
        } break_stmt;
        struct {
            struct ASTNode* value;
        } continue_stmt;
        struct {
            struct ASTNode* value;
            char* resolved_class_name;
        } delete_stmt;
    } data;
} ASTNode;

ASTNode* ast_create_node(ASTNodeType type);
void ast_destroy(ASTNode* node);
void ast_print(ASTNode* node, int indent);

#endif