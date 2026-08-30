#ifndef AST_H
#define AST_H

#include "lexer.hpp"

#include <stddef.h>

typedef enum {
    EXPR_INT_LITERAL,
    EXPR_FLOAT_LITERAL,
    EXPR_STRING_LITERAL,
    EXPR_BOOL_LITERAL,
    EXPR_IDENTIFIER,

    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGNMENT,
    EXPR_CALL,
    EXPR_LIST_LITERAL,
    EXPR_INDEX,
    EXPR_METHOD_CALL,
    EXPR_CONVERSION,
    EXPR_INDEX_ASSIGNMENT
} ExprType;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    TokenType declared_type;
    char* name;
    int line;
} Parameter;

typedef enum {
    STMT_EXPRESSION,
    STMT_VARIABLE_DECLARATION,
    STMT_BLOCK,
    STMT_IF,
    STMT_WHILE,
    STMT_RETURN,
    STMT_FUNCTION_DECLARATION
} StmtType;

struct Expr {
    ExprType type;
    int line;

    union {
        struct {
            char* value;
        } literal;

        struct {
            char* name;
        } identifier;

        struct {
            TokenType operator_type;
            Expr* operand;
        } unary;

        struct {
            Expr* left;
            TokenType operator_type;
            Expr* right;
        } binary;

        struct {
            char* target_name;
            Expr* value;
        } assignment;

        struct {
            Expr* callee;
            Expr** arguments;
            size_t argument_count;
            size_t argument_capacity;
        } call;

        struct {
            Expr** elements;
            size_t element_count;
            size_t element_capacity;
        } list_literal;

        struct { Expr* object; Expr* index; } index;
        struct {
            Expr* object;
            char* method_name;
            Expr** arguments;
            size_t argument_count;
            size_t argument_capacity;
        } method_call;
        struct { TokenType target_type; Expr* value; } conversion;
        struct { Expr* object; Expr* index; Expr* value; } index_assignment;
    } as;
};

struct Stmt {
    StmtType type;
    int line;

    union {
        struct {
            Expr* expression;
        } expression;

        struct {
            TokenType declared_type;
            char* name;
            Expr* initializer;
        } variable_declaration;

        struct {
            Stmt** statements;
            size_t statement_count;
            size_t statement_capacity;
        } block;

        struct {
            Expr* condition;
            Stmt* then_branch;
            Stmt* else_branch;
        } if_stmt;

        struct {
            Expr* condition;
            Stmt* body;
        } while_stmt;

        struct {
            Expr* value;
        } return_stmt;

        struct {
            TokenType return_type;
            char* name;
            Parameter* parameters;
            size_t parameter_count;
            size_t parameter_capacity;
            Stmt* body;
        } function_declaration;
    } as;
};

typedef struct {
    Stmt** statements;
    size_t statement_count;
    size_t statement_capacity;
} Program;

Expr* create_literal_expr(
    ExprType type,
    const char* value,
    int line
);

Expr* create_identifier_expr(
    const char* name,
    int line
);

Expr* create_unary_expr(
    TokenType operator_type,
    Expr* operand,
    int line
);

Expr* create_binary_expr(
    Expr* left,
    TokenType operator_type,
    Expr* right,
    int line
);

Expr* create_assignment_expr(
    const char* target_name,
    Expr* value,
    int line
);

Expr* create_call_expr(
    Expr* callee,
    int line
);

int call_expr_add_argument(
    Expr* call_expr,
    Expr* argument
);
Expr* create_list_literal_expr(int line);
int list_literal_add_element(Expr* list, Expr* element);
Expr* create_index_expr(Expr* object, Expr* index, int line);
Expr* create_method_call_expr(Expr* object, const char* method_name, int line);
int method_call_add_argument(Expr* method, Expr* argument);
Expr* create_conversion_expr(TokenType target_type, Expr* value, int line);
Expr* create_index_assignment_expr(Expr* object, Expr* index, Expr* value, int line);

Stmt* create_expression_stmt(
    Expr* expression,
    int line
);

Stmt* create_variable_declaration_stmt(
    TokenType declared_type,
    const char* name,
    Expr* initializer,
    int line
);

Stmt* create_block_stmt(int line);

int block_stmt_add_statement(
    Stmt* block_stmt,
    Stmt* statement
);

Stmt* create_if_stmt(
    Expr* condition,
    Stmt* then_branch,
    Stmt* else_branch,
    int line
);

Stmt* create_while_stmt(
    Expr* condition,
    Stmt* body,
    int line
);

Stmt* create_return_stmt(
    Expr* value,
    int line
);

Stmt* create_function_declaration_stmt(
    TokenType return_type,
    const char* name,
    int line
);

int function_declaration_add_parameter(
    Stmt* function_stmt,
    TokenType declared_type,
    const char* name,
    int line
);

int function_declaration_set_body(
    Stmt* function_stmt,
    Stmt* body
);

Program* create_program(void);

int program_add_statement(
    Program* program,
    Stmt* statement
);

void free_expr(Expr* expr);
void free_stmt(Stmt* stmt);
void free_program(Program* program);

void print_expr(const Expr* expr);
void print_stmt_ast(const Stmt* stmt);
void print_program_ast(const Program* program);

#endif
