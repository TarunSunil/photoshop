# LumenForge — Current Engineering State

Updated: 2026-07-22

This note records the current uncommitted edits and their verification state
for the next development pass.

## Working-tree edits

### Base-layer insertion correction

`core/editor-core/DocumentModel.cpp` now calls `m_layers.push_back(base)`
immediately after assigning `kBaseLayerSourceAssetId` in
`DocumentModel::openSourceImage()`.

The prior committed layer-identity change accidentally placed this call after
a `//` comment, so opening a source image created its pixel entry without
adding the base `Layer` to `m_layers`. This one-line correction restores the
base-layer invariant. The broader layer-identity milestone is otherwise in
commit `436e84f` (`Fix base layer identity and reorder handling`).

### ONNX Runtime Release deployment

`app/CMakeLists.txt` now deploys the ONNX Runtime DLLs needed by a Release
build when `LUMEN_ENABLE_ONNX=ON`:

- `onnxruntime.dll`
- `onnxruntime_providers_shared.dll`
- `abseil_dll.dll`
- `libprotobuf-lite.dll`
- `libprotobuf.dll`
- `re2.dll`

Qt's normal deployment step does not include these non-Qt runtime
dependencies. The CMake change resolves the ONNX Runtime package location,
fails configuration clearly if its primary DLL is unavailable, and copies the
runtime files after building `LumenForge`.

## Real-ESRGAN diagnostic result

The existing `models/realesrgan-x4plus.onnx` was not modified or regenerated.

Verified diagnostics:

| Item | Result |
| --- | --- |
| ONNX IR version | 6 |
| ONNX opset | `ai.onnx` 11 |
| Python `onnx` package | 1.22.0 |
| vcpkg ONNX Runtime | 1.23.2 |
| Python ONNX Runtime | 1.27.0 |
| `onnx.checker` | Passes |
| Tensor interface | `input [1,3,512,512]` → `output [1,3,2048,2048]` |

The model session loads successfully through the actual project
`OnnxSession` implementation with the deployed vcpkg DLL set:

```text
loaded=true
lastError=[]
```

Therefore, the current model is not incompatible with the project runtime.
The confirmed issue was Release deployment: required ONNX Runtime DLLs were
not copied alongside the executable. The earlier
`Invalid model` / `/conv_first/Conv` message is not reproducible from the unchanged current
model and should not be treated as a model-export defect.

## Layer renaming milestone

Layer renaming is now implemented across the model, controller, and layer
list UI:

- `DocumentModel::setLayerName()` trims input, rejects empty names, and records
  a single undoable `Rename layer: …` history step.
- `DocumentController::renameLayer()` exposes the operation to QML.
- The layer row enters an inline `TextInput` on double-click; Enter or focus
  loss commits the trimmed name.

The Release build completed successfully after this change. No manual UI
verification has been performed; that remains a developer-side check.

## Drag responsiveness milestone

Transform drags now avoid preview-composite churn while the gesture is active:

- `DocumentController` still emits `layersChanged()` on every transform tick,
  so the QML transform overlay remains live.
- While `m_layerTransformEditOpen` is true, the document-change handler skips
  starting the asynchronous preview and HQ-preview timers.
- Beginning a transform cancels stale preview work and invalidates its request
  id; committing the transform schedules one debounced preview rebuild.

This preserves the existing single undo transaction for move/resize/rotate,
while preventing a full 1400×1050 composite from being launched repeatedly
during a drag. Crop-box movement was already QML-only until confirmation, and
canvas pan remains local `Flickable` state, so neither path needed backend
changes. The Release build passes after this optimization; manual interaction
verification remains outstanding.

## Verification performed

- Configured with `LUMEN_ENABLE_ONNX=ON`.
- Built the Release solution successfully after the deployment change.
- Confirmed all six ONNX Runtime DLLs listed above exist in `build/Release`.
- Invoked `OnnxSession::ensureLoaded()` against the current model using that
  deployed runtime set; it returned `true` and left `lastError()` empty.
- Ran `git diff --check`; no whitespace errors were reported for the current
  edits.

## Next safe work

No Real-ESRGAN model or export-script change is required. The next AI task,
if desired, is a functional inference/performance pass (including the known
tile-overlap blending limitation) after manual app-level verification.

For UI work, resume the roadmap's design-system milestone (`Theme.qml`, icon
assets, and shared QML controls). Keep it independent from the AI work.

## Working-tree status at handoff

- Modified: `core/editor-core/DocumentModel.cpp`
- Modified: `app/CMakeLists.txt`
- Modified: `core/editor-core/DocumentModel.hpp`
- Modified: `app/src/editor/DocumentController.hpp`
- Modified: `app/src/editor/DocumentController.cpp`
- Modified: `app/resources/qml/Main.qml`
- Modified: `app/src/editor/DocumentController.cpp`
- New: `CURRENT_STATE.md`
- Untracked: `Real-ESRGAN/` (existing local checkout; not modified by this
  handoff)

These edits are intentionally uncommitted.
