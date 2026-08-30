#include "clumsyPL/parser.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CALL_ARGUMENTS 255U

/* Forward declarations */

static Stmt* parse_declaration(Parser* parser);
static Stmt* parse_typed_declaration(Parser* parser);
static Stmt* parse_statement(Parser* parser);
static Stmt* parse_expression_statement(Parser* parser);
static Stmt* parse_block_statement(Parser* parser);
static Stmt* parse_if_statement(Parser* parser);
static Stmt* parse_while_statement(Parser* parser);
static Stmt* parse_return_statement(Parser* parser);

static Expr* parse_assignment(Parser* parser);
static Expr* parse_logical_or(Parser* parser);
static Expr* parse_logical_and(Parser* parser);
static Expr* parse_equality(Parser* parser);
static Expr* parse_comparison(Parser* parser);
static Expr* parse_term(Parser* parser);
static Expr* parse_factor(Parser* parser);
static Expr* parse_unary(Parser* parser);
static Expr* parse_call(Parser* parser);
static Expr* parse_primary(Parser* parser);

static int is_type_token(TokenType type);
static void parser_advance(Parser* parser);
static int parser_check(Parser* parser, TokenType type);
static int parser_match(Parser* parser, TokenType type);
static int parser_match_any(
    Parser* parser,
    const TokenType* types,
    size_t type_count
);
static int parser_consume(
    Parser* parser,
    TokenType type,
    const char* message
);
static void parser_error_at_current(
    Parser* parser,
    const char* message
);
static void parser_error_at_previous(
    Parser* parser,
    const char* message
);
static void parser_synchronize(Parser* parser);
static char* parser_copy_string(const char* text);
static void parser_record_error(Parser* parser, int line, const char* message);

/* Parser lifecycle */

Parser create_parser(Lexer* lexer)
{
    Parser parser;

    parser.lexer = lexer;
    parser.current.type = TOKEN_EOF;
    parser.current.value = NULL;
    parser.current.line = 1;

    parser.previous.type = TOKEN_EOF;
    parser.previous.value = NULL;
    parser.previous.line = 1;

    parser.has_previous = 0;
    parser.had_error = 0;
    parser.emit_errors = 0;
    parser.diagnostics = NULL;
    parser.diagnostic_count = 0U;
    parser.diagnostic_capacity = 0U;

    parser_advance(&parser);

    return parser;
}

void destroy_parser(Parser* parser)
{
    size_t i;
    if (parser == NULL) {
        return;
    }

    if (parser->has_previous) {
        free_token(&parser->previous);
        parser->has_previous = 0;
    }

    free_token(&parser->current);
    for (i = 0U; i < parser->diagnostic_count; ++i) {
        free(parser->diagnostics[i].message);
    }
    free(parser->diagnostics);
    parser->diagnostics = NULL;
    parser->diagnostic_count = 0U;
    parser->diagnostic_capacity = 0U;
}

void parser_set_error_output(Parser* parser, int enabled)
{
    if (parser != NULL) parser->emit_errors = enabled != 0;
}

static void parser_record_error(Parser* parser, int line, const char* message)
{
    ParserDiagnostic* resized;
    size_t capacity;
    char* copy;
    if (parser == NULL || message == NULL) return;
    copy = parser_copy_string(message);
    if (copy == NULL) return;
    if (parser->diagnostic_count == parser->diagnostic_capacity) {
        capacity = parser->diagnostic_capacity == 0U ? 4U : parser->diagnostic_capacity * 2U;
        resized = (ParserDiagnostic*)realloc(parser->diagnostics, capacity * sizeof(ParserDiagnostic));
        if (resized == NULL) { free(copy); return; }
        parser->diagnostics = resized;
        parser->diagnostic_capacity = capacity;
    }
    parser->diagnostics[parser->diagnostic_count].line = line;
    parser->diagnostics[parser->diagnostic_count].message = copy;
    parser->diagnostic_count++;
}

/* Token movement */

static void parser_advance(Parser* parser)
{
    if (parser->has_previous) {
        free_token(&parser->previous);
        parser->has_previous = 0;
    }

    parser->previous = parser->current;
    parser->has_previous = (parser->previous.value != NULL);

    for (;;) {
        parser->current = next_token(parser->lexer);

        if (parser->current.type != TOKEN_ERROR) {
            break;
        }

        parser_error_at_current(
            parser,
            parser->current.value != NULL
                ? parser->current.value
                : "Lexer error."
        );

        free_token(&parser->current);
    }
}

static int parser_check(Parser* parser, TokenType type)
{
    return parser->current.type == type;
}

static int parser_match(Parser* parser, TokenType type)
{
    if (!parser_check(parser, type)) {
        return 0;
    }

    parser_advance(parser);
    return 1;
}

static int parser_match_any(
    Parser* parser,
    const TokenType* types,
    size_t type_count
)
{
    size_t i;

    for (i = 0U; i < type_count; ++i) {
        if (parser_check(parser, types[i])) {
            parser_advance(parser);
            return 1;
        }
    }

    return 0;
}

static int parser_consume(
    Parser* parser,
    TokenType type,
    const char* message
)
{
    if (parser_check(parser, type)) {
        parser_advance(parser);
        return 1;
    }

    parser_error_at_current(parser, message);
    return 0;
}

/* Error handling */

static void parser_error_at_current(
    Parser* parser,
    const char* message
)
{
    parser_record_error(parser, parser->current.line, message);
    if (parser->emit_errors) {
        fprintf(stderr, "[line %d] Parse error at '%s': %s\n", parser->current.line,
            parser->current.value != NULL ? parser->current.value : "<unknown>", message);
    }

    parser->had_error = 1;
}

static void parser_error_at_previous(
    Parser* parser,
    const char* message
)
{
    parser_record_error(parser, parser->previous.line, message);
    if (parser->emit_errors) {
        fprintf(stderr, "[line %d] Parse error at '%s': %s\n", parser->previous.line,
            parser->previous.value != NULL ? parser->previous.value : "<unknown>", message);
    }

    parser->had_error = 1;
}

static int is_type_token(TokenType type)
{
    return type == TOKEN_INT ||
           type == TOKEN_FLOAT ||
           type == TOKEN_STRING ||
           type == TOKEN_BOOL ||
           type == TOKEN_LIST ||
           type == TOKEN_VOID;
}

static void parser_synchronize(Parser* parser)
{
    while (!parser_check(parser, TOKEN_EOF)) {
        if (parser->has_previous &&
            parser->previous.type == TOKEN_SEMICOLON) {
            return;
        }

        if (parser_check(parser, TOKEN_RBRACE) ||
            is_type_token(parser->current.type) ||
            parser_check(parser, TOKEN_IF) ||
            parser_check(parser, TOKEN_WHILE) ||
            parser_check(parser, TOKEN_RETURN)) {
            return;
        }

        parser_advance(parser);
    }
}

static char* parser_copy_string(const char* text)
{
    size_t length;
    char* copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char*)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

/* Entry points */

Expr* parse_expression(Parser* parser)
{
    return parse_assignment(parser);
}

Program* parse_program(Parser* parser)
{
    Program* program = create_program();

    if (program == NULL) {
        parser_error_at_current(parser, "Out of memory while creating program AST.");
        return NULL;
    }

    while (!parser_check(parser, TOKEN_EOF)) {
        if (parser_check(parser, TOKEN_RBRACE)) {
            parser_error_at_current(parser, "Unexpected '}' at top-level.");
            parser_advance(parser);
            continue;
        }

        Stmt* declaration = parse_declaration(parser);

        if (declaration == NULL) {
            parser_synchronize(parser);
            continue;
        }

        if (!program_add_statement(program, declaration)) {
            parser_error_at_current(parser, "Out of memory while appending statement.");
            free_stmt(declaration);
            free_program(program);
            return NULL;
        }
    }

    return program;
}

/* Declaration grammar */

static Stmt* parse_declaration(Parser* parser)
{
    if (is_type_token(parser->current.type)) {
        return parse_typed_declaration(parser);
    }

    return parse_statement(parser);
}

static Stmt* parse_typed_declaration(Parser* parser)
{
    TokenType declared_type = parser->current.type;
    int declaration_line = parser->current.line;
    char* name;
    Stmt* stmt;

    parser_advance(parser);

    if (!parser_consume(parser, TOKEN_IDENTIFIER, "Expected identifier after type.")) {
        return NULL;
    }

    name = parser_copy_string(parser->previous.value);
    if (name == NULL) {
        parser_error_at_current(parser, "Out of memory while copying identifier.");
        return NULL;
    }

    if (parser_match(parser, TOKEN_LPAREN)) {
        Stmt* function_stmt =
            create_function_declaration_stmt(declared_type, name, declaration_line);

        if (function_stmt == NULL) {
            parser_error_at_current(parser, "Out of memory while creating function declaration.");
            free(name);
            return NULL;
        }

        free(name);

        if (!parser_check(parser, TOKEN_RPAREN)) {
            do {
                TokenType parameter_type;
                const char* parameter_name;
                int parameter_line;

                if (!is_type_token(parser->current.type)) {
                    parser_error_at_current(parser, "Expected parameter type.");
                    free_stmt(function_stmt);
                    return NULL;
                }

                parameter_type = parser->current.type;
                parameter_line = parser->current.line;
                parser_advance(parser);

                if (!parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name.")) {
                    free_stmt(function_stmt);
                    return NULL;
                }

                parameter_name = parser->previous.value;
                if (!function_declaration_add_parameter(
                        function_stmt,
                        parameter_type,
                        parameter_name,
                        parameter_line
                    )) {
                    parser_error_at_current(parser, "Out of memory while appending function parameter.");
                    free_stmt(function_stmt);
                    return NULL;
                }
            } while (parser_match(parser, TOKEN_COMMA));
        }

        if (!parser_consume(parser, TOKEN_RPAREN, "Expected ')' after parameters.")) {
            free_stmt(function_stmt);
            return NULL;
        }

        stmt = parse_block_statement(parser);
        if (stmt == NULL) {
            free_stmt(function_stmt);
            return NULL;
        }

        if (!function_declaration_set_body(function_stmt, stmt)) {
            parser_error_at_current(parser, "Failed to attach function body.");
            free_stmt(stmt);
            free_stmt(function_stmt);
            return NULL;
        }

        return function_stmt;
    }

    {
        Expr* initializer = NULL;

        if (parser_match(parser, TOKEN_ASSIGN)) {
            initializer = parse_expression(parser);
            if (initializer == NULL) {
                free(name);
                return NULL;
            }
        }

        if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration.")) {
            free_expr(initializer);
            free(name);
            return NULL;
        }

        stmt = create_variable_declaration_stmt(
            declared_type,
            name,
            initializer,
            declaration_line
        );

        if (stmt == NULL) {
            parser_error_at_current(parser, "Out of memory while creating variable declaration.");
            free_expr(initializer);
            free(name);
            return NULL;
        }

        free(name);

        return stmt;
    }
}

/* Statement grammar */

static Stmt* parse_statement(Parser* parser)
{
    if (parser_match(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }

    if (parser_match(parser, TOKEN_WHILE)) {
        return parse_while_statement(parser);
    }

    if (parser_match(parser, TOKEN_RETURN)) {
        return parse_return_statement(parser);
    }

    if (parser_check(parser, TOKEN_LBRACE)) {
        return parse_block_statement(parser);
    }

    return parse_expression_statement(parser);
}

static Stmt* parse_expression_statement(Parser* parser)
{
    Expr* expression = parse_expression(parser);
    int line;
    Stmt* stmt;

    if (expression == NULL) {
        return NULL;
    }

    line = expression->line;

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression.")) {
        free_expr(expression);
        return NULL;
    }

    stmt = create_expression_stmt(expression, line);
    if (stmt == NULL) {
        parser_error_at_current(parser, "Out of memory while creating expression statement.");
        free_expr(expression);
        return NULL;
    }

    return stmt;
}

static Stmt* parse_block_statement(Parser* parser)
{
    int line;
    Stmt* block_stmt;

    if (!parser_consume(parser, TOKEN_LBRACE, "Expected '{' to start block.")) {
        return NULL;
    }

    line = parser->previous.line;
    block_stmt = create_block_stmt(line);
    if (block_stmt == NULL) {
        parser_error_at_current(parser, "Out of memory while creating block.");
        return NULL;
    }

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_check(parser, TOKEN_EOF)) {
        Stmt* declaration = parse_declaration(parser);

        if (declaration == NULL) {
            parser_synchronize(parser);
            continue;
        }

        if (!block_stmt_add_statement(block_stmt, declaration)) {
            parser_error_at_current(parser, "Out of memory while appending block statement.");
            free_stmt(declaration);
            free_stmt(block_stmt);
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_RBRACE, "Expected '}' after block.")) {
        free_stmt(block_stmt);
        return NULL;
    }

    return block_stmt;
}

static Stmt* parse_if_statement(Parser* parser)
{
    int line = parser->previous.line;
    Expr* condition;
    Stmt* then_branch;
    Stmt* else_branch = NULL;
    Stmt* if_stmt;

    if (!parser_consume(parser, TOKEN_LPAREN, "Expected '(' after 'if'.")) {
        return NULL;
    }

    condition = parse_expression(parser);
    if (condition == NULL) {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_RPAREN, "Expected ')' after condition.")) {
        free_expr(condition);
        return NULL;
    }

    then_branch = parse_statement(parser);
    if (then_branch == NULL) {
        free_expr(condition);
        return NULL;
    }

    if (parser_match(parser, TOKEN_ELSE)) {
        else_branch = parse_statement(parser);
        if (else_branch == NULL) {
            free_expr(condition);
            free_stmt(then_branch);
            return NULL;
        }
    }

    if_stmt = create_if_stmt(condition, then_branch, else_branch, line);
    if (if_stmt == NULL) {
        parser_error_at_current(parser, "Out of memory while creating if statement.");
        free_expr(condition);
        free_stmt(then_branch);
        free_stmt(else_branch);
        return NULL;
    }

    return if_stmt;
}

static Stmt* parse_while_statement(Parser* parser)
{
    int line = parser->previous.line;
    Expr* condition;
    Stmt* body;
    Stmt* while_stmt;

    if (!parser_consume(parser, TOKEN_LPAREN, "Expected '(' after 'while'.")) {
        return NULL;
    }

    condition = parse_expression(parser);
    if (condition == NULL) {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_RPAREN, "Expected ')' after condition.")) {
        free_expr(condition);
        return NULL;
    }

    body = parse_statement(parser);
    if (body == NULL) {
        free_expr(condition);
        return NULL;
    }

    while_stmt = create_while_stmt(condition, body, line);
    if (while_stmt == NULL) {
        parser_error_at_current(parser, "Out of memory while creating while statement.");
        free_expr(condition);
        free_stmt(body);
        return NULL;
    }

    return while_stmt;
}

static Stmt* parse_return_statement(Parser* parser)
{
    int line = parser->previous.line;
    Expr* value = NULL;
    Stmt* stmt;

    if (!parser_check(parser, TOKEN_SEMICOLON)) {
        value = parse_expression(parser);
        if (value == NULL) {
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after return statement.")) {
        free_expr(value);
        return NULL;
    }

    stmt = create_return_stmt(value, line);
    if (stmt == NULL) {
        parser_error_at_current(parser, "Out of memory while creating return statement.");
        free_expr(value);
        return NULL;
    }

    return stmt;
}

/* Expression grammar */

static Expr* parse_assignment(Parser* parser)
{
    Expr* left = parse_logical_or(parser);

    if (left == NULL) {
        return NULL;
    }

    if (parser_match(parser, TOKEN_ASSIGN)) {
        int operator_line = parser->previous.line;
        Expr* right = parse_assignment(parser);
        Expr* assignment;

        if (right == NULL) {
            free_expr(left);
            return NULL;
        }

        if (left->type != EXPR_IDENTIFIER && left->type != EXPR_INDEX) {
            parser_error_at_previous(parser, "Invalid assignment target.");
            free_expr(left);
            free_expr(right);
            return NULL;
        }

        if (left->type == EXPR_IDENTIFIER) {
            assignment = create_assignment_expr(left->as.identifier.name, right, operator_line);
            free_expr(left);
        } else {
            Expr* object = left->as.index.object;
            Expr* index = left->as.index.index;
            left->as.index.object = NULL;
            left->as.index.index = NULL;
            free_expr(left);
            assignment = create_index_assignment_expr(object, index, right, operator_line);
        }

        if (assignment == NULL) {
            parser_error_at_current(parser, "Out of memory while creating assignment expression.");
            free_expr(right);
            return NULL;
        }

        return assignment;
    }

    return left;
}

static Expr* parse_logical_or(Parser* parser)
{
    Expr* expr = parse_logical_and(parser);

    while (parser_match(parser, TOKEN_OROR)) {
        TokenType operator_type = parser->previous.type;
        int operator_line = parser->previous.line;
        Expr* right = parse_logical_and(parser);
        Expr* next;

        if (right == NULL) {
            free_expr(expr);
            return NULL;
        }

        next = create_binary_expr(expr, operator_type, right, operator_line);
        if (next == NULL) {
            parser_error_at_current(parser, "Out of memory while creating logical-or node.");
            free_expr(expr);
            free_expr(right);
            return NULL;
        }

        expr = next;
    }

    return expr;
}

static Expr* parse_logical_and(Parser* parser)
{
    Expr* expr = parse_equality(parser);

    while (parser_match(parser, TOKEN_ANDAND)) {
        TokenType operator_type = parser->previous.type;
        int operator_line = parser->previous.line;
        Expr* right = parse_equality(parser);
        Expr* next;

        if (right == NULL) {
            free_expr(expr);
            return NULL;
        }

        next = create_binary_expr(expr, operator_type, right, operator_line);
        if (next == NULL) {
            parser_error_at_current(parser, "Out of memory while creating logical-and node.");
            free_expr(expr);
            free_expr(right);
            return NULL;
        }

        expr = next;
    }

    return expr;
}

static Expr* parse_equality(Parser* parser)
{
    const TokenType operators[] = { TOKEN_EQ, TOKEN_BANG_EQ };
    Expr* expr = parse_comparison(parser);

    while (parser_match_any(
        parser,
        operators,
        sizeof(operators) / sizeof(operators[0])
    )) {
        TokenType operator_type = parser->previous.type;
        int operator_line = parser->previous.line;
        Expr* right = parse_comparison(parser);
        Expr* next;

        if (right == NULL) {
            free_expr(expr);
            return NULL;
        }

        next = create_binary_expr(expr, operator_type, right, operator_line);
        if (next == NULL) {
            parser_error_at_current(parser, "Out of memory while creating equality node.");
            free_expr(expr);
            free_expr(right);
            return NULL;
        }

        expr = next;
    }

    return expr;
}

static Expr* parse_comparison(Parser* parser)
{
    const TokenType operators[] = {
        TOKEN_GT,
        TOKEN_GTE,
        TOKEN_LT,
        TOKEN_LTE
    };
    Expr* expr = parse_term(parser);

    while (parser_match_any(
        parser,
        operators,
        sizeof(operators) / sizeof(operators[0])
    )) {
        TokenType operator_type = parser->previous.type;
        int operator_line = parser->previous.line;
        Expr* right = parse_term(parser);
        Expr* next;

        if (right == NULL) {
            free_expr(expr);
            return NULL;
        }

        next = create_binary_expr(expr, operator_type, right, operator_line);
        if (next == NULL) {
            parser_error_at_current(parser, "Out of memory while creating comparison node.");
            free_expr(expr);
            free_expr(right);
            return NULL;
        }

        expr = next;
    }

    return expr;
}

static Expr* parse_term(Parser* parser)
{
    const TokenType operators[] = { TOKEN_PLUS, TOKEN_MINUS };
    Expr* expr = parse_factor(parser);

    while (parser_match_any(
        parser,
        operators,
        sizeof(operators) / sizeof(operators[0])
    )) {
        TokenType operator_type = parser->previous.type;
        int operator_line = parser->previous.line;
        Expr* right = parse_factor(parser);
        Expr* next;

        if (right == NULL) {
            free_expr(expr);
            return NULL;
        }

        next = create_binary_expr(expr, operator_type, right, operator_line);
        if (next == NULL) {
            parser_error_at_current(parser, "Out of memory while creating term node.");
            free_expr(expr);
            free_expr(right);
            return NULL;
        }

        expr = next;
    }

    return expr;
}

static Expr* parse_factor(Parser* parser)
{
    const TokenType operators[] = {
        TOKEN_STAR,
        TOKEN_SLASH,
        TOKEN_PERCENT
    };
    Expr* expr = parse_unary(parser);

    while (parser_match_any(
        parser,
        operators,
        sizeof(operators) / sizeof(operators[0])
    )) {
        TokenType operator_type = parser->previous.type;
        int operator_line = parser->previous.line;
        Expr* right = parse_unary(parser);
        Expr* next;

        if (right == NULL) {
            free_expr(expr);
            return NULL;
        }

        next = create_binary_expr(expr, operator_type, right, operator_line);
        if (next == NULL) {
            parser_error_at_current(parser, "Out of memory while creating factor node.");
            free_expr(expr);
            free_expr(right);
            return NULL;
        }

        expr = next;
    }

    return expr;
}

static Expr* parse_unary(Parser* parser)
{
    const TokenType operators[] = {
        TOKEN_BANG,
        TOKEN_MINUS,
        TOKEN_PLUS
    };

    if (parser_match_any(
            parser,
            operators,
            sizeof(operators) / sizeof(operators[0])
        )) {
        TokenType operator_type = parser->previous.type;
        int operator_line = parser->previous.line;
        Expr* operand = parse_unary(parser);
        Expr* unary_expr;

        if (operand == NULL) {
            return NULL;
        }

        unary_expr = create_unary_expr(operator_type, operand, operator_line);
        if (unary_expr == NULL) {
            parser_error_at_current(parser, "Out of memory while creating unary node.");
            free_expr(operand);
            return NULL;
        }

        return unary_expr;
    }

    return parse_call(parser);
}

static Expr* parse_call(Parser* parser)
{
    Expr* expr = parse_primary(parser);

    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
      if (parser_match(parser, TOKEN_LPAREN)) {
        Expr* call_expr = create_call_expr(expr, parser->previous.line);

        if (call_expr == NULL) {
            parser_error_at_current(parser, "Out of memory while creating call expression.");
            free_expr(expr);
            return NULL;
        }

        if (!parser_check(parser, TOKEN_RPAREN)) {
            do {
                Expr* argument;

                if (call_expr->as.call.argument_count >= MAX_CALL_ARGUMENTS) {
                    parser_error_at_current(parser, "Too many call arguments.");
                    free_expr(call_expr);
                    return NULL;
                }

                argument = parse_expression(parser);
                if (argument == NULL) {
                    free_expr(call_expr);
                    return NULL;
                }

                if (!call_expr_add_argument(call_expr, argument)) {
                    parser_error_at_current(parser, "Out of memory while appending call argument.");
                    free_expr(argument);
                    free_expr(call_expr);
                    return NULL;
                }
            } while (parser_match(parser, TOKEN_COMMA));
        }

        if (!parser_consume(parser, TOKEN_RPAREN, "Expected ')' after arguments.")) {
            free_expr(call_expr);
            return NULL;
        }

        expr = call_expr;
      } else if (parser_match(parser, TOKEN_LBRACKET)) {
        int line = parser->previous.line;
        Expr* index = parse_expression(parser);
        Expr* indexed;
        if (index == NULL || !parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after list index.")) {
            free_expr(index); free_expr(expr); return NULL;
        }
        indexed = create_index_expr(expr, index, line);
        if (indexed == NULL) { free_expr(expr); free_expr(index); return NULL; }
        expr = indexed;
      } else if (parser_match(parser, TOKEN_DOT)) {
        int line = parser->previous.line;
        char* method_name;
        Expr* method;
        if (!parser_consume(parser, TOKEN_IDENTIFIER, "Expected method name after '.'.")) { free_expr(expr); return NULL; }
        method_name = parser_copy_string(parser->previous.value);
        if (method_name == NULL || !parser_consume(parser, TOKEN_LPAREN, "Expected '(' after method name.")) { free(method_name); free_expr(expr); return NULL; }
        method = create_method_call_expr(expr, method_name, line); free(method_name);
        if (method == NULL) { free_expr(expr); return NULL; }
        if (!parser_check(parser, TOKEN_RPAREN)) {
            do {
                Expr* argument = parse_expression(parser);
                if (argument == NULL || !method_call_add_argument(method, argument)) { free_expr(argument); free_expr(method); return NULL; }
            } while (parser_match(parser, TOKEN_COMMA));
        }
        if (!parser_consume(parser, TOKEN_RPAREN, "Expected ')' after method arguments.")) { free_expr(method); return NULL; }
        expr = method;
      } else break;
    }

    return expr;
}

static Expr* parse_primary(Parser* parser)
{
    if (parser_match(parser, TOKEN_LBRACKET)) {
        int line = parser->previous.line;
        Expr* list = create_list_literal_expr(line);
        if (list == NULL) return NULL;
        if (!parser_check(parser, TOKEN_RBRACKET)) {
            do {
                Expr* element = parse_expression(parser);
                if (element == NULL || !list_literal_add_element(list, element)) { free_expr(element); free_expr(list); return NULL; }
            } while (parser_match(parser, TOKEN_COMMA));
        }
        if (!parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after list literal.")) { free_expr(list); return NULL; }
        return list;
    }

    if (parser_check(parser, TOKEN_INT) || parser_check(parser, TOKEN_FLOAT) || parser_check(parser, TOKEN_STRING)) {
        TokenType target = parser->current.type;
        int line = parser->current.line;
        Expr* value;
        parser_advance(parser);
        if (!parser_consume(parser, TOKEN_LPAREN, "Expected '(' after conversion type.")) return NULL;
        value = parse_expression(parser);
        if (value == NULL || !parser_consume(parser, TOKEN_RPAREN, "Expected ')' after conversion value.")) { free_expr(value); return NULL; }
        return create_conversion_expr(target, value, line);
    }
    if (parser_match(parser, TOKEN_INT_LITERAL)) {
        return create_literal_expr(
            EXPR_INT_LITERAL,
            parser->previous.value,
            parser->previous.line
        );
    }

    if (parser_match(parser, TOKEN_FLOAT_LITERAL)) {
        return create_literal_expr(
            EXPR_FLOAT_LITERAL,
            parser->previous.value,
            parser->previous.line
        );
    }

    if (parser_match(parser, TOKEN_STRING_LITERAL)) {
        return create_literal_expr(
            EXPR_STRING_LITERAL,
            parser->previous.value,
            parser->previous.line
        );
    }

    if (parser_match(parser, TOKEN_TRUE)) {
        return create_literal_expr(
            EXPR_BOOL_LITERAL,
            parser->previous.value,
            parser->previous.line
        );
    }

    if (parser_match(parser, TOKEN_FALSE)) {
        return create_literal_expr(
            EXPR_BOOL_LITERAL,
            parser->previous.value,
            parser->previous.line
        );
    }

    if (parser_match(parser, TOKEN_IDENTIFIER)) {
        return create_identifier_expr(
            parser->previous.value,
            parser->previous.line
        );
    }

    if (parser_match(parser, TOKEN_LPAREN)) {
        Expr* expression = parse_expression(parser);

        if (expression == NULL) {
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_RPAREN, "Expected ')' after expression.")) {
            free_expr(expression);
            return NULL;
        }

        return expression;
    }

    parser_error_at_current(parser, "Expected an expression.");
    return NULL;
}
