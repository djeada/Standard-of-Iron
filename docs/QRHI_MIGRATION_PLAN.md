# QRhi Migration — Sub-Issue Breakdown

Source: [#363 QRhi Migration and Windows Graphics Backend Modernization](https://github.com/djeada/Standard-of-Iron/issues/363).

This is a working draft, not filed on GitHub yet. Copy each section below into its own
issue on `djeada/Standard-of-Iron`, in the order given — later issues depend on earlier
ones. Ordering was corrected from the original issue based on a codebase read on
2026-07-24 (see "Current state" note at the bottom).

Suggested labels: `rendering`, `windows`, `macos`, `linux`, `ci`, `epic:qrhi-migration`.
Link every sub-issue back to #363 and to its direct predecessor.

---

## 0. RFC: QRhi abstraction design + pipeline migration order

**Depends on:** nothing. **Blocks:** everything else.

Nothing QRhi-related exists in the codebase yet — no `rhi_backend.h/cpp`, no partial
work. Before writing code, decide and document:

- Where the seam sits: `render/gl/backend.cpp` currently owns ~25 pipeline classes
  (cylinder, terrain, vegetation, character, water, effects, banner, mesh-instancing,
  rigged-character, combat-dust, rain, healer-aura, healing-beam, mode-indicator,
  primitive-batch, scatter, dead-tree...) that call `glDrawElements` /
  `glMapBufferRange` / etc. directly via `QOpenGLFunctions_3_3_Core` /
  `QOpenGLExtraFunctions`. QRhi has no raw-GL escape hatch — it works through
  `QRhiBuffer` / `QRhiTexture` / `QRhiGraphicsPipeline` / `QRhiResourceUpdateBatch`
  and an explicit render-graph. Every pipeline needs an actual rewrite, not a wrapper.
- Migration order for the ~25 pipelines (simplest/most isolated first — e.g.
  `cylinder_pipeline` or `primitive_batch_pipeline` — proven pattern extended outward;
  `character_pipeline` / `terrain_pipeline` last, since they're the most complex and
  everything else can be validated before touching them).
- How `QQuickFramebufferObject` (`ui/gl_view.cpp`) integration changes — QRhi wants
  either `QQuickRhiItem`/`QSGRenderNode` or a manually-managed offscreen `QRhi`
  instance; FBO-based injection may not carry over cleanly.
- What happens to `render/gl/persistent_buffer.h` and `platform_gl.h` — QRhi buffers
  have their own upload/mapping model; the current persistent-mapping-with-fallback
  logic is GL-specific and won't port as-is.

**Deliverable:** a design doc (e.g. `docs/RHI_BACKEND_DESIGN.md`) covering the above,
reviewed and agreed before any pipeline rewrite starts.

---

## 1. QRhi backend abstraction layer (skeleton only)

**Depends on:** #0. **Blocks:** #2, #4, #6.

- Introduce `render/rhi/rhi_backend.h/.cpp`: a `QRhi`-owning backend that can
  initialize against whatever `QRhi::create()` picks for the platform, log the
  selected backend (D3D11/Metal/Vulkan/GL), and execute an *empty* frame (clear +
  present) end to end.
- Migrate exactly one pipeline (per the order chosen in #0) as the proof of pattern —
  not all 25.
- Keep the existing `render/gl/*` path fully intact and selectable; this is additive,
  not a replacement yet.
- Deliverables: `rhi_backend.h`, `rhi_backend.cpp`, one migrated pipeline, integration
  test that runs both backends and diffs a rendered frame.

**Acceptance:** app can launch with `QSG_RHI_BACKEND` unset (platform default) and
render the one migrated pipeline through QRhi while everything else still renders
through the existing GL path.

---

## 2. D3D11 backend for Windows — full pipeline migration

**Depends on:** #1.

- Migrate the remaining ~24 pipelines to the QRhi backend, targeting D3D11 as the
  concrete backend on Windows.
- Update `CMakeLists.txt`: currently only `find_package(OpenGL REQUIRED)` exists with
  no per-backend linking (checked 2026-07-24) — add D3D11-specific Windows linkage.
- Validate on NVIDIA, AMD, and Intel GPUs.
- Keep OpenGL path as an explicit fallback/override.

**Acceptance:** app compiles and runs on Windows using D3D11 by default; OpenGL
remains available via override flag.

---

## 3. Windows CI: build + runtime validation on D3D11

**Depends on:** #2.

- `.github/workflows/windows.yml` currently only triggers on version tags (release
  builds), not on every PR, and its `--renderer-self-test` step forces
  `QT_OPENGL=software` (checked 2026-07-24) — it validates the software-GL fallback,
  not a GPU-backed path at all.
- Add a PR-triggered Windows job (or extend the existing one) that runs the same
  self-test against the real D3D11 backend on GitHub's Windows runners, and logs which
  QRhi backend was actually selected.
- Run on both `windows-2022` (Windows 11) and `windows-2019` (Windows 10) runner
  images.
- Upload runtime logs as artifacts.

---

## 4. Metal backend for macOS — full pipeline migration

**Depends on:** #1. Independent of #2/#3 — can run in parallel once #1 lands.

- Migrate all pipelines to run through QRhi's Metal backend on macOS.
- `.github/workflows/macos.yml` currently forces `CMAKE_OSX_ARCHITECTURES=x86_64` and
  runs Apple Silicon runners under Rosetta 2 (checked 2026-07-24) — decide whether
  Metal migration is also the point to add a native `arm64` build, since Metal is the
  only forward-compatible path once OpenGL is fully deprecated (capped at 4.1 on
  macOS already).
- Validate rendering parity against the existing OpenGL output on both Intel and
  Apple Silicon.

**Acceptance:** app runs natively on macOS via Metal with parity to the current
OpenGL renderer.

---

## 5. macOS CI: Metal validation

**Depends on:** #4.

- Extend `.github/workflows/macos.yml` (or add a PR-triggered job — current one is
  tag-only) to run the renderer self-test against Metal specifically, with Metal
  device detection and basic perf logging.
- Run on both `arm64` and `x86_64` if #4 added native arm64 support.

---

## 6. Vulkan backend for Linux/Windows

**Depends on:** #1 (and ideally #2/#4 already validated the pipeline-by-pipeline
migration pattern against two other backends first, so treat this as lower priority
than it might look).

- Add Vulkan as a third concrete QRhi backend.
- Runtime detection/fallback when Vulkan isn't available.
- Extend Linux CI (`.github/workflows/linux.yml`) with Vulkan validation.

**Acceptance:** Vulkan backend renders correctly and passes the same visual
validation used for D3D11/Metal.

---

## 7. Performance profiling: persistent-mapping vs. fallback buffer modes

**Depends on:** nothing — this targets the *existing* OpenGL path
(`render/gl/persistent_buffer.h`, `render/gl/platform_gl.h`) and can be done any time,
independent of the QRhi work. Useful as a baseline to compare QRhi backends against
later.

- Benchmark `PersistentRingBuffer` in `Platform::BufferStorageHelper::Mode::Persistent`
  vs. `::Fallback` (the fallback path already exists and is exercised when
  `GL_ARB_buffer_storage`/GL 4.4 isn't available — see `platform_gl.h`).
- Target: <5% overhead in fallback mode.
- Auto-log metrics in debug builds.
- Document results in `docs/performance_report.md`.

---

## 8. Windows OpenGL fallback validation (current path, not QRhi)

**Depends on:** nothing — validates existing behavior as a regression baseline.

- Test on Windows 10/11 across NVIDIA/AMD/Intel.
- Confirm the fallback in `platform_gl.h::BufferStorageHelper` activates correctly
  when persistent mapping is unavailable, and that `gl_capabilities.h` logs GL
  capabilities/fallback status accurately.
- Acceptance: fallback path stable, 60 FPS under expected load.

---

## Also worth filing separately (found during investigation, not in original #363)

**Quick, low-risk, independent of the whole epic:** `main.cpp` (lines 437, 442)
currently forces `QSG_RHI_BACKEND=opengl` and
`QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi)` unconditionally on
every platform. Qt Quick's own scenegraph (the QML UI layer, separate from the
`QQuickFramebufferObject`-injected game rendering) is already QRhi-based in Qt6 and
would pick D3D11 on Windows / Metal on macOS by default if not overridden. This only
affects the QML UI surface, not game rendering — worth a standalone small issue/PR
to remove the override (behind a flag, since it may have been pinned deliberately for
a driver-compatibility reason not documented in the code — check history/blame before
changing).

---

## Current state note (as read 2026-07-24)

- No `rhi_*` files anywhere in the repo.
- All game rendering is raw OpenGL 3.3 Core across ~25 pipeline classes in
  `render/gl/backend/`, injected into Qt Quick via `QQuickFramebufferObject`
  (`ui/gl_view.cpp`).
- `CMakeLists.txt` only does `find_package(OpenGL REQUIRED)` — no per-platform
  backend linkage exists yet.
- Windows/macOS/Linux CI exist but are tag-triggered (release builds only) and
  validate the existing OpenGL/software-GL path, not any RHI backend.
