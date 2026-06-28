#include "ai-core/OnnxSession.hpp"

// ── CRITICAL: use the C API, NOT <onnxruntime_cxx_api.h> ─────────────────────
//
// <onnxruntime_cxx_api.h> defines:
//   template <typename T>
//   const OrtApi* Global<T>::api_{ OrtGetApiBase()->GetApi(ORT_API_VERSION) };
//
// That template static member fires during C++ static-initialisation —
// BEFORE main() — which triggers /DELAYLOAD for onnxruntime.dll.  If any
// transitive dependency (abseil_dll.dll, re2.dll, cpuinfo.dll …) is missing
// from the exe directory, the delay-load throws 0xC06D007E and the process
// dies silently before printing a single character.
//
// <onnxruntime_c_api.h> is a plain C header with ZERO global statics.
// OrtGetApiBase() is called lazily inside ensureLoaded(), which runs inside
// the Qt event loop after main() has started.  Any failure surfaces as a
// human-readable error message in the UI rather than a pre-main crash.

#ifdef HAVE_ONNXRUNTIME
#  include <onnxruntime_c_api.h>  // ← C API only — zero global statics
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <cstring>
#include <cstdlib>

namespace lumen {

// ── PIMPL — only lives on the heap, allocated lazily on first ensureLoaded() ─
struct OnnxSession::Impl {
#ifdef HAVE_ONNXRUNTIME
    const OrtApi*       api     = nullptr;
    OrtEnv*             env     = nullptr;
    OrtSessionOptions*  opts    = nullptr;
    OrtSession*         session = nullptr;

    // Release all ORT objects in reverse construction order.
    void release()
    {
        if (!api) return;
        if (session) { api->ReleaseSession(session);       session = nullptr; }
        if (opts)    { api->ReleaseSessionOptions(opts);   opts    = nullptr; }
        if (env)     { api->ReleaseEnv(env);               env     = nullptr; }
        api = nullptr;
    }
    ~Impl() { release(); }
#endif
};

// ── findModelPath ─────────────────────────────────────────────────────────────
QString OnnxSession::findModelPath(const QString& relativePath)
{
    QStringList candidates;
    candidates << relativePath;
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << appDir + "/" + relativePath;
    QDir dir(appDir);
    for (int i = 0; i < 5; ++i) {
        if (!dir.cdUp()) break;
        candidates << dir.filePath(relativePath);
    }
    for (const QString& c : candidates)
        if (QFileInfo::exists(c)) return c;
    return relativePath;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────
OnnxSession::OnnxSession(const QString& modelPath)
    : d(std::make_unique<Impl>())
    , m_modelPath(findModelPath(modelPath))
{}

OnnxSession::~OnnxSession() = default;

bool    OnnxSession::isLoaded()  const { return m_loaded; }
QString OnnxSession::lastError() const { return m_lastError; }

// ── ensureLoaded ──────────────────────────────────────────────────────────────
// All ORT API calls live here.  Called lazily from run() → inside Qt event
// loop.  OrtGetApiBase() is the FIRST ORT symbol touched, so the DLL load
// (or delay-load) happens here and failures are catchable.
bool OnnxSession::ensureLoaded()
{
    if (m_loaded) return true;

#ifndef HAVE_ONNXRUNTIME
    m_lastError = "ONNX Runtime not available in this build (LUMEN_ENABLE_ONNX=OFF)";
    return false;
#else

    if (!QFileInfo::exists(m_modelPath)) {
        m_lastError = "Model file not found: " + m_modelPath;
        return false;
    }

    // ── Obtain the API table (lazy — first ORT call in the whole process) ──
    // If onnxruntime.dll or any dependency is missing, GetApiBase() will
    // fail here (inside the event loop) rather than before main().
    const OrtApiBase* base = nullptr;
    try {
        base = OrtGetApiBase();
    } catch (...) {
        m_lastError = "Failed to load ONNX Runtime DLL. "
                      "Ensure onnxruntime.dll and its dependencies "
                      "(abseil_dll.dll, re2.dll, cpuinfo.dll …) are "
                      "present in the application directory.";
        return false;
    }
    if (!base) {
        m_lastError = "OrtGetApiBase() returned null";
        return false;
    }

    d->api = base->GetApi(ORT_API_VERSION);
    if (!d->api) {
        m_lastError = QString("ONNX Runtime API version %1 not supported by the installed DLL")
                          .arg(ORT_API_VERSION);
        return false;
    }
    const OrtApi* api = d->api;

    // ── Create environment ────────────────────────────────────────────────
    OrtStatus* st = api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "lumen", &d->env);
    if (st) {
        m_lastError = QString("OrtCreateEnv: %1").arg(
            QString::fromUtf8(api->GetErrorMessage(st)));
        api->ReleaseStatus(st);
        d->release();
        return false;
    }

    // ── Session options ───────────────────────────────────────────────────
    st = api->CreateSessionOptions(&d->opts);
    if (st) {
        m_lastError = QString("OrtCreateSessionOptions: %1").arg(
            QString::fromUtf8(api->GetErrorMessage(st)));
        api->ReleaseStatus(st);
        d->release();
        return false;
    }
    api->SetIntraOpNumThreads(d->opts, 4);
    api->SetSessionGraphOptimizationLevel(d->opts, ORT_ENABLE_ALL);

    // ── Create session (loads model weights) ──────────────────────────────
#ifdef _WIN32
    st = api->CreateSession(d->env, m_modelPath.toStdWString().c_str(),
                            d->opts, &d->session);
#else
    st = api->CreateSession(d->env, m_modelPath.toLocal8Bit().constData(),
                            d->opts, &d->session);
#endif
    if (st) {
        m_lastError = QString("Failed to load model %1: %2")
                          .arg(m_modelPath,
                               QString::fromUtf8(api->GetErrorMessage(st)));
        api->ReleaseStatus(st);
        d->release();
        return false;
    }

    m_loaded = true;
    m_lastError.clear();
    return true;
#endif // HAVE_ONNXRUNTIME
}

// ── run ───────────────────────────────────────────────────────────────────────
std::vector<float> OnnxSession::run(
    const std::vector<std::vector<float>>&   inputData,
    const std::vector<std::vector<int64_t>>& inputShapes,
    const std::vector<const char*>&          inputNames,
    const std::vector<const char*>&          outputNames)
{
    if (!ensureLoaded()) return {};

#ifndef HAVE_ONNXRUNTIME
    return {};
#else
    const OrtApi* api = d->api;

    // ── Memory info (CPU) ─────────────────────────────────────────────────
    OrtMemoryInfo* memInfo = nullptr;
    OrtStatus* st = api->CreateCpuMemoryInfo(OrtArenaAllocator,
                                              OrtMemTypeDefault, &memInfo);
    if (st) {
        m_lastError = QString("CreateCpuMemoryInfo: %1")
                          .arg(QString::fromUtf8(api->GetErrorMessage(st)));
        api->ReleaseStatus(st);
        return {};
    }

    // ── Build input tensors ───────────────────────────────────────────────
    const size_t nIn = inputData.size();
    std::vector<OrtValue*> inputTensors(nIn, nullptr);
    bool inputOk = true;

    for (size_t i = 0; i < nIn; ++i) {
        st = api->CreateTensorWithDataAsOrtValue(
            memInfo,
            const_cast<float*>(inputData[i].data()),
            inputData[i].size() * sizeof(float),
            inputShapes[i].data(),
            inputShapes[i].size(),
            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
            &inputTensors[i]);
        if (st) {
            m_lastError = QString("CreateTensor[%1]: %2")
                              .arg(i).arg(QString::fromUtf8(api->GetErrorMessage(st)));
            api->ReleaseStatus(st);
            inputOk = false;
            break;
        }
    }
    api->ReleaseMemoryInfo(memInfo);

    if (!inputOk) {
        for (auto* t : inputTensors) if (t) api->ReleaseValue(t);
        return {};
    }

    // ── Run inference ─────────────────────────────────────────────────────
    const size_t nOut = outputNames.size();
    std::vector<OrtValue*> outputTensors(nOut, nullptr);

    st = api->Run(d->session,
                  nullptr,                   // RunOptions (default)
                  inputNames.data(),
                  inputTensors.data(),
                  nIn,
                  outputNames.data(),
                  nOut,
                  outputTensors.data());

    for (auto* t : inputTensors) if (t) api->ReleaseValue(t);

    if (st) {
        m_lastError = QString("Inference failed: %1")
                          .arg(QString::fromUtf8(api->GetErrorMessage(st)));
        api->ReleaseStatus(st);
        for (auto* t : outputTensors) if (t) api->ReleaseValue(t);
        return {};
    }

    // ── Extract first output tensor ───────────────────────────────────────
    if (!outputTensors[0]) {
        m_lastError = "Model produced no output";
        for (auto* t : outputTensors) if (t) api->ReleaseValue(t);
        return {};
    }

    // Get shape → element count
    OrtTensorTypeAndShapeInfo* shapeInfo = nullptr;
    api->GetTensorTypeAndShape(outputTensors[0], &shapeInfo);
    size_t count = 0;
    api->GetTensorShapeElementCount(shapeInfo, &count);
    api->ReleaseTensorTypeAndShapeInfo(shapeInfo);

    // Get data pointer
    float* dataPtr = nullptr;
    api->GetTensorMutableData(outputTensors[0], reinterpret_cast<void**>(&dataPtr));

    std::vector<float> result(dataPtr, dataPtr + count);

    for (auto* t : outputTensors) if (t) api->ReleaseValue(t);

    m_lastError.clear();
    return result;
#endif // HAVE_ONNXRUNTIME
}

} // namespace lumen