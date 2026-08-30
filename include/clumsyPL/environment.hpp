#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "value.hpp"

#include <stddef.h>

typedef struct {
    char* name;
    Value value;
    ValueType declared_type;
} RuntimeVariable;

typedef struct Environment Environment;
struct Environment {
    RuntimeVariable* variables;
    size_t count;
    size_t capacity;
    Environment* parent;
};

Environment* environment_create(Environment* parent);
void environment_destroy(Environment* environment);
int environment_define(Environment* environment, const char* name, Value value);
int environment_define_typed(Environment* environment, const char* name, ValueType declared_type, const Value* value);
Value* environment_get(Environment* environment, const char* name);
int environment_assign(Environment* environment, const char* name, Value value);

#endif
