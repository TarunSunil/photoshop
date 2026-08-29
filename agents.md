# AGENTS.md — LumenForge

This file is instructions for AI coding agents (Claude Code, Codex, Cursor,
Copilot Agent, OpenCode, etc.) working in this repository. It encodes
project-specific engineering rules discovered by studying the actual
codebase — not generic C++/Qt advice. Read this before touching any file.

---

## Project Overview

LumenForge is a native, offline-first Windows desktop photo editor built on
**Qt 6 / QML** with a **C++20** core. It is not a toy project — it has a
multi-layer compositor, mask-based local adjustments, layer transforms,
project persistence (SQLite), an optional ONNX Runtime AI subsystem
(inpainting, upscaling, subject-mask prediction), and a full non-destructive
adjustment pipeline with undo/redo.

**Layered core architecture** (`core/`), each layer a separate CMake static
library, all rolled into one `lumen_core` static lib linked by the `app`
executable:

```
shared-types   (Adjustment, Layer, Mask — plain structs, no logic)
     ↓
mask-core, image-core   (BrushEngine, RenderPipeline, RawImporter, ColorManager)
     ↓
editor-core   (DocumentModel — the single source of truth / undo engine)
     ↓
ai-core, export-core, storage   (AiRuntime, ExportService, ProjectStore)
     ↓
app/  (DocumentController — Qt/QML bridge, QML files)
```

`DocumentModel` (in `editor-core`) owns all document state and the
undo/redo stack. `DocumentController` (in `app/src/editor`) is a thin
Qt-property/Q_INVOKABLE wrapper around it that QML binds to — it also owns
transient UI-only state (preview debouncing, brush engine, temp file
paths, AI job orchestration) that must never leak into `DocumentModel`.
`RenderPipeline` is stateless and pure-functional: same inputs → same
pixels, always. Never give it mutable document state.

---

## Technologies

- **Language:** C++20, CMake ≥ 3.24, MSVC (Visual Studio 2022) on Windows.
- **UI:** Qt 6.5+ Quick/QML (`Qt6::Quick`, `Qt6::QuickControls2`), custom
  frameless window with manual title-bar/resize handling.
- **Storage:** SQLite via `Qt6::Sql` (project files are `.lfproj` = a
  SQLite DB + sidecar PNGs for masks/overlay images).
- **Image processing:** `QImage` (RGBA64 internal precision) + OpenCV
  (optional, `HAVE_OPENCV`) for noise reduction/sharpening/edge refinement.
- **RAW import:** LibRaw (optional, `HAVE_LIBRAW`).
- **Color management:** Little CMS 2 (optional, `HAVE_LCMS2`).
- **AI inference:** ONNX Runtime (optional, gated **OFF by default** behind
  `LUMEN_ENABLE_ONNX` — see "ONNX Runtime is special" below). Models:
  MobileSAM (combined encoder+decoder via `samexporter`), LaMa inpainting,
  Real-ESRGAN ×4 upscaling.
- **Package manager:** vcpkg (`onnxruntime`, `libraw`, `lcms2`, `sqlite3`,
  `opencv`).

Every optional dependency is wired the same way: `find_package(... QUIET)`
in `core/CMakeLists.txt`, gated by a `HAVE_<LIB>` compile definition, with
`#ifdef HAVE_<LIB>` guarding the actual usage and a graceful fallback (a
no-op or an unsupported-feature message) when absent. Follow this pattern
exactly if you add a new optional dependency — do not make a new dependency
a hard `REQUIRED`.

---

## Coding Standards

- **Comments are load-bearing documentation, not clutter.** This codebase
  uses long, dense comments to explain *why*, especially for anything that
  was previously buggy (see e.g. `RenderPipeline::buildCurveLut`,
  `DocumentModel::AutoHistoryStep`, `RenderPipeline::canvasToLayerLocalTransform`,
  `OnnxSession.hpp`'s entire header comment). These comments are the
  project's institutional memory of past bugs. **Never delete or shorten
  them** when touching nearby code — extend them if behavior changes.
- **Naming:** `m_` prefix for private members, `PascalCase` for
  types/classes, `camelCase` for functions/variables, `k`-prefixed
  constants for compile-time identity markers (`kBaseLayerSourceAssetId`).
  Qt signal/slot naming (`onXChanged`, `xChanged()`) follows Qt convention.
- **`[[nodiscard]]` is used consistently** on pure query methods across
  `DocumentModel`, `RenderPipeline`, `OnnxSession`. Keep using it on new
  getters/query functions.
- **Files are one class per header/cpp pair**, named after the class.
  `namespace lumen { ... }` wraps everything in `core/`; the `app/` layer
  (`DocumentController`) is deliberately **not** namespaced.
- QML files use 4-space indent, inline component definitions at the bottom
  of `Main.qml` (`component ZoomControl: Rectangle { ... }`,
  `component AdjustmentSlider: ColumnLayout { ... }`) rather than separate
  files, for tightly-coupled single-use UI pieces. Reusable, standalone
  overlays (`MaskCanvas.qml`, `CropOverlay.qml`, `LayerTransformOverlay.qml`)
  are separate files registered in `qt_add_qml_module`.

---

## Architecture Rules (must never be broken)

1. **`Layer::isBaseLayer()` (checks `sourceAssetId == kBaseLayerSourceAssetId`)
   is the only correct way to identify the base/background layer.**
   Never use `layer.order == 0` — a real bug was caused by exactly this
   (the base layer is reorderable via Move Up/Down, so `order` is not a
   stable identity). This was previously fixed in commit `436e84f`.
2. **Layer-scoped masks are stored in the owning layer's native pixel
   space, not canvas space.** `DocumentController::bakeMaskForTarget()` /
   `unbakeMaskFromTarget()` convert between them using
   `RenderPipeline::canvasToLayerLocalTransform()`. This is a deliberate
   design (masks stay attached to the layer's surface as it moves/rotates,
   instead of being re-sampled against the layer's current transform every
   render, which was the old, buggy design — "overlay masks don't follow
   the overlay"). Never re-introduce render-time mask re-sampling against a
   layer's live transform.
3. **All interactive drag/resize/rotate gesture state must live in a single,
   stable, top-level `MouseArea` that is never inside a `Repeater`
   delegate.** `LayerTransformOverlay.qml`'s header comment documents the
   exact failure mode: any signal that causes the QML model
   (`documentController.layerModel`) to be re-read produces a brand-new
   `QVariantList`/JS array, which makes a `Repeater` bound to it
   destroy-and-recreate every delegate — including any `MouseArea` living
   inside one — silently dropping an in-progress gesture. Presentational
   elements (borders, handle squares) may safely live inside `Repeater`
   delegates since they hold no state across frames.
4. **`begin/commitHistoryTransaction()` pairs must always close.** Any new
   gesture-based edit (drag, multi-step edit) must bracket itself with
   `beginXEdit()`/`commitXEdit()` on `DocumentController`, mirroring
   `beginAdjustmentEdit()`/`commitAdjustmentEdit()` and
   `beginLayerTransformEdit()`/`commitLayerTransformEdit()`. An unclosed
   transaction permanently swallows all future undo steps for the session
   (see `beginHistoryTransaction()`'s "nested begin — first begin wins"
   guard). QML overlays must have defensive `onVisibleChanged` cleanup that
   force-commits if a gesture is torn down mid-drag (see
   `LayerTransformOverlay.qml`).
5. **`structural=true` history transactions capture/restore `sourceImage`,
   `masks`, and `layerImages`** (crop, inpaint, upscale, delete-layer).
   Plain adjustment/transform edits stay `structural=false` (cheap — only
   `QVector<Adjustment>`/`QVector<Layer>`, both value types over
   implicitly-shared Qt containers). Don't make a cheap edit accidentally
   structural — it multiplies undo-stack memory cost for no reason.
6. **`RenderPipeline` is stateless and side-effect-free.** It never touches
   `DocumentModel` directly except as a read-only `const DocumentModel&`
   parameter in the two convenience overloads at the top of the class. All
   the real work (`renderWithLayers`, `applyAdjustments`,
   `compositeOverlayLayers`) takes plain data (`QImage`,
   `QVector<Adjustment>`, `QHash<QString,QImage>`) so it can run safely on
   a `QtConcurrent` worker thread — see rule under Threading below.
7. **`Adjustment::targetMaskId` empty string = global/full-image scope.**
   Every adjustment query/set path (`adjustmentsForTarget`,
   `setScalarAdjustmentForTarget`) is target-aware; the legacy
   no-target overloads are thin wrappers around `targetMaskId=""`. Do not
   add a new adjustment code path that bypasses target-awareness — it will
   silently apply per-mask edits globally (this exact regression was fixed
   once already in `RenderPipeline::renderFullResolution`, see its comment).
8. **`kBaseLayerSourceAssetId` / `"source"` is a fixed, well-known ID**
   used both as the base layer's `Layer::sourceAssetId` and as the DB row
   ID for the base image in `source_assets`. `ProjectStore::loadProject()`
   depends on this exact string to skip re-creating the base layer when
   restoring overlay layers. Don't change this value or its meaning.

---

## Performance Rules

- **`ctx.drawImage()` cost in a QML `Canvas` scales with the destination
  canvas's pixel area (`sourceWidth * zoom` × `sourceHeight * zoom`), not
  the source image's resolution.** This was the root cause of the entire
  `MaskCanvas.qml` performance rewrite (see git history / handoff notes).
  Any new QML `Canvas`-based overlay must separate **infrequently-updated
  content** (committed state, redrawn only on a data-changed signal) from
  **per-frame/per-mouse-move content** (live cursor/drag previews) into
  two separate `Canvas` items, exactly like `staticCanvas` /
  `liveCanvas` in `MaskCanvas.qml`.
- **Never replay a growing history array every frame.** The original
  `MaskCanvas.qml` accumulated `strokeHistory` and replayed the entire
  array on every `onPaint`, which is O(n) per frame → O(n²) over a whole
  stroke. The fix: paint each new dab **once**, directly onto the canvas's
  persistent backing store (`liveCanvas.paintDab()`), never replayed.
  Follow the same "paint once, persist" pattern for any new incremental
  drawing feature.
- **`requestPaint()` must be called explicitly after any direct mutation
  of a `Canvas`'s backing store** (`getContext("2d")` calls made outside
  `onPaint`, e.g. in `paintDab()`/`clearDabs()`/`drawRing()`). A missing
  call produces a *silent* visual failure — pixels are correct on the
  backing store but never composited to screen. This exact class of bug
  (missing `requestPaint()` in `paintDab()` and `clearDabs()`, plus a
  redundant one in `onExited`) was caught during Step-3 testing of
  `MaskCanvas.qml`. Audit every caller-vs-callee `requestPaint()` site
  when adding new canvas-mutation code paths.
- **`BrushEngine`'s paint surface is capped at 2000px on the longest side**
  (`brushEngineSize()` in `DocumentController.cpp`) regardless of source
  image resolution — running `QPainter` on a full 20MP+ `QImage` per
  mouse-move event causes multi-hundred-millisecond stalls. Strokes are
  painted in this capped space and **upsampled to source resolution**
  before being stored (`upsampleMaskToSource`). Never paint directly into
  a full-resolution mask buffer on the UI thread.
- **`RenderPipeline::buildCurveLut()` heap-allocates its 512KB
  `std::vector<double>` lookup tables** rather than using
  `std::array<double,65536>` on the stack. Four of the old stack arrays
  (2MB total) exceeded the default 1MB thread stack on `QtConcurrent`
  worker threads and caused a `0xc00000fd` stack-overflow crash. Never
  reintroduce large fixed-size stack arrays in code that can run on a
  `QtConcurrent` worker.
- **Layer-transform drags avoid full preview composites per tick.** While
  `m_layerTransformEditOpen` is true, `DocumentController` publishes layer
  state to QML at a throttled ~60 Hz (`m_transformUiTimer`, 16ms) instead
  of on every native mouse event, and skips starting the async
  preview/HQ-preview pipeline entirely until the gesture commits. Preserve
  this pattern for any future high-frequency continuous-gesture feature.
- **PNG vs JPEG quality scales are inverted in Qt.** JPEG quality 100 = max
  quality/least compression; PNG quality 100 = *no* compression (huge
  files), PNG quality 0 = max compression. `ExportService::qualityFor()`
  encodes the correct per-format defaults (JPEG 92, WebP 85, PNG -1 = Qt's
  default zlib level 6). Never pass a single `quality` value straight
  through to `QImage::save()` across multiple formats.

---

## Rendering Rules

- **Internal pipeline precision is `QImage::Format_RGBA64`** (16-bit/
  channel) throughout `RenderPipeline`. Convert to 8-bit
  (`Format_ARGB32`/`Format_RGB888`) exactly once, immediately before
  writing to disk (`ExportService::toExportFormat`) or before handing to
  `QPainter` premultiplied-alpha compositing
  (`compositeOverlayLayers` uses `Format_ARGB32_Premultiplied`
  temporarily, then converts back to RGBA64).
- **Adjustment order is fixed and intentional** inside
  `RenderPipeline::applyAdjustments()`: rotation/flip → exposure/brightness
  gain → whites/blacks offset → contrast → highlights → shadows →
  saturation → vibrance → white balance (temp/tint) → tone curves (RGB
  then luma) → mask blend → OpenCV noise-reduction/sharpening. Do not
  reorder these without understanding that later stages read luma
  computed from earlier stages' output (`0.2126R + 0.7152G + 0.0722B`
  Rec.709 luma, recomputed multiple times through the pipeline).
- **Global adjustments are `targetMaskId == ""`; per-mask adjustments are
  applied via `MaskAdjLayer`.** Base-image-scoped masks apply inside
  `renderWithLayers()`'s main loop; layer-scoped masks (`targetLayerId`
  non-empty) apply *inside* `compositeOverlayLayers()`, directly to that
  layer's own pixels, before compositing. Never apply a layer-scoped
  mask's adjustments to the base composite.
- **`renderFullResolution()` passes an invalid `QSize()` (not
  `QSize(0,0)`)** to `renderWithLayers()` so its "only downscale if
  `maximumSize.isValid()`" check is skipped, giving a true native-res
  render through the identical pipeline the on-screen preview uses. Don't
  special-case the export path — it should always share code with
  preview rendering, not duplicate it (this itself was a past bug —
  `compositeOverlayLayers()` used to never run on export).

---

## Qt/QML Rules

- **QML `Canvas` performance discipline** — see Performance Rules above;
  this is the single most consequential rendering lesson in the repo.
- **`documentController.layerModel` (and similar list-Q_PROPERTYs) return
  a brand-new `QVariantList` on every read.** Any QML code that derives a
  JS array/object from it (e.g. `overlayModel`, `selectedLayerData` in
  `LayerTransformOverlay.qml`) must not assume identity stability across
  reads, and no persistent interactive state may be attached to
  `Repeater` delegates bound to it.
- **Signal-forwarding in `DocumentController`'s constructor is intentional
  and layered.** `DocumentModel::changed()` is *not* forwarded 1:1 to
  every Qt signal — see the lambda in the constructor: it special-cases
  `m_layerTransformEditOpen` (throttle instead of full rebuild), skips
  preview rebuilds while brush/eraser tools are active (tool == 1 or 2),
  and only starts `m_previewDebounce` for "settled" states. When adding a
  new mutation path, decide deliberately which of `documentChanged`,
  `adjustmentsChanged`, `layersChanged`, `maskChanged` actually need to
  fire — don't reflexively emit all of them.
- **Timers own debouncing, not ad-hoc `QTimer::singleShot`.** The class
  already owns `m_previewDebounce` (100ms), `m_hqTimer` (1500ms),
  `m_maskSaveTimer` (50ms), `m_edgeRefineTimer` (350ms),
  `m_transformUiTimer` (16ms), each with a single clear owner and
  cancellation flag (`m_cancelFlag`/`m_hqCancelFlag`, both
  `std::shared_ptr<std::atomic<bool>>`). Reuse this pattern; don't add a
  parallel ad-hoc timer for a new debounced operation.
- **Frameless window (`Qt.FramelessWindowHint`) reimplements the title bar
  and resize handles manually** in `Main.qml` using
  `startSystemMove()`/`startSystemResize()` — the only Qt-recommended,
  cross-platform-safe mechanism for this. Any new top-level window chrome
  must use the same APIs, not platform-specific Win32 calls.
- **QML `Slider`/drag components bracket their own transactions**
  (`onPressedChanged` → `begin/commitAdjustmentEdit()`). New sliders/drag
  controls added to the Adjustments panel must follow the identical
  press/release-to-transaction wiring, not push one undo entry per tick.

---

## C++ Rules (memory, threading, ownership)

- **ONNX Runtime is special — read `OnnxSession.hpp`'s full header comment
  before touching anything ONNX-related.** `<onnxruntime_cxx_api.h>` has a
  global template static (`Global<T>::api_`) that calls `OrtGetApiBase()`
  during C++ static initialization, **before `main()` runs** — if any
  transitive DLL dependency is missing, the process dies with
  `0xC06D007E` silently, before any log line or message box. The fix
  (already implemented, do not revert):
  - `OnnxSession.cpp` includes **only** `<onnxruntime_c_api.h>` (the pure
    C function-pointer API), never the C++ header, in any translation
    unit that links into the executable.
  - `OrtGetApiBase()` is called lazily inside `ensureLoaded()`, on first
    real inference call, inside the Qt event loop, wrapped in try/catch.
  - The whole subsystem is compiled out by default:
    `LUMEN_ENABLE_ONNX` defaults `OFF` in `core/CMakeLists.txt`; without
    it, `HAVE_ONNXRUNTIME` is undefined and `<onnxruntime_c_api.h>` is
    never included at all.
  - `app/CMakeLists.txt` only applies `/DELAYLOAD:onnxruntime.dll` when
    `LUMEN_ENABLE_ONNX` is on, and only then copies the ONNX Runtime DLL
    set alongside the exe (Qt's own deploy step does not do this).
  - `InpaintEngine`/`UpscaleEngine` are constructed **lazily**
    (`std::unique_ptr`, first use) in `DocumentController`, not as direct
    members — direct-member construction previously ran `Ort::Env` init
    at `main()` startup, i.e. exactly the crash this whole design avoids.
- **Threading model:** `QtConcurrent::run()` + `QFutureWatcher` for
  preview rendering, HQ preview, AI inference, and edge refinement, each
  with its own cancellation flag and a monotonically increasing request ID
  (`m_previewRequestId`) checked before applying a stale result. New
  long-running work must follow this pattern: cancellable
  `std::shared_ptr<std::atomic<bool>>`, request-ID staleness check on
  completion, never block the UI thread. `m_refinePool` is a dedicated
  single-thread, low-priority `QThreadPool` for edge refinement so it
  never competes with preview rendering for CPU.
- **`Adjustment`/`Layer`/`Mask` are plain value structs** (`shared-types/`)
  with defaulted `operator==` (`= default`) used by
  `DocumentModel::transactionChangedAnything()` to detect no-op edits.
  Adding a new field to any of these structs requires it to participate
  meaningfully in equality — don't add a field that silently breaks no-op
  detection (e.g. a field that always differs would make every edit look
  "changed").
- **`QImage`/`QVector`/`QHash` are implicitly shared (copy-on-write).**
  `HistorySnapshot` deliberately relies on this — capturing a snapshot is
  cheap even for structural transactions because nothing is deep-copied
  until it actually diverges. Don't defeat this by forcing a `.detach()`
  or manual deep copy in a hot path.
- **File encoding is mixed and deliberate — do not "fix" it.**
  `core.autocrlf=true`; `DocumentController.hpp/.cpp` and
  `DocumentModel.hpp/.cpp` are CRLF on disk; `Layer.hpp` is plain LF with
  **no trailing newline**. Patches must be generated with plain LF and
  zero manually-embedded `\r`, and applied with `git apply --index`
  (never `git am`), verified via `--check → --index → commit → byte-diff`
  against a fresh clone. Do not run a repo-wide line-ending normalization.

---

## Bug Fix Workflow

Before changing any code:

1. **Get the actual current file content.** Every confirmed bug in this
   project's history was root-caused by obtaining and diffing real file
   content — never by reasoning about what a file "should" contain. If
   GitHub is the authoritative source and may have moved on, ask whether
   it's current before trusting anything from memory or an older chat.
2. **Identify the root cause, not the symptom.** State it as a specific,
   falsifiable claim ("`ctx.drawImage()` cost scales with canvas pixel
   area", not "canvas painting is slow").
3. **Name every affected file explicitly** before writing a patch. This
   codebase's layering means a fix in `RenderPipeline` almost always has a
   corresponding caller-side implication in `DocumentController` and
   sometimes `ProjectStore` (persistence) — check all three before
   declaring a fix complete.
4. **Propose the smallest safe fix.** Do not restructure surrounding code,
   rename things, or "clean up while you're in there" as part of a bug
   fix. Scope creep has been explicitly called out and corrected before in
   this project's process.
5. **Deliver as a patch, not a rewritten file**, unless the change is
   trivially small and self-contained. Patches must be `git apply
   --index`-compatible (see C++ Rules → file encoding above).

---

## Refactoring Rules

**Acceptable:**
- Extracting a repeated calculation into a named local/helper *within the
  same function/class*, when doing so doesn't change behavior.
- Adding a missing `[[nodiscard]]`, a missing doc comment, or a defensive
  null-check that matches existing patterns elsewhere in the same file.
- Consolidating duplicate logic that was clearly copy-pasted by mistake
  (rare in this codebase — most apparent duplication is deliberate, e.g.
  `RenderPipeline.cpp`'s two nearly-identical `applyAdjustments`
  implementations are NOT the same file — verify before assuming
  duplication).

**Not acceptable without an explicit, separate request:**
- Reordering the adjustment pipeline stages (see Rendering Rules).
- Changing `Layer`/`Mask`/`Adjustment` field layout or semantics.
- Touching the CRLF/LF mix of existing files.
- Renaming anything QML binds to by string (`Q_PROPERTY`, `Q_INVOKABLE`
  names) — these are cross-language contracts with no compiler check.
- Merging `staticCanvas`/`liveCanvas` back into one `Canvas`, or otherwise
  undoing the documented performance-driven splits in `MaskCanvas.qml`.
- "Simplifying" the history-transaction system (`AutoHistoryStep`,
  `beginHistoryTransaction`/`commitHistoryTransaction`) — it already
  handles nested transactions, labels, boundaries, and no-op detection
  correctly; a naive rewrite will reintroduce the "one undo step per
  slider tick" bug this system was built to fix.

---

## Debugging Rules

- **Reproduce and isolate before touching source.** If OpenCV/ONNX/LibRaw
  are involved, check whether the relevant `HAVE_*` macro is even defined
  in this build configuration first — many "bugs" in this codebase are
  actually a feature compiled out (`LUMEN_ENABLE_ONNX=OFF` is the default).
- **For rendering/visual bugs:** determine whether the code path runs on
  the UI thread or a `QtConcurrent` worker before proposing a fix — stack
  size (1MB default) and Qt object thread-affinity rules apply.
- **For QML interaction bugs (drag lag, lost grab, state reset):** check
  first whether interactive state lives inside a `Repeater` delegate
  bound to a Q_PROPERTY list. This exact bug class has occurred before
  (see Architecture Rule 3) and will recur if reintroduced elsewhere.
- **For "changes don't survive save/load":** check `ProjectStore.cpp`'s
  save and load paths *together* — this project has repeatedly had bugs
  where one side of persistence was updated and the other wasn't (layer
  transforms, mask target-layer-id migration, blend mode not persisted at
  all — see Project-Specific Knowledge below).
- **For "undo/redo doesn't restore X":** check whether the mutating
  function opens its transaction with the correct `structural` flag and
  whether `HistorySnapshot`/`captureSnapshot`/`applySnapshot` actually
  include the field in question.

---

## Testing Checklist (after every change)

- [ ] Does it build with `LUMEN_ENABLE_ONNX=OFF` (the default)? This must
      always work — most contributors will build this way.
- [ ] If ONNX-related: does it also build with `LUMEN_ENABLE_ONNX=ON` and
      still gate all `onnxruntime_cxx_api.h`/C++ API usage out of any file
      that gets included when the flag is off?
- [ ] Does undo/redo correctly restore the changed state, including after
      the affected action, and does History panel labeling stay sensible?
- [ ] If touching `RenderPipeline`: does on-screen preview
      (`rebuildPreview`/`buildHqPreview`) still match
      `renderFullResolution()`'s export output for the same document state?
- [ ] If touching masks: verify base-image-scoped **and** layer-scoped
      masks both still behave correctly (paint, move the owning layer,
      repaint — mask should follow the layer).
- [ ] If touching `MaskCanvas.qml`: verify brush/eraser feel responsive on
      a large (>2000px) source image, and that a long stroke doesn't
      degrade in speed partway through.
- [ ] Save a project, close, reopen — confirm the specific field/feature
      you touched round-trips through `ProjectStore`.
- [ ] `git diff --check` clean (no stray whitespace/CRLF issues) before
      proposing a patch.

---

## Commit Guidelines

- One logical change per commit/patch — matches this repo's existing
  history style (e.g. "Fix base layer identity and reorder handling" as
  one atomic commit).
- A commit that fixes a bug should include, in its message or accompanying
  comment, the root cause — this project consistently documents *why* a
  bug happened in-line (see almost every `.cpp` file), not just what
  changed. Continue that practice.
- Don't bundle an unrelated cleanup into a bug-fix commit, even a small one.

---

## Forbidden Changes

- Do not rewrite a whole file when a targeted patch will do.
- Do not "improve" formatting/whitespace in code you weren't asked to
  touch — this repo's dense, single-line-per-statement style in files
  like `RenderPipeline.cpp`/`DocumentController.cpp` is intentional and
  consistent within each file; don't reformat it to a different style.
- Never delete or shorten a comment that explains a past bug, a
  non-obvious invariant, or a performance decision — these comments are
  the project's only defense against regression.
- Never reintroduce `layer.order == 0` as a base-layer check.
- Never remove the `LUMEN_ENABLE_ONNX` gate or make ONNX Runtime a hard
  build requirement.
- Never make `RenderPipeline` stateful or give it a mutable reference to
  `DocumentModel`.
- Never move gesture-interaction `MouseArea` state into a `Repeater`
  delegate.
- Never silently normalize file line endings across the repo.
- Do not expand scope beyond what was requested — no drive-by renames,
  no "while I'm here" restructuring, no removing "unused" code without
  confirming it's actually dead (see `MaskDocument` note below — it looks
  unused but confirm before deleting anything).

---

## Project-Specific Knowledge

**Known, confirmed technical debt (do not "fix" silently — flag it and ask
before touching, since some of it may be intentionally deferred):**
- `core/mask-core/MaskDocument` (`MaskDocument.hpp/.cpp`) appears to be
  dead code — `DocumentModel` manages masks directly via
  `QVector<Mask> m_masks`, not through `MaskDocument`. Confirm zero
  call-sites before removing.
- `Adjustment::targetLayerId` is defined but never assigned/read anywhere
  in the current codebase (per prior investigation) — likely vestigial
  from an earlier design where adjustments, not masks, carried layer
  scoping. `Adjustment::targetMaskId` is the field actually used.
- Loading a project re-applies adjustments via
  `setScalarAdjustmentForTarget()`, which goes through the normal
  history-transaction path — this pollutes the undo stack with the
  "loading" as if it were user edits. `restoreLayer()`/`restoreMask()`
  correctly bypass history for their own state; adjustment restoration on
  load does not yet have an equivalent bypass.
- `Layer::blendMode` is not persisted by `ProjectStore` (schema has no
  column for it) — it resets to `Normal` on reload.
- Opacity, visibility, and blend-mode changes currently have no undo
  support (`setLayerOpacity`/`setLayerVisible`/`setLayerBlendMode` in
  `DocumentModel` don't wrap themselves in `AutoHistoryStep`).

**Confirmed architectural decisions (do not second-guess without strong
justification):**
- Masks are always painted at **base-image pixel resolution**
  (`MaskCanvas.qml` doesn't change based on layer selection); layer-scoped
  masks are baked into the target layer's native space only at
  paint-commit time via `bakeMaskForTarget()`. This is why
  `RenderPipeline::canvasToLayerLocalTransform()` exists and is public.
- The brush engine operates in a resolution-capped, separate coordinate
  space from the document; `commitMaskPaint()`'s comment documents the
  scale conversion carefully — read it before touching stroke coordinate
  math.
- `AiRuntime`/`InpaintEngine`/`UpscaleEngine`/`MaskPredictor` are three
  independent subsystems, each owning its own `OnnxSession`, not sharing
  one `Ort::Env`. If consolidating these ever becomes a goal, the
  lazy-init/pre-main-crash constraints in `OnnxSession.hpp` still apply to
  whatever replaces them.
- Real-ESRGAN model diagnostic (per `CURRENT_STATE.md`, 2026-07-22): the
  bundled `models/realesrgan-x4plus.onnx` is **not** the cause of prior
  inference failures — verified IR version 6 / opset 11, loads cleanly
  under the actual `OnnxSession` implementation once required DLLs
  (`onnxruntime_providers_shared.dll`, `abseil_dll.dll`,
  `libprotobuf(-lite).dll`, `re2.dll`) are deployed alongside the exe.
  `app/CMakeLists.txt`'s post-build copy step handles this when
  `LUMEN_ENABLE_ONNX=ON`. Do not re-diagnose this as a model-export
  problem without new evidence.
- MobileSAM integration requires an **encoder/decoder split** with
  embedding caching (current `MaskPredictor` uses a `samexporter`-combined
  single-graph model instead, per its own header comment, as an interim
  design) — a patch toward the split architecture was generated
  previously but not yet applied; verify current state before assuming
  either design is in place.

**Edge cases seen in production already, don't reintroduce:**
- A mask created but never painted has a null `QImage` — `ProjectStore`
  and `DocumentModel::maskImage()` both handle this as a valid, expected
  state, not an error.
- `RenderPipeline::applyAdjustments()`'s mask-blend step uses
  `qAlpha(maskPixel)/255.0` as a linear blend factor between the original
  and adjusted pixel — this is the mechanism that makes masks act as
  "local adjustment strength," not a hard cutout.
- A slider pressed and released without moving must **not** push an undo
  step — `transactionChangedAnything()` exists specifically to detect and
  suppress this no-op case; the same discipline must extend to any new
  gesture-based edit.

---

*This document reflects the repository state as of the most recent commits
referenced in `CURRENT_STATE.md` (2026-07-22) plus subsequent confirmed
GitHub updates. If a future agent finds code here doesn't match what's on
disk, trust the disk/GitHub content and flag the discrepancy rather than
silently reconciling it.*
