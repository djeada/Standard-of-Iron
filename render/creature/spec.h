

#pragma once

#include "part_graph.h"
#include "skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <span>
#include <string_view>

namespace Render::GL {
class ISubmitter;
}

namespace Render::Creature {

struct CreatureSpec {
  std::string_view species_name{};
  SkeletonTopology topology{};

  PartGraph lod_full{};
  PartGraph lod_reduced{};
  PartGraph lod_minimal{};
  PartGraph lod_billboard{};
};

[[nodiscard]] constexpr auto
part_graph_for(const CreatureSpec &spec,
               CreatureLOD lod) noexcept -> const PartGraph & {
  switch (lod) {
  case CreatureLOD::Full:
    return spec.lod_full;
  case CreatureLOD::Reduced:
    return spec.lod_reduced;
  case CreatureLOD::Minimal:
    return spec.lod_minimal;
  case CreatureLOD::Billboard:
    return spec.lod_billboard;
  }
  return spec.lod_full;
}

inline auto submit_creature(
    const CreatureSpec &spec, std::span<const QMatrix4x4> palette,
    CreatureLOD lod, const QMatrix4x4 &world_from_unit,
    Render::GL::ISubmitter &out,
    std::span<const QVector3D> role_colors = {}) -> PartSubmissionStats {
  return submit_part_graph(spec.topology, part_graph_for(spec, lod), palette,
                           lod, world_from_unit, out, role_colors);
}

[[nodiscard]] auto
validate_creature_spec(const CreatureSpec &spec) noexcept -> bool;

} // namespace Render::Creature
