#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.hpp"

#include <stddef.h>

typedef enum {
    SEM_TYPE_ERROR,
    SEM_TYPE_VOID,
    SEM_TYPE_INT,
    SEM_TYPE_FLOAT,
    SEM_TYPE_STRING,
    SEM_TYPE_BOOL,
    SEM_TYPE_LIST,
    SEM_TYPE_FUNCTION
} SemanticType;

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_PARAMETER,
    SYMBOL_FUNCTION
} SymbolKind;

typedef enum {
    SYMBOL_STATE_DECLARED,
    SYMBOL_STATE_DEFINED
} SymbolState;

typedef struct {
    SemanticType return_type;
    SemanticType* parameter_types;
    size_t parameter_count;
    int is_variadic;
    int accepts_any;
} FunctionSignature;

typedef struct {
    char* name;
    SymbolKind kind;
    SemanticType type;
    int line;
    SymbolState state;
    FunctionSignature function;
} Symbol;

typedef struct Scope Scope;

struct Scope {
    Symbol* symbols;
    size_t count;
    size_t capacity;
    Scope* parent;
};

typedef enum {
    DIAGNOSTIC_ERROR,
    DIAGNOSTIC_WARNING
} DiagnosticSeverity;

typedef struct {
    DiagnosticSeverity severity;
    int line;
    char* message;
} SemanticDiagnostic;

typedef struct {
    Scope* global_scope;
    Scope* current_scope;

    SemanticType current_function_return_type;
    const char* current_function_name;
    int inside_function;

    SemanticDiagnostic* diagnostics;
    size_t diagnostic_count;
    size_t diagnostic_capacity;

    int had_error;
    int had_fatal_memory_error;
} SemanticAnalyzer;

SemanticAnalyzer create_semantic_analyzer(void);
void destroy_semantic_analyzer(SemanticAnalyzer* analyzer);

int semantic_analyze_program(
    SemanticAnalyzer* analyzer,
    const Program* program
);

int semantic_define_native_function(
    SemanticAnalyzer* analyzer,
    const char* name,
    SemanticType return_type,
    const SemanticType* parameter_types,
    size_t parameter_count
);

int semantic_define_native_function_ex(
    SemanticAnalyzer* analyzer,
    const char* name,
    SemanticType return_type,
    const SemanticType* parameter_types,
    size_t parameter_count,
    int is_variadic,
    int accepts_any
);

size_t semantic_error_count(const SemanticAnalyzer* analyzer);
size_t semantic_warning_count(const SemanticAnalyzer* analyzer);
void semantic_print_diagnostics(const SemanticAnalyzer* analyzer);

SemanticType semantic_type_from_token(TokenType type);
const char* semantic_type_to_string(SemanticType type);
int semantic_can_assign(SemanticType destination, SemanticType source);

#endif
