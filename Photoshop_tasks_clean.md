## TASK 36 — CREATE `models/.gitkeep`

```
```

(Empty file — ensures the `models/` directory is tracked by git even though `*.onnx` files are ignored.)

---

That is the complete task list — **35 files** to create or replace covering all six milestones. Execute them top to bottom. A few notes before you hand this to your agent:

**Build order for external dependencies** — before the first cmake configure after Task 27, run `vcpkg install onnxruntime libraw lcms2 opencv4` and pass `-DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake`. Each library is optional at compile time (guarded by `HAVE_*` defines), so the app builds and runs without them — AI features just won't function until the libs and model weights are present.

**Model weights** — after Task 36, manually download `mobile_sam.onnx`, `big-lama.onnx`, and `realesrgan-x4plus.onnx` into `models/`. Nothing in the build system fetches them.

**`app/resources/icon.ico`** — Task 34 references it for the installer. Create a placeholder `.ico` file or skip the CPack step until you have one; the app itself builds without it.

