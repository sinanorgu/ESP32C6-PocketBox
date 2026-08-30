#pragma once

#include "clumsyPL/interpreter.hpp"
#include "clumsyPL/semantic.hpp"

#include <stddef.h>

#ifdef ARDUINO
#include <FS.h>
#endif

namespace clumsy {

enum class Status {
    Ok,
    InvalidArgument,
    OutOfMemory,
    FileError,
    ParseError,
    SemanticError,
    RuntimeError
};

struct Result {
    Status status;
    int line;
    char message[256];

    bool ok() const { return status == Status::Ok; }
};

struct NativeDefinition {
    const char* name;
    SemanticType returnType;
    const SemanticType* parameterTypes;
    size_t parameterCount;
    bool variadic;
    bool acceptsAny;
    NativeFunctionCallback callback;
};

class Runtime {
public:
    Runtime();

    void setOutput(InterpreterOutputCallback callback, void* userData = nullptr);
    void setMaxCallDepth(size_t maximum);

    // The definition is applied to both semantic analysis and execution. This is
    // the extension point for device functions such as draw(...).
    bool addNative(const NativeDefinition& definition);

    Result execute(const char* source) const;

#ifdef ARDUINO
    Result executeFile(fs::FS& filesystem, const char* path) const;
    void useSerialOutput();
#endif

private:
    static constexpr size_t MaxNativeFunctions = 16U;
    static constexpr size_t MaxNativeParameters = 8U;

    struct StoredNative {
        NativeDefinition definition;
        SemanticType parameters[MaxNativeParameters];
    };

    InterpreterOutputCallback output_;
    void* outputUserData_;
    size_t maxCallDepth_;
    StoredNative natives_[MaxNativeFunctions];
    size_t nativeCount_;
};

const char* statusText(Status status);

} // namespace clumsy
