#pragma once

// Stage 8a — Rig DSL interpreter.
//
// Walks a RigDef and emits DrawPartCmds via ISubmitter::part().
//
// The interpreter is intentionally thin: all domain knowledge (bone frames,
// species-specific anchors, palette lookup) lives behind AnchorResolver and
// PaletteResolver abstractions. That keeps the interpreter a ~100-line
// file regardless of how many rigs we add.

#include "rig_def.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

namespace Render::GL {
class ISubmitter;
struct Material;
} // namespace Render::GL

namespace Render::RigDSL {

// Resolves a named anchor to a model-space point. Implementations can delegate
// to bone frames, to a static table, or to anything else.
class AnchorResolver {
public:
  virtual ~AnchorResolver() = default;
  // Must return a point in the same space the caller expects (typically
  // unit-local, i.e. the space in which `ctx.model` is defined).
  [[nodiscard]] virtual auto resolve(AnchorId id) const -> QVector3D = 0;
};

// Resolves a palette slot to an RGB colour in linear space. Faction tinting,
// variant jitter, etc. live here.
class PaletteResolver {
public:
  virtual ~PaletteResolver() = default;
  [[nodiscard]] virtual auto resolve(PaletteSlot slot) const -> QVector3D = 0;
};

// Resolves named scalar channels (e.g. "helmet radius", "limb thickness")
// to floats. Per-frame and per-unit, without needing a new rig definition.
// A null ScalarResolver means all scale_ids are ignored (size fields are
// interpreted as absolute).
class ScalarResolver {
public:
  virtual ~ScalarResolver() = default;
  [[nodiscard]] virtual auto resolve(ScalarId id) const -> float = 0;
};

// Parameters shared across the whole rig for one render call. Passing by
// struct keeps the interpreter's API stable when we add (e.g.) culling hints.
struct InterpretContext {
  QMatrix4x4 model;                     // Unit-local -> world matrix.
  const AnchorResolver *anchors{nullptr};
  const PaletteResolver *palette{nullptr};
  const ScalarResolver *scalars{nullptr};
  Render::GL::Material *material{nullptr}; // May be null → legacy path.
  std::uint8_t lod{0};                  // Active LOD index (0 = highest).
  float global_alpha{1.0F};             // Multiplied into each part.
};

// Render a whole rig definition. Parts whose lod_mask bit for ctx.lod is 0
// are skipped without any matrix work — LOD is handled at the DSL level so
// lower LODs get an automatic speedup.
void render_rig(const RigDef &def, const InterpretContext &ctx,
                Render::GL::ISubmitter &submitter);

// Emit a single part (exposed so custom code paths can mix-and-match DSL
// parts with hand-rolled emissions during incremental migration).
void render_part(const PartDef &part, const InterpretContext &ctx,
                 Render::GL::ISubmitter &submitter);

} // namespace Render::RigDSL
