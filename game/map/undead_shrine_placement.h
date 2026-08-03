#pragma once

#include <QString>
#include <QVector3D>

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "map_definition.h"

namespace Game::Map {

class TerrainService;

inline constexpr float k_undead_shrine_clearance = 2.6F;
inline constexpr float k_undead_shrine_adopt_distance = 3.5F;
inline constexpr float k_undead_shrine_min_search_radius = 6.0F;

struct UndeadShrinePlacement {
  QString zone_id;

  bool placed = false;

  bool adopted_existing_prop = false;

  bool moved_off_center = false;
  std::uint64_t prop_id = 0;
  QVector3D world_position;
};

struct UndeadShrineExclusions {
  std::unordered_set<std::uint64_t> claimed_prop_ids;
  std::vector<QVector3D> reserved_sites;
};

[[nodiscard]] constexpr auto
world_prop_blocks_shrine(WorldProp::Type type) noexcept -> bool {
  return type != WorldProp::Type::Plant && type != WorldProp::Type::FireCamp;
}

[[nodiscard]] auto undead_zone_center_world(const MapDefinition& map_definition,
                                            const UndeadZone& zone) -> QVector3D;

[[nodiscard]] auto
is_undead_shrine_site_clear(const TerrainService& terrain,
                            float world_x,
                            float world_z,
                            const UndeadShrineExclusions& exclusions = {}) -> bool;

[[nodiscard]] auto plan_undead_zone_shrine(
    const TerrainService& terrain,
    const MapDefinition& map_definition,
    const UndeadZone& zone,
    const UndeadShrineExclusions& exclusions = {}) -> UndeadShrinePlacement;

[[nodiscard]] auto plan_undead_zone_shrines(const TerrainService& terrain,
                                            const MapDefinition& map_definition)
    -> std::vector<UndeadShrinePlacement>;

} // namespace Game::Map
