#include "clumsyPL/semantic.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEMANTIC_GROWTH_FACTOR 2U

static int ensure_capacity(
    void** buffer,
    size_t element_size,
    size_t* capacity,
    size_t min_capacity
);
static char* semantic_copy_string(const char* text);

static Scope* create_scope(Scope* parent);
static void free_scope(Scope* scope);
static Symbol* scope_lookup_current(Scope* scope, const char* name);
static Symbol* scope_lookup(Scope* scope, const char* name);
static int scope_define_symbol(Scope* scope, const Symbol* symbol);

static int symbol_init_variable(
    Symbol* symbol,
    const char* name,
    SymbolKind kind,
    SemanticType type,
    int line,
    SymbolState state
);
static int symbol_init_function(
    Symbol* symbol,
    const char* name,
    int line,
    SemanticType return_type,
    const SemanticType* parameter_types,
    size_t parameter_count,
    int is_variadic,
    int accepts_any
);
static void symbol_free(Symbol* symbol);

static FunctionSignature function_signature_copy(
    const FunctionSignature* signature
);
static void function_signature_free(FunctionSignature* signature);

static int add_diagnostic_v(
    SemanticAnalyzer* analyzer,
    DiagnosticSeverity severity,
    int line,
    const char* format,
    va_list args
);
static int add_error(
    SemanticAnalyzer* analyzer,
    int line,
    const char* format,
    ...
);
static int add_warning(
    SemanticAnalyzer* analyzer,
    int line,
    const char* format,
    ...
);

static int begin_scope(SemanticAnalyzer* analyzer);
static void end_scope(SemanticAnalyzer* analyzer);

static void collect_top_level_declarations(
    SemanticAnalyzer* analyzer,
    const Program* program
);

static void analyze_statement(SemanticAnalyzer* analyzer, const Stmt* stmt);
static void analyze_block_contents(
    SemanticAnalyzer* analyzer,
    const Stmt* block_stmt,
    int create_child_scope
);
static SemanticType analyze_expression(
    SemanticAnalyzer* analyzer,
    const Expr* expr
);

static int statement_guarantees_return(const Stmt* stmt);
static int is_numeric_type(SemanticType type);
static SemanticType numeric_result_type(SemanticType left, SemanticType right);
static int can_compare_equality(SemanticType left, SemanticType right);

SemanticType semantic_type_from_token(TokenType type)
{
    switch (type) {
        case TOKEN_VOID: return SEM_TYPE_VOID;
        case TOKEN_INT: return SEM_TYPE_INT;
        case TOKEN_FLOAT: return SEM_TYPE_FLOAT;
        case TOKEN_STRING: return SEM_TYPE_STRING;
        case TOKEN_BOOL: return SEM_TYPE_BOOL;
        case TOKEN_LIST: return SEM_TYPE_LIST;
        default: return SEM_TYPE_ERROR;
    }
}

const char* semantic_type_to_string(SemanticType type)
{
    switch (type) {
        case SEM_TYPE_ERROR: return "error";
        case SEM_TYPE_VOID: return "void";
        case SEM_TYPE_INT: return "int";
        case SEM_TYPE_FLOAT: return "float";
        case SEM_TYPE_STRING: return "string";
        case SEM_TYPE_BOOL: return "bool";
        case SEM_TYPE_LIST: return "list";
        case SEM_TYPE_FUNCTION: return "function";
        default: return "unknown";
    }
}

int semantic_can_assign(SemanticType destination, SemanticType source)
{
    if (destination == SEM_TYPE_ERROR || source == SEM_TYPE_ERROR) {
        return 1;
    }

    if (destination == source) {
        return 1;
    }

    if (destination == SEM_TYPE_FLOAT && source == SEM_TYPE_INT) {
        return 1;
    }

    return 0;
}

SemanticAnalyzer create_semantic_analyzer(void)
{
    SemanticAnalyzer analyzer;

    memset(&analyzer, 0, sizeof(SemanticAnalyzer));

    analyzer.global_scope = create_scope(NULL);
    analyzer.current_scope = analyzer.global_scope;
    analyzer.current_function_return_type = SEM_TYPE_VOID;
    analyzer.current_function_name = NULL;

    if (analyzer.global_scope == NULL) {
        analyzer.had_fatal_memory_error = 1;
        analyzer.had_error = 1;
    }

    return analyzer;
}

void destroy_semantic_analyzer(SemanticAnalyzer* analyzer)
{
    size_t i;

    if (analyzer == NULL) {
        return;
    }

    free_scope(analyzer->global_scope);
    analyzer->global_scope = NULL;
    analyzer->current_scope = NULL;

    for (i = 0U; i < analyzer->diagnostic_count; ++i) {
        free(analyzer->diagnostics[i].message);
    }

    free(analyzer->diagnostics);
    analyzer->diagnostics = NULL;
    analyzer->diagnostic_count = 0U;
    analyzer->diagnostic_capacity = 0U;
}

int semantic_define_native_function(
    SemanticAnalyzer* analyzer,
    const char* name,
    SemanticType return_type,
    const SemanticType* parameter_types,
    size_t parameter_count
)
{
    return semantic_define_native_function_ex(
        analyzer,
        name,
        return_type,
        parameter_types,
        parameter_count,
        0,
        0
    );
}

int semantic_define_native_function_ex(
    SemanticAnalyzer* analyzer,
    const char* name,
    SemanticType return_type,
    const SemanticType* parameter_types,
    size_t parameter_count,
    int is_variadic,
    int accepts_any
)
{
    Symbol symbol;

    if (analyzer == NULL || analyzer->global_scope == NULL || name == NULL) {
        return 0;
    }

    if (scope_lookup_current(analyzer->global_scope, name) != NULL) {
        add_error(analyzer, 0, "Native function '%s' collides with existing symbol.", name);
        return 0;
    }

    if (!symbol_init_function(
            &symbol,
            name,
            0,
            return_type,
            parameter_types,
            parameter_count,
            is_variadic,
            accepts_any
        )) {
        analyzer->had_fatal_memory_error = 1;
        analyzer->had_error = 1;
        return 0;
    }

    if (!scope_define_symbol(analyzer->global_scope, &symbol)) {
        symbol_free(&symbol);
        analyzer->had_fatal_memory_error = 1;
        analyzer->had_error = 1;
        return 0;
    }

    symbol_free(&symbol);
    return 1;
}

size_t semantic_error_count(const SemanticAnalyzer* analyzer)
{
    size_t i;
    size_t count = 0U;

    if (analyzer == NULL) {
        return 0U;
    }

    for (i = 0U; i < analyzer->diagnostic_count; ++i) {
        if (analyzer->diagnostics[i].severity == DIAGNOSTIC_ERROR) {
            count++;
        }
    }

    return count;
}

size_t semantic_warning_count(const SemanticAnalyzer* analyzer)
{
    size_t i;
    size_t count = 0U;

    if (analyzer == NULL) {
        return 0U;
    }

    for (i = 0U; i < analyzer->diagnostic_count; ++i) {
        if (analyzer->diagnostics[i].severity == DIAGNOSTIC_WARNING) {
            count++;
        }
    }

    return count;
}

void semantic_print_diagnostics(const SemanticAnalyzer* analyzer)
{
    size_t i;

    if (analyzer == NULL) {
        return;
    }

    for (i = 0U; i < analyzer->diagnostic_count; ++i) {
        const SemanticDiagnostic* diagnostic = &analyzer->diagnostics[i];
        const char* label = diagnostic->severity == DIAGNOSTIC_ERROR
            ? "error"
            : "warning";

        printf(
            "[line %d] Semantic %s: %s\n",
            diagnostic->line,
            label,
            diagnostic->message
        );
    }
}

int semantic_analyze_program(
    SemanticAnalyzer* analyzer,
    const Program* program
)
{
    size_t i;

    if (analyzer == NULL || analyzer->global_scope == NULL || program == NULL) {
        return 0;
    }

    analyzer->current_scope = analyzer->global_scope;
    analyzer->inside_function = 0;
    analyzer->current_function_return_type = SEM_TYPE_VOID;
    analyzer->current_function_name = NULL;

    collect_top_level_declarations(analyzer, program);

    for (i = 0U; i < program->statement_count; ++i) {
        analyze_statement(analyzer, program->statements[i]);
    }

    if (analyzer->had_fatal_memory_error) {
        return 0;
    }

    return semantic_error_count(analyzer) == 0U;
}

static int ensure_capacity(
    void** buffer,
    size_t element_size,
    size_t* capacity,
    size_t min_capacity
)
{
    size_t new_capacity;
    void* resized;

    if (*capacity >= min_capacity) {
        return 1;
    }

    new_capacity = (*capacity == 0U) ? 4U : (*capacity * SEMANTIC_GROWTH_FACTOR);
    while (new_capacity < min_capacity) {
        new_capacity *= SEMANTIC_GROWTH_FACTOR;
    }

    resized = realloc(*buffer, new_capacity * element_size);
    if (resized == NULL) {
        return 0;
    }

    *buffer = resized;
    *capacity = new_capacity;
    return 1;
}

static char* semantic_copy_string(const char* text)
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

static Scope* create_scope(Scope* parent)
{
    Scope* scope = (Scope*)malloc(sizeof(Scope));

    if (scope == NULL) {
        return NULL;
    }

    scope->symbols = NULL;
    scope->count = 0U;
    scope->capacity = 0U;
    scope->parent = parent;

    return scope;
}

static void free_scope(Scope* scope)
{
    size_t i;

    if (scope == NULL) {
        return;
    }

    for (i = 0U; i < scope->count; ++i) {
        symbol_free(&scope->symbols[i]);
    }

    free(scope->symbols);
    free(scope);
}

static Symbol* scope_lookup_current(Scope* scope, const char* name)
{
    size_t i;

    if (scope == NULL || name == NULL) {
        return NULL;
    }

    for (i = 0U; i < scope->count; ++i) {
        if (strcmp(scope->symbols[i].name, name) == 0) {
            return &scope->symbols[i];
        }
    }

    return NULL;
}

static Symbol* scope_lookup(Scope* scope, const char* name)
{
    Scope* cursor = scope;

    while (cursor != NULL) {
        Symbol* symbol = scope_lookup_current(cursor, name);
        if (symbol != NULL) {
            return symbol;
        }
        cursor = cursor->parent;
    }

    return NULL;
}

static int scope_define_symbol(Scope* scope, const Symbol* symbol)
{
    Symbol* destination;

    if (scope == NULL || symbol == NULL) {
        return 0;
    }

    if (!ensure_capacity(
            (void**)&scope->symbols,
            sizeof(Symbol),
            &scope->capacity,
            scope->count + 1U
        )) {
        return 0;
    }

    destination = &scope->symbols[scope->count];
    destination->name = semantic_copy_string(symbol->name);
    if (destination->name == NULL) {
        return 0;
    }

    destination->kind = symbol->kind;
    destination->type = symbol->type;
    destination->line = symbol->line;
    destination->state = symbol->state;
    destination->function = function_signature_copy(&symbol->function);

    if (symbol->kind == SYMBOL_FUNCTION &&
        destination->function.parameter_count != symbol->function.parameter_count) {
        free(destination->name);
        destination->name = NULL;
        return 0;
    }

    scope->count++;
    return 1;
}

static int symbol_init_variable(
    Symbol* symbol,
    const char* name,
    SymbolKind kind,
    SemanticType type,
    int line,
    SymbolState state
)
{
    if (symbol == NULL || name == NULL) {
        return 0;
    }

    memset(symbol, 0, sizeof(Symbol));

    symbol->name = semantic_copy_string(name);
    if (symbol->name == NULL) {
        return 0;
    }

    symbol->kind = kind;
    symbol->type = type;
    symbol->line = line;
    symbol->state = state;

    return 1;
}

static int symbol_init_function(
    Symbol* symbol,
    const char* name,
    int line,
    SemanticType return_type,
    const SemanticType* parameter_types,
    size_t parameter_count,
    int is_variadic,
    int accepts_any
)
{
    if (!symbol_init_variable(
            symbol,
            name,
            SYMBOL_FUNCTION,
            SEM_TYPE_FUNCTION,
            line,
            SYMBOL_STATE_DEFINED
        )) {
        return 0;
    }

    symbol->function.return_type = return_type;
    symbol->function.parameter_count = parameter_count;
    symbol->function.is_variadic = is_variadic;
    symbol->function.accepts_any = accepts_any;

    if (parameter_count > 0U) {
        size_t bytes = parameter_count * sizeof(SemanticType);
        symbol->function.parameter_types = (SemanticType*)malloc(bytes);
        if (symbol->function.parameter_types == NULL) {
            symbol_free(symbol);
            return 0;
        }

        memcpy(symbol->function.parameter_types, parameter_types, bytes);
    }

    return 1;
}

static void symbol_free(Symbol* symbol)
{
    if (symbol == NULL) {
        return;
    }

    free(symbol->name);
    symbol->name = NULL;
    function_signature_free(&symbol->function);
}

static FunctionSignature function_signature_copy(
    const FunctionSignature* signature
)
{
    FunctionSignature copy;

    memset(&copy, 0, sizeof(FunctionSignature));

    if (signature == NULL) {
        return copy;
    }

    copy.return_type = signature->return_type;
    copy.parameter_count = signature->parameter_count;
    copy.is_variadic = signature->is_variadic;
    copy.accepts_any = signature->accepts_any;

    if (copy.parameter_count > 0U) {
        size_t bytes = copy.parameter_count * sizeof(SemanticType);
        copy.parameter_types = (SemanticType*)malloc(bytes);
        if (copy.parameter_types == NULL) {
            copy.parameter_count = 0U;
            return copy;
        }

        memcpy(copy.parameter_types, signature->parameter_types, bytes);
    }

    return copy;
}

static void function_signature_free(FunctionSignature* signature)
{
    if (signature == NULL) {
        return;
    }

    free(signature->parameter_types);
    signature->parameter_types = NULL;
    signature->parameter_count = 0U;
}

static int add_diagnostic_v(
    SemanticAnalyzer* analyzer,
    DiagnosticSeverity severity,
    int line,
    const char* format,
    va_list args
)
{
    va_list args_copy;
    int length;
    char* message;
    SemanticDiagnostic* diagnostic;

    if (analyzer == NULL || format == NULL) {
        return 0;
    }

    va_copy(args_copy, args);
    length = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (length < 0) {
        return 0;
    }

    message = (char*)malloc((size_t)length + 1U);
    if (message == NULL) {
        analyzer->had_fatal_memory_error = 1;
        analyzer->had_error = 1;
        return 0;
    }

    vsnprintf(message, (size_t)length + 1U, format, args);

    if (!ensure_capacity(
            (void**)&analyzer->diagnostics,
            sizeof(SemanticDiagnostic),
            &analyzer->diagnostic_capacity,
            analyzer->diagnostic_count + 1U
        )) {
        free(message);
        analyzer->had_fatal_memory_error = 1;
        analyzer->had_error = 1;
        return 0;
    }

    diagnostic = &analyzer->diagnostics[analyzer->diagnostic_count];
    diagnostic->severity = severity;
    diagnostic->line = line;
    diagnostic->message = message;
    analyzer->diagnostic_count++;

    if (severity == DIAGNOSTIC_ERROR) {
        analyzer->had_error = 1;
    }

    return 1;
}

static int add_error(
    SemanticAnalyzer* analyzer,
    int line,
    const char* format,
    ...
)
{
    int ok;
    va_list args;

    va_start(args, format);
    ok = add_diagnostic_v(analyzer, DIAGNOSTIC_ERROR, line, format, args);
    va_end(args);

    return ok;
}

static int add_warning(
    SemanticAnalyzer* analyzer,
    int line,
    const char* format,
    ...
)
{
    int ok;
    va_list args;

    va_start(args, format);
    ok = add_diagnostic_v(analyzer, DIAGNOSTIC_WARNING, line, format, args);
    va_end(args);

    return ok;
}

static int begin_scope(SemanticAnalyzer* analyzer)
{
    Scope* child;

    if (analyzer == NULL || analyzer->current_scope == NULL) {
        return 0;
    }

    child = create_scope(analyzer->current_scope);
    if (child == NULL) {
        analyzer->had_fatal_memory_error = 1;
        analyzer->had_error = 1;
        add_error(analyzer, 0, "Out of memory while creating scope.");
        return 0;
    }

    analyzer->current_scope = child;
    return 1;
}

static void end_scope(SemanticAnalyzer* analyzer)
{
    Scope* scope;

    if (analyzer == NULL || analyzer->current_scope == NULL) {
        return;
    }

    scope = analyzer->current_scope;
    analyzer->current_scope = scope->parent;
    free_scope(scope);
}

static void collect_top_level_declarations(
    SemanticAnalyzer* analyzer,
    const Program* program
)
{
    size_t i;

    for (i = 0U; i < program->statement_count; ++i) {
        const Stmt* statement = program->statements[i];

        if (statement->type != STMT_FUNCTION_DECLARATION) {
            continue;
        }

        if (scope_lookup_current(
                analyzer->global_scope,
                statement->as.function_declaration.name
            ) != NULL) {
            add_error(
                analyzer,
                statement->line,
                "'%s' is already declared in this scope.",
                statement->as.function_declaration.name
            );
            continue;
        }

        {
            const size_t parameter_count =
                statement->as.function_declaration.parameter_count;
            SemanticType return_type =
                semantic_type_from_token(statement->as.function_declaration.return_type);
            SemanticType* parameter_types = NULL;
            Symbol symbol;
            size_t p;

            if (parameter_count > 0U) {
                parameter_types = (SemanticType*)malloc(
                    parameter_count * sizeof(SemanticType)
                );

                if (parameter_types == NULL) {
                    analyzer->had_fatal_memory_error = 1;
                    analyzer->had_error = 1;
                    add_error(analyzer, statement->line, "Out of memory while collecting function signature.");
                    continue;
                }

                for (p = 0U; p < parameter_count; ++p) {
                    parameter_types[p] = semantic_type_from_token(
                        statement->as.function_declaration.parameters[p].declared_type
                    );
                }
            }

            if (!symbol_init_function(
                    &symbol,
                    statement->as.function_declaration.name,
                    statement->line,
                    return_type,
                    parameter_types,
                    parameter_count,
                    0,
                    0
                )) {
                free(parameter_types);
                analyzer->had_fatal_memory_error = 1;
                analyzer->had_error = 1;
                continue;
            }

            if (!scope_define_symbol(analyzer->global_scope, &symbol)) {
                add_error(
                    analyzer,
                    statement->line,
                    "Out of memory while registering function '%s'.",
                    statement->as.function_declaration.name
                );
                analyzer->had_fatal_memory_error = 1;
                analyzer->had_error = 1;
            }

            symbol_free(&symbol);
            free(parameter_types);
        }
    }
}

static void analyze_statement(SemanticAnalyzer* analyzer, const Stmt* stmt)
{
    if (analyzer == NULL || stmt == NULL) {
        return;
    }

    switch (stmt->type) {
        case STMT_EXPRESSION:
            analyze_expression(analyzer, stmt->as.expression.expression);
            break;

        case STMT_VARIABLE_DECLARATION:
        {
            const char* variable_name = stmt->as.variable_declaration.name;
            SemanticType declared_type =
                semantic_type_from_token(stmt->as.variable_declaration.declared_type);
            Symbol symbol;
            Symbol* stored_symbol;

            if (declared_type == SEM_TYPE_VOID) {
                add_error(
                    analyzer,
                    stmt->line,
                    "Variable '%s' cannot be declared with type void.",
                    variable_name
                );
            }

            if (scope_lookup_current(analyzer->current_scope, variable_name) != NULL) {
                add_error(
                    analyzer,
                    stmt->line,
                    "'%s' is already declared in this scope.",
                    variable_name
                );
                break;
            }

            if (!symbol_init_variable(
                    &symbol,
                    variable_name,
                    SYMBOL_VARIABLE,
                    declared_type,
                    stmt->line,
                    SYMBOL_STATE_DECLARED
                )) {
                analyzer->had_fatal_memory_error = 1;
                analyzer->had_error = 1;
                add_error(analyzer, stmt->line, "Out of memory while declaring variable '%s'.", variable_name);
                break;
            }

            if (!scope_define_symbol(analyzer->current_scope, &symbol)) {
                symbol_free(&symbol);
                analyzer->had_fatal_memory_error = 1;
                analyzer->had_error = 1;
                add_error(analyzer, stmt->line, "Out of memory while storing variable '%s'.", variable_name);
                break;
            }

            symbol_free(&symbol);
            stored_symbol = scope_lookup_current(analyzer->current_scope, variable_name);
            if (stored_symbol == NULL) {
                analyzer->had_fatal_memory_error = 1;
                analyzer->had_error = 1;
                break;
            }

            if (stmt->as.variable_declaration.initializer != NULL) {
                SemanticType initializer_type = analyze_expression(
                    analyzer,
                    stmt->as.variable_declaration.initializer
                );

                if (declared_type != SEM_TYPE_ERROR &&
                    initializer_type != SEM_TYPE_ERROR &&
                    !semantic_can_assign(declared_type, initializer_type)) {
                    add_error(
                        analyzer,
                        stmt->line,
                        "Cannot initialize variable '%s' of type %s with value of type %s.",
                        variable_name,
                        semantic_type_to_string(declared_type),
                        semantic_type_to_string(initializer_type)
                    );
                }
            }

            stored_symbol->state = SYMBOL_STATE_DEFINED;
            break;
        }

        case STMT_BLOCK:
            analyze_block_contents(analyzer, stmt, 1);
            break;

        case STMT_IF:
        {
            SemanticType condition_type =
                analyze_expression(analyzer, stmt->as.if_stmt.condition);

            if (condition_type != SEM_TYPE_ERROR && condition_type != SEM_TYPE_BOOL) {
                add_error(
                    analyzer,
                    stmt->line,
                    "If condition must be bool, got %s.",
                    semantic_type_to_string(condition_type)
                );
            }

            analyze_statement(analyzer, stmt->as.if_stmt.then_branch);
            if (stmt->as.if_stmt.else_branch != NULL) {
                analyze_statement(analyzer, stmt->as.if_stmt.else_branch);
            }
            break;
        }

        case STMT_WHILE:
        {
            SemanticType condition_type =
                analyze_expression(analyzer, stmt->as.while_stmt.condition);

            if (condition_type != SEM_TYPE_ERROR && condition_type != SEM_TYPE_BOOL) {
                add_error(
                    analyzer,
                    stmt->line,
                    "While condition must be bool, got %s.",
                    semantic_type_to_string(condition_type)
                );
            }

            analyze_statement(analyzer, stmt->as.while_stmt.body);
            break;
        }

        case STMT_RETURN:
        {
            if (!analyzer->inside_function) {
                add_error(analyzer, stmt->line, "Return statement outside of a function.");
                break;
            }

            if (analyzer->current_function_return_type == SEM_TYPE_VOID) {
                if (stmt->as.return_stmt.value != NULL) {
                    add_error(
                        analyzer,
                        stmt->line,
                        "Cannot return a value from function '%s' returning void.",
                        analyzer->current_function_name != NULL
                            ? analyzer->current_function_name
                            : "<anonymous>"
                    );
                }
            } else {
                if (stmt->as.return_stmt.value == NULL) {
                    add_error(
                        analyzer,
                        stmt->line,
                        "Function '%s' must return a value of type %s.",
                        analyzer->current_function_name != NULL
                            ? analyzer->current_function_name
                            : "<anonymous>",
                        semantic_type_to_string(analyzer->current_function_return_type)
                    );
                } else {
                    SemanticType value_type =
                        analyze_expression(analyzer, stmt->as.return_stmt.value);

                    if (value_type != SEM_TYPE_ERROR &&
                        !semantic_can_assign(analyzer->current_function_return_type, value_type)) {
                        add_error(
                            analyzer,
                            stmt->line,
                            "Return type mismatch in function '%s': expected %s, got %s.",
                            analyzer->current_function_name != NULL
                                ? analyzer->current_function_name
                                : "<anonymous>",
                            semantic_type_to_string(analyzer->current_function_return_type),
                            semantic_type_to_string(value_type)
                        );
                    }
                }
            }
            break;
        }

        case STMT_FUNCTION_DECLARATION:
        {
            Symbol* function_symbol = scope_lookup_current(
                analyzer->current_scope,
                stmt->as.function_declaration.name
            );
            SemanticType previous_return_type;
            const char* previous_function_name;
            int previous_inside_function;
            size_t i;

            if (function_symbol == NULL) {
                SemanticType* parameter_types = NULL;
                Symbol new_symbol;

                if (stmt->as.function_declaration.parameter_count > 0U) {
                    parameter_types = (SemanticType*)malloc(
                        stmt->as.function_declaration.parameter_count * sizeof(SemanticType)
                    );

                    if (parameter_types == NULL) {
                        analyzer->had_fatal_memory_error = 1;
                        analyzer->had_error = 1;
                        add_error(analyzer, stmt->line, "Out of memory while preparing function '%s'.", stmt->as.function_declaration.name);
                        break;
                    }

                    for (i = 0U; i < stmt->as.function_declaration.parameter_count; ++i) {
                        parameter_types[i] = semantic_type_from_token(
                            stmt->as.function_declaration.parameters[i].declared_type
                        );
                    }
                }

                if (!symbol_init_function(
                        &new_symbol,
                        stmt->as.function_declaration.name,
                        stmt->line,
                        semantic_type_from_token(stmt->as.function_declaration.return_type),
                        parameter_types,
                        stmt->as.function_declaration.parameter_count,
                        0,
                        0
                    )) {
                    free(parameter_types);
                    analyzer->had_fatal_memory_error = 1;
                    analyzer->had_error = 1;
                    break;
                }

                if (scope_lookup_current(analyzer->current_scope, stmt->as.function_declaration.name) != NULL) {
                    add_error(
                        analyzer,
                        stmt->line,
                        "'%s' is already declared in this scope.",
                        stmt->as.function_declaration.name
                    );
                    symbol_free(&new_symbol);
                    free(parameter_types);
                    break;
                }

                if (!scope_define_symbol(analyzer->current_scope, &new_symbol)) {
                    analyzer->had_fatal_memory_error = 1;
                    analyzer->had_error = 1;
                    add_error(analyzer, stmt->line, "Out of memory while storing function '%s'.", stmt->as.function_declaration.name);
                    symbol_free(&new_symbol);
                    free(parameter_types);
                    break;
                }

                symbol_free(&new_symbol);
                free(parameter_types);
                function_symbol = scope_lookup_current(
                    analyzer->current_scope,
                    stmt->as.function_declaration.name
                );
            }

            if (function_symbol == NULL || function_symbol->kind != SYMBOL_FUNCTION) {
                add_error(analyzer, stmt->line, "Function symbol resolution failed for '%s'.", stmt->as.function_declaration.name);
                break;
            }

            previous_return_type = analyzer->current_function_return_type;
            previous_function_name = analyzer->current_function_name;
            previous_inside_function = analyzer->inside_function;

            analyzer->inside_function = 1;
            analyzer->current_function_return_type = function_symbol->function.return_type;
            analyzer->current_function_name = function_symbol->name;

            if (!begin_scope(analyzer)) {
                analyzer->inside_function = previous_inside_function;
                analyzer->current_function_return_type = previous_return_type;
                analyzer->current_function_name = previous_function_name;
                break;
            }

            for (i = 0U; i < stmt->as.function_declaration.parameter_count; ++i) {
                const Parameter* parameter = &stmt->as.function_declaration.parameters[i];
                SemanticType parameter_type =
                    semantic_type_from_token(parameter->declared_type);
                Symbol parameter_symbol;

                if (parameter_type == SEM_TYPE_VOID) {
                    add_error(
                        analyzer,
                        parameter->line,
                        "Parameter '%s' cannot have type void.",
                        parameter->name
                    );
                    continue;
                }

                if (scope_lookup_current(analyzer->current_scope, parameter->name) != NULL) {
                    add_error(
                        analyzer,
                        parameter->line,
                        "'%s' is already declared in this scope.",
                        parameter->name
                    );
                    continue;
                }

                if (!symbol_init_variable(
                        &parameter_symbol,
                        parameter->name,
                        SYMBOL_PARAMETER,
                        parameter_type,
                        parameter->line,
                        SYMBOL_STATE_DEFINED
                    )) {
                    analyzer->had_fatal_memory_error = 1;
                    analyzer->had_error = 1;
                    add_error(analyzer, parameter->line, "Out of memory while adding parameter '%s'.", parameter->name);
                    continue;
                }

                if (!scope_define_symbol(analyzer->current_scope, &parameter_symbol)) {
                    analyzer->had_fatal_memory_error = 1;
                    analyzer->had_error = 1;
                    add_error(analyzer, parameter->line, "Out of memory while storing parameter '%s'.", parameter->name);
                }

                symbol_free(&parameter_symbol);
            }

            if (stmt->as.function_declaration.body != NULL &&
                stmt->as.function_declaration.body->type == STMT_BLOCK) {
                analyze_block_contents(
                    analyzer,
                    stmt->as.function_declaration.body,
                    0
                );
            } else {
                analyze_statement(analyzer, stmt->as.function_declaration.body);
            }

            if (function_symbol->function.return_type != SEM_TYPE_VOID &&
                !statement_guarantees_return(stmt->as.function_declaration.body)) {
                add_error(
                    analyzer,
                    stmt->line,
                    "Function '%s' may exit without returning %s.",
                    function_symbol->name,
                    semantic_type_to_string(function_symbol->function.return_type)
                );
            }

            end_scope(analyzer);

            analyzer->inside_function = previous_inside_function;
            analyzer->current_function_return_type = previous_return_type;
            analyzer->current_function_name = previous_function_name;
            break;
        }
    }
}

static void analyze_block_contents(
    SemanticAnalyzer* analyzer,
    const Stmt* block_stmt,
    int create_child_scope
)
{
    size_t i;
    int reached_return = 0;

    if (analyzer == NULL || block_stmt == NULL || block_stmt->type != STMT_BLOCK) {
        return;
    }

    if (create_child_scope && !begin_scope(analyzer)) {
        return;
    }

    for (i = 0U; i < block_stmt->as.block.statement_count; ++i) {
        const Stmt* child = block_stmt->as.block.statements[i];

        if (reached_return) {
            add_warning(analyzer, child->line, "unreachable statement.");
        }

        analyze_statement(analyzer, child);

        if (!reached_return && statement_guarantees_return(child)) {
            reached_return = 1;
        }
    }

    if (create_child_scope) {
        end_scope(analyzer);
    }
}

static SemanticType analyze_expression(
    SemanticAnalyzer* analyzer,
    const Expr* expr
)
{
    if (expr == NULL) {
        return SEM_TYPE_ERROR;
    }

    switch (expr->type) {
        case EXPR_INT_LITERAL:
            return SEM_TYPE_INT;

        case EXPR_FLOAT_LITERAL:
            return SEM_TYPE_FLOAT;

        case EXPR_STRING_LITERAL:
            return SEM_TYPE_STRING;

        case EXPR_BOOL_LITERAL:
            return SEM_TYPE_BOOL;

        case EXPR_IDENTIFIER:
        {
            Symbol* symbol = scope_lookup(analyzer->current_scope, expr->as.identifier.name);

            if (symbol == NULL) {
                add_error(
                    analyzer,
                    expr->line,
                    "Undefined identifier '%s'.",
                    expr->as.identifier.name
                );
                return SEM_TYPE_ERROR;
            }

            if (symbol->kind != SYMBOL_FUNCTION && symbol->state == SYMBOL_STATE_DECLARED) {
                add_error(
                    analyzer,
                    expr->line,
                    "Cannot read variable '%s' in its own initializer.",
                    expr->as.identifier.name
                );
                return SEM_TYPE_ERROR;
            }

            if (symbol->kind == SYMBOL_FUNCTION) {
                return SEM_TYPE_FUNCTION;
            }

            return symbol->type;
        }

        case EXPR_UNARY:
        {
            SemanticType operand_type =
                analyze_expression(analyzer, expr->as.unary.operand);

            if (operand_type == SEM_TYPE_ERROR) {
                return SEM_TYPE_ERROR;
            }

            switch (expr->as.unary.operator_type) {
                case TOKEN_BANG:
                    if (operand_type != SEM_TYPE_BOOL) {
                        add_error(
                            analyzer,
                            expr->line,
                            "Operator ! expects bool operand, got %s.",
                            semantic_type_to_string(operand_type)
                        );
                        return SEM_TYPE_ERROR;
                    }
                    return SEM_TYPE_BOOL;

                case TOKEN_PLUS:
                case TOKEN_MINUS:
                    if (!is_numeric_type(operand_type)) {
                        add_error(
                            analyzer,
                            expr->line,
                            "Unary operator %s expects numeric operand, got %s.",
                            expr->as.unary.operator_type == TOKEN_PLUS ? "+" : "-",
                            semantic_type_to_string(operand_type)
                        );
                        return SEM_TYPE_ERROR;
                    }
                    return operand_type;

                default:
                    add_error(analyzer, expr->line, "Unsupported unary operator.");
                    return SEM_TYPE_ERROR;
            }
        }

        case EXPR_BINARY:
        {
            SemanticType left = analyze_expression(analyzer, expr->as.binary.left);
            SemanticType right = analyze_expression(analyzer, expr->as.binary.right);
            TokenType op = expr->as.binary.operator_type;

            if (left == SEM_TYPE_ERROR || right == SEM_TYPE_ERROR) {
                return SEM_TYPE_ERROR;
            }

            switch (op) {
                case TOKEN_PLUS:
                    if (left == SEM_TYPE_STRING && right == SEM_TYPE_STRING) {
                        return SEM_TYPE_STRING;
                    }

                    if (!is_numeric_type(left) || !is_numeric_type(right)) {
                        add_error(
                            analyzer,
                            expr->line,
                            "Operator + expects numeric operands or string+string, got %s and %s.",
                            semantic_type_to_string(left),
                            semantic_type_to_string(right)
                        );
                        return SEM_TYPE_ERROR;
                    }

                    return numeric_result_type(left, right);

                case TOKEN_MINUS:
                case TOKEN_STAR:
                case TOKEN_SLASH:
                    if (!is_numeric_type(left) || !is_numeric_type(right)) {
                        add_error(
                            analyzer,
                            expr->line,
                            "Arithmetic operator expects numeric operands, got %s and %s.",
                            semantic_type_to_string(left),
                            semantic_type_to_string(right)
                        );
                        return SEM_TYPE_ERROR;
                    }

                    return numeric_result_type(left, right);

                case TOKEN_PERCENT:
                    if (left != SEM_TYPE_INT || right != SEM_TYPE_INT) {
                        add_error(
                            analyzer,
                            expr->line,
                            "Operator %% expects int operands, got %s and %s.",
                            semantic_type_to_string(left),
                            semantic_type_to_string(right)
                        );
                        return SEM_TYPE_ERROR;
                    }
                    return SEM_TYPE_INT;

                case TOKEN_GT:
                case TOKEN_GTE:
                case TOKEN_LT:
                case TOKEN_LTE:
                    if (!is_numeric_type(left) || !is_numeric_type(right)) {
                        add_error(
                            analyzer,
                            expr->line,
                            "Comparison operator expects numeric operands, got %s and %s.",
                            semantic_type_to_string(left),
                            semantic_type_to_string(right)
                        );
                        return SEM_TYPE_ERROR;
                    }
                    return SEM_TYPE_BOOL;

                case TOKEN_EQ:
                case TOKEN_BANG_EQ:
                    if (!can_compare_equality(left, right)) {
                        add_error(
                            analyzer,
                            expr->line,
                            "Equality operator cannot compare %s and %s.",
                            semantic_type_to_string(left),
                            semantic_type_to_string(right)
                        );
                        return SEM_TYPE_ERROR;
                    }
                    return SEM_TYPE_BOOL;

                case TOKEN_ANDAND:
                case TOKEN_OROR:
                    if (left != SEM_TYPE_BOOL || right != SEM_TYPE_BOOL) {
                        add_error(
                            analyzer,
                            expr->line,
                            "Logical operator expects bool operands, got %s and %s.",
                            semantic_type_to_string(left),
                            semantic_type_to_string(right)
                        );
                        return SEM_TYPE_ERROR;
                    }
                    return SEM_TYPE_BOOL;

                default:
                    add_error(analyzer, expr->line, "Unsupported binary operator.");
                    return SEM_TYPE_ERROR;
            }
        }

        case EXPR_ASSIGNMENT:
        {
            Symbol* target = scope_lookup(analyzer->current_scope, expr->as.assignment.target_name);
            SemanticType value_type;

            if (target == NULL) {
                add_error(
                    analyzer,
                    expr->line,
                    "Undefined identifier '%s'.",
                    expr->as.assignment.target_name
                );
                return SEM_TYPE_ERROR;
            }

            if (target->kind == SYMBOL_FUNCTION) {
                add_error(
                    analyzer,
                    expr->line,
                    "Cannot assign to function '%s'.",
                    expr->as.assignment.target_name
                );
                return SEM_TYPE_ERROR;
            }

            value_type = analyze_expression(analyzer, expr->as.assignment.value);
            if (value_type == SEM_TYPE_ERROR) {
                return SEM_TYPE_ERROR;
            }

            if (!semantic_can_assign(target->type, value_type)) {
                add_error(
                    analyzer,
                    expr->line,
                    "Cannot assign value of type %s to '%s' of type %s.",
                    semantic_type_to_string(value_type),
                    target->name,
                    semantic_type_to_string(target->type)
                );
                return SEM_TYPE_ERROR;
            }

            return value_type;
        }

        case EXPR_CALL:
        {
            Symbol* callee_symbol = NULL;
            size_t i;

            if (expr->as.call.callee->type == EXPR_IDENTIFIER) {
                callee_symbol = scope_lookup(
                    analyzer->current_scope,
                    expr->as.call.callee->as.identifier.name
                );
            } else {
                analyze_expression(analyzer, expr->as.call.callee);
                add_error(
                    analyzer,
                    expr->line,
                    "Callee expression is not callable in this language version."
                );
                return SEM_TYPE_ERROR;
            }

            if (callee_symbol == NULL) {
                add_error(analyzer, expr->line, "Undefined function in call expression.");
                for (i = 0U; i < expr->as.call.argument_count; ++i) {
                    analyze_expression(analyzer, expr->as.call.arguments[i]);
                }
                return SEM_TYPE_ERROR;
            }

            if (callee_symbol->kind != SYMBOL_FUNCTION) {
                add_error(
                    analyzer,
                    expr->line,
                    "'%s' is not callable.",
                    callee_symbol->name
                );
                for (i = 0U; i < expr->as.call.argument_count; ++i) {
                    analyze_expression(analyzer, expr->as.call.arguments[i]);
                }
                return SEM_TYPE_ERROR;
            }

            for (i = 0U; i < expr->as.call.argument_count; ++i) {
                SemanticType argument_type =
                    analyze_expression(analyzer, expr->as.call.arguments[i]);

                if (argument_type == SEM_TYPE_ERROR) {
                    continue;
                }

                if (callee_symbol->function.accepts_any) {
                    continue;
                }

                if (i < callee_symbol->function.parameter_count) {
                    SemanticType parameter_type =
                        callee_symbol->function.parameter_types[i];

                    if (!semantic_can_assign(parameter_type, argument_type)) {
                        add_error(
                            analyzer,
                            expr->as.call.arguments[i]->line,
                            "Function '%s' argument %zu expects %s but got %s.",
                            callee_symbol->name,
                            i + 1U,
                            semantic_type_to_string(parameter_type),
                            semantic_type_to_string(argument_type)
                        );
                    }
                }
            }

            if (!callee_symbol->function.is_variadic) {
                if (expr->as.call.argument_count != callee_symbol->function.parameter_count) {
                    add_error(
                        analyzer,
                        expr->line,
                        "Function '%s' expects %zu arguments but received %zu.",
                        callee_symbol->name,
                        callee_symbol->function.parameter_count,
                        expr->as.call.argument_count
                    );
                    return SEM_TYPE_ERROR;
                }
            } else if (expr->as.call.argument_count < callee_symbol->function.parameter_count) {
                add_error(
                    analyzer,
                    expr->line,
                    "Function '%s' expects at least %zu arguments but received %zu.",
                    callee_symbol->name,
                    callee_symbol->function.parameter_count,
                    expr->as.call.argument_count
                );
                return SEM_TYPE_ERROR;
            }

            return callee_symbol->function.return_type;
        }

        case EXPR_LIST_LITERAL:
        {
            size_t i;
            for (i = 0U; i < expr->as.list_literal.element_count; ++i)
                analyze_expression(analyzer, expr->as.list_literal.elements[i]);
            return SEM_TYPE_LIST;
        }

        case EXPR_INDEX:
        {
            SemanticType object = analyze_expression(analyzer, expr->as.index.object);
            SemanticType index = analyze_expression(analyzer, expr->as.index.index);
            if (object != SEM_TYPE_LIST && object != SEM_TYPE_ERROR) add_error(analyzer, expr->line, "Index access expects a list.");
            if (index != SEM_TYPE_INT) add_error(analyzer, expr->line, "List index must be int.");
            return SEM_TYPE_ERROR; /* heterogeneous list element: dynamically typed */
        }

        case EXPR_METHOD_CALL:
        {
            SemanticType object = analyze_expression(analyzer, expr->as.method_call.object);
            size_t i;
            for (i = 0U; i < expr->as.method_call.argument_count; ++i)
                analyze_expression(analyzer, expr->as.method_call.arguments[i]);
            if (object != SEM_TYPE_LIST) { add_error(analyzer, expr->line, "Methods are currently supported only on lists."); return SEM_TYPE_ERROR; }
            if (strcmp(expr->as.method_call.method_name, "size") == 0) {
                if (expr->as.method_call.argument_count != 0U) add_error(analyzer, expr->line, "list.size() expects no arguments.");
                return SEM_TYPE_INT;
            }
            if (strcmp(expr->as.method_call.method_name, "append") == 0) {
                if (expr->as.method_call.argument_count != 1U) add_error(analyzer, expr->line, "list.append() expects one argument.");
                return SEM_TYPE_VOID;
            }
            add_error(analyzer, expr->line, "Unknown list method '%s'.", expr->as.method_call.method_name);
            return SEM_TYPE_ERROR;
        }

        case EXPR_CONVERSION:
        {
            SemanticType source = analyze_expression(analyzer, expr->as.conversion.value);
            SemanticType target = semantic_type_from_token(expr->as.conversion.target_type);
            if (source == SEM_TYPE_ERROR) return target;
            if (target == SEM_TYPE_INT && (source == SEM_TYPE_INT || source == SEM_TYPE_FLOAT || source == SEM_TYPE_STRING || source == SEM_TYPE_BOOL)) return target;
            if (target == SEM_TYPE_FLOAT && (source == SEM_TYPE_INT || source == SEM_TYPE_FLOAT || source == SEM_TYPE_STRING || source == SEM_TYPE_BOOL)) return target;
            if (target == SEM_TYPE_STRING && (source == SEM_TYPE_INT || source == SEM_TYPE_FLOAT || source == SEM_TYPE_STRING || source == SEM_TYPE_BOOL)) return target;
            add_error(analyzer, expr->line, "Cannot convert %s to %s.", semantic_type_to_string(source), semantic_type_to_string(target));
            return SEM_TYPE_ERROR;
        }

        case EXPR_INDEX_ASSIGNMENT:
        {
            SemanticType object = analyze_expression(analyzer, expr->as.index_assignment.object);
            SemanticType index = analyze_expression(analyzer, expr->as.index_assignment.index);
            SemanticType value = analyze_expression(analyzer, expr->as.index_assignment.value);
            if (object != SEM_TYPE_LIST && object != SEM_TYPE_ERROR)
                add_error(analyzer, expr->line, "Index assignment expects a list.");
            if (index != SEM_TYPE_INT)
                add_error(analyzer, expr->line, "List assignment index must be int.");
            return value;
        }
    }

    add_error(analyzer, expr->line, "Unsupported expression node.");
    return SEM_TYPE_ERROR;
}

static int statement_guarantees_return(const Stmt* stmt)
{
    size_t i;

    if (stmt == NULL) {
        return 0;
    }

    switch (stmt->type) {
        case STMT_RETURN:
            return 1;

        case STMT_BLOCK:
            for (i = 0U; i < stmt->as.block.statement_count; ++i) {
                if (statement_guarantees_return(stmt->as.block.statements[i])) {
                    return 1;
                }
            }
            return 0;

        case STMT_IF:
            if (stmt->as.if_stmt.else_branch == NULL) {
                return 0;
            }

            return statement_guarantees_return(stmt->as.if_stmt.then_branch) &&
                   statement_guarantees_return(stmt->as.if_stmt.else_branch);

        default:
            return 0;
    }
}

static int is_numeric_type(SemanticType type)
{
    return type == SEM_TYPE_INT || type == SEM_TYPE_FLOAT;
}

static SemanticType numeric_result_type(SemanticType left, SemanticType right)
{
    if (left == SEM_TYPE_FLOAT || right == SEM_TYPE_FLOAT) {
        return SEM_TYPE_FLOAT;
    }

    return SEM_TYPE_INT;
}

static int can_compare_equality(SemanticType left, SemanticType right)
{
    if (left == SEM_TYPE_ERROR || right == SEM_TYPE_ERROR) {
        return 1;
    }

    if (left == right) {
        return 1;
    }

    if (is_numeric_type(left) && is_numeric_type(right)) {
        return 1;
    }

    return 0;
}
