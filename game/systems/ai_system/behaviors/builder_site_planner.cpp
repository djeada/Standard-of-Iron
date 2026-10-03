#include "builder_site_planner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "../../../map/terrain_service.h"
#include "../../building_collision_registry.h"
#include "../../nation_registry.h"
#include "../ai_utils.h"
#include "builder_affordability.h"
#include "builder_catalog.h"
#include "builder_site_geometry.h"
#include "builder_town_plan.h"

namespace Game::Systems::AI {

auto expanding_ring_offset(const AIContext& context,
                           int index,
                           int per_ring,
                           float first_radius,
                           float radius_step) -> QVector3D {
  constexpr float k_plan_clearance = 6.0F;
  constexpr int k_rings_before_the_cap = 3;

  const float inner =
      std::max(first_radius, plan_footprint_radius(context) + k_plan_clearance);
  const float outer =
      inner + (radius_step * static_cast<float>(k_rings_before_the_cap));

  const int ring = index / std::max(1, per_ring);
  const int step = index % std::max(1, per_ring);
  const float radius =
      std::min(inner + (static_cast<float>(ring) * radius_step), outer);
  const float angle = (6.2831853F * static_cast<float>(step) /
                       static_cast<float>(std::max(1, per_ring))) +
                      (static_cast<float>(ring) * 0.4F);
  return {radius * std::cos(angle), 0.0F, radius * std::sin(angle)};
}

auto planned_settlement_offset(const AIContext& context,
                               const char* building_type,
                               int construction_index) -> QVector3D {
  const bool carthaginian =
      context.nation != nullptr && context.nation->id == NationID::Carthage;
  if (building_type == BUILDING_TYPE_DEFENSE_TOWER) {
    static const std::array<QVector3D, 4> roman = {QVector3D{-8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, 8.0F},
                                                   QVector3D{-8.0F, 0.0F, 8.0F}};
    static const std::array<QVector3D, 4> punic = {QVector3D{-8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, -8.0F},
                                                   QVector3D{-8.0F, 0.0F, 8.0F},
                                                   QVector3D{8.0F, 0.0F, 8.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    return offsets[static_cast<std::size_t>(construction_index) % offsets.size()];
  }
  if (building_type == BUILDING_TYPE_HOME) {
    static const std::array<QVector3D, 6> roman = {QVector3D{-8.0F, 0.0F, 5.0F},
                                                   QVector3D{-4.0F, 0.0F, 5.0F},
                                                   QVector3D{4.0F, 0.0F, 5.0F},
                                                   QVector3D{8.0F, 0.0F, 5.0F},
                                                   QVector3D{-8.0F, 0.0F, -5.0F},
                                                   QVector3D{8.0F, 0.0F, -5.0F}};
    static const std::array<QVector3D, 6> punic = {QVector3D{-6.0F, 0.0F, 4.0F},
                                                   QVector3D{-2.0F, 0.0F, 6.0F},
                                                   QVector3D{2.0F, 0.0F, 6.0F},
                                                   QVector3D{6.0F, 0.0F, 4.0F},
                                                   QVector3D{-5.0F, 0.0F, -5.0F},
                                                   QVector3D{5.0F, 0.0F, -5.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    if (construction_index < static_cast<int>(offsets.size())) {
      return offsets[static_cast<std::size_t>(construction_index)];
    }
    return expanding_ring_offset(
        context, construction_index - static_cast<int>(offsets.size()), 7, 12.0F, 4.0F);
  }
  if (building_type == BUILDING_TYPE_FARM) {

    static const std::array<QVector3D, 6> fields = {QVector3D{-18.0F, 0.0F, 14.0F},
                                                    QVector3D{18.0F, 0.0F, 14.0F},
                                                    QVector3D{-24.0F, 0.0F, 2.0F},
                                                    QVector3D{24.0F, 0.0F, 2.0F},
                                                    QVector3D{-14.0F, 0.0F, 20.0F},
                                                    QVector3D{14.0F, 0.0F, 20.0F}};
    if (construction_index < static_cast<int>(fields.size())) {
      return fields[static_cast<std::size_t>(construction_index)];
    }
    return expanding_ring_offset(
        context, construction_index - static_cast<int>(fields.size()), 6, 28.0F, 8.0F);
  }
  if (building_type == BUILDING_TYPE_MARKETPLACE) {
    static const std::array<QVector3D, 4> roman = {QVector3D{3.0F, 0.0F, 9.0F},
                                                   QVector3D{-3.0F, 0.0F, 9.0F},
                                                   QVector3D{11.0F, 0.0F, 10.0F},
                                                   QVector3D{-11.0F, 0.0F, 10.0F}};
    static const std::array<QVector3D, 4> punic = {QVector3D{-3.0F, 0.0F, 9.0F},
                                                   QVector3D{3.0F, 0.0F, 9.0F},
                                                   QVector3D{-11.0F, 0.0F, 10.0F},
                                                   QVector3D{11.0F, 0.0F, 10.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    if (construction_index < static_cast<int>(offsets.size())) {
      return offsets[static_cast<std::size_t>(construction_index)];
    }
    return expanding_ring_offset(
        context, construction_index - static_cast<int>(offsets.size()), 6, 16.0F, 5.0F);
  }
  if (building_type == BUILDING_TYPE_BARRACKS) {
    static const std::array<QVector3D, 5> roman = {QVector3D{0.0F, 0.0F, -8.0F},
                                                   QVector3D{-10.0F, 0.0F, -6.0F},
                                                   QVector3D{10.0F, 0.0F, -6.0F},
                                                   QVector3D{-16.0F, 0.0F, -12.0F},
                                                   QVector3D{16.0F, 0.0F, -12.0F}};
    static const std::array<QVector3D, 5> punic = {QVector3D{0.0F, 0.0F, -7.0F},
                                                   QVector3D{-12.0F, 0.0F, -4.0F},
                                                   QVector3D{12.0F, 0.0F, -4.0F},
                                                   QVector3D{-16.0F, 0.0F, -11.0F},
                                                   QVector3D{16.0F, 0.0F, -11.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    if (construction_index < static_cast<int>(offsets.size())) {
      return offsets[static_cast<std::size_t>(construction_index)];
    }
    return expanding_ring_offset(
        context, construction_index - static_cast<int>(offsets.size()), 6, 20.0F, 6.0F);
  }
  if (building_type == BUILDING_TYPE_WALL_SEGMENT) {
    const int slot = construction_index % 11;
    return QVector3D{
        -10.0F + static_cast<float>(slot) * 2.0F, 0.0F, carthaginian ? -13.0F : -14.0F};
  }
  if (is_siege_engine_building(building_type)) {
    return expanding_ring_offset(context, construction_index, 8, 12.0F, 5.0F);
  }
  const float angle = static_cast<float>(construction_index) * 0.8F;
  return {18.0F * std::cos(angle), 0.0F, 18.0F * std::sin(angle)};
}

auto node_on_the_ground(const AISnapshot& snapshot,
                        const SourNodes& sour,
                        const char* building_type,
                        float world_x,
                        float world_z) -> NodeOnTheGround {

  const auto size = BuildingCollisionRegistry::get_building_size(building_type);
  constexpr float k_site_margin = 0.35F;
  constexpr float k_root_margin = 0.1F;
  const float own_reach = std::hypot((0.5F * size.width) + k_site_margin,
                                     (0.5F * size.depth) + k_site_margin);
  NodeOnTheGround found;
  float nearest_sq = std::numeric_limits<float>::infinity();
  for (const auto& node : snapshot.resource_nodes) {
    if (node.reserved || node_is_sour(sour, node.id, snapshot.game_time)) {
      continue;
    }
    const float reach = own_reach +
                        Game::Map::world_prop_ground_radius(node.type, node.scale) +
                        k_root_margin;
    const float distance_sq =
        distance_squared(node.pos_x, 0.0F, node.pos_z, world_x, 0.0F, world_z);
    if (distance_sq > reach * reach || distance_sq >= nearest_sq) {
      continue;
    }
    for (const ResourceType resource :
         {ResourceType::Wood, ResourceType::Stone, ResourceType::Iron}) {
      if (node_matches_resource(node, resource)) {
        found = {&node, resource};
        nearest_sq = distance_sq;
        break;
      }
    }
  }
  return found;
}

auto find_free_site(const AISnapshot& snapshot,
                    const AIContext& context,
                    const char* building_type,
                    int& construction_counter) -> ResolvedSite {
  constexpr int k_site_search_attempts = 24;

  ResolvedSite site;
  for (const bool honour_plan : {true, false}) {
    for (int attempt = 0; attempt < k_site_search_attempts && !site.resolved;
         ++attempt) {
      const QVector3D offset = planned_settlement_offset(
          context, building_type, construction_counter + attempt);
      const float candidate_x = context.base_pos_x + offset.x();
      const float candidate_z = context.base_pos_z + offset.z();
      if (!site_is_free(snapshot, building_type, candidate_x, candidate_z)) {
        continue;
      }
      if (context.strategy_config.posture == AIPosture::Garrison && snapshot.has_ward &&
          (std::abs(candidate_x - snapshot.ward_x) > snapshot.ward_half_x ||
           std::abs(candidate_z - snapshot.ward_z) > snapshot.ward_half_z)) {
        continue;
      }
      if (plan_reserves_ground(context,
                               snapshot,
                               building_type,
                               candidate_x,
                               candidate_z,
                               !honour_plan)) {
        continue;
      }
      site.x = candidate_x;
      site.z = candidate_z;
      construction_counter += attempt;
      site.resolved = true;
    }
    if (site.resolved) {
      break;
    }
  }
  if (!site.resolved) {
    construction_counter += k_site_search_attempts;
    site.exhausted = true;
  }
  return site;
}

} // namespace Game::Systems::AI
