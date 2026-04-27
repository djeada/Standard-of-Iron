

#pragma once

#include "../draw_part.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <span>
#include <vector>

namespace Render::GL {
struct DrawPartCmd;
}

namespace Render::Pipeline {

struct InstanceEntry {
  QMatrix4x4 world;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
};

struct InstancedPartBatch {
  const Render::GL::DrawPartCmd *header{nullptr};
  std::size_t start{0};
  std::size_t count{0};
  std::vector<InstanceEntry> instances;
};

struct CoalesceStats {
  std::size_t input_parts{0};
  std::size_t batched_parts{0};
  std::size_t batch_count{0};
};

[[nodiscard]] auto
coalesce_instances(std::span<const Render::GL::DrawPartCmd> parts,
                   std::size_t min_run = 4) -> std::vector<InstancedPartBatch>;

[[nodiscard]] auto
compute_coalesce_stats(std::span<const InstancedPartBatch> batches,
                       std::size_t input_parts) -> CoalesceStats;

[[nodiscard]] auto
parts_are_compatible(const Render::GL::DrawPartCmd &a,
                     const Render::GL::DrawPartCmd &b) noexcept -> bool;

} // namespace Render::Pipeline
