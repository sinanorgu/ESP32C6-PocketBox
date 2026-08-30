#include "clumsyPL/ast.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AST_GROWTH_FACTOR 2U

static void* checked_malloc(size_t size)
{
    void* pointer = malloc(size);

    if (pointer == NULL) {
        fprintf(stderr, "Fatal error: memory allocation failed.\n");
    }

    return pointer;
}

static char* copy_string(const char* source)
{
    size_t length;
    char* result;

    if (source == NULL) {
        return NULL;
    }

    length = strlen(source);
    result = (char*)checked_malloc(length + 1U);

    if (result == NULL) {
        return NULL;
    }

    memcpy(result, source, length + 1U);
    return result;
}

static int ensure_capacity_void_ptr(
    void** array,
    size_t element_size,
    size_t* capacity,
    size_t min_capacity
)
{
    size_t new_capacity;
    void* new_buffer;

    if (*capacity >= min_capacity) {
        return 1;
    }

    new_capacity = (*capacity == 0U) ? 4U : (*capacity * AST_GROWTH_FACTOR);
    while (new_capacity < min_capacity) {
        new_capacity *= AST_GROWTH_FACTOR;
    }

    new_buffer = realloc(*array, new_capacity * element_size);
    if (new_buffer == NULL) {
        return 0;
    }

    *array = new_buffer;
    *capacity = new_capacity;
    return 1;
}

static Expr* create_expr_base(ExprType type, int line)
{
    Expr* expr = (Expr*)checked_malloc(sizeof(Expr));

    if (expr == NULL) {
        return NULL;
    }

    memset(expr, 0, sizeof(Expr));
    expr->type = type;
    expr->line = line;

    return expr;
}

static Stmt* create_stmt_base(StmtType type, int line)
{
    Stmt* stmt = (Stmt*)checked_malloc(sizeof(Stmt));

    if (stmt == NULL) {
        return NULL;
    }

    memset(stmt, 0, sizeof(Stmt));
    stmt->type = type;
    stmt->line = line;

    return stmt;
}

Expr* create_literal_expr(
    ExprType type,
    const char* value,
    int line
)
{
    Expr* expr = create_expr_base(type, line);

    if (expr == NULL) {
        return NULL;
    }

    expr->as.literal.value = copy_string(value);

    if (expr->as.literal.value == NULL) {
        free(expr);
        return NULL;
    }

    return expr;
}

Expr* create_identifier_expr(
    const char* name,
    int line
)
{
    Expr* expr = create_expr_base(EXPR_IDENTIFIER, line);

    if (expr == NULL) {
        return NULL;
    }

    expr->as.identifier.name = copy_string(name);

    if (expr->as.identifier.name == NULL) {
        free(expr);
        return NULL;
    }

    return expr;
}

Expr* create_unary_expr(
    TokenType operator_type,
    Expr* operand,
    int line
)
{
    Expr* expr = create_expr_base(EXPR_UNARY, line);

    if (expr == NULL) {
        return NULL;
    }

    expr->as.unary.operator_type = operator_type;
    expr->as.unary.operand = operand;

    return expr;
}

Expr* create_binary_expr(
    Expr* left,
    TokenType operator_type,
    Expr* right,
    int line
)
{
    Expr* expr = create_expr_base(EXPR_BINARY, line);

    if (expr == NULL) {
        return NULL;
    }

    expr->as.binary.left = left;
    expr->as.binary.operator_type = operator_type;
    expr->as.binary.right = right;

    return expr;
}

Expr* create_assignment_expr(
    const char* target_name,
    Expr* value,
    int line
)
{
    Expr* expr = create_expr_base(EXPR_ASSIGNMENT, line);

    if (expr == NULL) {
        return NULL;
    }

    expr->as.assignment.target_name = copy_string(target_name);
    if (expr->as.assignment.target_name == NULL) {
        free(expr);
        return NULL;
    }

    expr->as.assignment.value = value;
    return expr;
}

Expr* create_call_expr(
    Expr* callee,
    int line
)
{
    Expr* expr = create_expr_base(EXPR_CALL, line);

    if (expr == NULL) {
        return NULL;
    }

    expr->as.call.callee = callee;
    return expr;
}

int call_expr_add_argument(
    Expr* call_expr,
    Expr* argument
)
{
    if (call_expr == NULL || call_expr->type != EXPR_CALL) {
        return 0;
    }

    if (!ensure_capacity_void_ptr(
            (void**)&call_expr->as.call.arguments,
            sizeof(Expr*),
            &call_expr->as.call.argument_capacity,
            call_expr->as.call.argument_count + 1U
        )) {
        return 0;
    }

    call_expr->as.call.arguments[
        call_expr->as.call.argument_count
    ] = argument;
    call_expr->as.call.argument_count++;

    return 1;
}

Expr* create_list_literal_expr(int line) { return create_expr_base(EXPR_LIST_LITERAL, line); }

int list_literal_add_element(Expr* list, Expr* element)
{
    if (list == NULL || list->type != EXPR_LIST_LITERAL) return 0;
    if (!ensure_capacity_void_ptr((void**)&list->as.list_literal.elements, sizeof(Expr*),
            &list->as.list_literal.element_capacity, list->as.list_literal.element_count + 1U)) return 0;
    list->as.list_literal.elements[list->as.list_literal.element_count++] = element;
    return 1;
}

Expr* create_index_expr(Expr* object, Expr* index, int line)
{
    Expr* expr = create_expr_base(EXPR_INDEX, line);
    if (expr != NULL) { expr->as.index.object = object; expr->as.index.index = index; }
    return expr;
}

Expr* create_method_call_expr(Expr* object, const char* method_name, int line)
{
    Expr* expr = create_expr_base(EXPR_METHOD_CALL, line);
    if (expr == NULL) return NULL;
    expr->as.method_call.object = object;
    expr->as.method_call.method_name = copy_string(method_name);
    if (expr->as.method_call.method_name == NULL) { free(expr); return NULL; }
    return expr;
}

int method_call_add_argument(Expr* method, Expr* argument)
{
    if (method == NULL || method->type != EXPR_METHOD_CALL) return 0;
    if (!ensure_capacity_void_ptr((void**)&method->as.method_call.arguments, sizeof(Expr*),
            &method->as.method_call.argument_capacity, method->as.method_call.argument_count + 1U)) return 0;
    method->as.method_call.arguments[method->as.method_call.argument_count++] = argument;
    return 1;
}

Expr* create_conversion_expr(TokenType target_type, Expr* value, int line)
{
    Expr* expr = create_expr_base(EXPR_CONVERSION, line);
    if (expr != NULL) { expr->as.conversion.target_type = target_type; expr->as.conversion.value = value; }
    return expr;
}

Expr* create_index_assignment_expr(Expr* object, Expr* index, Expr* value, int line)
{
    Expr* expr = create_expr_base(EXPR_INDEX_ASSIGNMENT, line);
    if (expr != NULL) {
        expr->as.index_assignment.object = object;
        expr->as.index_assignment.index = index;
        expr->as.index_assignment.value = value;
    }
    return expr;
}

Stmt* create_expression_stmt(
    Expr* expression,
    int line
)
{
    Stmt* stmt = create_stmt_base(STMT_EXPRESSION, line);

    if (stmt == NULL) {
        return NULL;
    }

    stmt->as.expression.expression = expression;
    return stmt;
}

Stmt* create_variable_declaration_stmt(
    TokenType declared_type,
    const char* name,
    Expr* initializer,
    int line
)
{
    Stmt* stmt = create_stmt_base(STMT_VARIABLE_DECLARATION, line);

    if (stmt == NULL) {
        return NULL;
    }

    stmt->as.variable_declaration.declared_type = declared_type;
    stmt->as.variable_declaration.name = copy_string(name);
    if (stmt->as.variable_declaration.name == NULL) {
        free(stmt);
        return NULL;
    }
    stmt->as.variable_declaration.initializer = initializer;

    return stmt;
}

Stmt* create_block_stmt(int line)
{
    return create_stmt_base(STMT_BLOCK, line);
}

int block_stmt_add_statement(
    Stmt* block_stmt,
    Stmt* statement
)
{
    if (block_stmt == NULL || block_stmt->type != STMT_BLOCK) {
        return 0;
    }

    if (!ensure_capacity_void_ptr(
            (void**)&block_stmt->as.block.statements,
            sizeof(Stmt*),
            &block_stmt->as.block.statement_capacity,
            block_stmt->as.block.statement_count + 1U
        )) {
        return 0;
    }

    block_stmt->as.block.statements[
        block_stmt->as.block.statement_count
    ] = statement;
    block_stmt->as.block.statement_count++;

    return 1;
}

Stmt* create_if_stmt(
    Expr* condition,
    Stmt* then_branch,
    Stmt* else_branch,
    int line
)
{
    Stmt* stmt = create_stmt_base(STMT_IF, line);

    if (stmt == NULL) {
        return NULL;
    }

    stmt->as.if_stmt.condition = condition;
    stmt->as.if_stmt.then_branch = then_branch;
    stmt->as.if_stmt.else_branch = else_branch;
    return stmt;
}

Stmt* create_while_stmt(
    Expr* condition,
    Stmt* body,
    int line
)
{
    Stmt* stmt = create_stmt_base(STMT_WHILE, line);

    if (stmt == NULL) {
        return NULL;
    }

    stmt->as.while_stmt.condition = condition;
    stmt->as.while_stmt.body = body;
    return stmt;
}

Stmt* create_return_stmt(
    Expr* value,
    int line
)
{
    Stmt* stmt = create_stmt_base(STMT_RETURN, line);

    if (stmt == NULL) {
        return NULL;
    }

    stmt->as.return_stmt.value = value;
    return stmt;
}

Stmt* create_function_declaration_stmt(
    TokenType return_type,
    const char* name,
    int line
)
{
    Stmt* stmt = create_stmt_base(STMT_FUNCTION_DECLARATION, line);

    if (stmt == NULL) {
        return NULL;
    }

    stmt->as.function_declaration.return_type = return_type;
    stmt->as.function_declaration.name = copy_string(name);
    if (stmt->as.function_declaration.name == NULL) {
        free(stmt);
        return NULL;
    }

    return stmt;
}

int function_declaration_add_parameter(
    Stmt* function_stmt,
    TokenType declared_type,
    const char* name,
    int line
)
{
    Parameter* parameters;
    Parameter* parameter;

    if (function_stmt == NULL ||
        function_stmt->type != STMT_FUNCTION_DECLARATION) {
        return 0;
    }

    if (!ensure_capacity_void_ptr(
            (void**)&function_stmt->as.function_declaration.parameters,
            sizeof(Parameter),
            &function_stmt->as.function_declaration.parameter_capacity,
            function_stmt->as.function_declaration.parameter_count + 1U
        )) {
        return 0;
    }

    parameters = function_stmt->as.function_declaration.parameters;
    parameter = &parameters[
        function_stmt->as.function_declaration.parameter_count
    ];

    parameter->declared_type = declared_type;
    parameter->line = line;
    parameter->name = copy_string(name);
    if (parameter->name == NULL) {
        return 0;
    }

    function_stmt->as.function_declaration.parameter_count++;
    return 1;
}

int function_declaration_set_body(
    Stmt* function_stmt,
    Stmt* body
)
{
    if (function_stmt == NULL ||
        function_stmt->type != STMT_FUNCTION_DECLARATION) {
        return 0;
    }

    function_stmt->as.function_declaration.body = body;
    return 1;
}

Program* create_program(void)
{
    Program* program = (Program*)checked_malloc(sizeof(Program));

    if (program == NULL) {
        return NULL;
    }

    memset(program, 0, sizeof(Program));
    return program;
}

int program_add_statement(
    Program* program,
    Stmt* statement
)
{
    if (program == NULL) {
        return 0;
    }

    if (!ensure_capacity_void_ptr(
            (void**)&program->statements,
            sizeof(Stmt*),
            &program->statement_capacity,
            program->statement_count + 1U
        )) {
        return 0;
    }

    program->statements[program->statement_count] = statement;
    program->statement_count++;
    return 1;
}

void free_expr(Expr* expr)
{
    size_t i;

    if (expr == NULL) {
        return;
    }

    switch (expr->type) {
        case EXPR_INT_LITERAL:
        case EXPR_FLOAT_LITERAL:
        case EXPR_STRING_LITERAL:
        case EXPR_BOOL_LITERAL:
            free(expr->as.literal.value);
            break;

        case EXPR_IDENTIFIER:
            free(expr->as.identifier.name);
            break;

        case EXPR_UNARY:
            free_expr(expr->as.unary.operand);
            break;

        case EXPR_BINARY:
            free_expr(expr->as.binary.left);
            free_expr(expr->as.binary.right);
            break;

        case EXPR_ASSIGNMENT:
            free(expr->as.assignment.target_name);
            free_expr(expr->as.assignment.value);
            break;

        case EXPR_CALL:
            free_expr(expr->as.call.callee);
            for (i = 0U; i < expr->as.call.argument_count; ++i) {
                free_expr(expr->as.call.arguments[i]);
            }
            free(expr->as.call.arguments);
            break;
        case EXPR_LIST_LITERAL:
            for (i = 0U; i < expr->as.list_literal.element_count; ++i) free_expr(expr->as.list_literal.elements[i]);
            free(expr->as.list_literal.elements); break;
        case EXPR_INDEX: free_expr(expr->as.index.object); free_expr(expr->as.index.index); break;
        case EXPR_METHOD_CALL:
            free_expr(expr->as.method_call.object); free(expr->as.method_call.method_name);
            for (i = 0U; i < expr->as.method_call.argument_count; ++i) free_expr(expr->as.method_call.arguments[i]);
            free(expr->as.method_call.arguments); break;
        case EXPR_CONVERSION: free_expr(expr->as.conversion.value); break;
        case EXPR_INDEX_ASSIGNMENT:
            free_expr(expr->as.index_assignment.object);
            free_expr(expr->as.index_assignment.index);
            free_expr(expr->as.index_assignment.value);
            break;
    }

    free(expr);
}

void free_stmt(Stmt* stmt)
{
    size_t i;

    if (stmt == NULL) {
        return;
    }

    switch (stmt->type) {
        case STMT_EXPRESSION:
            free_expr(stmt->as.expression.expression);
            break;

        case STMT_VARIABLE_DECLARATION:
            free(stmt->as.variable_declaration.name);
            free_expr(stmt->as.variable_declaration.initializer);
            break;

        case STMT_BLOCK:
            for (i = 0U; i < stmt->as.block.statement_count; ++i) {
                free_stmt(stmt->as.block.statements[i]);
            }
            free(stmt->as.block.statements);
            break;

        case STMT_IF:
            free_expr(stmt->as.if_stmt.condition);
            free_stmt(stmt->as.if_stmt.then_branch);
            free_stmt(stmt->as.if_stmt.else_branch);
            break;

        case STMT_WHILE:
            free_expr(stmt->as.while_stmt.condition);
            free_stmt(stmt->as.while_stmt.body);
            break;

        case STMT_RETURN:
            free_expr(stmt->as.return_stmt.value);
            break;

        case STMT_FUNCTION_DECLARATION:
            free(stmt->as.function_declaration.name);
            for (i = 0U; i < stmt->as.function_declaration.parameter_count; ++i) {
                free(stmt->as.function_declaration.parameters[i].name);
            }
            free(stmt->as.function_declaration.parameters);
            free_stmt(stmt->as.function_declaration.body);
            break;
    }

    free(stmt);
}

void free_program(Program* program)
{
    size_t i;

    if (program == NULL) {
        return;
    }

    for (i = 0U; i < program->statement_count; ++i) {
        free_stmt(program->statements[i]);
    }

    free(program->statements);
    free(program);
}

static const char* operator_to_string(TokenType type)
{
    switch (type) {
        case TOKEN_PLUS:    return "+";
        case TOKEN_MINUS:   return "-";
        case TOKEN_STAR:    return "*";
        case TOKEN_SLASH:   return "/";
        case TOKEN_PERCENT: return "%";

        case TOKEN_GT:      return ">";
        case TOKEN_LT:      return "<";
        case TOKEN_GTE:     return ">=";
        case TOKEN_LTE:     return "<=";

        case TOKEN_EQ:      return "==";
        case TOKEN_BANG_EQ: return "!=";

        case TOKEN_BANG:    return "!";
        case TOKEN_ANDAND:  return "&&";
        case TOKEN_OROR:    return "||";
        case TOKEN_ASSIGN:  return "=";

        default:
            return "<unknown-operator>";
    }
}

static const char* type_to_string(TokenType type)
{
    switch (type) {
        case TOKEN_INT: return "int";
        case TOKEN_FLOAT: return "float";
        case TOKEN_STRING: return "string";
        case TOKEN_BOOL: return "bool";
        case TOKEN_VOID: return "void";
        case TOKEN_LIST: return "list";
        default: return "<unknown-type>";
    }
}

static void print_indent(int indent)
{
    int i;
    for (i = 0; i < indent; ++i) {
        printf("  ");
    }
}

static void print_expr_internal(const Expr* expr, int indent)
{
    size_t i;

    if (expr == NULL) {
        print_indent(indent);
        printf("<null-expression>\n");
        return;
    }

    switch (expr->type) {
        case EXPR_INT_LITERAL:
            print_indent(indent);
            printf("Int(%s)\n", expr->as.literal.value);
            break;

        case EXPR_FLOAT_LITERAL:
            print_indent(indent);
            printf("Float(%s)\n", expr->as.literal.value);
            break;

        case EXPR_STRING_LITERAL:
            print_indent(indent);
            printf("String(\"%s\")\n", expr->as.literal.value);
            break;

        case EXPR_BOOL_LITERAL:
            print_indent(indent);
            printf("Bool(%s)\n", expr->as.literal.value);
            break;

        case EXPR_IDENTIFIER:
            print_indent(indent);
            printf("Identifier(%s)\n", expr->as.identifier.name);
            break;

        case EXPR_UNARY:
            print_indent(indent);
            printf("Unary(%s)\n", operator_to_string(
                expr->as.unary.operator_type
            ));
            print_expr_internal(expr->as.unary.operand, indent + 1);
            break;

        case EXPR_BINARY:
            print_indent(indent);
            printf("Binary(%s)\n", operator_to_string(
                expr->as.binary.operator_type
            ));
            print_expr_internal(expr->as.binary.left, indent + 1);
            print_expr_internal(expr->as.binary.right, indent + 1);
            break;

        case EXPR_ASSIGNMENT:
            print_indent(indent);
            printf("Assignment(%s)\n", expr->as.assignment.target_name);
            print_expr_internal(expr->as.assignment.value, indent + 1);
            break;

        case EXPR_CALL:
            print_indent(indent);
            printf("Call\n");
            print_indent(indent + 1);
            printf("Callee:\n");
            print_expr_internal(expr->as.call.callee, indent + 2);
            print_indent(indent + 1);
            printf("Arguments:\n");
            for (i = 0U; i < expr->as.call.argument_count; ++i) {
                print_expr_internal(expr->as.call.arguments[i], indent + 2);
            }
            break;
        case EXPR_LIST_LITERAL:
            print_indent(indent); printf("List\n");
            for (i = 0U; i < expr->as.list_literal.element_count; ++i) print_expr_internal(expr->as.list_literal.elements[i], indent + 1);
            break;
        case EXPR_INDEX:
            print_indent(indent); printf("Index\n"); print_expr_internal(expr->as.index.object, indent + 1); print_expr_internal(expr->as.index.index, indent + 1); break;
        case EXPR_METHOD_CALL:
            print_indent(indent); printf("MethodCall(%s)\n", expr->as.method_call.method_name); print_expr_internal(expr->as.method_call.object, indent + 1);
            for (i = 0U; i < expr->as.method_call.argument_count; ++i) print_expr_internal(expr->as.method_call.arguments[i], indent + 1);
            break;
        case EXPR_CONVERSION:
            print_indent(indent); printf("Conversion(%s)\n", type_to_string(expr->as.conversion.target_type)); print_expr_internal(expr->as.conversion.value, indent + 1); break;
        case EXPR_INDEX_ASSIGNMENT:
            print_indent(indent); printf("IndexAssignment\n");
            print_expr_internal(expr->as.index_assignment.object, indent + 1);
            print_expr_internal(expr->as.index_assignment.index, indent + 1);
            print_expr_internal(expr->as.index_assignment.value, indent + 1);
            break;
    }
}

void print_expr(const Expr* expr)
{
    print_expr_internal(expr, 0);
}

static void print_stmt_internal(const Stmt* stmt, int indent)
{
    size_t i;

    if (stmt == NULL) {
        print_indent(indent);
        printf("<null-statement>\n");
        return;
    }

    switch (stmt->type) {
        case STMT_EXPRESSION:
            print_indent(indent);
            printf("ExpressionStmt\n");
            print_expr_internal(stmt->as.expression.expression, indent + 1);
            break;

        case STMT_VARIABLE_DECLARATION:
            print_indent(indent);
            printf("VariableDeclaration(type=%s, name=%s)\n",
                type_to_string(stmt->as.variable_declaration.declared_type),
                stmt->as.variable_declaration.name);
            if (stmt->as.variable_declaration.initializer != NULL) {
                print_indent(indent + 1);
                printf("Initializer:\n");
                print_expr_internal(
                    stmt->as.variable_declaration.initializer,
                    indent + 2
                );
            }
            break;

        case STMT_BLOCK:
            print_indent(indent);
            printf("BlockStmt\n");
            for (i = 0U; i < stmt->as.block.statement_count; ++i) {
                print_stmt_internal(stmt->as.block.statements[i], indent + 1);
            }
            break;

        case STMT_IF:
            print_indent(indent);
            printf("IfStmt\n");
            print_indent(indent + 1);
            printf("Condition:\n");
            print_expr_internal(stmt->as.if_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Then:\n");
            print_stmt_internal(stmt->as.if_stmt.then_branch, indent + 2);
            if (stmt->as.if_stmt.else_branch != NULL) {
                print_indent(indent + 1);
                printf("Else:\n");
                print_stmt_internal(stmt->as.if_stmt.else_branch, indent + 2);
            }
            break;

        case STMT_WHILE:
            print_indent(indent);
            printf("WhileStmt\n");
            print_indent(indent + 1);
            printf("Condition:\n");
            print_expr_internal(stmt->as.while_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            print_stmt_internal(stmt->as.while_stmt.body, indent + 2);
            break;

        case STMT_RETURN:
            print_indent(indent);
            printf("ReturnStmt\n");
            if (stmt->as.return_stmt.value != NULL) {
                print_expr_internal(stmt->as.return_stmt.value, indent + 1);
            }
            break;

        case STMT_FUNCTION_DECLARATION:
            print_indent(indent);
            printf("FunctionDeclaration(type=%s, name=%s)\n",
                type_to_string(stmt->as.function_declaration.return_type),
                stmt->as.function_declaration.name);
            print_indent(indent + 1);
            printf("Parameters:\n");
            for (i = 0U; i < stmt->as.function_declaration.parameter_count; ++i) {
                const Parameter* parameter =
                    &stmt->as.function_declaration.parameters[i];
                print_indent(indent + 2);
                printf("%s %s\n",
                    type_to_string(parameter->declared_type),
                    parameter->name);
            }
            print_indent(indent + 1);
            printf("Body:\n");
            print_stmt_internal(
                stmt->as.function_declaration.body,
                indent + 2
            );
            break;
    }
}

void print_stmt_ast(const Stmt* stmt)
{
    print_stmt_internal(stmt, 0);
}

void print_program_ast(const Program* program)
{
    size_t i;

    if (program == NULL) {
        printf("<null-program>\n");
        return;
    }

    printf("Program\n");
    for (i = 0U; i < program->statement_count; ++i) {
        print_stmt_internal(program->statements[i], 1);
    }
}
