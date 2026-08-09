# Changelog

All notable changes to Standard of Iron are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
the project uses [semantic versioning](https://semver.org/spec/v2.0.0.html).
While the major version is 0, the save format and the mission and map schemas
may change in any release — see [Save compatibility](#save-compatibility).

## [Unreleased]

## [0.1.0] — 2026-08-09

The first tagged release, and the first build published as a download rather
than as something you compile yourself.

### Campaign and content

- The **Second Punic War** campaign: nine missions from the crossing of the
  Rhône through the Alps to Zama, with Trebia, Trasimene, Ticino and Cannae in
  between.
- Thirteen maps, covering the campaign battles plus standalone skirmish terrain
  (forest, mountain, rivers, the Spanish grove, the Iron Sepulcher watch).
- Rome and Carthage as playable nations, each with its own units, ornaments and
  commander roster.

### Gameplay

- Real-time battles with formations, including testudo and shield-wall
  defensive layouts, and a formation planning interface.
- A first-person commander mode alongside the top-down army view, each with its
  own control scheme.
- Economy: gathering timber, stone and iron, hauling to barracks stockpiles, and
  a builder Auto Gather standing order.
- Siege play against walls, gates and towers.
- Ambient settlement life and wildlife — wolves, sheep, horses and elephants
  with their own gaits and behaviour.

### Presentation

- A custom OpenGL 3.3 renderer with multi-pass drawing, batching and culling,
  plus an optional GPU-driven crowd path on drivers that support compute
  shaders.
- A CPU rasteriser fallback (`--force-software`) for machines without a 3.3
  driver. It is a diagnostic, not a supported way to play.
- The "Iron and Ember" interface: activity medallions, Roman numerals, and a
  selection summary that regroups itself as a selection grows.

### Accessibility

- Every gameplay command is rebindable to a key or mouse button, with conflict
  detection scoped per control context.
- Team palettes and ring patterns for colour identification, and a camera motion
  scale.
- Audible feedback for controls that refuse an action.

### Languages

- English, German, Spanish, Brazilian Portuguese, and Arabic. Arabic switches the
  whole interface to a right-to-left layout.

### Platforms

- Linux (AppImage), macOS (DMG) and Windows (ZIP), each built and smoke-tested
  through a real OpenGL self-test before publication.
- **macOS builds are universal**: Intel and Apple Silicon both run natively. The
  architectures are read from the installed Qt rather than assumed, and the build
  fails if the binary does not come out carrying them.
- The renderer now asserts at startup that the driver actually granted an OpenGL
  3.3 Core context, rather than assuming the one it asked for. All three release
  workflows fail if it did not.
- Linux and Windows now prefer OpenGL 4.5 Core, exposing the existing 4.3 GPU
  crowd path and 4.4 persistent-buffer path when available; macOS explicitly
  requests Apple's 4.1 ceiling and keeps the complete 3.3 fallback.
- The macOS and Windows toolchains are now exercised from Linux, where the
  project is actually developed: every translation unit is reparsed with Clang
  and libc++, every shader is compiled by a spec-literal GLSL front end, and
  the sources are scanned for what MSVC and NTFS reject. `make portability`
  runs it locally; CI runs it on every pull request and every build. The
  entries under **Fixed** below are what it found on its first run.

### Fixed

- The Windows package no longer copies Qt 6.8's Mesa 11.2 software renderer,
  which exposes only OpenGL 3.0 on the hosted runner. It bundles a pinned,
  checksum-verified Mesa 26.1.6 llvmpipe runtime and verifies a real 4.5 Core
  gameplay frame before publishing.

- **Optimised builds had deleted every NaN and infinity check in the game.**
  `-ffast-math` (and MSVC's `/fp:fast`) licenses the compiler to assume no
  operand is ever NaN or infinite, so `std::isfinite` folds to `true` and the
  clamp behind it disappears — verified on the project's own release flags.
  Around forty guards were affected, on volumes read back from the settings
  file, camera angles and impact geometry, and so was every use of
  `infinity()` as a sentinel in pathfinding, AI target selection and bounding
  boxes. Debug builds were unaffected, which is why it never showed in
  development, and the three shipped platforms did not agree on the outcome.
  Linux and macOS now build with `-fno-finite-math-only` and Windows with
  `/fp:precise`.
- Two places computed a value from a variable and incremented it in the same
  function call, where the order is unspecified and compilers differ: the
  Roman market stall's produce, and the balance simulator's spawn jitter — the
  latter meaning a seeded simulation did not have to produce the same
  battle on Windows as on Linux.
- `stone_instanced.frag` declared a function named `noise3`, which is a GLSL
  built-in with a different return type. Mesa does not declare the built-in so
  it compiled here; a spec-literal front end rejects the shader, and a shader
  that fails to compile is a silently missing object rather than a crash.
- Eight files used `M_PI`, which MSVC's `<cmath>` does not define. They
  compiled on Windows only because Qt's `qmath.h` defines it as a fallback and
  happened to be included first; they now use `std::numbers`.
- **A crash on drivers that advertise compute shaders on a context below 4.3.**
  The GPU crowd-culling path is guarded by a capability probe that accepted an
  extension string, but its shaders are `#version 430` and need OpenGL 4.3 to
  compile at all; on such a driver the game segfaulted inside the GL driver
  partway through the first frame. The probe now gates on the context version,
  which is both necessary and sufficient. Verified against simulated 4.5, 4.1
  (Apple's cap), 3.3 and 3.1 drivers.
- macOS bundles are re-sealed after deployment and asset copying. Apple Silicon
  refuses to run a binary whose signature does not match its contents, and every
  step after `macdeployqt` had been invalidating it.

### Known limitations

- The AI is capable but unfinished: it lacks siege groups, flankers, and
  regroup-after-failed-push logic. `docs/AI_ARCHITECTURE.md` is candid about
  where the gaps are.

### Audio provenance

- Thirty-two battle and alert effects that had been generated with AudioCraft
  were **rebuilt from CC0 recordings** and are no longer restricted. The recipe
  is `tools/audio_field/battle.py`. Rebuilding them also fixed two defects: 28
  of the 32 decoded above 0 dBFS and clipped, and every alert was padded to
  exactly 10 seconds because that is AudioCraft's generation length.
- Every effect now carries a `source` tag in the audio manifest — `synth` for
  the 106 generated by this repository's own code, `field` for the 69 cut or
  composed from CC0 recordings.
- The thirty-one voice lines were recorded by the project author.

### Distribution restriction

The twenty music tracks were generated locally with Meta's AudioCraft, whose
model weights are licensed CC BY-NC 4.0 — **non-commercial use only**. Standard
of Iron is MIT-licensed and distributed free of charge, which is consistent with
that. Selling the game, or bundling it into anything sold, would require
replacing those tracks first — they are now the only part of the game carrying
any such restriction. `THIRD_PARTY_LICENSES.md` records the details.

## Save compatibility

Saves are keyed to a schema version. When the game finds a save database written
by a different version, it **discards it and starts a clean one** — there is no
migration path, and campaign progress goes with it.

While the project is at 0.x this is the intended behaviour, and any release may
trigger it. Before 1.0 the policy needs to become one of: migrate forward, or
warn the player before the wipe rather than only writing to the log.

[unreleased]: https://github.com/djeada/Standard-of-Iron/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/djeada/Standard-of-Iron/releases/tag/v0.1.0
