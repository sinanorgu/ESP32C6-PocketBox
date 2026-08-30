#include <unity.h>

#include "clumsyPL/ClumsyPL.hpp"

#include <stdlib.h>
#include <string.h>

namespace {

struct Output {
    char text[512];
    size_t length;
};

void capture(const char* text, void* context)
{
    Output* output = static_cast<Output*>(context);
    const size_t available = sizeof(output->text) - output->length - 1U;
    const size_t amount = strlen(text) < available ? strlen(text) : available;
    memcpy(output->text + output->length, text, amount);
    output->length += amount;
    output->text[output->length] = '\0';
}

RuntimeResult nativeDouble(Interpreter*, const Value* arguments, size_t)
{
    RuntimeResult result;
    result.status = RUNTIME_OK;
    result.value = value_int(arguments[0].as.int_value * 2);
    return result;
}

void test_language_semantics_and_output()
{
    Output output{};
    clumsy::Runtime runtime;
    runtime.setOutput(capture, &output);
    const clumsy::Result result = runtime.execute(
        "int factorial(int n){if(n<=1){return 1;}return n*factorial(n-1);}"
        "list values=[factorial(5),\"same language\",true];print(values);"
    );
    TEST_ASSERT_TRUE_MESSAGE(result.ok(), result.message);
    TEST_ASSERT_EQUAL_STRING("[120, same language, true]\n", output.text);
}

void test_parse_error_is_reported()
{
    clumsy::Runtime runtime;
    const clumsy::Result result = runtime.execute("int value = ;");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(clumsy::Status::ParseError), static_cast<int>(result.status));
    TEST_ASSERT_GREATER_THAN(0, result.line);
    TEST_ASSERT_NOT_EQUAL(0, strlen(result.message));
}

void test_runtime_error_is_reported()
{
    clumsy::Runtime runtime;
    const clumsy::Result result = runtime.execute("int zero=0;print(10/zero);");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(clumsy::Status::RuntimeError), static_cast<int>(result.status));
    TEST_ASSERT_NOT_EQUAL(0, strlen(result.message));
}

void test_device_native_is_visible_to_semantics_and_runtime()
{
    Output output{};
    clumsy::Runtime runtime;
    runtime.setOutput(capture, &output);
    const SemanticType parameters[] = { SEM_TYPE_INT };
    const clumsy::NativeDefinition definition = {
        "deviceDouble", SEM_TYPE_INT, parameters, 1U, false, false, nativeDouble
    };
    TEST_ASSERT_TRUE(runtime.addNative(definition));
    const clumsy::Result result = runtime.execute("print(deviceDouble(21));");
    TEST_ASSERT_TRUE_MESSAGE(result.ok(), result.message);
    TEST_ASSERT_EQUAL_STRING("42\n", output.text);
}

} // namespace

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_language_semantics_and_output);
    RUN_TEST(test_parse_error_is_reported);
    RUN_TEST(test_runtime_error_is_reported);
    RUN_TEST(test_device_native_is_visible_to_semantics_and_runtime);
    return UNITY_END();
}
