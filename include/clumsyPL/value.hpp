#ifndef VALUE_H
#define VALUE_H

#include <stddef.h>

typedef struct UserFunction UserFunction;
typedef struct NativeFunction NativeFunction;
typedef struct RuntimeList RuntimeList;

typedef enum {
    VALUE_VOID,
    VALUE_INT,
    VALUE_FLOAT,
    VALUE_STRING,
    VALUE_BOOL,
    VALUE_FUNCTION,
    VALUE_NATIVE_FUNCTION,
    VALUE_LIST
} ValueType;

typedef struct {
    ValueType type;
    union {
        long long int_value;
        double float_value;
        int bool_value;
        char* string_value;
        UserFunction* function_value;
        NativeFunction* native_function_value;
        RuntimeList* list_value;
    } as;
} Value;

Value value_void(void);
Value value_int(long long value);
Value value_float(double value);
Value value_bool(int value);
Value value_string(const char* value);
Value value_function(UserFunction* value);
Value value_native_function(NativeFunction* value);
Value value_list(void);
size_t value_list_size(const Value* value);
int value_list_append(Value* list, const Value* element);
const Value* value_list_get(const Value* list, size_t index);
int value_list_set(Value* list, size_t index, const Value* element);
int value_copy(Value* destination, const Value* source);
void value_destroy(Value* value);
const char* value_type_name(ValueType type);

#endif
