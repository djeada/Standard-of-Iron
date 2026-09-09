#include "ai_settlement_frame.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

#include "../nav_grid.h"
#include "ai_base_manager.h"
#include "ai_formation.h"
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
}

auto shortest_angle_between(float left, float right) -> float {
  float delta = std::fmod(left - right, 360.0F);
  if (delta < -180.0F) {
    delta += 360.0F;
  } else if (delta > 180.0F) {
    delta -= 360.0F;
  }
  return std::abs(delta);
}

auto station_facing_degrees(const AIContext& context,
                            const AISnapshot& snapshot) -> float {
  return plan_rotation_to_world(locked_settlement_facing(context, snapshot), 0.0F);
}

namespace {

constexpr float k_station_facing_epsilon_deg = 5.0F;

constexpr float k_station_facing_dwell_seconds = 8.0F;

constexpr float k_station_relocation_metres = 6.0F;

constexpr float k_station_fit_recheck_seconds = 10.0F;

constexpr float k_station_fit_move_metres = 2.0F;

constexpr float k_station_manoeuvre_margin = 2.0F;

auto muster_units(const AISnapshot& snapshot) -> std::vector<const EntitySnapshot*> {
  std::vector<const EntitySnapshot*> units;
  units.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (stands_in_the_muster(entity)) {
      units.push_back(&entity);
    }
  }
  return units;
}

auto muster_ground(const AISnapshot& snapshot,
                   const AIContext& context,
                   const std::vector<const EntitySnapshot*>& units) -> MusterFootprint {
  AIFormationRequest request;
  request.player_id = context.player_id;
  request.nation = context.nation;
  request.spacing = context.macro_targets.gather_spacing;
  request.intent = select_ai_intent(
      snapshot, context, context.strategy_config.posture == AIPosture::Garrison, false);
  request.facing = context.station.facing_deg;
  request.preserve_member_order = true;
  request.resolve_terrain = false;
  auto footprint = muster_footprint(request, units);
  footprint.frontage += k_station_manoeuvre_margin * 2.0F;
  footprint.depth += k_station_manoeuvre_margin * 2.0F;
  return footprint;
}

auto seats_available(const QVector3D& centre,
                     const MusterFootprint& ground,
                     float facing_deg,
                     float step) -> int {
  const float yaw = facing_deg * (3.14159265358979323846F / 180.0F);
  const float sin_yaw = std::sin(yaw);
  const float cos_yaw = std::cos(yaw);
  const float half_frontage = ground.frontage * 0.5F;
  const float half_depth = ground.depth * 0.5F;

  int seats = 0;
  for (float dz = -half_depth; dz <= half_depth + 1.0e-3F; dz += step) {
    for (float dx = -half_frontage; dx <= half_frontage + 1.0e-3F; dx += step) {
      const QVector3D probe(centre.x() + (dx * cos_yaw) + (dz * sin_yaw),
                            centre.y(),
                            centre.z() - (dx * sin_yaw) + (dz * cos_yaw));
      if (Game::Systems::NavGrid::is_world_position_walkable(probe)) {
        ++seats;
      }
    }
  }
  return seats;
}

auto ground_fits(const QVector3D& centre,
                 const MusterFootprint& ground,
                 float facing_deg,
                 float step,
                 int members) -> bool {
  if (!Game::Systems::NavGrid::is_world_position_walkable(centre)) {
    return false;
  }
  return seats_available(centre, ground, facing_deg, step) >= members;
}

auto choose_candidate(const AIContext& context) -> std::pair<StationSource, QVector3D> {
  if (context.has_settlement_stations) {
    return {StationSource::SettlementMuster,
            QVector3D(context.musters_outside ? context.muster_outside_x
                                              : context.muster_inside_x,
                      context.base_pos_y,
                      context.musters_outside ? context.muster_outside_z
                                              : context.muster_inside_z)};
  }
  if (const AIBase* main = AIBaseManager::main_base(context); main != nullptr) {
    return {StationSource::MainBaseRally,
            QVector3D(main->rally_x, context.base_pos_y, main->rally_z)};
  }
  if (context.anchor_station.offered) {
    return {StationSource::Anchor,
            QVector3D(context.anchor_station.x,
                      context.base_pos_y,
                      context.anchor_station.z)};
  }
  return {StationSource::None, QVector3D(context.station.x, 0.0F, context.station.z)};
}

auto measurement_is_stale(const AIStation& station,
                          const QVector3D& candidate,
                          int members,
                          std::uint64_t revision,
                          float now) -> bool {
  return station.measured_strength < 0 ||
         distance_squared(station.measured_candidate_x,
                          0.0F,
                          station.measured_candidate_z,
                          candidate.x(),
                          0.0F,
                          candidate.z()) >
             k_station_fit_move_metres * k_station_fit_move_metres ||
         std::abs(members - station.measured_strength) >
             std::max(2, station.measured_strength / 4) ||
         revision != station.measured_navigation_revision ||
         now - station.measured_at > k_station_fit_recheck_seconds;
}

auto fitted_position(AIStation& station,
                     const AISnapshot& snapshot,
                     const AIContext& context,
                     const QVector3D& candidate,
                     const std::vector<const EntitySnapshot*>& units) -> QVector3D {
  const auto ground = muster_ground(snapshot, context, units);
  const auto members = static_cast<int>(units.size());
  const float step = std::max(1.0F, context.macro_targets.gather_spacing);
  const float required = 0.5F * std::hypot(ground.frontage, ground.depth);
  station.required_radius = required;

  if (members == 0 || !ground.valid) {
    station.fits = true;
    station.relocated = false;
    return candidate;
  }

  if (ground_fits(candidate, ground, station.facing_deg, step, members)) {
    station.fits = true;
    station.relocated = false;
    return candidate;
  }

  static constexpr std::array<std::pair<float, float>, 8> k_compass = {
      {{0.0F, -1.0F},
       {0.0F, 1.0F},
       {-1.0F, 0.0F},
       {1.0F, 0.0F},
       {-0.7071F, -0.7071F},
       {0.7071F, -0.7071F},
       {-0.7071F, 0.7071F},
       {0.7071F, 0.7071F}}};

  for (const float reach : {1.0F, 1.8F, 2.6F}) {
    for (const auto& [dx, dz] : k_compass) {
      const QVector3D probe(candidate.x() + (dx * required * reach),
                            candidate.y(),
                            candidate.z() + (dz * required * reach));
      if (!ground_fits(probe, ground, station.facing_deg, step, members)) {
        continue;
      }
      station.fits = true;
      station.relocated = true;
      return probe;
    }
  }

  station.fits = false;
  station.relocated = false;
  return candidate;
}

void hold_or_turn(AIStation& station, float desired, bool station_moved, float now) {
  if (station_moved) {
    station.facing_deg = desired;
    station.turn_pending = false;
    return;
  }
  if (shortest_angle_between(desired, station.facing_deg) <=
      k_station_facing_epsilon_deg) {
    station.turn_pending = false;
    return;
  }
  if (!station.turn_pending ||
      shortest_angle_between(desired, station.turn_pending_deg) >
          k_station_facing_epsilon_deg) {
    station.turn_pending = true;
    station.turn_pending_deg = desired;
    station.turn_pending_since = now;
    return;
  }
  if (now - station.turn_pending_since >= k_station_facing_dwell_seconds) {
    station.facing_deg = desired;
    station.turn_pending = false;
    ++station.turns;
  }
}

} // namespace

void resolve_station(const AISnapshot& snapshot, AIContext& context) {
  AIStation& station = context.station;
  const auto [source, candidate] = choose_candidate(context);
  station.source = source;

  const bool first_decision = station.measured_strength < 0;
  const bool frame_moved =
      first_decision || distance_squared(station.measured_candidate_x,
                                         0.0F,
                                         station.measured_candidate_z,
                                         candidate.x(),
                                         0.0F,
                                         candidate.z()) >
                            k_station_relocation_metres * k_station_relocation_metres;

  hold_or_turn(station,
               station_facing_degrees(context, snapshot),
               frame_moved,
               snapshot.game_time);

  if (source == StationSource::None) {
    return;
  }

  const auto units = muster_units(snapshot);
  const auto members = static_cast<int>(units.size());
  if (!measurement_is_stale(station,
                            candidate,
                            members,
                            snapshot.navigation_revision,
                            snapshot.game_time)) {
    return;
  }

  const QVector3D placed =
      fitted_position(station, snapshot, context, candidate, units);
  station.x = placed.x();
  station.z = placed.z();
  station.measured_candidate_x = candidate.x();
  station.measured_candidate_z = candidate.z();
  station.measured_strength = members;
  station.measured_navigation_revision = snapshot.navigation_revision;
  station.measured_at = snapshot.game_time;
}

auto station_radius(const AIContext& context) -> float {
  return std::max(8.0F, context.macro_targets.assembly_radius);
}

auto station_standing(const EntitySnapshot& entity,
                      const AIContext& context,
                      float current_time) -> StationStanding {
  const auto motion = soldier_motion(entity, context, current_time);
  if (motion == SoldierMotion::Blocked) {
    return StationStanding::Blocked;
  }

  const float radius = station_radius(context);
  const bool inside = distance_squared(entity.pos_x,
                                       0.0F,
                                       entity.pos_z,
                                       context.station.x,
                                       0.0F,
                                       context.station.z) <= radius * radius;

  if (motion == SoldierMotion::UnderWay) {
    return inside ? StationStanding::Reforming : StationStanding::Arriving;
  }

  if (!inside) {
    return StationStanding::Arriving;
  }

  if (entity.movement.has_objective) {
    const float tolerance =
        std::max(2.0F, context.macro_targets.gather_spacing * 2.0F) + 1.0F;
    const float goal_error = distance_squared(entity.pos_x,
                                              0.0F,
                                              entity.pos_z,
                                              entity.movement.objective_x,
                                              0.0F,
                                              entity.movement.objective_z);
    if (goal_error > tolerance * tolerance) {
      return StationStanding::Reforming;
    }
  }

  return StationStanding::Ready;
}

void update_station_report(const AISnapshot& snapshot, AIContext& context) {
  AIContext::StationReport report;
  const float radius_sq = station_radius(context) * station_radius(context);
  constexpr float k_spawn_ring = 5.5F;
  constexpr float k_spawn_ring_sq = k_spawn_ring * k_spawn_ring;

  std::vector<QVector3D> stations;
  stations.emplace_back(context.station.x, 0.0F, context.station.z);
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
    if (!stands_in_the_muster(entity)) {
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
    switch (station_standing(entity, context, snapshot.game_time)) {
    case StationStanding::Ready:
      ++report.ready;
      break;
    case StationStanding::Reforming:
      ++report.reforming;
      break;
    case StationStanding::Arriving:
      ++report.arriving;
      break;
    case StationStanding::Blocked:
      ++report.blocked;
      break;
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
