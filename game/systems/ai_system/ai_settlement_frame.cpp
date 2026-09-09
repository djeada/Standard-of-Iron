#include "ai_settlement_frame.h"

#include <algorithm>
#include <cmath>

#include "ai_base_manager.h"
#include "ai_utils.h"

namespace Game::Systems::AI {

auto settlement_facing(const AIContext& context,
                       const AISnapshot& snapshot) -> QVector2D {
  float sum_x = 0.0F;
  float sum_z = 0.0F;
  int count = 0;
  for (const auto& objective : snapshot.strategic_objectives) {
    if (objective.health <= 0 || !objective.is_building) {
      continue;
    }
    sum_x += objective.pos_x;
    sum_z += objective.pos_z;
    count++;
  }
  if (count == 0) {
    for (const auto& contact : snapshot.visible_enemies) {
      if (contact.health <= 0) {
        continue;
      }
      const float weight = contact.is_building ? 4.0F : 1.0F;
      sum_x += contact.pos_x * weight;
      sum_z += contact.pos_z * weight;
      count += static_cast<int>(weight);
    }
  }
  if (count == 0) {
    return {0.0F, -1.0F};
  }

  const float dx = (sum_x / static_cast<float>(count)) - context.base_pos_x;
  const float dz = (sum_z / static_cast<float>(count)) - context.base_pos_z;
  const float length = std::sqrt(std::max(0.0F, dx * dx + dz * dz));
  if (length < 1.0F) {
    return {0.0F, -1.0F};
  }

  if (std::abs(dx) >= std::abs(dz)) {
    return {dx > 0.0F ? -1.0F : 1.0F, 0.0F};
  }
  return {0.0F, dz > 0.0F ? -1.0F : 1.0F};
}

auto locked_settlement_facing(const AIContext& context,
                              const AISnapshot& snapshot) -> QVector2D {
  if (context.settlement_facing_locked) {
    return {context.settlement_facing_x, context.settlement_facing_z};
  }
  return settlement_facing(context, snapshot);
}

auto plan_offset_to_world(const QVector2D& facing,
                          float local_x,
                          float local_z) -> QVector3D {
  const QVector2D forward = facing;
  const QVector2D right(forward.y(), -forward.x());
  return QVector3D(local_z * forward.x() + local_x * right.x(),
                   0.0F,
                   local_z * forward.y() + local_x * right.y());
}

auto plan_rotation_to_world(const QVector2D& facing, float local_rotation) -> float {
  constexpr float k_rad_to_deg = 180.0F / 3.14159265358979323846F;
  const float frame_yaw = std::atan2(facing.x(), facing.y()) * k_rad_to_deg;
  float yaw = std::fmod(frame_yaw + local_rotation, 360.0F);
  if (yaw < 0.0F) {
    yaw += 360.0F;
  }
  return yaw;
}

auto settlement_muster_world(const AIContext& context,
                             const AISnapshot& snapshot,
                             MusterSide side) -> std::optional<QVector3D> {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->town_plan == nullptr ||
      !context.has_base_anchor || !context.anchor_is_structural) {
    return std::nullopt;
  }
  const auto offset = doctrine->town_plan->muster_offset(side);
  const QVector3D world = plan_offset_to_world(
      locked_settlement_facing(context, snapshot), offset.x, offset.z);
  return QVector3D(context.base_pos_x + world.x(),
                   context.base_pos_y,
                   context.base_pos_z + world.z());
}

auto doctrine_muster_side(const AIStrategyConfig& strategy) -> MusterSide {
  constexpr float k_bold_aggression = 0.70F;
  if (strategy.posture == AIPosture::Garrison ||
      strategy.personality.aggression < k_bold_aggression) {
    return MusterSide::Inside;
  }
  return MusterSide::Outside;
}

void apply_settlement_stations(const AISnapshot& snapshot, AIContext& context) {
  const auto inside = settlement_muster_world(context, snapshot, MusterSide::Inside);
  const auto outside = settlement_muster_world(context, snapshot, MusterSide::Outside);
  if (!inside.has_value() || !outside.has_value()) {
    context.has_settlement_stations = false;
    return;
  }
  context.has_settlement_stations = true;
  context.musters_outside =
      doctrine_muster_side(context.strategy_config) == MusterSide::Outside;
  context.muster_inside_x = inside->x();
  context.muster_inside_z = inside->z();
  context.muster_outside_x = outside->x();
  context.muster_outside_z = outside->z();

  constexpr float k_flank_reach = 10.0F;
  const QVector2D facing = locked_settlement_facing(context, snapshot);
  const QVector2D right(facing.y(), -facing.x());
  context.detachment_x = outside->x() + right.x() * k_flank_reach;
  context.detachment_z = outside->z() + right.y() * k_flank_reach;

  const QVector3D& rally = context.musters_outside ? *outside : *inside;
  context.rally_x = rally.x();
  context.rally_z = rally.z();
  const AIBase* main = AIBaseManager::main_base(context);
  for (auto& base : context.bases) {
    if (main != nullptr && base.id == main->id) {
      base.rally_x = rally.x();
      base.rally_z = rally.z();
    }
  }
}

auto station_radius(const AIContext& context) -> float {
  return std::max(8.0F, context.macro_targets.assembly_radius);
}

void update_station_report(const AISnapshot& snapshot, AIContext& context) {
  AIContext::StationReport report;
  const float radius_sq = station_radius(context) * station_radius(context);
  constexpr float k_spawn_ring = 5.5F;
  constexpr float k_spawn_ring_sq = k_spawn_ring * k_spawn_ring;

  std::vector<QVector3D> stations;
  stations.emplace_back(context.rally_x, 0.0F, context.rally_z);
  if (context.has_settlement_stations) {
    stations.emplace_back(context.muster_inside_x, 0.0F, context.muster_inside_z);
    stations.emplace_back(context.detachment_x, 0.0F, context.detachment_z);
  }
  for (const auto& base : context.bases) {
    stations.emplace_back(base.rally_x, 0.0F, base.rally_z);
  }
  if (context.anchor_is_structural && context.effective_reserve_units > 0) {
    stations.emplace_back(context.base_pos_x, 0.0F, context.base_pos_z);
  }

  for (const auto& entity : snapshot.friendly_units) {
    if (!is_combat_role_unit(entity) || entity.is_commander || entity.is_assault ||
        entity.health <= 0) {
      continue;
    }
    ++report.combat_units;
    if (marches_with_the_wave(entity.id, context)) {
      ++report.marching;
      continue;
    }
    if (is_entity_engaged(entity, snapshot.visible_enemies)) {
      ++report.fighting;
      continue;
    }
    bool stationed = false;
    for (const auto& station : stations) {
      if (distance_squared(
              entity.pos_x, 0.0F, entity.pos_z, station.x(), 0.0F, station.z()) <=
          radius_sq) {
        stationed = true;
        break;
      }
    }
    if (stationed) {
      ++report.stationed;
      continue;
    }
    bool at_spawn = false;
    for (const auto& building : snapshot.friendly_units) {
      if (!building.is_building ||
          building.spawn_type != Game::Units::SpawnType::Barracks) {
        continue;
      }
      if (distance_squared(
              entity.pos_x, 0.0F, entity.pos_z, building.pos_x, 0.0F, building.pos_z) <=
          k_spawn_ring_sq) {
        at_spawn = true;
        break;
      }
    }
    if (at_spawn) {
      ++report.at_spawn;
    } else {
      ++report.adrift;
    }
  }
  context.station_report = report;
}

} // namespace Game::Systems::AI
