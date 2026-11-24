#pragma once

#include <QVector3D>

#include <span>

#include "render/frame/render_archetype.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

struct TorchMount {
  QVector3D at;
  QVector3D outward{0.0F, 0.0F, 1.0F};

  bool brazier{false};

  // Carthage's sea-green fire (the incense-stand flame) in place of firelight.
  bool ghost{false};
};

void submit_building_torches(const DrawContext& ctx,
                             ISubmitter& out,
                             std::span<const TorchMount> mounts);

// The live fires an archetype's motifs keep (RenderArchetype::fires).
void submit_archetype_fires(const DrawContext& ctx,
                            ISubmitter& out,
                            std::span<const RenderArchetypeFire> fires);

void submit_temple_incense(const DrawContext& ctx,
                           ISubmitter& out,
                           std::span<const QVector3D> vents);

void submit_cookfire_smoke(const DrawContext& ctx,
                           ISubmitter& out,
                           std::span<const QVector3D> vents);

} // namespace Render::GL
