#pragma once

// Stage 3 seam — unified "draw one posed part" command.
//
// DrawPartCmd is the migration target for the 21-variant DrawCmd soup: any
// piece of gameplay geometry (unit limb, equipment piece, building part)
// should eventually be expressible as a single DrawPartCmd. The command
// carries a mesh, a Material (see material.h) that knows how to pick a
// shader per ShaderQuality tier, and an optional bone palette for future
// GPU skinning (Stage 7).
//
// For now DrawPartCmd coexists with MeshCmd. The backend dispatch routes
// it through the same pipeline as MeshCmd, but the command itself is
// Material-aware, so Stage 5 quality-plumbing lands at the DrawPartCmd
// edge without touching every draw call site.

#include "frame_budget.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

class Mesh;
class Texture;
struct Material;

// A lightweight reference to a bone palette. The referenced data is owned
// by a cache (see pose_template_cache.h) and must outlive the DrawPartCmd.
// When `count == 0` the part is treated as un-skinned — the backend simply
// uses `DrawPartCmd::world` directly.
struct BonePaletteRef {
  const QMatrix4x4 *data = nullptr;
  std::size_t count = 0;

  [[nodiscard]] auto empty() const noexcept -> bool { return count == 0; }
};

struct DrawPartCmd {
  Mesh *mesh = nullptr;
  const Material *material = nullptr;
  QMatrix4x4 world;
  BonePaletteRef palette{};

  // Per-instance overrides. The Material provides tier shaders + defaults;
  // per-call tint/alpha/texture live here so a shared Material can serve
  // many units without per-unit allocation.
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha = 1.0F;
  Texture *texture = nullptr;
  std::int32_t material_id = 0;

  CommandPriority priority{CommandPriority::Normal};
};

} // namespace Render::GL
