#include "army_formation_morph.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../core/component_combat.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/navigation/nav_grid.h"
#include "../systems/navigation/pathfinding.h"
#include "../util/planar_math.h"
#include "army_formation_planner.h"
#include "army_formation_registry.h"

namespace Game::Formation::Morph {

namespace {

constexpr float k_morph_pace_share = 0.85F;
constexpr float k_morph_about_face_degrees = 135.0F;
constexpr int k_morph_samples = 12;
constexpr float k_min_duration = 0.5F;
constexpr float k_rigid_tolerance = 0.75F;

struct MorphStarts {
  std::vector<QVector3D> positions;
  std::vector<float> speeds;
};

auto kind_of(Engine::Core::World& world, EntityID id) -> std::uint64_t {
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
  const auto* movement = world.try_get<Engine::Core::MovementComponent>(id);
  std::uint64_t kind =
      unit != nullptr ? static_cast<std::uint64_t>(unit->spawn_type) : 0U;
  kind = (kind << 1U) |
         ((movement != nullptr && !movement->get_can_enter_forest()) ? 1U : 0U);
  // Contingents of different nations keep their own places in the shape.
  if (unit != nullptr) {
    kind = (kind << 8U) | static_cast<std::uint64_t>(unit->nation_id);
  }
  return kind;
}

auto ground_position(const Engine::Core::TransformComponent& transform) -> QVector3D {
  return {transform.position.x, 0.0F, transform.position.z};
}

auto index_of(const FormationMorph& morph, EntityID entity) -> std::size_t {
  auto const found = std::find(morph.occupants.begin(), morph.occupants.end(), entity);
  return static_cast<std::size_t>(std::distance(morph.occupants.begin(), found));
}

struct HeldPlaces {
  std::vector<std::size_t> held;
  QVector3D troop_centre;
  QVector3D slot_centre;
};

auto collect_held_places(Engine::Core::World& world,
                         const ArmyFormation& formation) -> HeldPlaces {
  HeldPlaces places;
  for (std::size_t i = 0; i < formation.slot_list.size(); ++i) {
    const auto& slot = formation.slot_list[i];
    const auto* transform =
        world.try_get<Engine::Core::TransformComponent>(slot.occupant);
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked ||
        transform == nullptr) {
      continue;
    }
    places.held.push_back(i);
    places.troop_centre += ground_position(*transform);
    places.slot_centre +=
        QVector3D(slot.world_position.x(), 0.0F, slot.world_position.z());
  }
  if (!places.held.empty()) {
    places.troop_centre /= static_cast<float>(places.held.size());
    places.slot_centre /= static_cast<float>(places.held.size());
  }
  return places;
}

void assign_bucket(Engine::Core::World& world,
                   const ArmyFormation& formation,
                   const HeldPlaces& places,
                   const std::vector<std::size_t>& bucket,
                   float facing_from,
                   float facing_to,
                   std::vector<EntityID>& occupants) {
  std::vector<std::vector<float>> cost(bucket.size(),
                                       std::vector<float>(bucket.size()));
  for (std::size_t r = 0; r < bucket.size(); ++r) {
    const auto* transform = world.try_get<Engine::Core::TransformComponent>(
        formation.slot_list[bucket[r]].occupant);
    QVector3D const place =
        rotate_yaw(ground_position(*transform) - places.troop_centre, -facing_from);
    for (std::size_t c = 0; c < bucket.size(); ++c) {
      auto const& target = formation.slot_list[bucket[c]].world_position;
      QVector3D const shape_place = rotate_yaw(
          QVector3D(target.x(), 0.0F, target.z()) - places.slot_centre, -facing_to);
      cost[r][c] = (place - shape_place).length();
    }
  }
  auto const chosen = ArmyFormationPlanner::min_cost_assignment(cost);
  for (std::size_t r = 0; r < bucket.size(); ++r) {
    auto const column = chosen[r] >= 0 ? static_cast<std::size_t>(chosen[r]) : r;
    occupants[bucket[column]] = formation.slot_list[bucket[r]].occupant;
  }
}

void keep_places_in_shape(Engine::Core::World& world,
                          ArmyFormation& formation,
                          float facing_from,
                          float facing_to) {
  auto const places = collect_held_places(world, formation);
  auto const& held = places.held;
  if (held.size() < 2U) {
    return;
  }

  std::vector<bool> done(held.size(), false);
  std::vector<EntityID> occupants(formation.slot_list.size(), 0U);
  for (std::size_t first = 0; first < held.size(); ++first) {
    if (done[first]) {
      continue;
    }
    auto const kind = kind_of(world, formation.slot_list[held[first]].occupant);
    std::vector<std::size_t> bucket;
    for (std::size_t k = first; k < held.size(); ++k) {
      if (!done[k] && kind_of(world, formation.slot_list[held[k]].occupant) == kind) {
        done[k] = true;
        bucket.push_back(held[k]);
      }
    }
    assign_bucket(world, formation, places, bucket, facing_from, facing_to, occupants);
  }
  for (auto const index : held) {
    formation.slot_list[index].occupant = occupants[index];
  }
  ArmyFormationRuntime::sync_membership_components(world, formation);
}

auto gather_targets(Engine::Core::World& world,
                    const ArmyFormation& formation,
                    FormationMorph& morph,
                    MorphStarts& starts) -> bool {
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked) {
      continue;
    }
    const auto* transform =
        world.try_get<Engine::Core::TransformComponent>(slot.occupant);
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(slot.occupant);
    if (transform == nullptr || unit == nullptr || unit->speed <= 0.0F) {
      return false;
    }
    morph.occupants.push_back(slot.occupant);
    morph.world_to.push_back(slot.world_position);
    QVector3D const offset(slot.world_position.x() - morph.anchor_to.x(),
                           0.0F,
                           slot.world_position.z() - morph.anchor_to.z());
    morph.local_to.push_back(rotate_yaw(offset, -morph.facing_to));
    starts.positions.emplace_back(transform->position.x, 0.0F, transform->position.z);
    starts.speeds.push_back(unit->speed);
  }
  return !morph.occupants.empty();
}

void derive_start_frame(FormationMorph& morph, const MorphStarts& starts) {
  QVector3D anchor_from;
  for (std::size_t i = 0; i < starts.positions.size(); ++i) {
    anchor_from +=
        starts.positions[i] - rotate_yaw(morph.local_to[i], morph.facing_from);
  }
  anchor_from /= static_cast<float>(starts.positions.size());
  anchor_from.setY(morph.anchor_to.y());
  morph.anchor_from = anchor_from;
  for (auto const& start : starts.positions) {
    morph.local_from.push_back(rotate_yaw(start - anchor_from, -morph.facing_from));
  }
}

auto path_length_if_walkable(Engine::Core::World& world,
                             const FormationMorph& morph,
                             std::size_t i,
                             Game::Systems::Pathfinding* pathfinder,
                             float& length) -> bool {
  const auto* movement =
      world.try_get<Engine::Core::MovementComponent>(morph.occupants[i]);
  auto const passability = movement != nullptr && !movement->get_can_enter_forest()
                               ? Game::Systems::Pathfinding::Passability::Heavy
                               : Game::Systems::Pathfinding::Passability::Light;
  float const clearance =
      movement != nullptr ? movement->get_navigation_clearance() : 0.0F;
  length = 0.0F;
  QVector3D previous = point(morph, i, 0.0F);
  for (int k = 1; k <= k_morph_samples; ++k) {
    QVector3D const next =
        point(morph, i, static_cast<float>(k) / static_cast<float>(k_morph_samples));
    if (pathfinder != nullptr && !pathfinder->is_world_segment_walkable(
                                     previous, next, passability, clearance)) {
      return false;
    }
    length +=
        QVector3D(next.x() - previous.x(), 0.0F, next.z() - previous.z()).length();
    previous = next;
  }
  return true;
}

auto measure_paths(Engine::Core::World& world,
                   FormationMorph& morph,
                   const MorphStarts& starts) -> bool {
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  float duration = k_min_duration;
  for (std::size_t i = 0; i < morph.occupants.size(); ++i) {
    float length = 0.0F;
    if (!path_length_if_walkable(world, morph, i, pathfinder, length)) {
      return false;
    }
    duration = std::max(duration, length / (starts.speeds[i] * k_morph_pace_share));
    morph.path_speed.push_back(length);
  }
  for (auto& speed : morph.path_speed) {
    speed /= duration;
  }
  morph.rigid = true;
  for (std::size_t i = 0; i < morph.local_from.size(); ++i) {
    if ((morph.local_from[i] - morph.local_to[i]).length() > k_rigid_tolerance) {
      morph.rigid = false;
      break;
    }
  }
  morph.duration = duration;
  morph.active = true;
  return true;
}

void snap_slots_to_morph_start(ArmyFormation& formation) {
  for (auto& slot : formation.slot_list) {
    auto const& occupants = formation.morph.occupants;
    if (std::find(occupants.begin(), occupants.end(), slot.occupant) ==
        occupants.end()) {
      continue;
    }
    auto const index = index_of(formation.morph, slot.occupant);
    slot.world_position = point(formation.morph, index, 0.0F);
    slot.facing = formation.facing + slot.local_facing;
  }
}

} // namespace

auto rotate_yaw(const QVector3D& local, float yaw_degrees) -> QVector3D {
  float const yaw = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  float const s = std::sin(yaw);
  float const c = std::cos(yaw);
  return {local.x() * c + local.z() * s, 0.0F, -local.x() * s + local.z() * c};
}

auto point(const FormationMorph& morph, std::size_t index, float t) -> QVector3D {
  QVector3D const anchor =
      morph.anchor_from + (morph.anchor_to - morph.anchor_from) * t;
  float const facing =
      morph.facing_from +
      Game::Systems::signed_yaw_delta(morph.facing_from, morph.facing_to) * t;
  QVector3D const local =
      morph.local_from[index] + (morph.local_to[index] - morph.local_from[index]) * t;
  QVector3D const offset = rotate_yaw(local, facing);
  return {anchor.x() + offset.x(), anchor.y(), anchor.z() + offset.z()};
}

auto start(Engine::Core::World& world,
           ArmyFormation& formation,
           std::optional<float> marching_facing) -> bool {
  FormationMorph morph;
  morph.anchor_to = formation.destination;
  morph.facing_to = formation.destination_facing;
  morph.facing_from = marching_facing.value_or(morph.facing_to);
  if (std::abs(Game::Systems::signed_yaw_delta(morph.facing_from, morph.facing_to)) >
      k_morph_about_face_degrees) {
    morph.facing_from = morph.facing_to;
  }

  if (marching_facing.has_value()) {
    keep_places_in_shape(world, formation, morph.facing_from, morph.facing_to);
  }

  MorphStarts starts;
  if (!gather_targets(world, formation, morph, starts)) {
    return false;
  }
  derive_start_frame(morph, starts);
  if (!measure_paths(world, morph, starts)) {
    return false;
  }

  formation.morph = std::move(morph);
  formation.anchor = formation.morph.anchor_from;
  formation.facing = formation.morph.facing_from;
  snap_slots_to_morph_start(formation);
  return true;
}

void advance(ArmyFormation& formation, float delta_time) {
  auto& morph = formation.morph;
  morph.elapsed += delta_time;
  float const t = morph.duration > 0.0F
                      ? std::clamp(morph.elapsed / morph.duration, 0.0F, 1.0F)
                      : 1.0F;
  formation.anchor = morph.anchor_from + (morph.anchor_to - morph.anchor_from) * t;
  formation.facing =
      morph.facing_from +
      Game::Systems::signed_yaw_delta(morph.facing_from, morph.facing_to) * t;
  for (auto& slot : formation.slot_list) {
    auto const found =
        std::find(morph.occupants.begin(), morph.occupants.end(), slot.occupant);
    if (found == morph.occupants.end()) {
      continue;
    }
    auto const index =
        static_cast<std::size_t>(std::distance(morph.occupants.begin(), found));
    slot.world_position = t >= 1.0F ? morph.world_to[index] : point(morph, index, t);
    slot.facing = formation.facing + slot.local_facing;
  }
  formation.moves_pending = true;
  if (t >= 1.0F) {
    formation.anchor = morph.anchor_to;
    formation.facing = morph.facing_to;
    morph.active = false;
  }
}

auto target(const ArmyFormation& formation,
            EntityID entity) -> std::optional<QVector3D> {
  constexpr float k_lead_seconds = 1.0F;
  const auto& morph = formation.morph;
  if (!morph.active || morph.duration <= 0.0F) {
    return std::nullopt;
  }
  auto const found = std::find(morph.occupants.begin(), morph.occupants.end(), entity);
  if (found == morph.occupants.end()) {
    return std::nullopt;
  }
  auto const index =
      static_cast<std::size_t>(std::distance(morph.occupants.begin(), found));
  float const t =
      std::clamp((morph.elapsed + k_lead_seconds) / morph.duration, 0.0F, 1.0F);
  return t >= 1.0F ? morph.world_to[index] : point(morph, index, t);
}

auto pace(const ArmyFormation& formation,
          EntityID entity,
          const QVector3D& position,
          float full_speed) -> float {
  constexpr float k_catch_up_per_metre = 0.5F;
  const auto& morph = formation.morph;
  auto const found = std::find(morph.occupants.begin(), morph.occupants.end(), entity);
  if (!morph.active || found == morph.occupants.end()) {
    return 0.0F;
  }
  auto const index =
      static_cast<std::size_t>(std::distance(morph.occupants.begin(), found));
  const auto* slot = formation.find_slot_for(entity);
  float const behind = slot == nullptr
                           ? 0.0F
                           : QVector3D(slot->world_position.x() - position.x(),
                                       0.0F,
                                       slot->world_position.z() - position.z())
                                 .length();
  float const catch_up =
      morph.path_speed[index] * (1.0F + k_catch_up_per_metre * behind);
  return std::clamp(catch_up, 0.1F, std::max(0.1F, full_speed));
}

} // namespace Game::Formation::Morph
