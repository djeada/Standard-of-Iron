#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

#include "game/systems/building_collision_registry.h"
#include "tools/arena/arena_scenario.h"
#include "tools/arena/arena_scenarios.h"

namespace {

struct Footprint {
  QString name;
  float x{0.0F};
  float z{0.0F};
  float half_w{0.0F};
  float half_d{0.0F};
};

[[nodiscard]] auto
building_half_extent(Game::Units::SpawnType type) -> std::pair<float, float> {
  const auto size = Game::Systems::BuildingCollisionRegistry::get_building_size(
      Game::Units::spawn_typeToQString(type).toStdString());
  return {size.width * 0.5F, size.depth * 0.5F};
}

[[nodiscard]] auto collect_wall_footprints(
    const Arena::ArenaScenarioDefinition& scenario) -> std::vector<Footprint> {
  std::vector<Footprint> out;
  for (const auto& group : scenario.groups) {
    if (!group.spawn_type.has_value()) {
      continue;
    }
    if (*group.spawn_type != Game::Units::SpawnType::WallSegment &&
        *group.spawn_type != Game::Units::SpawnType::WallGate &&
        *group.spawn_type != Game::Units::SpawnType::DefenseTower) {
      continue;
    }
    const auto [half_w, half_d] = building_half_extent(*group.spawn_type);
    const float center = (static_cast<float>(group.count) - 1.0F) * 0.5F;
    for (int index = 0; index < group.count; ++index) {
      const QVector3D at =
          group.origin + (group.spacing * (static_cast<float>(index) - center));
      out.push_back({group.name, at.x(), at.z(), half_w, half_d});
    }
  }
  return out;
}

[[nodiscard]] auto collect_footprints(const Arena::ArenaScenarioDefinition& scenario)
    -> std::vector<Footprint> {
  std::vector<Footprint> out;
  for (const auto& group : scenario.groups) {
    if (!group.spawn_type.has_value() ||
        !Game::Units::is_building_spawn(*group.spawn_type)) {
      continue;
    }

    if (*group.spawn_type == Game::Units::SpawnType::WallSegment ||
        *group.spawn_type == Game::Units::SpawnType::WallGate ||
        *group.spawn_type == Game::Units::SpawnType::DefenseTower) {
      continue;
    }
    const auto [half_w, half_d] = building_half_extent(*group.spawn_type);
    const float center = (static_cast<float>(group.count) - 1.0F) * 0.5F;
    for (int index = 0; index < group.count; ++index) {
      const QVector3D at =
          group.origin + (group.spacing * (static_cast<float>(index) - center));
      out.push_back({group.name, at.x(), at.z(), half_w, half_d});
    }
  }
  return out;
}

[[nodiscard]] auto point_segment_distance(float px,
                                          float pz,
                                          const Game::Map::RoadSegment& road) -> float {
  const float ax = road.start.x();
  const float az = road.start.z();
  const float bx = road.end.x();
  const float bz = road.end.z();
  const float dx = bx - ax;
  const float dz = bz - az;
  const float length_sq = (dx * dx) + (dz * dz);
  float t = 0.0F;
  if (length_sq > 1.0e-6F) {
    t = std::clamp((((px - ax) * dx) + ((pz - az) * dz)) / length_sq, 0.0F, 1.0F);
  }
  const float cx = ax + (dx * t);
  const float cz = az + (dz * t);
  return std::hypot(px - cx, pz - cz);
}

[[nodiscard]] auto
river_as_road(const Game::Map::RiverSegment& river) -> Game::Map::RoadSegment {
  Game::Map::RoadSegment as_road;
  as_road.start = river.start;
  as_road.end = river.end;
  as_road.width = river.width;
  return as_road;
}

[[nodiscard]] auto
trailer_scenarios() -> std::vector<const Arena::ArenaScenarioDefinition*> {
  std::vector<const Arena::ArenaScenarioDefinition*> out;
  for (const auto& scenario : Arena::Scenarios::definitions()) {
    if (scenario.id.startsWith(QStringLiteral("trailer_"))) {
      out.push_back(&scenario);
    }
  }
  return out;
}

TEST(SettlementLayoutTest, TrailerScenariosExist) {
  EXPECT_FALSE(trailer_scenarios().empty());
}

TEST(SettlementLayoutTest, NoBuildingOverlapsAnother) {
  for (const auto* scenario : trailer_scenarios()) {
    const auto prints = collect_footprints(*scenario);
    for (std::size_t a = 0; a < prints.size(); ++a) {
      for (std::size_t b = a + 1; b < prints.size(); ++b) {
        const bool overlap_x =
            std::abs(prints[a].x - prints[b].x) < (prints[a].half_w + prints[b].half_w);
        const bool overlap_z =
            std::abs(prints[a].z - prints[b].z) < (prints[a].half_d + prints[b].half_d);
        EXPECT_FALSE(overlap_x && overlap_z)
            << scenario->id.toStdString() << ": " << prints[a].name.toStdString()
            << " at (" << prints[a].x << "," << prints[a].z << ") overlaps "
            << prints[b].name.toStdString() << " at (" << prints[b].x << ","
            << prints[b].z << ")";
      }
    }
  }
}

TEST(SettlementLayoutTest, NoBuildingStandsOnARoad) {
  for (const auto* scenario : trailer_scenarios()) {
    const auto prints = collect_footprints(*scenario);
    for (const auto& print : prints) {
      for (const auto& road : scenario->roads) {
        const float clearance = point_segment_distance(print.x, print.z, road);
        const float forbidden =
            (road.width * 0.5F) + std::min(print.half_w, print.half_d);
        EXPECT_GE(clearance, forbidden)
            << scenario->id.toStdString() << ": " << print.name.toStdString() << " at ("
            << print.x << "," << print.z << ") sits " << clearance << " m from a road "
            << road.width << " m wide";
      }
    }
  }
}

TEST(SettlementLayoutTest, NoBuildingIsBuiltIntoAWall) {

  for (const auto* scenario : trailer_scenarios()) {
    const auto walls = collect_wall_footprints(*scenario);
    for (const auto& print : collect_footprints(*scenario)) {
      for (const auto& wall : walls) {
        const bool overlap_x =
            std::abs(print.x - wall.x) < (print.half_w + wall.half_w);
        const bool overlap_z =
            std::abs(print.z - wall.z) < (print.half_d + wall.half_d);
        EXPECT_FALSE(overlap_x && overlap_z)
            << scenario->id.toStdString() << ": " << print.name.toStdString() << " at ("
            << print.x << "," << print.z << ") is built into "
            << wall.name.toStdString() << " at (" << wall.x << "," << wall.z << ")";
      }
    }
  }
}

TEST(SettlementLayoutTest, NoBuildingStandsInWater) {

  for (const auto* scenario : trailer_scenarios()) {
    for (const auto& print : collect_footprints(*scenario)) {
      for (const auto& river : scenario->rivers) {
        const float clearance =
            point_segment_distance(print.x, print.z, river_as_road(river));
        const float forbidden =
            (river.width * 0.5F) + std::min(print.half_w, print.half_d);
        EXPECT_GE(clearance, forbidden)
            << scenario->id.toStdString() << ": " << print.name.toStdString() << " at ("
            << print.x << "," << print.z << ") stands in the river";
      }
    }
  }
}

TEST(SettlementLayoutTest, BridgeApproachIsKeptClear) {

  for (const auto* scenario : trailer_scenarios()) {
    if (scenario->bridges.empty()) {
      continue;
    }
    const auto& bridge = scenario->bridges.front();
    const float bridge_x = bridge.start.x();
    for (const auto& print : collect_footprints(*scenario)) {
      const bool near_lane = std::abs(print.x - bridge_x) < (print.half_w + 3.0F);
      const bool in_approach =
          print.z > std::min(bridge.start.z(), bridge.end.z()) - 12.0F &&
          print.z < std::max(bridge.start.z(), bridge.end.z()) + 12.0F;
      EXPECT_FALSE(near_lane && in_approach)
          << scenario->id.toStdString() << ": " << print.name.toStdString() << " at ("
          << print.x << "," << print.z << ") blocks the bridge approach";
    }
  }
}

} // namespace
