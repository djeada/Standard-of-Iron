#include "spec.h"

namespace Render::Creature {

auto validate_creature_spec(const CreatureSpec &spec) noexcept -> bool {
  if (!validate_topology(spec.topology)) {
    return false;
  }
  return validate_part_graph(spec.topology, spec.lod_full) &&
         validate_part_graph(spec.topology, spec.lod_minimal) &&
         validate_part_graph(spec.topology, spec.lod_billboard);
}

} // namespace Render::Creature
