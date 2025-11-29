#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "../parser/ast_nodes.h"
#include "../lexer/lexer.h"

#ifdef _WIN32
#define strdup _strdup
#endif


#define MAX_LOOP_ITERATIONS 1000000
#define MAX_RECURSION_DEPTH 1000
#define MAX_EXECUTION_TIME 30.0

typedef struct Variable {
    char* name;
    double number_value;
    char* string_value;
    int boolean_value;
    int is_function;
    int type;
    char** parameters;
    int parameter_count;
    ASTNode* body;
    EsTokenType access_modifier;
    int is_static;
} Variable;

typedef struct Scope {
    Variable* variables;
    int variable_count;
    struct Scope* parent;
    int scope_type;
    char* scope_name;
} Scope;

typedef struct {
    Scope* global_scope;
    Scope* current_scope;
    double return_value;
    int has_return;
    int recursion_depth;
    double start_time;
    int loop_iterations;
    int error_occurred;
    char error_message[256];
} Interpreter;

Interpreter* interpreter_create();
void interpreter_destroy(Interpreter* interpreter);
void interpreter_run(Interpreter* interpreter, ASTNode* ast);

#endif