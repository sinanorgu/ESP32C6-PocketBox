#include "clumsyPL/ClumsyPL.hpp"

#include "clumsyPL/builtins.hpp"
#include "clumsyPL/lexer.hpp"
#include "clumsyPL/parser.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace clumsy {
namespace {

Result makeResult(Status status, int line, const char* message)
{
    Result result{};
    result.status = status;
    result.line = line;
    snprintf(result.message, sizeof(result.message), "%s", message != nullptr ? message : "");
    return result;
}

#ifdef ARDUINO
void serialOutput(const char* text, void*)
{
    if (text != nullptr) Serial.print(text);
}
#endif

} // namespace

Runtime::Runtime()
    : output_(nullptr), outputUserData_(nullptr), maxCallDepth_(128U), nativeCount_(0U)
{
}

void Runtime::setOutput(InterpreterOutputCallback callback, void* userData)
{
    output_ = callback;
    outputUserData_ = userData;
}

void Runtime::setMaxCallDepth(size_t maximum)
{
    maxCallDepth_ = maximum;
}

bool Runtime::addNative(const NativeDefinition& definition)
{
    if (definition.name == nullptr || definition.name[0] == '\0' || definition.callback == nullptr ||
        definition.parameterCount > MaxNativeParameters || nativeCount_ >= MaxNativeFunctions ||
        (definition.parameterCount > 0U && definition.parameterTypes == nullptr)) return false;

    StoredNative& stored = natives_[nativeCount_];
    stored.definition = definition;
    for (size_t i = 0U; i < definition.parameterCount; ++i)
        stored.parameters[i] = definition.parameterTypes[i];
    stored.definition.parameterTypes = stored.parameters;
    ++nativeCount_;
    return true;
}

Result Runtime::execute(const char* source) const
{
    if (source == nullptr) return makeResult(Status::InvalidArgument, 0, "Source is null.");

    Lexer lexer = create_lexer(source);
    Parser parser = create_parser(&lexer);
    parser_set_error_output(&parser, 0);
    Program* program = parse_program(&parser);
    if (parser.had_error || program == nullptr) {
        Result result = parser.diagnostic_count > 0U
            ? makeResult(Status::ParseError, parser.diagnostics[0].line, parser.diagnostics[0].message)
            : makeResult(Status::ParseError, 0, "Could not parse source.");
        free_program(program);
        destroy_parser(&parser);
        return result;
    }

    SemanticAnalyzer semantic = create_semantic_analyzer();
    bool definitionsOk = builtin_register_semantic(&semantic) != 0;
    for (size_t i = 0U; definitionsOk && i < nativeCount_; ++i) {
        const NativeDefinition& native = natives_[i].definition;
        definitionsOk = semantic_define_native_function_ex(&semantic, native.name, native.returnType,
            native.parameterTypes, native.parameterCount, native.variadic, native.acceptsAny) != 0;
    }
    if (!definitionsOk || !semantic_analyze_program(&semantic, program)) {
        Result result = semantic.diagnostic_count > 0U
            ? makeResult(Status::SemanticError, semantic.diagnostics[0].line, semantic.diagnostics[0].message)
            : makeResult(Status::OutOfMemory, 0, "Could not initialize semantic analysis.");
        destroy_semantic_analyzer(&semantic);
        free_program(program);
        destroy_parser(&parser);
        return result;
    }

    Interpreter interpreter;
    if (!interpreter_init(&interpreter)) {
        destroy_semantic_analyzer(&semantic);
        free_program(program);
        destroy_parser(&parser);
        return makeResult(Status::OutOfMemory, 0, "Could not initialize interpreter.");
    }
    if (output_ != nullptr) interpreter_set_output(&interpreter, output_, outputUserData_);
    interpreter_set_max_call_depth(&interpreter, maxCallDepth_);

    bool runtimeDefinitionsOk = true;
    for (size_t i = 0U; runtimeDefinitionsOk && i < nativeCount_; ++i) {
        const NativeDefinition& native = natives_[i].definition;
        runtimeDefinitionsOk = interpreter_define_native(&interpreter, native.name, native.callback,
            native.parameterCount, native.variadic) != 0;
    }

    Result result;
    if (!runtimeDefinitionsOk) {
        result = makeResult(Status::OutOfMemory, 0, "Could not register native function.");
    } else {
        ExecResult execution = interpreter_execute(&interpreter, program);
        if (execution.type == EXEC_RUNTIME_ERROR) {
            const RuntimeDiagnostic* diagnostic = interpreter_get_diagnostic(&interpreter);
            result = makeResult(Status::RuntimeError, diagnostic != nullptr ? diagnostic->line : 0,
                diagnostic != nullptr ? diagnostic->message : "Runtime error.");
        } else {
            result = makeResult(Status::Ok, 0, "");
        }
        value_destroy(&execution.value);
    }

    interpreter_destroy(&interpreter);
    destroy_semantic_analyzer(&semantic);
    free_program(program);
    destroy_parser(&parser);
    return result;
}

#ifdef ARDUINO
Result Runtime::executeFile(fs::FS& filesystem, const char* path) const
{
    if (path == nullptr) return makeResult(Status::InvalidArgument, 0, "Path is null.");
    File file = filesystem.open(path, FILE_READ);
    if (!file || file.isDirectory()) return makeResult(Status::FileError, 0, "Could not open source file.");

    const size_t length = file.size();
    char* source = static_cast<char*>(malloc(length + 1U));
    if (source == nullptr) { file.close(); return makeResult(Status::OutOfMemory, 0, "Could not allocate source buffer."); }
    const size_t read = file.readBytes(source, length);
    file.close();
    if (read != length) { free(source); return makeResult(Status::FileError, 0, "Could not read complete source file."); }
    source[length] = '\0';
    Result result = execute(source);
    free(source);
    return result;
}

void Runtime::useSerialOutput()
{
    setOutput(serialOutput, nullptr);
}
#endif

const char* statusText(Status status)
{
    switch (status) {
        case Status::Ok: return "ok";
        case Status::InvalidArgument: return "invalid argument";
        case Status::OutOfMemory: return "out of memory";
        case Status::FileError: return "file error";
        case Status::ParseError: return "parse error";
        case Status::SemanticError: return "semantic error";
        case Status::RuntimeError: return "runtime error";
    }
    return "unknown";
}

} // namespace clumsy
