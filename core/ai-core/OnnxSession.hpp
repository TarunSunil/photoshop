#pragma once
// OnnxSession.hpp — ONNX Runtime session wrapper.
//
// IMPORTANT: This header intentionally does NOT include <onnxruntime_cxx_api.h>
// or <onnxruntime_c_api.h>.  Including either header in a translation unit
// that is linked into the executable causes onnxruntime_cxx_api.h to
// instantiate:
//
//   template <typename T>
//   const OrtApi* Global<T>::api_{ OrtGetApiBase()->GetApi(ORT_API_VERSION) };
//
// This non-local static calls OrtGetApiBase() — a function inside
// onnxruntime.dll — during C++ static-initialisation, BEFORE main() runs.
// Even with /DELAYLOAD:onnxruntime.dll the delay-load fires at that point,
// and if any transitive dependency (abseil_dll.dll, re2.dll, cpuinfo.dll …)
// is absent the process dies with 0xC06D007E before printing anything.
//
// OnnxSession.cpp uses <onnxruntime_c_api.h> (the pure-C function-pointer
// table) instead.  That header has NO global statics.  OrtGetApiBase() is
// called lazily inside ensureLoaded(), which runs inside the Qt event loop
// where failures surface as a QString error rather than a silent pre-main
// crash.

#include <QString>
#include <vector>
#include <cstdint>
#include <memory>

namespace lumen {

class OnnxSession {
public:
    explicit OnnxSession(const QString& modelPath);
    ~OnnxSession();

    // Returns true if the model was already loaded or loads successfully now.
    bool ensureLoaded();

    [[nodiscard]] bool    isLoaded()  const;
    [[nodiscard]] QString lastError() const;

    // Run inference.
    // inputData[i]   : flat float32 values for input tensor i
    // inputShapes[i] : NCHW (or whatever the model expects) shape for input i
    // inputNames     : names matching the model graph (must stay alive during call)
    // outputNames    : names of requested outputs (first output is returned)
    // Returns the flat float32 values of the FIRST requested output tensor.
    // Returns an empty vector on any error; inspect lastError() for details.
    std::vector<float> run(
        const std::vector<std::vector<float>>&   inputData,
        const std::vector<std::vector<int64_t>>& inputShapes,
        const std::vector<const char*>&          inputNames,
        const std::vector<const char*>&          outputNames);

    // Resolves modelPath against: the given path, next to the exe,
    // and up to 5 parent directories of the exe.  Returns the first
    // path that exists, or the original path if none is found.
    [[nodiscard]] static QString findModelPath(const QString& relativePath);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    QString m_modelPath;
    QString m_lastError;
    bool    m_loaded = false;
};

} // namespace lumen