#pragma once

#include <QVector3D>

#include <span>

namespace Render::GL {

struct DrawContext;
class ISubmitter;

struct TorchMount {
  QVector3D at;
  QVector3D outward{0.0F, 0.0F, 1.0F};

  bool brazier{false};
};

void submit_building_torches(const DrawContext& ctx,
                             ISubmitter& out,
                             std::span<const TorchMount> mounts);

void submit_temple_incense(const DrawContext& ctx,
                           ISubmitter& out,
                           std::span<const QVector3D> vents);

} // namespace Render::GL
