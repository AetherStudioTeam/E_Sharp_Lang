#ifndef ES_PARSER_H
#define ES_PARSER_H

#include "../lexer/tokenizer.h"
#include "ast.h"

typedef struct {
    Lexer* lexer;
    Token current_token;
    char** declared_functions;
    int declared_function_count;
    int declared_function_capacity;
} Parser;

Parser* parser_create(Lexer* lexer);
void parser_destroy(Parser* parser);
ASTNode* parser_parse(Parser* parser);


ASTNode* parser_parse_try_statement(Parser* parser);
ASTNode* parser_parse_catch_clause(Parser* parser);
ASTNode* parser_parse_finally_clause(Parser* parser);
ASTNode* parser_parse_throw_statement(Parser* parser);


ASTNode* parser_parse_template_declaration(Parser* parser);
ASTNode* parser_parse_template_parameter(Parser* parser);

ASTNode* parser_parse_switch_statement(Parser* parser);
ASTNode* parser_parse_case_clause(Parser* parser);
ASTNode* parser_parse_default_clause(Parser* parser);
ASTNode* parser_parse_break_statement(Parser* parser);
ASTNode* parser_parse_continue_statement(Parser* parser);
ASTNode* parser_parse_foreach_statement(Parser* parser);

#endif