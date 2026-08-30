#include "clumsyPL/value.hpp"

#include <stdlib.h>
#include <string.h>

static char* value_copy_string(const char* text)
{
    size_t length;
    char* copy;
    if (text == NULL) text = "";
    length = strlen(text);
    copy = (char*)malloc(length + 1U);
    if (copy != NULL) memcpy(copy, text, length + 1U);
    return copy;
}

struct RuntimeList {
    Value* elements;
    size_t count;
    size_t capacity;
    size_t ref_count;
};

Value value_void(void) { Value v; memset(&v, 0, sizeof(v)); v.type = VALUE_VOID; return v; }
Value value_int(long long x) { Value v = value_void(); v.type = VALUE_INT; v.as.int_value = x; return v; }
Value value_float(double x) { Value v = value_void(); v.type = VALUE_FLOAT; v.as.float_value = x; return v; }
Value value_bool(int x) { Value v = value_void(); v.type = VALUE_BOOL; v.as.bool_value = x != 0; return v; }
Value value_function(UserFunction* x) { Value v = value_void(); v.type = VALUE_FUNCTION; v.as.function_value = x; return v; }
Value value_native_function(NativeFunction* x) { Value v = value_void(); v.type = VALUE_NATIVE_FUNCTION; v.as.native_function_value = x; return v; }
Value value_list(void)
{
    Value v = value_void();
    RuntimeList* list = (RuntimeList*)calloc(1U, sizeof(RuntimeList));
    if (list == NULL) return v;
    list->ref_count = 1U; v.type = VALUE_LIST; v.as.list_value = list; return v;
}

size_t value_list_size(const Value* value) { return value != NULL && value->type == VALUE_LIST ? value->as.list_value->count : 0U; }

int value_list_append(Value* value, const Value* element)
{
    RuntimeList* list;
    Value* resized;
    size_t next;
    if (value == NULL || value->type != VALUE_LIST || element == NULL) return 0;
    list = value->as.list_value;
    if (list->count == list->capacity) {
        next = list->capacity == 0U ? 4U : list->capacity * 2U;
        resized = (Value*)realloc(list->elements, next * sizeof(Value));
        if (resized == NULL) return 0;
        list->elements = resized; list->capacity = next;
    }
    if (!value_copy(&list->elements[list->count], element)) return 0;
    list->count++; return 1;
}

const Value* value_list_get(const Value* value, size_t index)
{
    if (value == NULL || value->type != VALUE_LIST || index >= value->as.list_value->count) return NULL;
    return &value->as.list_value->elements[index];
}

int value_list_set(Value* value, size_t index, const Value* element)
{
    Value replacement;
    RuntimeList* list;
    if (value == NULL || value->type != VALUE_LIST || element == NULL) return 0;
    list = value->as.list_value;
    if (index >= list->count || !value_copy(&replacement, element)) return 0;
    value_destroy(&list->elements[index]);
    list->elements[index] = replacement;
    return 1;
}

Value value_string(const char* text)
{
    Value v = value_void();
    v.type = VALUE_STRING;
    v.as.string_value = value_copy_string(text);
    return v;
}

int value_copy(Value* destination, const Value* source)
{
    if (destination == NULL || source == NULL) return 0;
    *destination = *source;
    if (source->type == VALUE_STRING) {
        destination->as.string_value = value_copy_string(source->as.string_value);
        if (destination->as.string_value == NULL) {
            *destination = value_void();
            return 0;
        }
    } else if (source->type == VALUE_LIST) source->as.list_value->ref_count++, destination->as.list_value = source->as.list_value;
    return 1;
}

void value_destroy(Value* value)
{
    if (value == NULL) return;
    if (value->type == VALUE_STRING) free(value->as.string_value);
    else if (value->type == VALUE_LIST && --value->as.list_value->ref_count == 0U) {
        size_t i;
        for (i = 0U; i < value->as.list_value->count; ++i) value_destroy(&value->as.list_value->elements[i]);
        free(value->as.list_value->elements); free(value->as.list_value);
    }
    *value = value_void();
}

const char* value_type_name(ValueType type)
{
    switch (type) {
        case VALUE_VOID: return "void";
        case VALUE_INT: return "int";
        case VALUE_FLOAT: return "float";
        case VALUE_STRING: return "string";
        case VALUE_BOOL: return "bool";
        case VALUE_FUNCTION: return "function";
        case VALUE_NATIVE_FUNCTION: return "native function";
        case VALUE_LIST: return "list";
        default: return "unknown";
    }
}
