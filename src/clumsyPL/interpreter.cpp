#include "clumsyPL/interpreter.hpp"
#include "clumsyPL/builtins.hpp"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MAX_CALL_DEPTH 128U

static int register_user_function(Interpreter* interpreter, const Stmt* declaration, Environment* closure);

static RuntimeResult runtime_ok(Value value) { RuntimeResult r; r.status = RUNTIME_OK; r.value = value; return r; }
static ExecResult exec_result(ExecResultType type, Value value) { ExecResult r; r.type = type; r.value = value; return r; }
static ExecResult exec_normal(void) { return exec_result(EXEC_NORMAL, value_void()); }

static RuntimeResult runtime_error(Interpreter* interpreter, int line, const char* format, ...)
{
    va_list args;
    RuntimeResult result;
    if (interpreter != NULL) {
        interpreter->had_error = 1;
        interpreter->diagnostic.line = line;
        va_start(args, format);
        vsnprintf(interpreter->diagnostic.message, sizeof(interpreter->diagnostic.message), format, args);
        va_end(args);
    }
    result.status = RUNTIME_ERROR;
    result.value = value_void();
    return result;
}

static ExecResult exec_from_runtime(RuntimeResult result)
{
    value_destroy(&result.value);
    return exec_result(EXEC_RUNTIME_ERROR, value_void());
}

static void default_output(const char* text, void* user_data)
{
    (void)user_data;
    if (text != NULL) fputs(text, stdout);
}

static char* copy_text(const char* text)
{
    size_t length;
    char* copy;
    if (text == NULL) return NULL;
    length = strlen(text);
    copy = (char*)malloc(length + 1U);
    if (copy != NULL) memcpy(copy, text, length + 1U);
    return copy;
}

static ValueType value_type_from_token(TokenType type)
{
    switch (type) {
        case TOKEN_INT: return VALUE_INT;
        case TOKEN_FLOAT: return VALUE_FLOAT;
        case TOKEN_STRING: return VALUE_STRING;
        case TOKEN_BOOL: return VALUE_BOOL;
        case TOKEN_LIST: return VALUE_LIST;
        case TOKEN_VOID: return VALUE_VOID;
        default: return VALUE_VOID;
    }
}

static int value_is_numeric(const Value* value) { return value->type == VALUE_INT || value->type == VALUE_FLOAT; }
static double value_as_double(const Value* value) { return value->type == VALUE_INT ? (double)value->as.int_value : value->as.float_value; }

static Value default_value_for_type(TokenType type)
{
    switch (type) {
        case TOKEN_INT: return value_int(0);
        case TOKEN_FLOAT: return value_float(0.0);
        case TOKEN_BOOL: return value_bool(0);
        case TOKEN_STRING: return value_string("");
        case TOKEN_LIST: return value_list();
        default: return value_void();
    }
}

static int append_pointer(void*** array, size_t* count, size_t* capacity, void* pointer)
{
    void** resized;
    size_t next;
    if (*count == *capacity) {
        next = *capacity == 0U ? 8U : *capacity * 2U;
        resized = (void**)realloc(*array, next * sizeof(void*));
        if (resized == NULL) return 0;
        *array = resized;
        *capacity = next;
    }
    (*array)[(*count)++] = pointer;
    return 1;
}

int interpreter_init(Interpreter* interpreter)
{
    if (interpreter == NULL) return 0;
    memset(interpreter, 0, sizeof(*interpreter));
    interpreter->globals = environment_create(NULL);
    if (interpreter->globals == NULL) return 0;
    interpreter->current_environment = interpreter->globals;
    interpreter->output = default_output;
    interpreter->max_call_depth = DEFAULT_MAX_CALL_DEPTH;
    if (!builtin_register_runtime(interpreter)) {
        interpreter_destroy(interpreter);
        return 0;
    }
    return 1;
}

void interpreter_destroy(Interpreter* interpreter)
{
    size_t i;
    if (interpreter == NULL) return;
    environment_destroy(interpreter->globals);
    for (i = 0U; i < interpreter->user_function_count; ++i) free(interpreter->user_functions[i]);
    for (i = 0U; i < interpreter->native_function_count; ++i) {
        free(interpreter->native_functions[i]->name);
        free(interpreter->native_functions[i]);
    }
    free(interpreter->user_functions);
    free(interpreter->native_functions);
    memset(interpreter, 0, sizeof(*interpreter));
}

void interpreter_set_output(Interpreter* interpreter, InterpreterOutputCallback callback, void* user_data)
{
    if (interpreter == NULL) return;
    interpreter->output = callback != NULL ? callback : default_output;
    interpreter->output_user_data = user_data;
}

void interpreter_set_max_call_depth(Interpreter* interpreter, size_t maximum)
{
    if (interpreter != NULL) interpreter->max_call_depth = maximum;
}

int interpreter_define_native(Interpreter* interpreter, const char* name, NativeFunctionCallback callback, size_t arity, int is_variadic)
{
    NativeFunction* native;
    Value value;
    if (interpreter == NULL || name == NULL || callback == NULL || environment_get(interpreter->globals, name) != NULL) return 0;
    native = (NativeFunction*)calloc(1U, sizeof(NativeFunction));
    if (native == NULL) return 0;
    native->name = copy_text(name);
    native->callback = callback;
    native->arity = arity;
    native->is_variadic = is_variadic != 0;
    if (native->name == NULL || !append_pointer((void***)&interpreter->native_functions, &interpreter->native_function_count, &interpreter->native_function_capacity, native)) {
        free(native->name); free(native); return 0;
    }
    value = value_native_function(native);
    if (!environment_define(interpreter->globals, name, value)) {
        interpreter->native_function_count--;
        free(native->name); free(native); return 0;
    }
    return 1;
}

const RuntimeDiagnostic* interpreter_get_diagnostic(const Interpreter* interpreter)
{
    return interpreter != NULL && interpreter->had_error ? &interpreter->diagnostic : NULL;
}

static RuntimeResult evaluate_binary(Interpreter* interpreter, const Expr* expr)
{
    TokenType op = expr->as.binary.operator_type;
    RuntimeResult left = evaluate_expr(interpreter, expr->as.binary.left);
    RuntimeResult right;
    Value result = value_void();
    int equal = 0;
    if (left.status != RUNTIME_OK) return left;
    if (op == TOKEN_ANDAND || op == TOKEN_OROR) {
        if (left.value.type != VALUE_BOOL) { value_destroy(&left.value); return runtime_error(interpreter, expr->line, "Logical operator expects bool operands."); }
        if ((op == TOKEN_ANDAND && !left.value.as.bool_value) || (op == TOKEN_OROR && left.value.as.bool_value)) {
            result = value_bool(left.value.as.bool_value);
            value_destroy(&left.value);
            return runtime_ok(result);
        }
    }
    right = evaluate_expr(interpreter, expr->as.binary.right);
    if (right.status != RUNTIME_OK) { value_destroy(&left.value); return right; }
    if (op == TOKEN_ANDAND || op == TOKEN_OROR) {
        if (right.value.type != VALUE_BOOL) { value_destroy(&left.value); value_destroy(&right.value); return runtime_error(interpreter, expr->line, "Logical operator expects bool operands."); }
        result = value_bool(right.value.as.bool_value);
    } else if (op == TOKEN_PLUS && left.value.type == VALUE_STRING && right.value.type == VALUE_STRING) {
        size_t a = strlen(left.value.as.string_value), b = strlen(right.value.as.string_value);
        char* joined = (char*)malloc(a + b + 1U);
        if (joined == NULL) { value_destroy(&left.value); value_destroy(&right.value); return runtime_error(interpreter, expr->line, "Out of memory while concatenating strings."); }
        memcpy(joined, left.value.as.string_value, a); memcpy(joined + a, right.value.as.string_value, b + 1U);
        result.type = VALUE_STRING; result.as.string_value = joined;
    } else if (op == TOKEN_EQ || op == TOKEN_BANG_EQ) {
        if (value_is_numeric(&left.value) && value_is_numeric(&right.value)) equal = value_as_double(&left.value) == value_as_double(&right.value);
        else if (left.value.type == VALUE_BOOL && right.value.type == VALUE_BOOL) equal = left.value.as.bool_value == right.value.as.bool_value;
        else if (left.value.type == VALUE_STRING && right.value.type == VALUE_STRING) equal = strcmp(left.value.as.string_value, right.value.as.string_value) == 0;
        else { value_destroy(&left.value); value_destroy(&right.value); return runtime_error(interpreter, expr->line, "Invalid equality operands."); }
        result = value_bool(op == TOKEN_EQ ? equal : !equal);
    } else if (op == TOKEN_PERCENT) {
        if (left.value.type != VALUE_INT || right.value.type != VALUE_INT) { value_destroy(&left.value); value_destroy(&right.value); return runtime_error(interpreter, expr->line, "Operator %% expects int operands."); }
        if (right.value.as.int_value == 0) { value_destroy(&left.value); value_destroy(&right.value); return runtime_error(interpreter, expr->line, "Modulo by zero."); }
        result = value_int(left.value.as.int_value % right.value.as.int_value);
    } else if (value_is_numeric(&left.value) && value_is_numeric(&right.value)) {
        double a = value_as_double(&left.value), b = value_as_double(&right.value);
        int floats = left.value.type == VALUE_FLOAT || right.value.type == VALUE_FLOAT;
        if (op == TOKEN_SLASH && b == 0.0) { value_destroy(&left.value); value_destroy(&right.value); return runtime_error(interpreter, expr->line, "Division by zero."); }
        switch (op) {
            case TOKEN_PLUS: result = floats ? value_float(a + b) : value_int(left.value.as.int_value + right.value.as.int_value); break;
            case TOKEN_MINUS: result = floats ? value_float(a - b) : value_int(left.value.as.int_value - right.value.as.int_value); break;
            case TOKEN_STAR: result = floats ? value_float(a * b) : value_int(left.value.as.int_value * right.value.as.int_value); break;
            case TOKEN_SLASH: result = floats ? value_float(a / b) : value_int(left.value.as.int_value / right.value.as.int_value); break;
            case TOKEN_GT: result = value_bool(a > b); break;
            case TOKEN_GTE: result = value_bool(a >= b); break;
            case TOKEN_LT: result = value_bool(a < b); break;
            case TOKEN_LTE: result = value_bool(a <= b); break;
            default: value_destroy(&left.value); value_destroy(&right.value); return runtime_error(interpreter, expr->line, "Unsupported binary operator.");
        }
    } else {
        value_destroy(&left.value); value_destroy(&right.value);
        return runtime_error(interpreter, expr->line, "Invalid operands for binary operator.");
    }
    value_destroy(&left.value); value_destroy(&right.value);
    return runtime_ok(result);
}

static RuntimeResult call_value(Interpreter* interpreter, const Expr* expr, Value* callee)
{
    Value* arguments = NULL;
    size_t count = expr->as.call.argument_count, i;
    RuntimeResult result;
    if (count > 0U) {
        arguments = (Value*)calloc(count, sizeof(Value));
        if (arguments == NULL) return runtime_error(interpreter, expr->line, "Out of memory while preparing function call.");
    }
    for (i = 0U; i < count; ++i) {
        RuntimeResult argument = evaluate_expr(interpreter, expr->as.call.arguments[i]);
        if (argument.status != RUNTIME_OK) {
            size_t j; for (j = 0U; j < i; ++j) value_destroy(&arguments[j]); free(arguments); return argument;
        }
        arguments[i] = argument.value;
    }
    if (callee->type == VALUE_NATIVE_FUNCTION) {
        NativeFunction* native = callee->as.native_function_value;
        if ((!native->is_variadic && count != native->arity) || (native->is_variadic && count < native->arity))
            result = runtime_error(interpreter, expr->line, "Native function '%s' expects %s%zu arguments but received %zu.", native->name, native->is_variadic ? "at least " : "", native->arity, count);
        else result = native->callback(interpreter, arguments, count);
    } else if (callee->type == VALUE_FUNCTION) {
        UserFunction* function = callee->as.function_value;
        const Stmt* declaration = function->declaration;
        Environment* call_environment;
        Environment* previous;
        ExecResult execution;
        if (count != declaration->as.function_declaration.parameter_count) result = runtime_error(interpreter, expr->line, "Function '%s' expects %zu arguments but received %zu.", declaration->as.function_declaration.name, declaration->as.function_declaration.parameter_count, count);
        else if (interpreter->call_depth >= interpreter->max_call_depth) result = runtime_error(interpreter, expr->line, "Maximum call depth exceeded.");
        else if ((call_environment = environment_create(function->closure)) == NULL) result = runtime_error(interpreter, expr->line, "Out of memory while creating call environment.");
        else {
            int bindings_ok = 1;
            for (i = 0U; i < count; ++i) {
                const Parameter* parameter = &declaration->as.function_declaration.parameters[i];
                if (!environment_define_typed(call_environment, parameter->name, value_type_from_token(parameter->declared_type), &arguments[i])) { bindings_ok = 0; break; }
            }
            if (!bindings_ok) result = runtime_error(interpreter, expr->line, "Failed to bind function arguments.");
            else {
                previous = interpreter->current_environment;
                interpreter->current_environment = call_environment;
                interpreter->call_depth++;
                execution = execute_stmt(interpreter, declaration->as.function_declaration.body);
                interpreter->call_depth--;
                interpreter->current_environment = previous;
                if (execution.type == EXEC_RETURN) {
                    if (declaration->as.function_declaration.return_type == TOKEN_FLOAT && execution.value.type == VALUE_INT) {
                        long long integer = execution.value.as.int_value;
                        value_destroy(&execution.value);
                        execution.value = value_float((double)integer);
                    }
                    result = runtime_ok(execution.value);
                }
                else if (execution.type == EXEC_NORMAL && declaration->as.function_declaration.return_type == TOKEN_VOID) result = runtime_ok(value_void());
                else if (execution.type == EXEC_RUNTIME_ERROR) result.status = RUNTIME_ERROR, result.value = value_void();
                else { value_destroy(&execution.value); result = runtime_error(interpreter, expr->line, "Function '%s' completed without a return value.", declaration->as.function_declaration.name); }
            }
            environment_destroy(call_environment);
        }
    } else result = runtime_error(interpreter, expr->line, "Attempted to call a non-callable value.");
    for (i = 0U; i < count; ++i) value_destroy(&arguments[i]);
    free(arguments);
    return result;
}

static RuntimeResult evaluate_conversion(Interpreter* interpreter, const Expr* expr)
{
    RuntimeResult source = evaluate_expr(interpreter, expr->as.conversion.value);
    Value result = value_void();
    char buffer[96];
    char* end;
    if (source.status != RUNTIME_OK) return source;
    switch (expr->as.conversion.target_type) {
        case TOKEN_INT:
            if (source.value.type == VALUE_INT) result = value_int(source.value.as.int_value);
            else if (source.value.type == VALUE_FLOAT) result = value_int((long long)source.value.as.float_value);
            else if (source.value.type == VALUE_BOOL) result = value_int(source.value.as.bool_value);
            else if (source.value.type == VALUE_STRING) {
                errno = 0; result = value_int(strtoll(source.value.as.string_value, &end, 10));
                if (errno != 0 || *end != '\0') { value_destroy(&source.value); return runtime_error(interpreter, expr->line, "Cannot convert string to int."); }
            } else { value_destroy(&source.value); return runtime_error(interpreter, expr->line, "Cannot convert %s to int.", value_type_name(source.value.type)); }
            break;
        case TOKEN_FLOAT:
            if (source.value.type == VALUE_FLOAT) result = value_float(source.value.as.float_value);
            else if (source.value.type == VALUE_INT) result = value_float((double)source.value.as.int_value);
            else if (source.value.type == VALUE_BOOL) result = value_float(source.value.as.bool_value ? 1.0 : 0.0);
            else if (source.value.type == VALUE_STRING) {
                errno = 0; result = value_float(strtod(source.value.as.string_value, &end));
                if (errno != 0 || *end != '\0') { value_destroy(&source.value); return runtime_error(interpreter, expr->line, "Cannot convert string to float."); }
            } else { value_destroy(&source.value); return runtime_error(interpreter, expr->line, "Cannot convert value to float."); }
            break;
        case TOKEN_STRING:
            if (source.value.type == VALUE_STRING) result = value_string(source.value.as.string_value);
            else if (source.value.type == VALUE_INT) { snprintf(buffer, sizeof(buffer), "%lld", source.value.as.int_value); result = value_string(buffer); }
            else if (source.value.type == VALUE_FLOAT) { snprintf(buffer, sizeof(buffer), "%.15g", source.value.as.float_value); result = value_string(buffer); }
            else if (source.value.type == VALUE_BOOL) result = value_string(source.value.as.bool_value ? "true" : "false");
            else { value_destroy(&source.value); return runtime_error(interpreter, expr->line, "Cannot convert value to string."); }
            if (result.as.string_value == NULL) { value_destroy(&source.value); return runtime_error(interpreter, expr->line, "Out of memory during string conversion."); }
            break;
        default: value_destroy(&source.value); return runtime_error(interpreter, expr->line, "Unsupported conversion target.");
    }
    value_destroy(&source.value); return runtime_ok(result);
}

RuntimeResult evaluate_expr(Interpreter* interpreter, const Expr* expr)
{
    RuntimeResult operand;
    Value* found;
    if (interpreter == NULL || expr == NULL) return runtime_error(interpreter, 0, "Invalid expression.");
    switch (expr->type) {
        case EXPR_INT_LITERAL: {
            char* end; long long number; errno = 0; number = strtoll(expr->as.literal.value, &end, 10);
            if (errno != 0 || *end != '\0') return runtime_error(interpreter, expr->line, "Invalid integer literal.");
            return runtime_ok(value_int(number));
        }
        case EXPR_FLOAT_LITERAL: {
            char* end; double number; errno = 0; number = strtod(expr->as.literal.value, &end);
            if (errno != 0 || *end != '\0') return runtime_error(interpreter, expr->line, "Invalid float literal.");
            return runtime_ok(value_float(number));
        }
        case EXPR_STRING_LITERAL: {
            Value value = value_string(expr->as.literal.value);
            if (value.as.string_value == NULL) return runtime_error(interpreter, expr->line, "Out of memory while copying string literal.");
            return runtime_ok(value);
        }
        case EXPR_BOOL_LITERAL: return runtime_ok(value_bool(strcmp(expr->as.literal.value, "true") == 0));
        case EXPR_IDENTIFIER: {
            Value copy;
            found = environment_get(interpreter->current_environment, expr->as.identifier.name);
            if (found == NULL) return runtime_error(interpreter, expr->line, "Undefined runtime variable '%s'.", expr->as.identifier.name);
            if (!value_copy(&copy, found)) return runtime_error(interpreter, expr->line, "Out of memory while reading '%s'.", expr->as.identifier.name);
            return runtime_ok(copy);
        }
        case EXPR_UNARY:
            operand = evaluate_expr(interpreter, expr->as.unary.operand);
            if (operand.status != RUNTIME_OK) return operand;
            if (expr->as.unary.operator_type == TOKEN_BANG && operand.value.type == VALUE_BOOL) operand.value.as.bool_value = !operand.value.as.bool_value;
            else if (expr->as.unary.operator_type == TOKEN_MINUS && operand.value.type == VALUE_INT) operand.value.as.int_value = -operand.value.as.int_value;
            else if (expr->as.unary.operator_type == TOKEN_MINUS && operand.value.type == VALUE_FLOAT) operand.value.as.float_value = -operand.value.as.float_value;
            else if (expr->as.unary.operator_type == TOKEN_PLUS && value_is_numeric(&operand.value)) { }
            else { value_destroy(&operand.value); return runtime_error(interpreter, expr->line, "Invalid operand for unary operator."); }
            return operand;
        case EXPR_BINARY: return evaluate_binary(interpreter, expr);
        case EXPR_ASSIGNMENT: {
            RuntimeResult assigned = evaluate_expr(interpreter, expr->as.assignment.value);
            Value copy;
            if (assigned.status != RUNTIME_OK) return assigned;
            if (!environment_assign(interpreter->current_environment, expr->as.assignment.target_name, assigned.value)) { value_destroy(&assigned.value); return runtime_error(interpreter, expr->line, "Cannot assign runtime variable '%s'.", expr->as.assignment.target_name); }
            found = environment_get(interpreter->current_environment, expr->as.assignment.target_name);
            value_destroy(&assigned.value);
            if (found == NULL || !value_copy(&copy, found)) return runtime_error(interpreter, expr->line, "Out of memory while returning assignment result.");
            return runtime_ok(copy);
        }
        case EXPR_CALL: {
            RuntimeResult callee = evaluate_expr(interpreter, expr->as.call.callee);
            RuntimeResult result;
            if (callee.status != RUNTIME_OK) return callee;
            result = call_value(interpreter, expr, &callee.value);
            value_destroy(&callee.value);
            return result;
        }
        case EXPR_LIST_LITERAL: {
            Value list = value_list();
            size_t i;
            if (list.type != VALUE_LIST) return runtime_error(interpreter, expr->line, "Out of memory while creating list.");
            for (i = 0U; i < expr->as.list_literal.element_count; ++i) {
                RuntimeResult element = evaluate_expr(interpreter, expr->as.list_literal.elements[i]);
                if (element.status != RUNTIME_OK) { value_destroy(&list); return element; }
                if (!value_list_append(&list, &element.value)) { value_destroy(&element.value); value_destroy(&list); return runtime_error(interpreter, expr->line, "Out of memory while appending list element."); }
                value_destroy(&element.value);
            }
            return runtime_ok(list);
        }
        case EXPR_INDEX: {
            RuntimeResult object = evaluate_expr(interpreter, expr->as.index.object);
            RuntimeResult index;
            const Value* element;
            Value copy;
            if (object.status != RUNTIME_OK) return object;
            index = evaluate_expr(interpreter, expr->as.index.index);
            if (index.status != RUNTIME_OK) { value_destroy(&object.value); return index; }
            if (object.value.type != VALUE_LIST || index.value.type != VALUE_INT) { value_destroy(&object.value); value_destroy(&index.value); return runtime_error(interpreter, expr->line, "List indexing expects list[int]."); }
            if (index.value.as.int_value < 0 || (element = value_list_get(&object.value, (size_t)index.value.as.int_value)) == NULL) { value_destroy(&object.value); value_destroy(&index.value); return runtime_error(interpreter, expr->line, "List index out of range."); }
            if (!value_copy(&copy, element)) { value_destroy(&object.value); value_destroy(&index.value); return runtime_error(interpreter, expr->line, "Out of memory while reading list element."); }
            value_destroy(&object.value); value_destroy(&index.value); return runtime_ok(copy);
        }
        case EXPR_METHOD_CALL: {
            RuntimeResult object = evaluate_expr(interpreter, expr->as.method_call.object);
            if (object.status != RUNTIME_OK) return object;
            if (object.value.type != VALUE_LIST) { value_destroy(&object.value); return runtime_error(interpreter, expr->line, "Method receiver must be a list."); }
            if (strcmp(expr->as.method_call.method_name, "size") == 0) {
                Value size;
                if (expr->as.method_call.argument_count != 0U) { value_destroy(&object.value); return runtime_error(interpreter, expr->line, "list.size() expects no arguments."); }
                size = value_int((long long)value_list_size(&object.value)); value_destroy(&object.value); return runtime_ok(size);
            }
            if (strcmp(expr->as.method_call.method_name, "append") == 0) {
                RuntimeResult item;
                if (expr->as.method_call.argument_count != 1U) { value_destroy(&object.value); return runtime_error(interpreter, expr->line, "list.append() expects one argument."); }
                item = evaluate_expr(interpreter, expr->as.method_call.arguments[0]);
                if (item.status != RUNTIME_OK) { value_destroy(&object.value); return item; }
                if (!value_list_append(&object.value, &item.value)) { value_destroy(&item.value); value_destroy(&object.value); return runtime_error(interpreter, expr->line, "Out of memory while appending list element."); }
                value_destroy(&item.value); value_destroy(&object.value); return runtime_ok(value_void());
            }
            value_destroy(&object.value); return runtime_error(interpreter, expr->line, "Unknown list method '%s'.", expr->as.method_call.method_name);
        }
        case EXPR_CONVERSION: return evaluate_conversion(interpreter, expr);
        case EXPR_INDEX_ASSIGNMENT: {
            RuntimeResult object = evaluate_expr(interpreter, expr->as.index_assignment.object);
            RuntimeResult index;
            RuntimeResult assigned;
            Value result;
            if (object.status != RUNTIME_OK) return object;
            index = evaluate_expr(interpreter, expr->as.index_assignment.index);
            if (index.status != RUNTIME_OK) { value_destroy(&object.value); return index; }
            if (object.value.type != VALUE_LIST || index.value.type != VALUE_INT) {
                value_destroy(&object.value); value_destroy(&index.value);
                return runtime_error(interpreter, expr->line, "List assignment expects list[int].");
            }
            if (index.value.as.int_value < 0 || (size_t)index.value.as.int_value >= value_list_size(&object.value)) {
                value_destroy(&object.value); value_destroy(&index.value);
                return runtime_error(interpreter, expr->line, "List assignment index out of range.");
            }
            assigned = evaluate_expr(interpreter, expr->as.index_assignment.value);
            if (assigned.status != RUNTIME_OK) { value_destroy(&object.value); value_destroy(&index.value); return assigned; }
            if (!value_list_set(&object.value, (size_t)index.value.as.int_value, &assigned.value) ||
                !value_copy(&result, &assigned.value)) {
                value_destroy(&object.value); value_destroy(&index.value); value_destroy(&assigned.value);
                return runtime_error(interpreter, expr->line, "Out of memory during list assignment.");
            }
            value_destroy(&object.value); value_destroy(&index.value); value_destroy(&assigned.value);
            return runtime_ok(result);
        }
    }
    return runtime_error(interpreter, expr->line, "Unsupported expression node.");
}

static ExecResult execute_block(Interpreter* interpreter, const Stmt* stmt)
{
    Environment* child = environment_create(interpreter->current_environment);
    Environment* previous;
    ExecResult result = exec_normal();
    size_t i;
    if (child == NULL) return exec_from_runtime(runtime_error(interpreter, stmt->line, "Out of memory while creating block scope."));
    previous = interpreter->current_environment;
    interpreter->current_environment = child;
    for (i = 0U; i < stmt->as.block.statement_count; ++i) {
        result = execute_stmt(interpreter, stmt->as.block.statements[i]);
        if (result.type != EXEC_NORMAL) break;
    }
    interpreter->current_environment = previous;
    environment_destroy(child);
    return result;
}

ExecResult execute_stmt(Interpreter* interpreter, const Stmt* stmt)
{
    RuntimeResult evaluated;
    if (interpreter == NULL || stmt == NULL) return exec_from_runtime(runtime_error(interpreter, 0, "Invalid statement."));
    switch (stmt->type) {
        case STMT_EXPRESSION:
            evaluated = evaluate_expr(interpreter, stmt->as.expression.expression);
            if (evaluated.status != RUNTIME_OK) return exec_from_runtime(evaluated);
            value_destroy(&evaluated.value); return exec_normal();
        case STMT_VARIABLE_DECLARATION: {
            Value initial;
            if (stmt->as.variable_declaration.initializer != NULL) {
                evaluated = evaluate_expr(interpreter, stmt->as.variable_declaration.initializer);
                if (evaluated.status != RUNTIME_OK) return exec_from_runtime(evaluated);
                initial = evaluated.value;
            } else initial = default_value_for_type(stmt->as.variable_declaration.declared_type);
            if (initial.type == VALUE_STRING && initial.as.string_value == NULL) return exec_from_runtime(runtime_error(interpreter, stmt->line, "Out of memory while creating default string."));
            if (!environment_define_typed(interpreter->current_environment, stmt->as.variable_declaration.name, value_type_from_token(stmt->as.variable_declaration.declared_type), &initial)) { value_destroy(&initial); return exec_from_runtime(runtime_error(interpreter, stmt->line, "Failed to define runtime variable '%s'.", stmt->as.variable_declaration.name)); }
            value_destroy(&initial); return exec_normal();
        }
        case STMT_BLOCK: return execute_block(interpreter, stmt);
        case STMT_IF:
            evaluated = evaluate_expr(interpreter, stmt->as.if_stmt.condition);
            if (evaluated.status != RUNTIME_OK) return exec_from_runtime(evaluated);
            if (evaluated.value.type != VALUE_BOOL) { value_destroy(&evaluated.value); return exec_from_runtime(runtime_error(interpreter, stmt->line, "If condition must be bool.")); }
            { int condition = evaluated.value.as.bool_value; value_destroy(&evaluated.value); if (condition) return execute_stmt(interpreter, stmt->as.if_stmt.then_branch); if (stmt->as.if_stmt.else_branch != NULL) return execute_stmt(interpreter, stmt->as.if_stmt.else_branch); return exec_normal(); }
        case STMT_WHILE:
            for (;;) {
                ExecResult body;
                evaluated = evaluate_expr(interpreter, stmt->as.while_stmt.condition);
                if (evaluated.status != RUNTIME_OK) return exec_from_runtime(evaluated);
                if (evaluated.value.type != VALUE_BOOL) { value_destroy(&evaluated.value); return exec_from_runtime(runtime_error(interpreter, stmt->line, "While condition must be bool.")); }
                if (!evaluated.value.as.bool_value) { value_destroy(&evaluated.value); return exec_normal(); }
                value_destroy(&evaluated.value);
                body = execute_stmt(interpreter, stmt->as.while_stmt.body);
                if (body.type == EXEC_CONTINUE) continue;
                if (body.type == EXEC_BREAK) return exec_normal();
                if (body.type != EXEC_NORMAL) return body;
            }
        case STMT_RETURN:
            if (stmt->as.return_stmt.value == NULL) return exec_result(EXEC_RETURN, value_void());
            evaluated = evaluate_expr(interpreter, stmt->as.return_stmt.value);
            if (evaluated.status != RUNTIME_OK) return exec_from_runtime(evaluated);
            return exec_result(EXEC_RETURN, evaluated.value);
        case STMT_FUNCTION_DECLARATION:
            if (!register_user_function(interpreter, stmt, interpreter->current_environment))
                return exec_from_runtime(runtime_error(interpreter, stmt->line, "Failed to register function '%s'.", stmt->as.function_declaration.name));
            return exec_normal();
    }
    return exec_from_runtime(runtime_error(interpreter, stmt->line, "Unsupported statement node."));
}

static int register_user_function(Interpreter* interpreter, const Stmt* declaration, Environment* closure)
{
    UserFunction* function;
    Value value;
    const char* name = declaration->as.function_declaration.name;
    function = (UserFunction*)malloc(sizeof(UserFunction));
    if (function == NULL) return 0;
    function->declaration = declaration;
    function->closure = closure;
    if (!append_pointer((void***)&interpreter->user_functions, &interpreter->user_function_count, &interpreter->user_function_capacity, function)) { free(function); return 0; }
    value = value_function(function);
    if (!environment_define(closure, name, value)) { interpreter->user_function_count--; free(function); return 0; }
    return 1;
}

ExecResult interpreter_execute(Interpreter* interpreter, const Program* program)
{
    size_t i;
    ExecResult result;
    if (interpreter == NULL || program == NULL || interpreter->globals == NULL) return exec_from_runtime(runtime_error(interpreter, 0, "Invalid program or interpreter."));
    interpreter->had_error = 0;
    interpreter->diagnostic.line = 0;
    interpreter->diagnostic.message[0] = '\0';
    interpreter->current_environment = interpreter->globals;
    for (i = 0U; i < program->statement_count; ++i) {
        const Stmt* stmt = program->statements[i];
        if (stmt->type == STMT_FUNCTION_DECLARATION && !register_user_function(interpreter, stmt, interpreter->globals))
            return exec_from_runtime(runtime_error(interpreter, stmt->line, "Failed to register function '%s'.", stmt->as.function_declaration.name));
    }
    for (i = 0U; i < program->statement_count; ++i) {
        if (program->statements[i]->type == STMT_FUNCTION_DECLARATION) continue;
        result = execute_stmt(interpreter, program->statements[i]);
        if (result.type != EXEC_NORMAL) {
            if (result.type == EXEC_RETURN) { value_destroy(&result.value); return exec_from_runtime(runtime_error(interpreter, program->statements[i]->line, "Return outside function.")); }
            return result;
        }
    }
    return exec_normal();
}
