#include "../common/es_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "accelerator.h"

typedef struct {
    const char* word;
    EsTokenType token;
} Keyword;

static Keyword keywords[] = {
    {"function", TOKEN_FUNCTION},
    {"var", TOKEN_VAR},
    {"let", TOKEN_VAR},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"while", TOKEN_WHILE},
    {"for", TOKEN_FOR},
    {"return", TOKEN_RETURN},
    {"print", TOKEN_PRINT},
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    {"and", TOKEN_AND},
    {"or", TOKEN_OR},
    {"not", TOKEN_NOT},
    {"namespace", TOKEN_NAMESPACE},
    {"class", TOKEN_CLASS},
    {"struct", TOKEN_STRUCT},
    {"interface", TOKEN_INTERFACE},
    {"enum", TOKEN_ENUM},
    {"new", TOKEN_NEW},
    {"this", TOKEN_THIS},
    {"constructor", TOKEN_CONSTRUCTOR},
    {"public", TOKEN_PUBLIC},
    {"private", TOKEN_PRIVATE},
    {"protected", TOKEN_PROTECTED},
    {"static", TOKEN_STATIC},
    {"virtual", TOKEN_VIRTUAL},
    {"abstract", TOKEN_ABSTRACT},
    {"override", TOKEN_OVERRIDE},
    {"base", TOKEN_BASE},
    {"using", TOKEN_USING},
    {"import", TOKEN_IMPORT},
    {"package", TOKEN_PACKAGE},
    {"try", TOKEN_TRY},
    {"catch", TOKEN_CATCH},
    {"finally", TOKEN_FINALLY},
    {"throw", TOKEN_THROW},
    {"exception", TOKEN_EXCEPTION},
    {"template", TOKEN_TEMPLATE},
    {"typename", TOKEN_TYPENAME},
    {"switch", TOKEN_SWITCH},
    {"case", TOKEN_CASE},
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"default", TOKEN_DEFAULT},
    {"int", TOKEN_INT32},
    {"int8", TOKEN_INT8},
    {"int16", TOKEN_INT16},
    {"int32", TOKEN_INT32},
    {"int64", TOKEN_INT64},
    {"uint8", TOKEN_UINT8},
    {"uint16", TOKEN_UINT16},
    {"uint32", TOKEN_UINT32},
    {"uint64", TOKEN_UINT64},
    {"float32", TOKEN_FLOAT32},
    {"float64", TOKEN_FLOAT64},
    {"double", TOKEN_FLOAT64},
    {"bool", TOKEN_BOOL},
    {"char", TOKEN_CHAR},
    {"void", TOKEN_VOID},
    {"string", TOKEN_STRING},
    {"console", TOKEN_CONSOLE},
    {"destructor", TOKEN_DESTRUCTOR},
    {NULL, 0}
};

static void lexer_advance(Lexer* lexer) {
    if (lexer->source[lexer->position] == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    lexer->position++;
}

static char lexer_current_char(Lexer* lexer) {
    return lexer->source[lexer->position];
}

static void lexer_skip_whitespace(Lexer* lexer) {
    while (lexer->source[lexer->position] != '\0') {
        if (lexer->source[lexer->position] == ' ' ||
            lexer->source[lexer->position] == '\t' ||
            lexer->source[lexer->position] == '\n' ||
            lexer->source[lexer->position] == '\r') {
            lexer_advance(lexer);
        }
        else if (lexer->source[lexer->position] == '/' &&
                 lexer->source[lexer->position + 1] == '/') {
            lexer_advance(lexer);
            lexer_advance(lexer);
            while (lexer->source[lexer->position] != '\0' &&
                   lexer->source[lexer->position] != '\n') {
                lexer_advance(lexer);
            }
        }
        else if (lexer->source[lexer->position] == '/' &&
                 lexer->source[lexer->position + 1] == '*') {
            lexer_advance(lexer);
            lexer_advance(lexer);
            while (lexer->source[lexer->position] != '\0') {
                if (lexer->source[lexer->position] == '*' &&
                    lexer->source[lexer->position + 1] == '/') {
                    lexer_advance(lexer);
                    lexer_advance(lexer);
                    break;
                }
                lexer_advance(lexer);
            }
        }
        else {
            break;
        }
    }
}

static Token lexer_create_token(EsTokenType type, const char* value, int line, int column) {
    Token token;
    token.type = type;
    token.value = (char*)value;
    token.line = line;
    token.column = column;
    return token;
}

static Token lexer_read_number(Lexer* lexer) {
    int line = lexer->line;
    int column = lexer->column;

    int start = lexer->position;
    while (accelerator_is_digit(lexer->source[lexer->position])) {
        lexer_advance(lexer);
    }

    if (lexer->source[lexer->position] == '.') {
        lexer_advance(lexer);
        while (accelerator_is_digit(lexer->source[lexer->position])) {
            lexer_advance(lexer);
        }
    }

    int length = lexer->position - start;
    char* value = ES_MALLOC(length + 1);
    if (!value) {
        return lexer_create_token(TOKEN_EOF, "", line, column);
    }
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    return lexer_create_token(TOKEN_NUMBER, value, line, column);
}

static Token lexer_read_identifier(Lexer* lexer) {
    int line = lexer->line;
    int column = lexer->column;

    int start = lexer->position;
    while (accelerator_is_alpha(lexer->source[lexer->position]) ||
           accelerator_is_digit(lexer->source[lexer->position]) ||
           lexer->source[lexer->position] == '_') {
        lexer_advance(lexer);
    }

    int length = lexer->position - start;
    char* value = ES_MALLOC(length + 1);
    if (!value) {
        return lexer_create_token(TOKEN_EOF, "", line, column);
    }
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    for (int i = 0; keywords[i].word != NULL; i++) {
        if (strcmp(value, keywords[i].word) == 0) {
            EsTokenType type = keywords[i].token;
            ES_FREE(value);
            return lexer_create_token(type, keywords[i].word, line, column);
        }
    }

    return lexer_create_token(TOKEN_IDENTIFIER, value, line, column);
}

static Token lexer_read_string(Lexer* lexer) {
    int line = lexer->line;
    int column = lexer->column;

    lexer_advance(lexer);

    int start = lexer->position;
    while (lexer->source[lexer->position] != '"' && lexer->source[lexer->position] != '\0') {
        lexer_advance(lexer);
    }

    if (lexer->source[lexer->position] == '\0') {
        return lexer_create_token(TOKEN_EOF, "", line, column);
    }

    int length = lexer->position - start;
    char* value = ES_MALLOC(length + 1);
    if (!value) {
        return lexer_create_token(TOKEN_EOF, "", line, column);
    }
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    lexer_advance(lexer);

    return lexer_create_token(TOKEN_STRING, value, line, column);
}

Lexer* lexer_create(const char* source) {
    Lexer* lexer = ES_MALLOC(sizeof(Lexer));
    if (!lexer) return NULL;

    lexer->source = source;
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;

    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    ES_FREE(lexer);
}

void lexer_reset(Lexer* lexer) {
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
}

Token lexer_next_token(Lexer* lexer) {
    lexer_skip_whitespace(lexer);

    if (lexer->source[lexer->position] == '\0') {
        return lexer_create_token(TOKEN_EOF, "", lexer->line, lexer->column);
    }

    char c = lexer_current_char(lexer);
    int line = lexer->line;
    int column = lexer->column;

    if (accelerator_is_alpha(c) || c == '_') {
        return lexer_read_identifier(lexer);
    }

    if (accelerator_is_digit(c)) {
        return lexer_read_number(lexer);
    }

    if (c == '"') {
        return lexer_read_string(lexer);
    }

    switch (c) {
        case '+': {
            lexer_advance(lexer);
            char next = lexer_current_char(lexer);
            if (next == '+') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_INCREMENT, "++", line, column);
            }
            if (next == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_PLUS_ASSIGN, "+=", line, column);
            }
            return lexer_create_token(TOKEN_PLUS, "+", line, column);
        }
        case '-': {
            lexer_advance(lexer);
            char next = lexer_current_char(lexer);
            if (next == '-') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_DECREMENT, "--", line, column);
            }
            if (next == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_MINUS_ASSIGN, "-=", line, column);
            }
            if (next == '>') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_ARROW, "->", line, column);
            }
            return lexer_create_token(TOKEN_MINUS, "-", line, column);
        }
        case '*': {
            lexer_advance(lexer);
            if (lexer_current_char(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_MUL_ASSIGN, "*=", line, column);
            }
            return lexer_create_token(TOKEN_MULTIPLY, "*", line, column);
        }
        case '/': {
            lexer_advance(lexer);
            if (lexer_current_char(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_DIV_ASSIGN, "/=", line, column);
            }
            return lexer_create_token(TOKEN_DIVIDE, "/", line, column);
        }
        case '%': {
            lexer_advance(lexer);
            if (lexer_current_char(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_MOD_ASSIGN, "%=", line, column);
            }
            return lexer_create_token(TOKEN_MODULO, "%", line, column);
        }
        case '=':
            lexer_advance(lexer);
            if (lexer_current_char(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_EQUAL, "==", line, column);
            }
            return lexer_create_token(TOKEN_ASSIGN, "=", line, column);
        case '!':
            lexer_advance(lexer);
            if (lexer_current_char(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_NOT_EQUAL, "!=", line, column);
            }
            return lexer_create_token(TOKEN_NOT, "!", line, column);
        case '<':
            lexer_advance(lexer);
            if (lexer_current_char(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_LESS_EQUAL, "<=", line, column);
            }
            return lexer_create_token(TOKEN_LESS, "<", line, column);
        case '>':
            lexer_advance(lexer);
            if (lexer_current_char(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_GREATER_EQUAL, ">=", line, column);
            }
            return lexer_create_token(TOKEN_GREATER, ">", line, column);
        case '(': lexer_advance(lexer); return lexer_create_token(TOKEN_LEFT_PAREN, "(", line, column);
        case ')': lexer_advance(lexer); return lexer_create_token(TOKEN_RIGHT_PAREN, ")", line, column);
        case '{': lexer_advance(lexer); return lexer_create_token(TOKEN_LEFT_BRACE, "{", line, column);
        case '}': lexer_advance(lexer); return lexer_create_token(TOKEN_RIGHT_BRACE, "}", line, column);
        case '[': lexer_advance(lexer); return lexer_create_token(TOKEN_LEFT_BRACKET, "[", line, column);
        case ']': lexer_advance(lexer); return lexer_create_token(TOKEN_RIGHT_BRACKET, "]", line, column);
        case ',': lexer_advance(lexer); return lexer_create_token(TOKEN_COMMA, ",", line, column);
        case ';': lexer_advance(lexer); return lexer_create_token(TOKEN_SEMICOLON, ";", line, column);
        case ':': {
            lexer_advance(lexer);
            if (lexer_current_char(lexer) == ':') {
                lexer_advance(lexer);
                return lexer_create_token(TOKEN_DOUBLE_COLON, "::", line, column);
            }
            return lexer_create_token(TOKEN_COLON, ":", line, column);
        }
        case '.':
            lexer_advance(lexer);
            return lexer_create_token(TOKEN_DOT, ".", line, column);
        case '?': {
            int line = lexer->line;
            int column = lexer->column;

            ES_LEXER_DEBUG("Found '?' character at line %d, column %d", line, column);
            if (lexer->source[lexer->position + 1] == ':') {
                lexer_advance(lexer);
                lexer_advance(lexer);
                ES_LEXER_DEBUG("Found '?:' combination, returning TOKEN_QUESTION_COLON");
                return lexer_create_token(TOKEN_QUESTION_COLON, "?:", line, column);
            } else {
                lexer_advance(lexer);
                ES_LEXER_DEBUG("Returning TOKEN_QUESTION");
                return lexer_create_token(TOKEN_QUESTION, "?", line, column);
            }
        }
        case '~': lexer_advance(lexer); return lexer_create_token(TOKEN_TILDE, "~", line, column);
        default:
            if ((unsigned char)c >= 0xE0 && (unsigned char)c <= 0xEF) {
                ES_LEXER_DEBUG("Skipping UTF-8 Chinese character sequence at line %d, column %d", line, column);
                lexer_advance(lexer);
                int bytes_remaining = 2;
                while (bytes_remaining > 0 && lexer->source[lexer->position] != '\0') {
                    unsigned char next_byte = (unsigned char)lexer->source[lexer->position];
                    if (next_byte >= 0x80 && next_byte <= 0xBF) {
                        lexer_advance(lexer);
                        bytes_remaining--;
                    } else {
                        break;
                    }
                }
                return lexer_next_token(lexer);
            }
            ES_LEXER_DEBUG("Unexpected character: %c at line %d, column %d", c, line, column);
            lexer_advance(lexer);
            return lexer_create_token(TOKEN_UNKNOWN, "", line, column);
    }
}

Token lexer_peek_token(Lexer* lexer) {
    int saved_position = lexer->position;
    int saved_line = lexer->line;
    int saved_column = lexer->column;

    Token token = lexer_next_token(lexer);

    lexer->position = saved_position;
    lexer->line = saved_line;
    lexer->column = saved_column;

    return token;
}

const char* token_type_to_string(EsTokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_MULTIPLY: return "MULTIPLY";
        case TOKEN_DIVIDE: return "DIVIDE";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_EQUAL: return "EQUAL";
        case TOKEN_NOT_EQUAL: return "NOT_EQUAL";
        case TOKEN_LESS: return "LESS";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_FUNCTION: return "FUNCTION";
        case TOKEN_VAR: return "VAR";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_PRINT: return "PRINT";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_CLASS: return "CLASS";
        case TOKEN_NEW: return "NEW";
        case TOKEN_THIS: return "THIS";
        case TOKEN_CONSTRUCTOR: return "CONSTRUCTOR";
        case TOKEN_PUBLIC: return "PUBLIC";
        case TOKEN_PRIVATE: return "PRIVATE";
        case TOKEN_PROTECTED: return "PROTECTED";
        case TOKEN_STATIC: return "STATIC";
        case TOKEN_VIRTUAL: return "VIRTUAL";
        case TOKEN_ABSTRACT: return "ABSTRACT";
        case TOKEN_OVERRIDE: return "OVERRIDE";
        case TOKEN_TILDE: return "TILDE";
        case TOKEN_COLON: return "COLON";
        case TOKEN_DOUBLE_COLON: return "DOUBLE_COLON";
        case TOKEN_DOT: return "DOT";
        case TOKEN_LEFT_BRACKET: return "LEFT_BRACKET";
        case TOKEN_RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TOKEN_CONSOLE: return "CONSOLE";
        case TOKEN_TRY: return "TRY";
        case TOKEN_CATCH: return "CATCH";
        case TOKEN_FINALLY: return "FINALLY";
        case TOKEN_THROW: return "THROW";
        case TOKEN_EXCEPTION: return "EXCEPTION";
        case TOKEN_TEMPLATE: return "TEMPLATE";
        case TOKEN_TYPENAME: return "TYPENAME";
        case TOKEN_QUESTION: return "QUESTION";
        case TOKEN_QUESTION_COLON: return "QUESTION_COLON";
        case TOKEN_INT8: return "INT8";
        case TOKEN_INT16: return "INT16";
        case TOKEN_INT32: return "INT32";
        case TOKEN_INT64: return "INT64";
        case TOKEN_UINT8: return "UINT8";
        case TOKEN_UINT16: return "UINT16";
        case TOKEN_UINT32: return "UINT32";
        case TOKEN_UINT64: return "UINT64";
        case TOKEN_FLOAT32: return "FLOAT32";
        case TOKEN_FLOAT64: return "FLOAT64";
        case TOKEN_BOOL: return "BOOL";
        case TOKEN_VOID: return "VOID";
        case TOKEN_DESTRUCTOR: return "DESTRUCTOR";
        case TOKEN_BASE: return "BASE";
        case TOKEN_USING: return "USING";
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_PACKAGE: return "PACKAGE";
        case TOKEN_INCREMENT: return "INCREMENT";
        case TOKEN_DECREMENT: return "DECREMENT";
        case TOKEN_PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TOKEN_MINUS_ASSIGN: return "MINUS_ASSIGN";
        case TOKEN_MUL_ASSIGN: return "MUL_ASSIGN";
        case TOKEN_DIV_ASSIGN: return "DIV_ASSIGN";
        case TOKEN_MOD_ASSIGN: return "MOD_ASSIGN";
        case TOKEN_ARROW: return "ARROW";
        default: return "UNKNOWN";
    }
}