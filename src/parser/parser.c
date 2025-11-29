#define _GNU_SOURCE
#include "parser.h"
#include "../lexer/lexer.h"
#include "accelerator.h"
#include "../common/es_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parser_advance(Parser* parser) {
    parser->current_token = lexer_next_token(parser->lexer);
}

static ASTNode* parser_parse_expression(Parser* parser);
static ASTNode* parser_parse_statement(Parser* parser);
static ASTNode* parser_parse_block(Parser* parser);
static ASTNode* parser_parse_new_expression(Parser* parser);
static ASTNode* parser_parse_postfix_expression(Parser* parser);
static ASTNode* parser_parse_constructor_declaration(Parser* parser);
static ASTNode* parser_parse_destructor_declaration(Parser* parser);
static ASTNode* parser_parse_static_member_declaration(Parser* parser);
static ASTNode* parser_parse_static_variable_declaration(Parser* parser);
static ASTNode* parser_parse_static_function_declaration(Parser* parser);
static ASTNode* parser_parse_function_declaration(Parser* parser);
static ASTNode* parser_parse_ternary_operation(Parser* parser);
static ASTNode* parser_parse_type_declaration(Parser* parser);
static ASTNode* parser_parse_typed_function_declaration(Parser* parser,
                                                       EsTokenType return_type,
                                                       char* function_name,
                                                       int is_static);
static int parser_is_type_keyword(EsTokenType type);
static int parser_parse_parameter_list(Parser* parser,
                                       char*** parameters,
                                       EsTokenType** parameter_types,
                                       int* parameter_count);
static void parser_free_parameter_list(char** parameters,
                                       EsTokenType* parameter_types,
                                       int parameter_count);
static ASTNode* parser_create_function_node(Parser* parser,
                                            int is_static,
                                            char* name,
                                            char** parameters,
                                            EsTokenType* parameter_types,
                                            int parameter_count,
                                            ASTNode* body,
                                            EsTokenType return_type);


static void parser_add_declared_function(Parser* parser, const char* func_name) {
    if (!parser || !func_name) return;


    for (int i = 0; i < parser->declared_function_count; i++) {
        if (strcmp(parser->declared_functions[i], func_name) == 0) {
            return;
        }
    }


    if (parser->declared_function_count >= parser->declared_function_capacity) {
        int new_capacity = parser->declared_function_capacity == 0 ? 16 : parser->declared_function_capacity * 2;
        char** new_functions = (char**)realloc(parser->declared_functions, new_capacity * sizeof(char*));
        if (!new_functions) return;
        parser->declared_functions = new_functions;
        parser->declared_function_capacity = new_capacity;
    }


    parser->declared_functions[parser->declared_function_count++] = strdup(func_name);
}


static int parser_is_declared_function(Parser* parser, const char* identifier) {
    if (!parser || !identifier) return 0;

    for (int i = 0; i < parser->declared_function_count; i++) {
        if (strcmp(parser->declared_functions[i], identifier) == 0) {
            return 1;
        }
    }
    return 0;
}

Parser* parser_create(Lexer* lexer) {
    #ifdef DEBUG
    #ifdef ES_DEBUG_VERBOSE
    ES_PARSER_DEBUG("ENTERING FUNCTION");
    ES_COMPILER_DEBUG("parser_parse called");
    fflush(stderr);
    #endif
    #endif

    if (!lexer) {
        #ifdef DEBUG
        #ifdef ES_DEBUG_VERBOSE
        ES_PARSER_DEBUG("lexer is NULL!");
        #endif
        #endif
        return NULL;
    }

    Parser* parser = ES_MALLOC(sizeof(Parser));
    if (!parser) {
#ifdef DEBUG
        ES_PARSER_DEBUG("malloc failed!");
#endif
        return NULL;
    }

    parser->lexer = lexer;
    parser->current_token = lexer_next_token(lexer);
    parser->declared_functions = NULL;
    parser->declared_function_count = 0;
    parser->declared_function_capacity = 0;

    #ifdef DEBUG
    ES_PARSER_DEBUG("first token: %s (value: '%s')",
                   token_type_to_string(parser->current_token.type),
                   parser->current_token.value ? parser->current_token.value : "NULL");

    ES_PARSER_DEBUG("returning successfully");
    #endif

    return parser;
}

void parser_destroy(Parser* parser) {
    if (!parser) return;


    if (parser->declared_functions) {
        for (int i = 0; i < parser->declared_function_count; i++) {
            ES_FREE(parser->declared_functions[i]);
        }
        ES_FREE(parser->declared_functions);
    }

    ES_FREE(parser);
}

static int parser_is_type_keyword(EsTokenType type) {
    switch (type) {
        case TOKEN_INT8:
        case TOKEN_INT16:
        case TOKEN_INT32:
        case TOKEN_INT64:
        case TOKEN_UINT8:
        case TOKEN_UINT16:
        case TOKEN_UINT32:
        case TOKEN_UINT64:
        case TOKEN_FLOAT32:
        case TOKEN_FLOAT64:
        case TOKEN_BOOL:
        case TOKEN_STRING:
        case TOKEN_CHAR:
        case TOKEN_VOID:
            return 1;
        default:
            return 0;
    }
}

static void parser_free_parameter_list(char** parameters,
                                       EsTokenType* parameter_types,
                                       int parameter_count) {
    if (parameters) {
        for (int i = 0; i < parameter_count; i++) {
            if (parameters[i]) {
                ES_FREE(parameters[i]);
            }
        }
        ES_FREE(parameters);
    }
    if (parameter_types) {
        ES_FREE(parameter_types);
    }
}

static int parser_parse_parameter_list(Parser* parser,
                                       char*** parameters,
                                       EsTokenType** parameter_types,
                                       int* parameter_count) {
    if (!parameters || !parameter_types || !parameter_count) {
        return 0;
    }

    *parameters = NULL;
    *parameter_types = NULL;
    *parameter_count = 0;

    #ifdef DEBUG
    ES_PARSER_DEBUG("ENTERING parameter list parsing, current token: %s (value: '%s')",
                   token_type_to_string(parser->current_token.type),
                   parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
        #ifdef DEBUG
        ES_PARSER_DEBUG("Empty parameter list detected");
        #endif
        return 1;
    }

    while (1) {
        #ifdef DEBUG
        ES_PARSER_DEBUG("Parsing parameter %d, current token: %s (value: '%s')",
                       *parameter_count + 1,
                       token_type_to_string(parser->current_token.type),
                       parser->current_token.value ? parser->current_token.value : "NULL");
        #endif

        EsTokenType param_type = TOKEN_UNKNOWN;

        if (parser_is_type_keyword(parser->current_token.type)) {
            param_type = parser->current_token.type;
            #ifdef DEBUG
            ES_PARSER_DEBUG("Found parameter type: %s", token_type_to_string(param_type));
            #endif
            parser_advance(parser);
        }

        if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
            ES_PARSER_ERROR("Expected parameter name, got: %s", token_type_to_string(parser->current_token.type));
#endif
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_count = 0;
            return 0;
        }

        char* param_name = accelerator_strdup(parser->current_token.value);
        if (!param_name) {
#ifdef DEBUG
            ES_PARSER_ERROR("Memory allocation failed for parameter name");
#endif
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_count = 0;
            return 0;
        }

        char** new_parameters = realloc(*parameters, (*parameter_count + 1) * sizeof(char*));
        EsTokenType* new_param_types = realloc(*parameter_types, (*parameter_count + 1) * sizeof(EsTokenType));
        if (!new_parameters || !new_param_types) {
#ifdef DEBUG
            ES_PARSER_ERROR("Memory allocation failed while parsing parameters");
#endif
            ES_FREE(param_name);
            if (new_parameters) {
                *parameters = new_parameters;
            }
            if (new_param_types) {
                *parameter_types = new_param_types;
            }
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_count = 0;
            return 0;
        }

        *parameters = new_parameters;
        *parameter_types = new_param_types;
        (*parameters)[*parameter_count] = param_name;
        (*parameter_types)[*parameter_count] = param_type;
        (*parameter_count)++;

        parser_advance(parser);


        if (parser->current_token.type == TOKEN_COLON) {
            parser_advance(parser);
            if (!parser_is_type_keyword(parser->current_token.type)) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected type after ':' in parameter declaration");
#endif
                parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
                return 0;
            }

            (*parameter_types)[*parameter_count - 1] = parser->current_token.type;
            parser_advance(parser);
        }

        if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
            break;
        }

        if (parser->current_token.type != TOKEN_COMMA) {
            #ifdef DEBUG
            ES_PARSER_ERROR("Expected ',' or ')' in parameter list, got: %s (value: '%s')",
                           token_type_to_string(parser->current_token.type),
                           parser->current_token.value ? parser->current_token.value : "NULL");
            #endif
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
            return 0;
        }

        parser_advance(parser);
    }

    return 1;
}

static ASTNode* parser_create_function_node(Parser* parser,
                                            int is_static,
                                            char* name,
                                            char** parameters,
                                            EsTokenType* parameter_types,
                                            int parameter_count,
                                            ASTNode* body,
                                            EsTokenType return_type) {
    ASTNode* node = ast_create_node(is_static ? AST_STATIC_FUNCTION_DECLARATION
                                              : AST_FUNCTION_DECLARATION);
    if (!node) {
        return NULL;
    }

    if (is_static) {
        node->data.static_function_decl.name = name;
        node->data.static_function_decl.parameters = parameters;
        node->data.static_function_decl.parameter_types = parameter_types;
        node->data.static_function_decl.parameter_count = parameter_count;
        node->data.static_function_decl.body = body;
        node->data.static_function_decl.return_type = return_type;
    } else {
        node->data.function_decl.name = name;
        node->data.function_decl.parameters = parameters;
        node->data.function_decl.parameter_types = parameter_types;
        node->data.function_decl.parameter_count = parameter_count;
        node->data.function_decl.body = body;
        node->data.function_decl.return_type = return_type;
    }

    parser_add_declared_function(parser, name);

    return node;
}

static ASTNode* parser_parse_call(Parser* parser, ASTNode* callee) {
    #ifdef DEBUG
    ES_PARSER_DEBUG("current token before skipping '(': %s (value: '%s')",
                   token_type_to_string(parser->current_token.type),
                   parser->current_token.value ? parser->current_token.value : "NULL");
    #endif
    parser_advance(parser);
    #ifdef DEBUG
    ES_PARSER_DEBUG("current token after skipping '(': %s (value: '%s')",
                   token_type_to_string(parser->current_token.type),
                   parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    ASTNode** arguments = NULL;
    char** argument_names = NULL;
    int argument_count = 0;

    #ifdef DEBUG
    ES_PARSER_DEBUG("checking for arguments, current token: %s (value: '%s')",
                   token_type_to_string(parser->current_token.type),
                   parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        #ifdef DEBUG
        ES_PARSER_DEBUG("found arguments, starting parsing loop");
        #endif
        while (1) {

            char* param_name = NULL;
            ASTNode* arg = NULL;

            #ifdef DEBUG
            ES_PARSER_DEBUG("parsing argument %d, current token: %s (value: '%s')",
                    argument_count + 1, token_type_to_string(parser->current_token.type),
                    parser->current_token.value ? parser->current_token.value : "NULL");
            #endif

            if (parser->current_token.type == TOKEN_IDENTIFIER) {

                Token next = lexer_peek_token(parser->lexer);
                if (next.type == TOKEN_COLON) {

                    param_name = accelerator_strdup(parser->current_token.value);
                    parser_advance(parser);
                    parser_advance(parser);
                    arg = parser_parse_expression(parser);
                    if (!arg) {
                        ES_FREE(param_name);
                        for (int i = 0; i < argument_count; i++) {
                            ast_destroy(arguments[i]);
                            if (argument_names && argument_names[i]) ES_FREE(argument_names[i]);
                        }
                        ES_FREE(arguments);
                        ES_FREE(argument_names);
                        ast_destroy(callee);
                        return NULL;
                    }
                } else {

                    arg = parser_parse_expression(parser);
                    if (!arg) {
                        for (int i = 0; i < argument_count; i++) {
                            ast_destroy(arguments[i]);
                            if (argument_names && argument_names[i]) ES_FREE(argument_names[i]);
                        }
                        ES_FREE(arguments);
                        ES_FREE(argument_names);
                        ast_destroy(callee);
                        return NULL;
                    }
                }
            } else {

                arg = parser_parse_expression(parser);
                if (!arg) {
                    for (int i = 0; i < argument_count; i++) {
                            ast_destroy(arguments[i]);
                            if (argument_names && argument_names[i]) ES_FREE(argument_names[i]);
                        }
                        ES_FREE(arguments);
                        ES_FREE(argument_names);
                    ast_destroy(callee);
                    return NULL;
                }
            }

            ASTNode** new_arguments = realloc(arguments, (argument_count + 1) * sizeof(ASTNode*));
            char** new_argument_names = realloc(argument_names, (argument_count + 1) * sizeof(char*));
            if (!new_arguments || !new_argument_names) {
#ifdef DEBUG
                ES_PARSER_ERROR("Memory allocation failed in parser_parse_call");
#endif
                ES_FREE(param_name);
                ast_destroy(arg);
                for (int i = 0; i < argument_count; i++) {
                    ast_destroy(arguments[i]);
                    if (argument_names && argument_names[i]) ES_FREE(argument_names[i]);
                }
                ES_FREE(arguments);
                ES_FREE(argument_names);
                ast_destroy(callee);
                return NULL;
            }
            arguments = new_arguments;
            argument_names = new_argument_names;
            arguments[argument_count] = arg;
            argument_names[argument_count] = param_name;
            argument_count++;

            #ifdef DEBUG
            ES_PARSER_DEBUG("finished parsing argument %d, current token: %s (value: '%s')",
                    argument_count, token_type_to_string(parser->current_token.type),
                    parser->current_token.value ? parser->current_token.value : "NULL");
            #endif

            if (parser->current_token.type == TOKEN_RIGHT_PAREN) break;
            if (parser->current_token.type != TOKEN_COMMA) {
                #ifdef DEBUG
                ES_PARSER_ERROR("Expected ',' or ')'");
                #endif
                for (int i = 0; i < argument_count; i++) {
                    ast_destroy(arguments[i]);
                    if (argument_names && argument_names[i]) ES_FREE(argument_names[i]);
                }
                ES_FREE(arguments);
                ES_FREE(argument_names);
                ast_destroy(callee);
                return NULL;
            }
            parser_advance(parser);
        }
    } else {
        #ifdef DEBUG
        ES_PARSER_DEBUG("no arguments found (right paren detected)");
        #endif
    }

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        #ifdef DEBUG
        ES_PARSER_ERROR("Expected ')'");
        #endif
        for (int i = 0; i < argument_count; i++) {
            ast_destroy(arguments[i]);
            if (argument_names && argument_names[i]) ES_FREE(argument_names[i]);
        }
        ES_FREE(arguments);
        ES_FREE(argument_names);
        ast_destroy(callee);
        return NULL;
    }
    #ifdef DEBUG
    ES_PARSER_DEBUG("about to skip ')', current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif
    parser_advance(parser);

    ASTNode* node = ast_create_node(AST_CALL);


    if (callee->type == AST_IDENTIFIER) {


        #ifdef DEBUG
        ES_PARSER_DEBUG("treating identifier call as function call: '%s'", callee->data.identifier_name);
        #endif
        node->data.call.name = accelerator_strdup(callee->data.identifier_name);
        node->data.call.object = NULL;
        ast_destroy(callee);
    } else if (callee->type == AST_MEMBER_ACCESS) {
        #ifdef DEBUG
        ES_PARSER_DEBUG("member access call: '%s' on object", callee->data.member_access.member_name);
        #endif
        node->data.call.name = accelerator_strdup(callee->data.member_access.member_name);
        ASTNode* object = callee->data.member_access.object;
        callee->data.member_access.object = NULL;
        node->data.call.object = object;
        ast_destroy(callee);
    } else {
        #ifdef DEBUG
        ES_PARSER_DEBUG("expression call with __expr_call__");
        #endif


        node->data.call.name = accelerator_strdup("__expr_call__");
        node->data.call.object = callee;
    }

    node->data.call.arguments = arguments;
    node->data.call.argument_count = argument_count;
    node->data.call.argument_names = argument_names;

    return node;
}

static ASTNode* parser_parse_console_writeline(Parser* parser) {
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_DOT) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '.' after console");
#endif
        return NULL;
    }
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected method name after console");
#endif
        return NULL;
    }


    if (strcmp(parser->current_token.value, "WriteLine") != 0 && strcmp(parser->current_token.value, "log") != 0) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected 'WriteLine' or 'log' after console");
#endif
        return NULL;
    }
    parser_advance(parser);


    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after 'Console.WriteLine'");
#endif
        return NULL;
    }
    parser_advance(parser);

    ASTNode** values = NULL;
    int value_count = 0;


    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {

        ASTNode* first_value = parser_parse_expression(parser);
        if (!first_value) return NULL;

        values = ES_MALLOC(sizeof(ASTNode*));
            if (!values) {
                return NULL;
            }
        if (!values) {
#ifdef DEBUG
            ES_PARSER_ERROR("Memory allocation failed in parser_parse_console_writeline");
#endif
            ast_destroy(first_value);
            return NULL;
        }
        values[0] = first_value;
        value_count = 1;


        while (parser->current_token.type == TOKEN_COMMA) {
            parser_advance(parser);

            ASTNode* next_value = parser_parse_expression(parser);
            if (!next_value) {

                for (int i = 0; i < value_count; i++) {
                    ast_destroy(values[i]);
                }
                free(values);
                return NULL;
            }


            ASTNode** new_values = realloc(values, (value_count + 1) * sizeof(ASTNode*));
            if (!new_values) {
#ifdef DEBUG
                ES_PARSER_ERROR("Memory allocation failed in parser_parse_console_writeline");
#endif
                ast_destroy(next_value);
                for (int i = 0; i < value_count; i++) {
                    ast_destroy(values[i]);
                }
                ES_FREE(values);
                return NULL;
            }
            values = new_values;
            values[value_count] = next_value;
            value_count++;
        }
    }


    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')'");
#endif

        for (int i = 0; i < value_count; i++) {
            ast_destroy(values[i]);
        }
        ES_FREE(values);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* node = ast_create_node(AST_PRINT_STATEMENT);
    if (!node) {
        for (int i = 0; i < value_count; i++) {
            ast_destroy(values[i]);
        }
        ES_FREE(values);
        return NULL;
    }

    node->data.print_stmt.values = values;
    node->data.print_stmt.value_count = value_count;

    return node;
}

static ASTNode* parser_parse_namespace_declaration(Parser* parser) {
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected namespace name");
#endif
        return NULL;
    }

    char* name = accelerator_strdup(parser->current_token.value);
    parser_advance(parser);


    if (parser->current_token.type == TOKEN_COLON) {
#ifdef DEBUG
        ES_PARSER_ERROR("Unexpected ':' in namespace declaration");
#endif
        ES_FREE(name);
        return NULL;
    }

    ASTNode* body = parser_parse_block(parser);
    if (!body) {
        ES_FREE(name);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_NAMESPACE_DECLARATION);
    node->data.namespace_decl.name = name;
    node->data.namespace_decl.body = body;

    return node;
}

static ASTNode* parser_parse_class_declaration(Parser* parser) {
    #ifdef DEBUG
    ES_PARSER_DEBUG("Entering parser_parse_class_declaration");
    #endif
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected class name");
#endif
        return NULL;
    }

    char* name = accelerator_strdup(parser->current_token.value);
    parser_advance(parser);


    ASTNode* base_class = NULL;
    if (parser->current_token.type == TOKEN_COLON) {
        parser_advance(parser);

        if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
            ES_PARSER_ERROR("Expected base class name after ':'");
#endif
            ES_FREE(name);
            return NULL;
        }


        base_class = ast_create_node(AST_IDENTIFIER);
        base_class->data.identifier_name = accelerator_strdup(parser->current_token.value);
        parser_advance(parser);
    }

    ASTNode* body = parser_parse_block(parser);
    if (!body) {
        ES_FREE(name);
        if (base_class) ast_destroy(base_class);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_CLASS_DECLARATION);
    node->data.class_decl.name = name;
    node->data.class_decl.body = body;
    node->data.class_decl.base_class = base_class;

    #ifdef DEBUG
    ES_PARSER_DEBUG("Created class declaration node: %p, name='%s'", node, name);
    #endif
    return node;
}

static ASTNode* parser_parse_primary(Parser* parser) {
    #ifdef DEBUG
    ES_PARSER_DEBUG("current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    ASTNode* node = NULL;

    switch (parser->current_token.type) {
        case TOKEN_NUMBER: {
            node = ast_create_node(AST_NUMBER);
            node->data.number_value = atof(parser->current_token.value);
            parser_advance(parser);
            break;
        }
        case TOKEN_STRING: {
            node = ast_create_node(AST_STRING);
            node->data.string_value = accelerator_strdup(parser->current_token.value);
            parser_advance(parser);
            break;
        }
        case TOKEN_TRUE: {
            node = ast_create_node(AST_BOOLEAN);
            node->data.boolean_value = 1;
            parser_advance(parser);
            break;
        }
        case TOKEN_FALSE: {
            node = ast_create_node(AST_BOOLEAN);
            node->data.boolean_value = 0;
            parser_advance(parser);
            break;
        }
        case TOKEN_IDENTIFIER: {
            #ifdef DEBUG
            ES_PARSER_DEBUG("processing IDENTIFIER: '%s'", parser->current_token.value);
            #endif
            node = ast_create_node(AST_IDENTIFIER);
            node->data.identifier_name = accelerator_strdup(parser->current_token.value);
            parser_advance(parser);
            #ifdef DEBUG
            ES_PARSER_DEBUG("after advancing from IDENTIFIER, current token: %s (value: '%s')",
                token_type_to_string(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "NULL");
            #endif



            break;
        }
        case TOKEN_LEFT_PAREN: {
            parser_advance(parser);


            if (parser_is_type_keyword(parser->current_token.type)) {
                #ifdef DEBUG
                ES_PARSER_DEBUG("检测到类型转换，类型: %s", token_type_to_string(parser->current_token.type));
                #endif

                EsTokenType cast_type = parser->current_token.type;
                parser_advance(parser);

                if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                    #ifdef DEBUG
                    ES_PARSER_ERROR("类型转换中期望 ')'");
                    #endif
                    return NULL;
                }
                parser_advance(parser);


                ASTNode* operand = parser_parse_primary(parser);
                if (!operand) {
                    return NULL;
                }


                node = ast_create_node(AST_UNARY_OPERATION);
                node->data.unary_op.operator = cast_type;
                node->data.unary_op.operand = operand;
                node->data.unary_op.is_postfix = false;

                #ifdef DEBUG
                ES_PARSER_DEBUG("成功创建类型转换节点");
                #endif
                break;
            }


            node = parser_parse_expression(parser);
            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                #ifdef DEBUG
                ES_PARSER_ERROR("Expected ')'");
                #endif
                ast_destroy(node);
                return NULL;
            }
            parser_advance(parser);
            break;
        }
        case TOKEN_LEFT_BRACKET: {
            parser_advance(parser);

            ASTNode** elements = NULL;
            int element_count = 0;

            if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
                while (1) {
                    ASTNode* element = parser_parse_expression(parser);
                    if (!element) {
                        for (int i = 0; i < element_count; i++) {
                            ast_destroy(elements[i]);
                        }
                        ES_FREE(elements);
                        return NULL;
                    }

                    ASTNode** new_elements = realloc(elements, (element_count + 1) * sizeof(ASTNode*));
        if (!new_elements) {
            for (int i = 0; i < element_count; i++) {
                ast_destroy(elements[i]);
            }
            ES_FREE(elements);
        return NULL;
    }
        elements = new_elements;
                    elements[element_count] = element;
                    element_count++;

                    if (parser->current_token.type == TOKEN_RIGHT_BRACKET) break;
                    if (parser->current_token.type != TOKEN_COMMA) {
                        #ifdef DEBUG
                        ES_PARSER_ERROR("Expected ',' or ']'");
                        #endif
                        for (int i = 0; i < element_count; i++) {
                            ast_destroy(elements[i]);
                        }
                        free(elements);
                        return NULL;
                    }
                    parser_advance(parser);
                }
            }

            if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
                #ifdef DEBUG
                ES_PARSER_ERROR("Expected ']'");
                #endif
                for (int i = 0; i < element_count; i++) {
                    ast_destroy(elements[i]);
                }
                ES_FREE(elements);
                return NULL;
            }
            parser_advance(parser);

            node = ast_create_node(AST_ARRAY_LITERAL);
            node->data.array_literal.elements = elements;
            node->data.array_literal.element_count = element_count;
            break;
        }
        case TOKEN_NEW:
            return parser_parse_new_expression(parser);
        case TOKEN_THIS:
            parser_advance(parser);
            return ast_create_node(AST_THIS);
        case TOKEN_TEMPLATE:

            #ifdef DEBUG
            ES_PARSER_DEBUG("TOKEN_TEMPLATE found in expression context, returning NULL");
            #endif
            return NULL;

        default:
            #ifdef DEBUG
            ES_DEBUG_LOG("PARSER", "=== ENTERING DEFAULT CASE ===");
            ES_DEBUG_LOG("PARSER", "UNEXPECTED TOKEN: %s (value: '%s')",
               token_type_to_string(parser->current_token.type),
               parser->current_token.value ? parser->current_token.value : "NULL");
            ES_DEBUG_LOG("PARSER", "Unexpected token: %s (value: '%s')",
                    token_type_to_string(parser->current_token.type),
                    parser->current_token.value ? parser->current_token.value : "NULL");
            #endif
            return NULL;
    }

    return node;
}

static ASTNode* parser_parse_binary_operation(Parser* parser, int precedence) {
    #ifdef DEBUG
    ES_PARSER_DEBUG("starting, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    ASTNode* left = parser_parse_postfix_expression(parser);
    if (!left) return NULL;

    #ifdef DEBUG
    ES_PARSER_DEBUG("after parser_parse_postfix_expression, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif


    if (parser->current_token.type == TOKEN_COLON) {
        #ifdef DEBUG
        ES_PARSER_DEBUG("found COLON token, returning left for ternary operation");
        #endif
        return left;
    }



    while (parser->current_token.type == TOKEN_PLUS ||
           parser->current_token.type == TOKEN_MINUS ||
           parser->current_token.type == TOKEN_MULTIPLY ||
           parser->current_token.type == TOKEN_DIVIDE ||
           parser->current_token.type == TOKEN_MODULO ||
           parser->current_token.type == TOKEN_EQUAL ||
           parser->current_token.type == TOKEN_NOT_EQUAL ||
           parser->current_token.type == TOKEN_LESS ||
           parser->current_token.type == TOKEN_GREATER ||
           parser->current_token.type == TOKEN_LESS_EQUAL ||
           parser->current_token.type == TOKEN_GREATER_EQUAL) {

        #ifdef DEBUG
        ES_PARSER_DEBUG("found binary operator: %s",
                token_type_to_string(parser->current_token.type));
        #endif

        EsTokenType operator = parser->current_token.type;
        parser_advance(parser);

        ASTNode* right = parser_parse_postfix_expression(parser);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }



        if (operator == TOKEN_GREATER ||
            operator == TOKEN_LESS ||
            operator == TOKEN_EQUAL ||
            operator == TOKEN_NOT_EQUAL ||
            operator == TOKEN_LESS_EQUAL ||
            operator == TOKEN_GREATER_EQUAL) {

            #ifdef DEBUG
            ES_PARSER_DEBUG("checking for ternary operator after binary expression %s",
                    token_type_to_string(operator));
            ES_PARSER_DEBUG("current token after parsing right: %s",
                    token_type_to_string(parser->current_token.type));

            if (parser->current_token.type == TOKEN_QUESTION) {
                ES_PARSER_DEBUG("found ternary operator pattern, returning left");

                ast_destroy(right);
                return left;
            }
            ES_PARSER_DEBUG("not a ternary operator, continuing with binary operation");
            #endif
        }

        ASTNode* new_node = ast_create_node(AST_BINARY_OPERATION);
        new_node->data.binary_op.left = left;
        new_node->data.binary_op.operator = operator;
        new_node->data.binary_op.right = right;

        left = new_node;
    }

    #ifdef DEBUG
    ES_PARSER_DEBUG("returning, current token: %s (value: '%s')",
           token_type_to_string(parser->current_token.type),
           parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    return left;
}

static ASTNode* parser_parse_postfix_expression(Parser* parser) {
    #ifdef DEBUG
    ES_PARSER_DEBUG("starting, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    ASTNode* left = parser_parse_primary(parser);
    if (!left) {
        #ifdef DEBUG
        ES_PARSER_DEBUG("parser_parse_primary returned NULL");
        #endif
        return NULL;
    }

    #ifdef DEBUG
    ES_PARSER_DEBUG("parser_parse_primary succeeded, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif


    while (parser->current_token.type == TOKEN_DOT || parser->current_token.type == TOKEN_DOUBLE_COLON || parser->current_token.type == TOKEN_LEFT_BRACKET) {

        if (parser->current_token.type == TOKEN_LEFT_BRACKET) {
            #ifdef DEBUG
            ES_PARSER_DEBUG("Found LEFT_BRACKET in postfix expression, processing array access");
            #endif
            parser_advance(parser);

            ASTNode* index = parser_parse_expression(parser);
            if (!index) {
                ast_destroy(left);
                return NULL;
            }

            if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected ']' after array index");
#endif
                ast_destroy(left);
                ast_destroy(index);
                return NULL;
            }
            parser_advance(parser);

            ASTNode* array_access = ast_create_node(AST_ARRAY_ACCESS);
            if (array_access) {
                array_access->data.array_access.array = left;
                array_access->data.array_access.index = index;
                left = array_access;
                #ifdef DEBUG
                ES_PARSER_DEBUG("Successfully created AST_ARRAY_ACCESS node");
                #endif
            } else {
                #ifdef DEBUG
                ES_PARSER_DEBUG("Failed to create AST_ARRAY_ACCESS node");
                #endif
                ast_destroy(left);
                ast_destroy(index);
                return NULL;
            }
        } else if (parser->current_token.type == TOKEN_DOT) {
            #ifdef DEBUG
            ES_PARSER_DEBUG("found DOT, advancing");
            #endif
            parser_advance(parser);

            if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected member name after '.'");
#endif
                ast_destroy(left);
                return NULL;
            }

            char* member_name = accelerator_strdup(parser->current_token.value);
            #ifdef DEBUG
            ES_PARSER_DEBUG("member name: '%s'", member_name);
            #endif
            parser_advance(parser);

            ASTNode* node = ast_create_node(AST_MEMBER_ACCESS);
            node->data.member_access.object = left;
            node->data.member_access.member_name = member_name;

            left = node;

            if (parser->current_token.type == TOKEN_LEFT_PAREN) {
                #ifdef DEBUG
                ES_PARSER_DEBUG("found LEFT_PAREN, calling parser_parse_call");
                #endif
                ASTNode* call_node = parser_parse_call(parser, left);
                if (!call_node) {
                    #ifdef DEBUG
                    ES_PARSER_DEBUG("parser_parse_call returned NULL");
                    #endif
                    return NULL;
                }
                left = call_node;
                #ifdef DEBUG
                ES_PARSER_DEBUG("after parser_parse_call, current token: %s (value: '%s')",
                        token_type_to_string(parser->current_token.type),
                        parser->current_token.value ? parser->current_token.value : "NULL");
                ES_PARSER_DEBUG("about to continue while loop, checking for more dots or double colons");
                #endif
            }
        } else if (parser->current_token.type == TOKEN_DOUBLE_COLON) {
            #ifdef DEBUG
            ES_PARSER_DEBUG("found DOUBLE_COLON, advancing");
            #endif
            parser_advance(parser);

            if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected identifier after '::'");
#endif
                ast_destroy(left);
                return NULL;
            }

            char* method_name = accelerator_strdup(parser->current_token.value);
            parser_advance(parser);

            if (left->type != AST_IDENTIFIER) {
                #ifdef DEBUG
                ES_PARSER_ERROR("Left side of '::' must be a class identifier");
                #endif
                ES_FREE(method_name);
                ast_destroy(left);
                return NULL;
            }
            char* class_name = accelerator_strdup(left->data.identifier_name);

            if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected '(' after static method name");
#endif
                ES_FREE(class_name);
                ES_FREE(method_name);
                ast_destroy(left);
                return NULL;
            }

            parser_advance(parser);

            ASTNode** arguments = NULL;
            int argument_count = 0;

            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                while (1) {
                    ASTNode* arg = parser_parse_expression(parser);
                    if (!arg) {
                        for (int i = 0; i < argument_count; i++) {
                            ast_destroy(arguments[i]);
                        }
                        ES_FREE(arguments);
                        ES_FREE(class_name);
                        ES_FREE(method_name);
                        ast_destroy(left);
                        return NULL;
                    }
                    arguments = (ASTNode**)realloc(arguments, (argument_count + 1) * sizeof(ASTNode*));
                    arguments[argument_count++] = arg;

                    if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                        break;
                    }
                    if (parser->current_token.type != TOKEN_COMMA) {
                        #ifdef DEBUG
                        ES_PARSER_ERROR("Expected ',' or ')' in argument list");
                        #endif
                        for (int i = 0; i < argument_count; i++) ast_destroy(arguments[i]);
                        ES_FREE(arguments);
                        ES_FREE(class_name);
                        ES_FREE(method_name);
                        ast_destroy(left);
                        return NULL;
                    }
                    parser_advance(parser);
                }
            }

            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected ')' after argument list");
#endif
                for (int i = 0; i < argument_count; i++) ast_destroy(arguments[i]);
                ES_FREE(arguments);
                ES_FREE(class_name);
                ES_FREE(method_name);
                ast_destroy(left);
                return NULL;
            }
            parser_advance(parser);

            ASTNode* static_call_node = ast_create_node(AST_STATIC_METHOD_CALL);
            static_call_node->data.static_call.class_name = class_name;
            static_call_node->data.static_call.method_name = method_name;
            static_call_node->data.static_call.arguments = arguments;
            static_call_node->data.static_call.argument_count = argument_count;

            ast_destroy(left);
            left = static_call_node;
        }

        #ifdef DEBUG
        ES_PARSER_DEBUG("end of while loop iteration, current token: %s (value: '%s')",
                token_type_to_string(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "NULL");
        #endif
    }


    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        #ifdef DEBUG
        ES_PARSER_DEBUG("found LEFT_PAREN after expression, calling parser_parse_call");
        ES_PARSER_DEBUG("callee type: %d, callee value: '%s'",
                left->type,
                left->type == AST_IDENTIFIER ? left->data.identifier_name : "not identifier");
        #endif
        ASTNode* call_node = parser_parse_call(parser, left);
        if (!call_node) {
            #ifdef DEBUG
            ES_PARSER_DEBUG("parser_parse_call returned NULL");
            #endif
            return NULL;
        }
        left = call_node;
        #ifdef DEBUG
        ES_PARSER_DEBUG("after parser_parse_call, current token: %s (value: '%s')",
                token_type_to_string(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "NULL");
        #endif
    }


    if (parser->current_token.type == TOKEN_INCREMENT) {
        parser_advance(parser);
        ASTNode* node = ast_create_node(AST_UNARY_OPERATION);
        node->data.unary_op.operator = TOKEN_INCREMENT;
        node->data.unary_op.operand = left;
        node->data.unary_op.is_postfix = true;
        return node;
    }

    #ifdef DEBUG
    ES_PARSER_DEBUG("returning, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    return left;
}

static ASTNode* parser_parse_ternary_operation(Parser* parser) {
    #ifdef DEBUG
    ES_PARSER_DEBUG("starting, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif


    ASTNode* condition = parser_parse_binary_operation(parser, 0);
    if (!condition) return NULL;

    #ifdef DEBUG
    ES_PARSER_DEBUG("after parser_parse_binary_operation, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif


    if (parser->current_token.type == TOKEN_QUESTION) {
        #ifdef DEBUG
        ES_PARSER_DEBUG("found QUESTION token, processing ternary operator");
        #endif

        parser_advance(parser);

        ASTNode* true_value = parser_parse_binary_operation(parser, 0);
        if (!true_value) {
            ast_destroy(condition);
            return NULL;
        }


        if (parser->current_token.type != TOKEN_COLON) {
            fprintf(stderr, "Expected ':' in ternary operator\n");
            fflush(stderr);
            ast_destroy(condition);
            ast_destroy(true_value);
            return NULL;
        }
        parser_advance(parser);

        ASTNode* false_value = parser_parse_binary_operation(parser, 0);
        if (!false_value) {
            ast_destroy(condition);
            ast_destroy(true_value);
            return NULL;
        }

        ASTNode* node = ast_create_node(AST_TERNARY_OPERATION);
        node->data.ternary_op.condition = condition;
        node->data.ternary_op.true_value = true_value;
        node->data.ternary_op.false_value = false_value;

        #ifdef DEBUG
        ES_PARSER_DEBUG("successfully created AST_TERNARY_OPERATION node");
        #endif

        return node;
    }

    #ifdef DEBUG
    ES_PARSER_DEBUG("no QUESTION token found, returning condition");
    #endif

    return condition;
}

static ASTNode* parser_parse_expression(Parser* parser) {
    #ifdef DEBUG
    ES_PARSER_DEBUG("starting, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    ASTNode* result = parser_parse_ternary_operation(parser);

    #ifdef DEBUG
    ES_PARSER_DEBUG("returning, current token: %s (value: '%s')",
           token_type_to_string(parser->current_token.type),
           parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    return result;
}

static ASTNode* parser_parse_variable_declaration(Parser* parser) {
    #ifdef DEBUG
    ES_PARSER_DEBUG("starting, current token: %s (value: '%s')",
           token_type_to_string(parser->current_token.type),
           parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    parser_advance(parser);

    #ifdef DEBUG
    ES_PARSER_DEBUG("after skipping 'var', current token: %s (value: '%s')",
           token_type_to_string(parser->current_token.type),
           parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        #ifdef DEBUG
        ES_PARSER_ERROR("Expected identifier after 'var'");
        #endif
        return NULL;
    }

    char* name = accelerator_strdup(parser->current_token.value);
    parser_advance(parser);

    #ifdef DEBUG
    ES_PARSER_DEBUG("after identifier '%s', current token: %s (value: '%s')",
           name,
           token_type_to_string(parser->current_token.type),
           parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    ASTNode* array_size = NULL;
    bool is_array = false;


    if (parser->current_token.type == TOKEN_LEFT_BRACKET) {
        parser_advance(parser);
        is_array = true;


        if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
            array_size = parser_parse_expression(parser);
            if (!array_size) {
                ES_FREE(name);
                return NULL;
            }
        } else {

            array_size = NULL;
        }

        if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
            #ifdef DEBUG
            ES_PARSER_ERROR("Expected ']' after array size");
            #endif
            ES_FREE(name);
            if (array_size) ast_destroy(array_size);
            return NULL;
        }
        parser_advance(parser);
    }

    ASTNode* value = NULL;
    if (parser->current_token.type == TOKEN_ASSIGN) {
        parser_advance(parser);

        #ifdef DEBUG
        ES_PARSER_DEBUG("after '=', current token: %s (value: '%s')",
               token_type_to_string(parser->current_token.type),
               parser->current_token.value ? parser->current_token.value : "NULL");
        #endif

        value = parser_parse_expression(parser);
        if (!value) {
            ES_FREE(name);
            if (array_size) ast_destroy(array_size);
            return NULL;
        }
    }

    ASTNode* node = ast_create_node(AST_VARIABLE_DECLARATION);
    if (!node) {
        ES_FREE(name);
        if (value) ast_destroy(value);
        if (array_size) ast_destroy(array_size);
        return NULL;
    }

    node->data.variable_decl.name = name;
    node->data.variable_decl.value = value;
    node->data.variable_decl.array_size = array_size;
    node->data.variable_decl.is_array = is_array;

    #ifdef DEBUG
    ES_PARSER_DEBUG("returning, current token: %s (value: '%s')",
           token_type_to_string(parser->current_token.type),
           parser->current_token.value ? parser->current_token.value : "NULL");
    #endif

    return node;
}

static ASTNode* parser_parse_static_member_declaration(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_STATIC) {
        #ifdef DEBUG
        ES_PARSER_ERROR("Expected 'static' keyword");
        #endif
        return NULL;
    }

    parser_advance(parser);


    if (parser->current_token.type == TOKEN_FUNCTION) {
        parser_advance(parser);

        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            #ifdef DEBUG
            ES_PARSER_ERROR("Expected function name after 'static function'");
            #endif
            return NULL;
        }

        char* name = accelerator_strdup(parser->current_token.value);
        if (!name) {
            ES_PARSER_ERROR("Memory allocation failed for function name");
            return NULL;
        }

        parser_advance(parser);

        if (parser->current_token.type != TOKEN_LEFT_PAREN) {
            #ifdef DEBUG
            ES_PARSER_ERROR("Expected '(' after static function name");
            #endif
            ES_FREE(name);
            return NULL;
        }
        parser_advance(parser);

        char** parameters = NULL;
        EsTokenType* parameter_types = NULL;
        int parameter_count = 0;

        if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_count)) {
            ES_FREE(name);
            return NULL;
        }

        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            #ifdef DEBUG
            ES_PARSER_ERROR("Expected ')' after parameter list");
            #endif
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
            ES_FREE(name);
            return NULL;
        }
        parser_advance(parser);


        EsTokenType return_type_token = TOKEN_UNKNOWN;
        if (parser->current_token.type == TOKEN_COLON) {
            parser_advance(parser);
            if (!parser_is_type_keyword(parser->current_token.type)) {
                #ifdef DEBUG
                ES_PARSER_ERROR("Expected return type after ':' in static function declaration");
                #endif
                parser_free_parameter_list(parameters, parameter_types, parameter_count);
                ES_FREE(name);
                return NULL;
            }
            return_type_token = parser->current_token.type;
            parser_advance(parser);
        }

        ASTNode* body = parser_parse_block(parser);
        if (!body) {
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
            ES_FREE(name);
            return NULL;
        }

        ASTNode* node = parser_create_function_node(parser, 1, name,
                                                parameters,
                                                parameter_types,
                                                parameter_count,
                                                body,
                                                return_type_token);
        if (!node) {
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
            ast_destroy(body);
            ES_FREE(name);
            return NULL;
        }

        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }

        return node;
    }


    if (parser_is_type_keyword(parser->current_token.type)) {
        EsTokenType type_token = parser->current_token.type;
        parser_advance(parser);

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
            #ifdef DEBUG
            ES_PARSER_ERROR("Expected identifier after static type declaration");
            #endif
            return NULL;
        }

        char* identifier_name = accelerator_strdup(parser->current_token.value);
        if (!identifier_name) {
#ifdef DEBUG
            ES_PARSER_ERROR("Memory allocation failed for identifier");
#endif
            return NULL;
        }

        Token next_token = lexer_peek_token(parser->lexer);
        if (next_token.type == TOKEN_LEFT_PAREN) {

            parser_advance(parser);
            return parser_parse_typed_function_declaration(parser, type_token, identifier_name, 1);
        }


        parser_advance(parser);

        ASTNode* value = NULL;
        if (parser->current_token.type == TOKEN_ASSIGN) {
            parser_advance(parser);
            value = parser_parse_expression(parser);
            if (!value) {
                ES_FREE(identifier_name);
                return NULL;
            }
        }

        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }

        ASTNode* node = ast_create_node(AST_STATIC_VARIABLE_DECLARATION);
        node->data.static_variable_decl.name = identifier_name;
        node->data.static_variable_decl.value = value;
        node->data.static_variable_decl.type = type_token;
        return node;
    }


    if (parser->current_token.type == TOKEN_VAR) {
        parser_advance(parser);

        if (parser->current_token.type != TOKEN_IDENTIFIER) {
        #ifdef DEBUG
        ES_PARSER_ERROR("Expected identifier after 'static var'");
        #endif
        return NULL;
    }

    char* name = accelerator_strdup(parser->current_token.value);
        if (!name) {
#ifdef DEBUG
            ES_PARSER_ERROR("Memory allocation failed for variable name");
#endif
            return NULL;
        }

    parser_advance(parser);

    ASTNode* value = NULL;
    if (parser->current_token.type == TOKEN_ASSIGN) {
        parser_advance(parser);
        value = parser_parse_expression(parser);
        if (!value) {
            ES_FREE(name);
            return NULL;
        }
    }

    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }

    ASTNode* node = ast_create_node(AST_STATIC_VARIABLE_DECLARATION);
    if (!node) {
        ES_FREE(name);
        if (value) ast_destroy(value);
        return NULL;
    }

    node->data.static_variable_decl.name = name;
    node->data.static_variable_decl.value = value;
        node->data.static_variable_decl.type = TOKEN_UNKNOWN;
        return node;
    }

    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        char* name = accelerator_strdup(parser->current_token.value);
        if (!name) {
#ifdef DEBUG
            ES_PARSER_ERROR("Memory allocation failed for variable name");
#endif
            return NULL;
        }

        parser_advance(parser);

        ASTNode* value = NULL;
        if (parser->current_token.type == TOKEN_ASSIGN) {
            parser_advance(parser);
            value = parser_parse_expression(parser);
            if (!value) {
                ES_FREE(name);
                return NULL;
            }
        }

        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }

        ASTNode* node = ast_create_node(AST_STATIC_VARIABLE_DECLARATION);
        if (!node) {
            ES_FREE(name);
            if (value) ast_destroy(value);
            return NULL;
        }

        node->data.static_variable_decl.name = name;
        node->data.static_variable_decl.value = value;
        node->data.static_variable_decl.type = TOKEN_UNKNOWN;
        return node;
    }

#ifdef DEBUG
    ES_PARSER_ERROR("Expected static member declaration");
#endif
    return NULL;
}

static ASTNode* parser_parse_static_variable_declaration(Parser* parser) {
    ASTNode* node = parser_parse_static_member_declaration(parser);
    if (!node) {
        return NULL;
    }
    if (node->type != AST_STATIC_VARIABLE_DECLARATION) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected static variable declaration");
#endif
        ast_destroy(node);
        return NULL;
    }
    return node;
}

static ASTNode* parser_parse_static_function_declaration(Parser* parser) {
    ASTNode* node = parser_parse_static_member_declaration(parser);
    if (!node) {
        return NULL;
    }
    if (node->type != AST_STATIC_FUNCTION_DECLARATION) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected static function declaration");
#endif
        ast_destroy(node);
        return NULL;
    }
    return node;
}

static ASTNode* parser_parse_assignment(Parser* parser) {

    ASTNode* left = parser_parse_expression(parser);
    if (!left) {
        return NULL;
    }

    if (parser->current_token.type != TOKEN_ASSIGN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '='");
#endif
        ast_destroy(left);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* value = parser_parse_expression(parser);
    if (!value) {
        ast_destroy(left);
        return NULL;
    }


    if (left->type == AST_ARRAY_ACCESS) {
        ASTNode* node = ast_create_node(AST_ARRAY_ASSIGNMENT);
        if (!node) {
            ast_destroy(left);
            ast_destroy(value);
            return NULL;
        }

        node->data.array_assignment.array = left->data.array_access.array;
        node->data.array_assignment.index = left->data.array_access.index;
        node->data.array_assignment.value = value;


        ES_FREE(left);

        return node;
    } else if (left->type == AST_IDENTIFIER) {

        ASTNode* node = ast_create_node(AST_ASSIGNMENT);
        if (!node) {
            ast_destroy(left);
            ast_destroy(value);
            return NULL;
        }

        node->data.assignment.name = accelerator_strdup(left->data.identifier_name);
        node->data.assignment.value = value;

        ast_destroy(left);

        return node;
    } else {
#ifdef DEBUG
        ES_PARSER_ERROR("Invalid left-hand side of assignment");
#endif
        ast_destroy(left);
        ast_destroy(value);
        return NULL;
    }
}

static ASTNode* parser_parse_print_statement(Parser* parser) {
    parser_advance(parser);


    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after 'print'");
#endif
        return NULL;
    }
    parser_advance(parser);

    ASTNode** values = NULL;
    int value_count = 0;


    ASTNode* first_value = parser_parse_expression(parser);
    if (!first_value) return NULL;

    values = ES_MALLOC(sizeof(ASTNode*));
    values[0] = first_value;
    value_count = 1;


    while (parser->current_token.type == TOKEN_COMMA) {
        parser_advance(parser);

        ASTNode* next_value = parser_parse_expression(parser);
        if (!next_value) {

            for (int i = 0; i < value_count; i++) {
                ast_destroy(values[i]);
            }
            ES_FREE(values);
        return NULL;
    }


        values = realloc(values, (value_count + 1) * sizeof(ASTNode*));
        values[value_count] = next_value;
        value_count++;
    }


    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')'");
#endif

        for (int i = 0; i < value_count; i++) {
            ast_destroy(values[i]);
        }
        ES_FREE(values);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* node = ast_create_node(AST_PRINT_STATEMENT);
    node->data.print_stmt.values = values;
    node->data.print_stmt.value_count = value_count;

    return node;
}

static ASTNode* parser_parse_return_statement(Parser* parser) {
    parser_advance(parser);

    ASTNode* value = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        value = parser_parse_expression(parser);
        if (!value) return NULL;
    }

    ASTNode* node = ast_create_node(AST_RETURN_STATEMENT);
    if (!node) {
        if (value) ast_destroy(value);
        return NULL;
    }

    node->data.return_stmt.value = value;

    return node;
}

static ASTNode* parser_parse_access_modifier(Parser* parser) {
    EsTokenType access_type = parser->current_token.type;
    parser_advance(parser);


    if (parser->current_token.type == TOKEN_STATIC) {
        ASTNode* member = parser_parse_static_member_declaration(parser);
        if (!member) return NULL;

        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER);
        if (!node) {
            ast_destroy(member);
            return NULL;
        }

        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    }
    else if (parser->current_token.type == TOKEN_IDENTIFIER &&
        strcmp(parser->current_token.value, "constructor") == 0) {
        ASTNode* member = parser_parse_constructor_declaration(parser);
        if (!member) return NULL;

        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER);
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    }
    else if (parser->current_token.type == TOKEN_DESTRUCTOR) {
        ASTNode* member = parser_parse_destructor_declaration(parser);
        if (!member) return NULL;

        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER);
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    }
    else if (parser_is_type_keyword(parser->current_token.type)) {
        ASTNode* member = parser_parse_type_declaration(parser);
        if (!member) return NULL;

        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER);
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    }
    else if (parser->current_token.type == TOKEN_FUNCTION) {
        ASTNode* member = parser_parse_function_declaration(parser);
        if (!member) return NULL;

        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER);
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    }
    else if (parser->current_token.type == TOKEN_VAR) {
        ASTNode* member = parser_parse_variable_declaration(parser);
        if (!member) return NULL;

        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER);
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    }
    else {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected member declaration after access modifier");
#endif
        return NULL;
    }
}

static ASTNode* parser_parse_constructor_declaration(Parser* parser) {
    parser_advance(parser);

    char** parameters = NULL;
    EsTokenType* parameter_types = NULL;
    int parameter_count = 0;


    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after 'constructor'");
#endif
        return NULL;
    }
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        while (1) {

            EsTokenType param_type = TOKEN_UNKNOWN;
            if ((parser->current_token.type >= TOKEN_INT8 && parser->current_token.type <= TOKEN_VOID) ||
                parser->current_token.type == TOKEN_STRING) {

                param_type = parser->current_token.type;
                parser_advance(parser);
            }

            if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected parameter name");
#endif
                for (int i = 0; i < parameter_count; i++) {
                    free(parameters[i]);
                }
                free(parameters);
                free(parameter_types);
                return NULL;
            }

            parameters = realloc(parameters, (parameter_count + 1) * sizeof(char*));
            parameter_types = realloc(parameter_types, (parameter_count + 1) * sizeof(EsTokenType));
            parameters[parameter_count] = accelerator_strdup(parser->current_token.value);
            parameter_types[parameter_count] = param_type;
            parameter_count++;
            parser_advance(parser);

            if (parser->current_token.type == TOKEN_RIGHT_PAREN) break;
            if (parser->current_token.type != TOKEN_COMMA) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected ',' or ')'");
#endif
                for (int i = 0; i < parameter_count; i++) {
                    free(parameters[i]);
                }
                free(parameters);
                free(parameter_types);
                return NULL;
            }
            parser_advance(parser);
        }
    }

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')'");
#endif
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        free(parameter_types);
        return NULL;
    }
    parser_advance(parser);


    ASTNode* body = parser_parse_block(parser);
    if (!body) {
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        free(parameter_types);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_CONSTRUCTOR_DECLARATION);
    node->data.constructor_decl.parameters = parameters;
    node->data.constructor_decl.parameter_types = parameter_types;
    node->data.constructor_decl.parameter_count = parameter_count;
    node->data.constructor_decl.body = body;


    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }

    return node;
}

static ASTNode* parser_parse_destructor_declaration(Parser* parser) {
    parser_advance(parser);

    char* class_name = NULL;








    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        parser_advance(parser);

        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
            ES_PARSER_ERROR("Destructor should not have parameters");
#endif
            if (class_name) free(class_name);
            return NULL;
        }
        parser_advance(parser);
    }


    ASTNode* body = parser_parse_block(parser);
    if (!body) {
        if (class_name) free(class_name);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_DESTRUCTOR_DECLARATION);
    node->data.destructor_decl.class_name = class_name;
    node->data.destructor_decl.body = body;


    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }

    return node;
}

static ASTNode* parser_parse_function_declaration(Parser* parser) {
    parser_advance(parser);

    EsTokenType return_type = TOKEN_VOID;
    char* name = NULL;


#ifdef DEBUG
    ES_PARSER_DEBUG("Current token before type check: %s (value: '%s')",
                   token_type_to_string(parser->current_token.type),
                   parser->current_token.value ? parser->current_token.value : "NULL");
#endif

    if (parser_is_type_keyword(parser->current_token.type)) {
        return_type = parser->current_token.type;
#ifdef DEBUG
        ES_PARSER_DEBUG("Found return type: %s", token_type_to_string(return_type));
#endif
                parser_advance(parser);

#ifdef DEBUG
        ES_PARSER_DEBUG("Token after return type: %s (value: '%s')",
                       token_type_to_string(parser->current_token.type),
                       parser->current_token.value ? parser->current_token.value : "NULL");
#endif

            if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
            ES_PARSER_ERROR("Expected function name after return type");
#endif
                return NULL;
            }
    } else if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected function name or return type");
#endif
                return NULL;
    }

    name = accelerator_strdup(parser->current_token.value);
#ifdef DEBUG
    ES_PARSER_DEBUG("Function name: %s", name);
#endif
    if (!name) {
#ifdef DEBUG
        ES_PARSER_ERROR("Memory allocation failed for function name");
#endif
        return NULL;
    }

        parser_advance(parser);

#ifdef DEBUG
    ES_PARSER_DEBUG("Token after function name: %s (value: '%s')",
                   token_type_to_string(parser->current_token.type),
                   parser->current_token.value ? parser->current_token.value : "NULL");
#endif

    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after function name");
#endif
        ES_FREE(name);
        return NULL;
    }
    parser_advance(parser);

    char** parameters = NULL;
    EsTokenType* parameter_types = NULL;
    int parameter_count = 0;

    if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_count)) {
        ES_FREE(name);
                return NULL;
            }

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')' after parameter list");
#endif
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        ES_FREE(name);
                return NULL;
            }
            parser_advance(parser);

    ASTNode* body = parser_parse_block(parser);
    if (!body) {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        ES_FREE(name);
        return NULL;
    }

    ASTNode* node = parser_create_function_node(parser, 0, name,
                                                parameters,
                                                parameter_types,
                                                parameter_count,
                                                body,
                                                return_type);
    if (!node) {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        ast_destroy(body);
        ES_FREE(name);
        return NULL;
    }


    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }

    return node;
}

static ASTNode* parser_parse_typed_function_declaration(Parser* parser,
                                                       EsTokenType return_type,
                                                       char* function_name,
                                                       int is_static) {
#ifdef DEBUG
    ES_PARSER_DEBUG("starting, function: %s, return type: %s",
                   function_name, token_type_to_string(return_type));
#endif

    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after function name '%s'", function_name);
#endif
        ES_FREE(function_name);
        return NULL;
    }
    parser_advance(parser);

    char** parameters = NULL;
    EsTokenType* parameter_types = NULL;
    int parameter_count = 0;

    if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_count)) {
        ES_FREE(function_name);
                return NULL;
    }

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')' after parameter list");
#endif
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        ES_FREE(function_name);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* body = parser_parse_block(parser);
    if (!body) {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        ES_FREE(function_name);
        return NULL;
    }

    ASTNode* node = parser_create_function_node(parser,
                                                is_static,
                                                function_name,
                                                parameters,
                                                parameter_types,
                                                parameter_count,
                                                body,
                                                return_type);
    if (!node) {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        ast_destroy(body);
        ES_FREE(function_name);
        return NULL;
    }

    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }

    return node;
}


static ASTNode* parser_parse_if_statement(Parser* parser) {
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after 'if'");
#endif
        return NULL;
    }
    parser_advance(parser);

    ASTNode* condition = parser_parse_expression(parser);
    if (!condition) return NULL;

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')'");
#endif
        ast_destroy(condition);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* then_branch = parser_parse_statement(parser);
    if (!then_branch) {
        ast_destroy(condition);
        return NULL;
    }

    ASTNode* else_branch = NULL;
    if (parser->current_token.type == TOKEN_ELSE) {
        parser_advance(parser);
        else_branch = parser_parse_statement(parser);
        if (!else_branch) {
            ast_destroy(condition);
            ast_destroy(then_branch);
            return NULL;
        }
    }

    ASTNode* node = ast_create_node(AST_IF_STATEMENT);
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_branch = then_branch;
    node->data.if_stmt.else_branch = else_branch;

    return node;
}

static ASTNode* parser_parse_while_statement(Parser* parser) {
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after 'while'");
#endif
        return NULL;
    }
    parser_advance(parser);

    ASTNode* condition = parser_parse_expression(parser);
    if (!condition) return NULL;

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')'");
#endif
        ast_destroy(condition);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* body = parser_parse_statement(parser);
    if (!body) {
        ast_destroy(condition);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_WHILE_STATEMENT);
    node->data.while_stmt.condition = condition;
    node->data.while_stmt.body = body;

    return node;
}

static ASTNode* parser_parse_for_statement(Parser* parser) {
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after 'for'");
#endif
        return NULL;
    }
    parser_advance(parser);


    ASTNode* init = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        if (parser->current_token.type == TOKEN_VAR) {
            init = parser_parse_variable_declaration(parser);
        } else {
            init = parser_parse_assignment(parser);
        }
        if (!init) return NULL;
    }

    if (parser->current_token.type != TOKEN_SEMICOLON) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ';'");
#endif
        if (init) ast_destroy(init);
        return NULL;
    }
    parser_advance(parser);


    ASTNode* condition = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        condition = parser_parse_expression(parser);
        if (!condition) {
            if (init) ast_destroy(init);
            return NULL;
        }
    }

    if (parser->current_token.type != TOKEN_SEMICOLON) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ';'");
#endif
        if (init) ast_destroy(init);
        if (condition) ast_destroy(condition);
        return NULL;
    }
    parser_advance(parser);


    ASTNode* increment = NULL;
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        increment = parser_parse_expression(parser);
        if (!increment) {
            if (init) ast_destroy(init);
            if (condition) ast_destroy(condition);
            return NULL;
        }
    }

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')'");
#endif
        if (init) ast_destroy(init);
        if (condition) ast_destroy(condition);
        if (increment) ast_destroy(increment);
        return NULL;
    }
    parser_advance(parser);


    ASTNode* body = parser_parse_statement(parser);
    if (!body) {
        if (init) ast_destroy(init);
        if (condition) ast_destroy(condition);
        if (increment) ast_destroy(increment);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_FOR_STATEMENT);
    node->data.for_stmt.init = init;
    node->data.for_stmt.condition = condition;
    node->data.for_stmt.increment = increment;
    node->data.for_stmt.body = body;

    return node;
}

static ASTNode* parser_parse_block(Parser* parser) {
    if (parser->current_token.type != TOKEN_LEFT_BRACE) {
        return parser_parse_statement(parser);
    }

    parser_advance(parser);

    ASTNode** statements = NULL;
    int statement_count = 0;

    while (parser->current_token.type != TOKEN_RIGHT_BRACE &&
           parser->current_token.type != TOKEN_EOF) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (!stmt) {
            for (int i = 0; i < statement_count; i++) {
                ast_destroy(statements[i]);
            }
            free(statements);
            return NULL;
        }

        statements = realloc(statements, (statement_count + 1) * sizeof(ASTNode*));
        statements[statement_count] = stmt;
        statement_count++;
    }

    if (parser->current_token.type != TOKEN_RIGHT_BRACE) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '}'");
#endif
        for (int i = 0; i < statement_count; i++) {
            ast_destroy(statements[i]);
        }
        free(statements);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* node = ast_create_node(AST_BLOCK);
    node->data.block.statements = statements;
    node->data.block.statement_count = statement_count;

    return node;
}

static ASTNode* parser_parse_statement(Parser* parser) {
#ifdef DEBUG
    ES_PARSER_DEBUG("FUNCTION ENTRY POINT REACHED!");
#endif

    if (!parser) {
#ifdef DEBUG
        ES_PARSER_DEBUG("parser is NULL!");
#endif
        return NULL;
    }

    ASTNode* node = NULL;

#ifdef DEBUG
    ES_PARSER_DEBUG("ENTERING, current token type: %d (%s), value: '%s'",
            parser->current_token.type,
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
#endif

    switch (parser->current_token.type) {
        case TOKEN_PUBLIC:
        case TOKEN_PRIVATE:
        case TOKEN_PROTECTED:
            return parser_parse_access_modifier(parser);
        case TOKEN_CONSTRUCTOR:
            return parser_parse_constructor_declaration(parser);
        case TOKEN_DESTRUCTOR:
            return parser_parse_destructor_declaration(parser);
            break;
        case TOKEN_STATIC:
            return parser_parse_static_member_declaration(parser);
            break;
        case TOKEN_VAR:
            node = parser_parse_variable_declaration(parser);
            break;
        case TOKEN_FUNCTION:
            node = parser_parse_function_declaration(parser);
            break;
        case TOKEN_PRINT:
            node = parser_parse_print_statement(parser);
            break;
        case TOKEN_CONSOLE:
            node = parser_parse_console_writeline(parser);
            break;
        case TOKEN_RETURN:
            node = parser_parse_return_statement(parser);
            break;
        case TOKEN_IF:
            node = parser_parse_if_statement(parser);
            break;
        case TOKEN_WHILE:
            node = parser_parse_while_statement(parser);
            break;
        case TOKEN_FOR:
            node = parser_parse_for_statement(parser);
            break;
        case TOKEN_TRY:
#ifdef DEBUG
            ES_PARSER_DEBUG("processing TOKEN_TRY");
#endif
            node = parser_parse_try_statement(parser);
            break;
        case TOKEN_THROW:
            node = parser_parse_throw_statement(parser);
            break;
        case TOKEN_SWITCH:
            node = parser_parse_switch_statement(parser);
            break;
        case TOKEN_BREAK:
            node = parser_parse_break_statement(parser);
            break;
        case TOKEN_CONTINUE:
            node = parser_parse_continue_statement(parser);
            break;
        case TOKEN_TEMPLATE:
#ifdef DEBUG
            ES_PARSER_DEBUG("processing TOKEN_TEMPLATE");
#endif
            node = parser_parse_template_declaration(parser);
            if (node) {
#ifdef DEBUG
                ES_PARSER_DEBUG("TOKEN_TEMPLATE parsed successfully");
#endif
            } else {
#ifdef DEBUG
                ES_PARSER_DEBUG("TOKEN_TEMPLATE parsing failed");
#endif
            }
            break;
        case TOKEN_NAMESPACE:
            node = parser_parse_namespace_declaration(parser);
            break;
        case TOKEN_CLASS:
#ifdef DEBUG
            ES_PARSER_DEBUG("Parsing class declaration");
#endif
            node = parser_parse_class_declaration(parser);
#ifdef DEBUG
            ES_PARSER_DEBUG("Class declaration parsed: %p", node);
#endif
            break;
        case TOKEN_INT8:
        case TOKEN_INT16:
        case TOKEN_INT32:
        case TOKEN_INT64:
        case TOKEN_UINT8:
        case TOKEN_UINT16:
        case TOKEN_UINT32:
        case TOKEN_UINT64:
        case TOKEN_FLOAT32:
        case TOKEN_FLOAT64:
        case TOKEN_BOOL:
        case TOKEN_STRING:
        case TOKEN_CHAR:
        case TOKEN_VOID:

            node = parser_parse_type_declaration(parser);
            break;
        case TOKEN_LEFT_BRACE:
            node = parser_parse_block(parser);
            break;
        case TOKEN_IDENTIFIER: {
#ifdef DEBUG
            ES_PARSER_DEBUG("processing IDENTIFIER: '%s'", parser->current_token.value);
#endif


            if (strcmp(parser->current_token.value, "try") == 0) {
                node = parser_parse_try_statement(parser);
            } else if (strcmp(parser->current_token.value, "catch") == 0) {
                node = parser_parse_catch_clause(parser);
            } else if (strcmp(parser->current_token.value, "throw") == 0) {
                node = parser_parse_throw_statement(parser);
            } else if (strcmp(parser->current_token.value, "console") == 0) {

                node = parser_parse_console_writeline(parser);
                if (!node) return NULL;
            } else if (strcmp(parser->current_token.value, "print") == 0) {

                node = parser_parse_print_statement(parser);
                if (!node) return NULL;
            } else if (strcmp(parser->current_token.value, "delete") == 0) {
                lexer_next_token(parser->lexer);
                ASTNode* expr = parser_parse_expression(parser);
                ASTNode* node = ast_create_node(AST_DELETE_STATEMENT);
                node->data.delete_stmt.value = expr;
                if (parser->current_token.type == TOKEN_SEMICOLON) {
                    lexer_next_token(parser->lexer);
                }
                return node;
            } else {

                Token next = lexer_peek_token(parser->lexer);
#ifdef DEBUG
                ES_PARSER_DEBUG("next token after IDENTIFIER: %s (value: '%s')",
                        token_type_to_string(next.type),
                        next.value ? next.value : "NULL");
#endif
                if (next.type == TOKEN_ASSIGN) {
#ifdef DEBUG
                    ES_PARSER_DEBUG("parsing assignment");
#endif
                    node = parser_parse_assignment(parser);
                } else if (next.type == TOKEN_LEFT_BRACKET) {
#ifdef DEBUG
                    ES_PARSER_DEBUG("Found array access pattern, parsing as postfix expression");
#endif

                    node = parser_parse_postfix_expression(parser);
#ifdef DEBUG
                    ES_PARSER_DEBUG("After parsing postfix expression, current token: %s",
                                token_type_to_string(parser->current_token.type));
                        if (node) {
                            ES_PARSER_DEBUG("Postfix expression node type: %d (expected: %d for AST_ARRAY_ACCESS)",
                                    node->type, AST_ARRAY_ACCESS);
                        }
#endif
                    if (node && parser->current_token.type == TOKEN_ASSIGN) {

#ifdef DEBUG
                        ES_PARSER_DEBUG("Converting array access to array assignment");
#endif
                        ASTNode* assignment_node = ast_create_node(AST_ARRAY_ASSIGNMENT);
                        if (assignment_node && node->type == AST_ARRAY_ACCESS) {
                            assignment_node->data.array_assignment.array = node->data.array_access.array;
                            assignment_node->data.array_assignment.index = node->data.array_access.index;


                            parser_advance(parser);
                            assignment_node->data.array_assignment.value = parser_parse_expression(parser);


                            ES_FREE(node);
                            node = assignment_node;
#ifdef DEBUG
                            ES_PARSER_DEBUG("Successfully created AST_ARRAY_ASSIGNMENT node");
#endif
                        } else {

#ifdef DEBUG
                            ES_PARSER_DEBUG("Failed to create array assignment node");
#endif
                        }
                    } else {

#ifdef DEBUG
                        ES_PARSER_DEBUG("Not an assignment, continuing with expression parsing");
#endif


                        ASTNode* left = node;
                        while (parser->current_token.type == TOKEN_PLUS ||
                               parser->current_token.type == TOKEN_MINUS ||
                               parser->current_token.type == TOKEN_MULTIPLY ||
                               parser->current_token.type == TOKEN_DIVIDE ||
                               parser->current_token.type == TOKEN_MODULO ||
                               parser->current_token.type == TOKEN_EQUAL ||
                               parser->current_token.type == TOKEN_NOT_EQUAL ||
                               parser->current_token.type == TOKEN_LESS ||
                               parser->current_token.type == TOKEN_GREATER ||
                               parser->current_token.type == TOKEN_LESS_EQUAL ||
                               parser->current_token.type == TOKEN_GREATER_EQUAL) {

                            EsTokenType operator = parser->current_token.type;
                            parser_advance(parser);

                            ASTNode* right = parser_parse_postfix_expression(parser);
                            if (!right) {
                                ast_destroy(left);
                                return NULL;
                            }

                            ASTNode* new_node = ast_create_node(AST_BINARY_OPERATION);
                            new_node->data.binary_op.left = left;
                            new_node->data.binary_op.operator = operator;
                            new_node->data.binary_op.right = right;

                            left = new_node;
                        }
                        node = left;
                    }
                } else {
#ifdef DEBUG
                    ES_PARSER_DEBUG("parsing expression");
#endif

                    node = parser_parse_expression(parser);
                }
            }
#ifdef DEBUG
            ES_PARSER_DEBUG("after parsing, current token: %s (value: '%s')",
                        token_type_to_string(parser->current_token.type),
                        parser->current_token.value ? parser->current_token.value : "NULL");
#endif
            break;
        }
        default:
#ifdef DEBUG
            ES_PARSER_DEBUG("default case, processing token: %s (value: '%s')",
                    token_type_to_string(parser->current_token.type),
                    parser->current_token.value ? parser->current_token.value : "NULL");
#endif

            if (parser->current_token.type == TOKEN_TEMPLATE) {
#ifdef DEBUG
                ES_PARSER_DEBUG("calling parser_parse_template_declaration");
#endif
                node = parser_parse_template_declaration(parser);
            } else {
                node = parser_parse_expression(parser);
            }
            break;
    }

    if (node && parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }

    return node;
}

ASTNode* parser_parse(Parser* parser) {
#ifdef DEBUG
    ES_PARSER_DEBUG("ENTERING FUNCTION, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
    fflush(stdout);
#endif


    if (!parser) {
#ifdef DEBUG
        ES_PARSER_ERROR("parser is NULL!");
        fflush(stderr);
#endif
        return NULL;
    }

#ifdef DEBUG
    ES_PARSER_DEBUG("parser is valid, continuing...");
    fflush(stderr);
#endif

    ASTNode** statements = NULL;
    int statement_count = 0;
    while (parser->current_token.type != TOKEN_EOF) {
#ifdef DEBUG
        ES_PARSER_DEBUG("parsing statement %d, current token: %s (value: '%s')",
                statement_count,
                token_type_to_string(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "NULL");

        ES_PARSER_DEBUG("about to call parser_parse_statement, function pointer: %p", (void*)parser_parse_statement);
        fflush(stderr);
#endif

        ASTNode* stmt = parser_parse_statement(parser);

#ifdef DEBUG
        ES_PARSER_DEBUG("parser_parse_statement returned: %p", stmt);
        fflush(stderr);
#endif

        if (!stmt) {
#ifdef DEBUG
            ES_PARSER_DEBUG("parser_parse_statement returned NULL");
#endif
            for (int i = 0; i < statement_count; i++) {
                ast_destroy(statements[i]);
            }
            free(statements);
            return NULL;
        }

        statements = realloc(statements, (statement_count + 1) * sizeof(ASTNode*));
        statements[statement_count] = stmt;
        statement_count++;

#ifdef DEBUG
        ES_PARSER_DEBUG("statement %d parsed successfully, current token: %s (value: '%s')",
                    statement_count,
                    token_type_to_string(parser->current_token.type),
                    parser->current_token.value ? parser->current_token.value : "NULL");
#endif
    }

#ifdef DEBUG
    ES_PARSER_DEBUG("completed, total statements: %d", statement_count);
#endif

    ASTNode* program = ast_create_node(AST_PROGRAM);
    program->data.block.statements = statements;
    program->data.block.statement_count = statement_count;

    return program;
}

static ASTNode* parser_parse_new_expression(Parser* parser) {
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected class name after 'new'");
#endif
        return NULL;
    }

    char* class_name = accelerator_strdup(parser->current_token.value);
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected '(' after class name");
#endif
        free(class_name);
        return NULL;
    }
    parser_advance(parser);

    ASTNode** arguments = NULL;
    char** argument_names = NULL;
    int argument_count = 0;

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        while (1) {

            if (parser->current_token.type == TOKEN_IDENTIFIER &&
                lexer_peek_token(parser->lexer).type == TOKEN_COLON) {

                char* name = accelerator_strdup(parser->current_token.value);
                parser_advance(parser);
                parser_advance(parser);

                ASTNode* arg = parser_parse_expression(parser);
                if (!arg) {
                    ES_FREE(name);
                    for (int i = 0; i < argument_count; i++) {
                        ast_destroy(arguments[i]);
                        if (argument_names && argument_names[i]) free(argument_names[i]);
                    }
                    free(arguments);
                    free(argument_names);
                    ES_FREE(class_name);
                    return NULL;
                }

                arguments = realloc(arguments, (argument_count + 1) * sizeof(ASTNode*));
                argument_names = realloc(argument_names, (argument_count + 1) * sizeof(char*));
                arguments[argument_count] = arg;
                argument_names[argument_count] = name;
                argument_count++;
            } else {

                ASTNode* arg = parser_parse_expression(parser);
                if (!arg) {
                    for (int i = 0; i < argument_count; i++) {
                        ast_destroy(arguments[i]);
                        if (argument_names && argument_names[i]) free(argument_names[i]);
                    }
                    free(arguments);
                    free(argument_names);
                    free(class_name);
                    return NULL;
                }

                arguments = realloc(arguments, (argument_count + 1) * sizeof(ASTNode*));
                argument_names = realloc(argument_names, (argument_count + 1) * sizeof(char*));
                arguments[argument_count] = arg;
                argument_names[argument_count] = NULL;
                argument_count++;
            }

            if (parser->current_token.type == TOKEN_RIGHT_PAREN) break;
            if (parser->current_token.type != TOKEN_COMMA) {
#ifdef DEBUG
                ES_PARSER_ERROR("Expected ',' or ')'");
#endif
                for (int i = 0; i < argument_count; i++) {
                    ast_destroy(arguments[i]);
                    if (argument_names && argument_names[i]) ES_FREE(argument_names[i]);
                }
                ES_FREE(arguments);
                ES_FREE(argument_names);
                free(class_name);
                return NULL;
            }
            parser_advance(parser);
        }
    }

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
#ifdef DEBUG
        ES_PARSER_ERROR("Expected ')'");
#endif
        for (int i = 0; i < argument_count; i++) {
            ast_destroy(arguments[i]);
            if (argument_names && argument_names[i]) free(argument_names[i]);
        }
        free(arguments);
        free(argument_names);
        free(class_name);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* node = ast_create_node(AST_NEW_EXPRESSION);
    node->data.new_expr.class_name = class_name;
    node->data.new_expr.arguments = arguments;
    node->data.new_expr.argument_count = argument_count;
    node->data.new_expr.argument_names = argument_names;

    return node;
}

ASTNode* parser_parse_try_statement(Parser* parser) {
    parser_advance(parser);


    ASTNode* try_block = parser_parse_block(parser);
    if (!try_block) {
        return NULL;
    }

    ASTNode** catch_clauses = NULL;
    int catch_clause_count = 0;
    ASTNode* finally_clause = NULL;


    while (parser->current_token.type == TOKEN_CATCH) {
        ASTNode* catch_clause = parser_parse_catch_clause(parser);
        if (!catch_clause) {
            ast_destroy(try_block);
            for (int i = 0; i < catch_clause_count; i++) {
                ast_destroy(catch_clauses[i]);
            }
            free(catch_clauses);
            return NULL;
        }

        catch_clauses = realloc(catch_clauses, (catch_clause_count + 1) * sizeof(ASTNode*));
        catch_clauses[catch_clause_count] = catch_clause;
        catch_clause_count++;
    }


    if (parser->current_token.type == TOKEN_FINALLY) {
        finally_clause = parser_parse_finally_clause(parser);
        if (!finally_clause) {
            ast_destroy(try_block);
            for (int i = 0; i < catch_clause_count; i++) {
                ast_destroy(catch_clauses[i]);
            }
            free(catch_clauses);
            return NULL;
        }
    }


    if (catch_clause_count == 0 && !finally_clause) {
#ifdef DEBUG
        ES_PARSER_ERROR("Try statement must have at least one catch or finally clause");
#endif
        ast_destroy(try_block);
        free(catch_clauses);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_TRY_STATEMENT);
    node->data.try_stmt.try_block = try_block;
    node->data.try_stmt.catch_clauses = catch_clauses;
    node->data.try_stmt.catch_clause_count = catch_clause_count;
    node->data.try_stmt.finally_clause = finally_clause;

    return node;
}

ASTNode* parser_parse_throw_statement(Parser* parser) {
    parser_advance(parser);

    ASTNode* exception_expr = NULL;


    if (parser->current_token.type != TOKEN_SEMICOLON && parser->current_token.type != TOKEN_EOF) {
        exception_expr = parser_parse_expression(parser);
        if (!exception_expr) {
            return NULL;
        }
    }


    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }

    ASTNode* node = ast_create_node(AST_THROW_STATEMENT);
    node->data.throw_stmt.exception_expr = exception_expr;

    return node;
}

ASTNode* parser_parse_catch_clause(Parser* parser) {
    parser_advance(parser);

    char* exception_type = NULL;
    char* exception_var = NULL;


    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        parser_advance(parser);


        if (parser->current_token.type == TOKEN_IDENTIFIER ||
            parser->current_token.type == TOKEN_INT8 ||
            parser->current_token.type == TOKEN_INT16 ||
            parser->current_token.type == TOKEN_INT32 ||
            parser->current_token.type == TOKEN_INT64 ||
            parser->current_token.type == TOKEN_STRING ||
            parser->current_token.type == TOKEN_BOOL ||
            parser->current_token.type == TOKEN_FLOAT32 ||
            parser->current_token.type == TOKEN_FLOAT64 ||
            parser->current_token.type == TOKEN_VOID) {
            exception_type = accelerator_strdup(parser->current_token.value);
            parser_advance(parser);


            if (parser->current_token.type == TOKEN_IDENTIFIER ||
                parser->current_token.type == TOKEN_EXCEPTION) {
                exception_var = accelerator_strdup(parser->current_token.value);
                parser_advance(parser);
            }
        }

        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            fprintf(stderr, "Expected ')' in catch clause\n");
            fflush(stderr);
            if (exception_type) free(exception_type);
            if (exception_var) free(exception_var);
            return NULL;
        }
        parser_advance(parser);
    }


    ASTNode* catch_block = parser_parse_block(parser);
    if (!catch_block) {
        if (exception_type) free(exception_type);
        if (exception_var) free(exception_var);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_CATCH_CLAUSE);
    node->data.catch_clause.exception_type = exception_type;
    node->data.catch_clause.exception_var = exception_var;
    node->data.catch_clause.catch_block = catch_block;

    return node;
}

ASTNode* parser_parse_finally_clause(Parser* parser) {
    parser_advance(parser);

    ASTNode* finally_block = parser_parse_block(parser);
    if (!finally_block) {
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_FINALLY_CLAUSE);
    node->data.finally_clause.finally_block = finally_block;

    return node;
}

ASTNode* parser_parse_template_declaration(Parser* parser) {
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_LESS) {
        fprintf(stderr, "Expected '<' after 'template'\n");
        fflush(stderr);
        return NULL;
    }
    parser_advance(parser);

    ASTNode** parameters = NULL;
    int parameter_count = 0;


    if (parser->current_token.type != TOKEN_GREATER) {
        while (1) {
            ASTNode* param = parser_parse_template_parameter(parser);
            if (!param) {
                for (int i = 0; i < parameter_count; i++) {
                    ast_destroy(parameters[i]);
                }
                free(parameters);
                return NULL;
            }

            parameters = realloc(parameters, (parameter_count + 1) * sizeof(ASTNode*));
            parameters[parameter_count] = param;
            parameter_count++;

            if (parser->current_token.type == TOKEN_GREATER) break;
            if (parser->current_token.type != TOKEN_COMMA) {
                fprintf(stderr, "Expected ',' or '>' in template parameter list\n");
                fflush(stderr);
                for (int i = 0; i < parameter_count; i++) {
                    ast_destroy(parameters[i]);
                }
                free(parameters);
                return NULL;
            }
            parser_advance(parser);
        }
    }

    if (parser->current_token.type != TOKEN_GREATER) {
        fprintf(stderr, "Expected '>' after template parameters\n");
        fflush(stderr);
        for (int i = 0; i < parameter_count; i++) {
            ast_destroy(parameters[i]);
        }
        free(parameters);
        return NULL;
    }
    parser_advance(parser);


    ASTNode* declaration = NULL;
    if (parser->current_token.type == TOKEN_CLASS) {
        declaration = parser_parse_class_declaration(parser);
    } else if (parser->current_token.type == TOKEN_FUNCTION) {
        declaration = parser_parse_function_declaration(parser);
    } else {
        fprintf(stderr, "Expected class or function declaration after template\n");
        fflush(stderr);
        for (int i = 0; i < parameter_count; i++) {
            ast_destroy(parameters[i]);
        }
        free(parameters);
        return NULL;
    }

    if (!declaration) {
        for (int i = 0; i < parameter_count; i++) {
            ast_destroy(parameters[i]);
        }
        free(parameters);
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_TEMPLATE_DECLARATION);
    node->data.template_decl.parameters = parameters;
    node->data.template_decl.parameter_count = parameter_count;
    node->data.template_decl.declaration = declaration;

    return node;
}

ASTNode* parser_parse_template_parameter(Parser* parser) {

    int is_typename = 0;
    if (parser->current_token.type == TOKEN_TYPENAME) {
        is_typename = 1;
        parser_advance(parser);
    } else if (parser->current_token.type == TOKEN_CLASS) {
        is_typename = 1;
        parser_advance(parser);
    }

    if (!is_typename) {
        fprintf(stderr, "Expected 'typename' or 'class' in template parameter\n");
        fflush(stderr);
        return NULL;
    }

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Expected identifier in template parameter\n");
        fflush(stderr);
        return NULL;
    }

    char* param_name = accelerator_strdup(parser->current_token.value);
    parser_advance(parser);

    ASTNode* node = ast_create_node(AST_TEMPLATE_PARAMETER);
    node->data.template_param.param_name = param_name;

    return node;
}

static ASTNode* parser_parse_type_declaration(Parser* parser) {
#ifdef DEBUG
    ES_PARSER_DEBUG("starting, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
#endif


    EsTokenType type_token = parser->current_token.type;
    char* type_name = accelerator_strdup(token_type_to_string(type_token));

    parser_advance(parser);

#ifdef DEBUG
    ES_PARSER_DEBUG("after skipping type keyword, current token: %s (value: '%s')",
            token_type_to_string(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "NULL");
#endif

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Expected identifier after type '%s'\n", type_name);
        fflush(stderr);
        free(type_name);
        return NULL;
    }

    char* identifier_name = accelerator_strdup(parser->current_token.value);


    Token next_token = lexer_peek_token(parser->lexer);
#ifdef DEBUG
    ES_PARSER_DEBUG("next token after identifier: %s (value: '%s')",
           token_type_to_string(next_token.type),
           next_token.value ? next_token.value : "NULL");
#endif

    if (next_token.type == TOKEN_LEFT_PAREN) {

#ifdef DEBUG
        ES_PARSER_DEBUG("detected function declaration for '%s'", identifier_name);
#endif


        parser_advance(parser);


        return parser_parse_typed_function_declaration(parser, type_token, identifier_name, 0);
    }


    parser_advance(parser);

#ifdef DEBUG
    ES_PARSER_DEBUG("after identifier '%s', current token: %s (value: '%s')",
                identifier_name,
                token_type_to_string(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "NULL");
#endif

    ASTNode* value = NULL;
    if (parser->current_token.type == TOKEN_ASSIGN) {
        parser_advance(parser);

#ifdef DEBUG
        ES_PARSER_DEBUG("next token after identifier: %s (value: '%s')",
                token_type_to_string(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "NULL");
#endif

        value = parser_parse_expression(parser);
        if (!value) {
            free(type_name);
            free(identifier_name);
            return NULL;
        }
    }


    ASTNode* node = ast_create_node(AST_VARIABLE_DECLARATION);
    node->data.variable_decl.name = identifier_name;
    node->data.variable_decl.value = value;
    node->data.variable_decl.type = type_token;

#ifdef DEBUG
    ES_PARSER_DEBUG("after '=', current token: %s (value: '%s')",
                token_type_to_string(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "NULL");
#endif

    free(type_name);
    return node;
}

ASTNode* parser_parse_switch_statement(Parser* parser) {
#ifdef DEBUG
    ES_PARSER_DEBUG("parsing switch statement");
#endif
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        fprintf(stderr, "Expected '(' after 'switch'\n");
        fflush(stderr);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* condition = parser_parse_expression(parser);
    if (!condition) {
        return NULL;
    }

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        fprintf(stderr, "Expected ')' after switch condition\n");
        fflush(stderr);
        ast_destroy(condition);
        return NULL;
    }
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_LEFT_BRACE) {
        fprintf(stderr, "Expected '{' after switch condition\n");
        fflush(stderr);
        ast_destroy(condition);
        return NULL;
    }
    parser_advance(parser);

    ASTNode** cases = NULL;
    int case_count = 0;
    ASTNode* default_case = NULL;

    while (parser->current_token.type != TOKEN_RIGHT_BRACE && parser->current_token.type != TOKEN_EOF) {
        if (parser->current_token.type == TOKEN_CASE) {
            ASTNode* case_node = parser_parse_case_clause(parser);
            if (!case_node) {
                for (int i = 0; i < case_count; i++) {
                    ast_destroy(cases[i]);
                }
                free(cases);
                ast_destroy(condition);
                return NULL;
            }
            cases = realloc(cases, (case_count + 1) * sizeof(ASTNode*));
            cases[case_count] = case_node;
            case_count++;
        } else if (parser->current_token.type == TOKEN_DEFAULT) {
            if (default_case) {
                fprintf(stderr, "Multiple default cases in switch statement\n");
            fflush(stderr);
                for (int i = 0; i < case_count; i++) {
                    ast_destroy(cases[i]);
                }
                free(cases);
                ast_destroy(condition);
                return NULL;
            }
            default_case = parser_parse_default_clause(parser);
            if (!default_case) {
                for (int i = 0; i < case_count; i++) {
                    ast_destroy(cases[i]);
                }
                free(cases);
                ast_destroy(condition);
                return NULL;
            }
        } else {
            fprintf(stderr, "Expected 'case' or 'default' in switch statement\n");
            fflush(stderr);
            for (int i = 0; i < case_count; i++) {
                ast_destroy(cases[i]);
            }
            free(cases);
            ast_destroy(condition);
            return NULL;
        }
    }

    if (parser->current_token.type != TOKEN_RIGHT_BRACE) {
        fprintf(stderr, "Expected '}' after switch cases\n");
        fflush(stderr);
        for (int i = 0; i < case_count; i++) {
            ast_destroy(cases[i]);
        }
        free(cases);
        ast_destroy(condition);
        return NULL;
    }
    parser_advance(parser);

    ASTNode* node = ast_create_node(AST_SWITCH_STATEMENT);
    node->data.switch_stmt.expression = condition;
    node->data.switch_stmt.cases = cases;
    node->data.switch_stmt.case_count = case_count;
    node->data.switch_stmt.default_case = default_case;

    return node;
}

ASTNode* parser_parse_case_clause(Parser* parser) {
#ifdef DEBUG
    ES_PARSER_DEBUG("parsing case clause");
#endif
    parser_advance(parser);

    ASTNode* value = parser_parse_expression(parser);
    if (!value) {
        return NULL;
    }

    if (parser->current_token.type != TOKEN_COLON) {
        fprintf(stderr, "Expected ':' after case value\n");
        fflush(stderr);
        ast_destroy(value);
        return NULL;
    }
    parser_advance(parser);

    ASTNode** statements = NULL;
    int statement_count = 0;

    while (parser->current_token.type != TOKEN_CASE &&
           parser->current_token.type != TOKEN_DEFAULT &&
           parser->current_token.type != TOKEN_RIGHT_BRACE &&
           parser->current_token.type != TOKEN_EOF) {

        ASTNode* stmt = parser_parse_statement(parser);
        if (!stmt) {
            for (int i = 0; i < statement_count; i++) {
                ast_destroy(statements[i]);
            }
            free(statements);
            ast_destroy(value);
            return NULL;
        }

        statements = realloc(statements, (statement_count + 1) * sizeof(ASTNode*));
        statements[statement_count] = stmt;
        statement_count++;
    }

    ASTNode* node = ast_create_node(AST_CASE_CLAUSE);
    node->data.case_clause.value = value;
    node->data.case_clause.statements = statements;
    node->data.case_clause.statement_count = statement_count;

    return node;
}

ASTNode* parser_parse_default_clause(Parser* parser) {
#ifdef DEBUG
    ES_PARSER_DEBUG("parsing default clause");
#endif
    parser_advance(parser);

    if (parser->current_token.type != TOKEN_COLON) {
        fprintf(stderr, "Expected ':' after 'default'\n");
        fflush(stderr);
        return NULL;
    }
    parser_advance(parser);

    ASTNode** statements = NULL;
    int statement_count = 0;

    while (parser->current_token.type != TOKEN_CASE &&
           parser->current_token.type != TOKEN_DEFAULT &&
           parser->current_token.type != TOKEN_RIGHT_BRACE &&
           parser->current_token.type != TOKEN_EOF) {

        ASTNode* stmt = parser_parse_statement(parser);
        if (!stmt) {
            for (int i = 0; i < statement_count; i++) {
                ast_destroy(statements[i]);
            }
            free(statements);
            return NULL;
        }

        statements = realloc(statements, (statement_count + 1) * sizeof(ASTNode*));
        statements[statement_count] = stmt;
        statement_count++;
    }

    ASTNode* node = ast_create_node(AST_DEFAULT_CLAUSE);
    node->data.default_clause.statements = statements;
    node->data.default_clause.statement_count = statement_count;

    return node;
}

ASTNode* parser_parse_break_statement(Parser* parser) {
#ifdef DEBUG
    ES_PARSER_DEBUG("parsing break statement");
#endif
    parser_advance(parser);

    ASTNode* node = ast_create_node(AST_BREAK_STATEMENT);
    return node;
}

ASTNode* parser_parse_continue_statement(Parser* parser) {
#ifdef DEBUG
    ES_PARSER_DEBUG("parsing continue statement");
#endif
    parser_advance(parser);
    ASTNode* node = ast_create_node(AST_CONTINUE_STATEMENT);
    return node;
}