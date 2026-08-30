#include "clumsyPL/environment.hpp"

#include <stdlib.h>
#include <string.h>

static char* copy_name(const char* name)
{
    size_t n;
    char* result;
    if (name == NULL) return NULL;
    n = strlen(name);
    result = (char*)malloc(n + 1U);
    if (result != NULL) memcpy(result, name, n + 1U);
    return result;
}

static RuntimeVariable* find_current(Environment* environment, const char* name)
{
    size_t i;
    if (environment == NULL || name == NULL) return NULL;
    for (i = 0U; i < environment->count; ++i)
        if (strcmp(environment->variables[i].name, name) == 0) return &environment->variables[i];
    return NULL;
}

static int convert_for_type(Value* output, ValueType type, const Value* input)
{
    if (type == VALUE_FLOAT && input->type == VALUE_INT) {
        *output = value_float((double)input->as.int_value);
        return 1;
    }
    if (type != input->type && type != VALUE_VOID) return 0;
    return value_copy(output, input);
}

Environment* environment_create(Environment* parent)
{
    Environment* environment = (Environment*)calloc(1U, sizeof(Environment));
    if (environment != NULL) environment->parent = parent;
    return environment;
}

void environment_destroy(Environment* environment)
{
    size_t i;
    if (environment == NULL) return;
    for (i = 0U; i < environment->count; ++i) {
        free(environment->variables[i].name);
        value_destroy(&environment->variables[i].value);
    }
    free(environment->variables);
    free(environment);
}

int environment_define_typed(Environment* environment, const char* name, ValueType declared_type, const Value* value)
{
    RuntimeVariable* resized;
    RuntimeVariable variable;
    size_t capacity;
    if (environment == NULL || name == NULL || value == NULL || find_current(environment, name) != NULL) return 0;
    memset(&variable, 0, sizeof(variable));
    variable.name = copy_name(name);
    variable.declared_type = declared_type;
    if (variable.name == NULL || !convert_for_type(&variable.value, declared_type, value)) {
        free(variable.name);
        return 0;
    }
    if (environment->count == environment->capacity) {
        capacity = environment->capacity == 0U ? 8U : environment->capacity * 2U;
        resized = (RuntimeVariable*)realloc(environment->variables, capacity * sizeof(RuntimeVariable));
        if (resized == NULL) {
            free(variable.name);
            value_destroy(&variable.value);
            return 0;
        }
        environment->variables = resized;
        environment->capacity = capacity;
    }
    environment->variables[environment->count++] = variable;
    return 1;
}

int environment_define(Environment* environment, const char* name, Value value)
{
    return environment_define_typed(environment, name, value.type, &value);
}

Value* environment_get(Environment* environment, const char* name)
{
    while (environment != NULL) {
        RuntimeVariable* variable = find_current(environment, name);
        if (variable != NULL) return &variable->value;
        environment = environment->parent;
    }
    return NULL;
}

int environment_assign(Environment* environment, const char* name, Value value)
{
    while (environment != NULL) {
        RuntimeVariable* variable = find_current(environment, name);
        if (variable != NULL) {
            Value replacement;
            if (!convert_for_type(&replacement, variable->declared_type, &value)) return 0;
            value_destroy(&variable->value);
            variable->value = replacement;
            return 1;
        }
        environment = environment->parent;
    }
    return 0;
}
