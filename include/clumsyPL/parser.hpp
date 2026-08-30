#ifndef PARSER_H
#define PARSER_H

#include "ast.hpp"
#include "lexer.hpp"

typedef struct {
    int line;
    char* message;
} ParserDiagnostic;

typedef struct {
    Lexer* lexer;

    Token current;
    Token previous;

    int has_previous;
    int had_error;
    int emit_errors;
    ParserDiagnostic* diagnostics;
    size_t diagnostic_count;
    size_t diagnostic_capacity;
} Parser;

Parser create_parser(Lexer* lexer);
void destroy_parser(Parser* parser);
void parser_set_error_output(Parser* parser, int enabled);

Expr* parse_expression(Parser* parser);
Program* parse_program(Parser* parser);

#endif
