#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.hpp"
#include "environment.hpp"

#include <stddef.h>

typedef struct Interpreter Interpreter;

typedef enum { RUNTIME_OK, RUNTIME_ERROR } RuntimeStatus;
typedef struct { RuntimeStatus status; Value value; } RuntimeResult;

typedef enum {
    EXEC_NORMAL,
    EXEC_RETURN,
    EXEC_RUNTIME_ERROR,
    EXEC_BREAK,
    EXEC_CONTINUE
} ExecResultType;
typedef struct { ExecResultType type; Value value; } ExecResult;

typedef void (*InterpreterOutputCallback)(const char* text, void* user_data);
typedef RuntimeResult (*NativeFunctionCallback)(
    Interpreter* interpreter,
    const Value* arguments,
    size_t argument_count
);

struct UserFunction {
    const Stmt* declaration;
    Environment* closure;
};

struct NativeFunction {
    char* name;
    NativeFunctionCallback callback;
    size_t arity;
    int is_variadic;
};

typedef struct {
    int line;
    char message[256];
} RuntimeDiagnostic;

struct Interpreter {
    Environment* globals;
    Environment* current_environment;
    InterpreterOutputCallback output;
    void* output_user_data;
    RuntimeDiagnostic diagnostic;
    int had_error;
    size_t call_depth;
    size_t max_call_depth;
    UserFunction** user_functions;
    size_t user_function_count;
    size_t user_function_capacity;
    NativeFunction** native_functions;
    size_t native_function_count;
    size_t native_function_capacity;
};

int interpreter_init(Interpreter* interpreter);
void interpreter_destroy(Interpreter* interpreter);
void interpreter_set_output(Interpreter* interpreter, InterpreterOutputCallback callback, void* user_data);
void interpreter_set_max_call_depth(Interpreter* interpreter, size_t maximum);
int interpreter_define_native(Interpreter* interpreter, const char* name, NativeFunctionCallback callback, size_t arity, int is_variadic);
RuntimeResult evaluate_expr(Interpreter* interpreter, const Expr* expr);
ExecResult execute_stmt(Interpreter* interpreter, const Stmt* stmt);
ExecResult interpreter_execute(Interpreter* interpreter, const Program* program);
const RuntimeDiagnostic* interpreter_get_diagnostic(const Interpreter* interpreter);

#endif
