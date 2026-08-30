#ifndef BUILTINS_H
#define BUILTINS_H

#include "interpreter.hpp"
#include "semantic.hpp"

#include <stddef.h>

typedef struct {
    const char* name;
    SemanticType type;
} BuiltinParameter;

typedef struct {
    const char* name;
    SemanticType return_type;
    const BuiltinParameter* parameters;
    size_t parameter_count;
    int variadic;
    int accepts_any;
    const char* documentation;
    const char* detail;
    NativeFunctionCallback callback;
} BuiltinFunctionDescriptor;

const BuiltinFunctionDescriptor* builtin_functions(size_t* count);
int builtin_register_semantic(SemanticAnalyzer* analyzer);
int builtin_register_runtime(Interpreter* interpreter);

#endif
