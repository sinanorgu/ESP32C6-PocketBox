#include "clumsyPL/builtins.hpp"

#include <stdio.h>

static RuntimeResult builtin_ok(Value value)
{
    RuntimeResult result;
    result.status = RUNTIME_OK;
    result.value = value;
    return result;
}

static RuntimeResult builtin_error(Interpreter* interpreter, const char* message)
{
    RuntimeResult result;
    if (interpreter != NULL) {
        interpreter->had_error = 1;
        interpreter->diagnostic.line = 0;
        snprintf(interpreter->diagnostic.message, sizeof(interpreter->diagnostic.message), "%s", message);
    }
    result.status = RUNTIME_ERROR;
    result.value = value_void();
    return result;
}

static void output_value(Interpreter* interpreter, const Value* value)
{
    char buffer[96];
    size_t i;
    switch (value->type) {
        case VALUE_VOID: interpreter->output("void", interpreter->output_user_data); break;
        case VALUE_INT: snprintf(buffer, sizeof(buffer), "%lld", value->as.int_value); interpreter->output(buffer, interpreter->output_user_data); break;
        case VALUE_FLOAT: snprintf(buffer, sizeof(buffer), "%.15g", value->as.float_value); interpreter->output(buffer, interpreter->output_user_data); break;
        case VALUE_BOOL: interpreter->output(value->as.bool_value ? "true" : "false", interpreter->output_user_data); break;
        case VALUE_STRING: interpreter->output(value->as.string_value != NULL ? value->as.string_value : "", interpreter->output_user_data); break;
        case VALUE_FUNCTION: interpreter->output("<function>", interpreter->output_user_data); break;
        case VALUE_NATIVE_FUNCTION: interpreter->output("<native function>", interpreter->output_user_data); break;
        case VALUE_LIST:
            interpreter->output("[", interpreter->output_user_data);
            for (i = 0U; i < value_list_size(value); ++i) {
                if (i > 0U) interpreter->output(", ", interpreter->output_user_data);
                output_value(interpreter, value_list_get(value, i));
            }
            interpreter->output("]", interpreter->output_user_data); break;
    }
}

static RuntimeResult native_print(Interpreter* interpreter, const Value* arguments, size_t argument_count)
{
    size_t i;
    if (interpreter == NULL || interpreter->output == NULL)
        return builtin_error(interpreter, "Output callback is unavailable.");
    for (i = 0U; i < argument_count; ++i) {
        if (i > 0U) interpreter->output(" ", interpreter->output_user_data);
        output_value(interpreter, &arguments[i]);
    }
    interpreter->output("\n", interpreter->output_user_data);
    return builtin_ok(value_void());
}

static const BuiltinParameter print_parameters[] = {
    { "value", SEM_TYPE_ERROR }
};

static const BuiltinFunctionDescriptor descriptors[] = {
    {
        "print", SEM_TYPE_VOID, print_parameters, 0U, 1, 1,
        "Prints values to the configured output, separated by spaces.",
        "print(...values: any) -> void", native_print
    }
};

const BuiltinFunctionDescriptor* builtin_functions(size_t* count)
{
    if (count != NULL) *count = sizeof(descriptors) / sizeof(descriptors[0]);
    return descriptors;
}

int builtin_register_semantic(SemanticAnalyzer* analyzer)
{
    size_t i, count;
    const BuiltinFunctionDescriptor* items = builtin_functions(&count);
    for (i = 0U; i < count; ++i) {
        SemanticType parameter_types[16];
        size_t p;
        if (items[i].parameter_count > 16U) return 0;
        for (p = 0U; p < items[i].parameter_count; ++p) parameter_types[p] = items[i].parameters[p].type;
        if (!semantic_define_native_function_ex(analyzer, items[i].name, items[i].return_type,
                parameter_types, items[i].parameter_count, items[i].variadic, items[i].accepts_any)) return 0;
    }
    return 1;
}

int builtin_register_runtime(Interpreter* interpreter)
{
    size_t i, count;
    const BuiltinFunctionDescriptor* items = builtin_functions(&count);
    for (i = 0U; i < count; ++i)
        if (!interpreter_define_native(interpreter, items[i].name, items[i].callback,
                items[i].parameter_count, items[i].variadic)) return 0;
    return 1;
}
