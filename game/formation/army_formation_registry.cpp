#include "army_formation_registry.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "../core/ambient_session.h"
#include "../core/component_combat.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/nav_grid.h"
#include "../systems/pathfinding.h"
#include "../systems/route_corridor_planner.h"
#include "../util/planar_math.h"
#include "army_formation_planner.h"

namespace Game::Formation {

namespace {

constexpr float k_replan_interval_seconds = 0.5F;
constexpr float k_advance_interval_seconds = 0.25F;
constexpr float k_maintain_speed_multiplier = 0.55F;

constexpr float k_corridor_waypoint_tolerance = 1.25F;
constexpr float k_corridor_max_anchor_lead = 6.0F;
constexpr float k_corridor_min_leg_length = 1.5F;

constexpr float k_max_wheel_degrees_per_second = 60.0F;
constexpr float k_wheel_in_place_degrees = 0.5F;
constexpr float k_final_sidestep_metres = 3.0F;

struct GroupTraversal {
  Game::Systems::Pathfinding::Passability passability{
      Game::Systems::Pathfinding::Passability::Light};
  float clearance{0.0F};
};

auto group_traversal(Engine::Core::World& world,
                     const ArmyFormation& formation) -> GroupTraversal {
  GroupTraversal traversal;
  float widest = 0.0F;
  for (auto const member : formation.members) {
    const auto* movement = world.try_get<Engine::Core::MovementComponent>(member);
    if (movement == nullptr) {
      continue;
    }
    if (!movement->get_can_enter_forest()) {
      traversal.passability = Game::Systems::Pathfinding::Passability::Heavy;
    }
    widest = std::max(widest, movement->get_navigation_clearance());
  }
  traversal.clearance = Game::Systems::Pathfinding::routing_clearance(widest);
  return traversal;
}

auto build_corridor(const QVector3D& start,
                    const QVector3D& destination,
                    const GroupTraversal& traversal) -> std::vector<QVector3D> {
  std::vector<QVector3D> corridor;
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    corridor.push_back(destination);
    return corridor;
  }

  if (pathfinder->is_world_segment_walkable(
          start, destination, traversal.passability, traversal.clearance)) {
    return {destination};
  }

  auto const planned = Game::Systems::RouteCorridorPlanner::plan(
      *pathfinder, start, destination, traversal.passability, traversal.clearance);
  if (!planned.reachable()) {
    return corridor;
  }

  QVector3D previous = start;
  for (auto const& point : planned.centerline) {
    QVector3D const step(point.x() - previous.x(), 0.0F, point.z() - previous.z());
    if (step.length() < k_corridor_min_leg_length) {
      continue;
    }
    corridor.push_back(point);
    previous = point;
  }

  QVector3D const tail = corridor.empty() ? start : corridor.back();
  QVector3D const to_destination(
      destination.x() - tail.x(), 0.0F, destination.z() - tail.z());
  if (corridor.empty() || to_destination.length() > 0.1F) {
    corridor.push_back(destination);
  }
  return corridor;
}

constexpr float k_cohesion_interval_seconds = 0.35F;

constexpr float k_in_slot_radius_scale = 1.35F;

constexpr float k_formed_cohesion = 0.8F;
constexpr float k_disrupted_cohesion = 0.45F;
constexpr float k_opening_progress_spacing_scale = 1.5F;

constexpr float k_formed_damage_floor = 0.88F;
constexpr float k_disrupted_damage_penalty = 1.08F;

auto vector_to_json(const QVector3D& value) -> QJsonArray {
  QJsonArray array;
  array.append(static_cast<double>(value.x()));
  array.append(static_cast<double>(value.y()));
  array.append(static_cast<double>(value.z()));
  return array;
}

auto vector_from_json(const QJsonArray& array) -> QVector3D {
  if (array.size() < 3) {
    return {};
  }
  return {static_cast<float>(array.at(0).toDouble()),
          static_cast<float>(array.at(1).toDouble()),
          static_cast<float>(array.at(2).toDouble())};
}

auto slot_to_json(const FormationSlot& slot) -> QJsonObject {
  QJsonObject obj;
  obj["id"] = slot.id;
  obj["role"] = static_cast<int>(slot.role);
  obj["local"] = vector_to_json(slot.local_offset);
  obj["world"] = vector_to_json(slot.world_position);
  obj["facing"] = static_cast<double>(slot.facing);
  obj["rank"] = slot.rank;
  obj["file"] = slot.file;
  obj["status"] = static_cast<int>(slot.status);
  obj["occupant"] = static_cast<qint64>(slot.occupant);
  obj["half_width"] = static_cast<double>(slot.half_width);
  obj["half_depth"] = static_cast<double>(slot.half_depth);
  obj["heavy"] = slot.heavy;
  return obj;
}

auto slot_from_json(const QJsonObject& obj) -> FormationSlot {
  FormationSlot slot;
  slot.id = obj["id"].toInt(k_invalid_slot);
  slot.role = static_cast<ArmyRole>(obj["role"].toInt(0));
  slot.local_offset = vector_from_json(obj["local"].toArray());
  slot.world_position = vector_from_json(obj["world"].toArray());
  slot.facing = static_cast<float>(obj["facing"].toDouble(0.0));
  slot.rank = obj["rank"].toInt(0);
  slot.file = obj["file"].toInt(0);
  slot.status = static_cast<SlotStatus>(obj["status"].toInt(0));
  slot.occupant = static_cast<EntityID>(obj["occupant"].toVariant().toULongLong());
  slot.half_width = static_cast<float>(obj["half_width"].toDouble(0.5));
  slot.half_depth = static_cast<float>(obj["half_depth"].toDouble(0.5));
  slot.heavy = obj["heavy"].toBool(false);
  return slot;
}

auto options_to_json(const ArmyFormationOptions& options) -> QJsonObject {
  QJsonObject obj;
  obj["flank"] =
      QString::fromLatin1(flank_preference_to_string(options.flank_preference));
  obj["movement"] =
      QString::fromLatin1(movement_policy_to_string(options.movement_policy));
  obj["ranged"] =
      QString::fromLatin1(ranged_placement_to_string(options.ranged_placement));
  obj["mixed"] = QString::fromLatin1(mixed_policy_to_string(options.mixed_policy));
  obj["frontage_scale"] = static_cast<double>(options.frontage_scale);
  obj["depth_scale"] = static_cast<double>(options.depth_scale);
  obj["spacing_scale"] = static_cast<double>(options.spacing_scale);
  obj["reserve_rows"] = options.reserve_rows;
  obj["preserve_member_order"] = options.preserve_member_order;
  obj["doctrine_locked"] = options.doctrine_locked;
  return obj;
}

auto options_from_json(const QJsonObject& obj) -> ArmyFormationOptions {
  ArmyFormationOptions options;
  if (auto parsed = try_parse_flank_preference(obj["flank"].toString())) {
    options.flank_preference = *parsed;
  }
  if (auto parsed = try_parse_movement_policy(obj["movement"].toString())) {
    options.movement_policy = *parsed;
  }
  if (auto parsed = try_parse_ranged_placement(obj["ranged"].toString())) {
    options.ranged_placement = *parsed;
  }
  if (auto parsed = try_parse_mixed_policy(obj["mixed"].toString())) {
    options.mixed_policy = *parsed;
  }
  options.frontage_scale = static_cast<float>(obj["frontage_scale"].toDouble(1.0));
  options.depth_scale = static_cast<float>(obj["depth_scale"].toDouble(1.0));
  options.spacing_scale = static_cast<float>(obj["spacing_scale"].toDouble(1.0));
  options.reserve_rows = obj["reserve_rows"].toInt(-1);
  options.preserve_member_order = obj["preserve_member_order"].toBool(false);
  options.doctrine_locked = obj["doctrine_locked"].toBool(false);
  return options;
}

} // namespace

auto ArmyFormationRegistry::instance() -> ArmyFormationRegistry& {
  return *Game::Session::ambient_services().army_formations;
}

auto ArmyFormationRegistry::for_world(const Engine::Core::World& world)
    -> ArmyFormationRegistry& {
  return *Game::Session::services_for(world).army_formations;
}

auto ArmyFormationRegistry::create_group(FormationDoctrineId doctrine,
                                         ArmyFormationIntent intent,
                                         std::vector<EntityID> members)
    -> FormationGroupID {
  ArmyFormation formation;
  formation.id = m_next_id++;
  formation.doctrine = std::move(doctrine);
  formation.intent = intent;
  formation.members = std::move(members);
  formation.needs_replan = true;

  for (auto const member : formation.members) {
    auto existing = m_membership.find(member);
    if (existing != m_membership.end() && existing->second != formation.id) {
      auto group = m_groups.find(existing->second);
      if (group != m_groups.end()) {
        auto& list = group->second.members;
        list.erase(std::remove(list.begin(), list.end(), member), list.end());
        group->second.needs_replan = true;
      }
    }
    m_membership[member] = formation.id;
  }

  auto const id = formation.id;
  m_groups.emplace(id, std::move(formation));
  return id;
}

auto ArmyFormationRegistry::find(FormationGroupID id) -> ArmyFormation* {
  auto it = m_groups.find(id);
  return it == m_groups.end() ? nullptr : &it->second;
}

auto ArmyFormationRegistry::find(FormationGroupID id) const -> const ArmyFormation* {
  auto it = m_groups.find(id);
  return it == m_groups.end() ? nullptr : &it->second;
}

void ArmyFormationRegistry::remove_group(FormationGroupID id) {
  auto it = m_groups.find(id);
  if (it == m_groups.end()) {
    return;
  }
  for (auto const member : it->second.members) {
    auto membership = m_membership.find(member);
    if (membership != m_membership.end() && membership->second == id) {
      m_membership.erase(membership);
    }
  }
  m_groups.erase(it);
}

auto ArmyFormationRegistry::add_member(FormationGroupID id, EntityID entity) -> bool {
  auto* formation = find(id);
  if (formation == nullptr) {
    return false;
  }
  if (formation->has_member(entity)) {
    return false;
  }
  remove_member(entity);
  formation->members.push_back(entity);
  formation->needs_replan = true;
  m_membership[entity] = id;
  return true;
}

auto ArmyFormationRegistry::remove_member(EntityID entity) -> bool {
  auto membership = m_membership.find(entity);
  if (membership == m_membership.end()) {
    return false;
  }
  auto const group_id = membership->second;
  m_membership.erase(membership);

  auto* formation = find(group_id);
  if (formation == nullptr) {
    return false;
  }
  auto& list = formation->members;
  list.erase(std::remove(list.begin(), list.end(), entity), list.end());
  for (auto& slot : formation->slot_list) {
    if (slot.occupant == entity) {
      slot.occupant = 0U;
    }
  }
  formation->needs_replan = true;
  if (list.empty()) {
    m_groups.erase(group_id);
  }
  return true;
}

auto ArmyFormationRegistry::group_of(EntityID entity) const -> FormationGroupID {
  auto it = m_membership.find(entity);
  return it == m_membership.end() ? k_invalid_group : it->second;
}

void ArmyFormationRegistry::apply_plan(FormationGroupID id,
                                       const ArmyFormationPlan& plan) {
  auto* formation = find(id);
  if (formation == nullptr) {
    return;
  }
  formation->doctrine = plan.doctrine;
  formation->intent = plan.intent;
  formation->anchor = plan.anchor;
  formation->facing = plan.facing;
  formation->frontage = plan.frontage;
  formation->depth = plan.depth;
  formation->spacing = plan.spacing;
  formation->slot_spacing = plan.slot_spacing;
  formation->slot_list = plan.slot_list;
  formation->compressed = plan.narrowed;
  if (!plan.narrowed) {
    formation->reference_slots = plan.slot_list;
  }
  formation->needs_replan = false;
  ++formation->plan_revision;
  reindex_membership(*formation);
}

void ArmyFormationRegistry::reindex_membership(const ArmyFormation& formation) {
  for (auto const member : formation.members) {
    m_membership[member] = formation.id;
  }
}

auto ArmyFormationRegistry::group_ids() const -> std::vector<FormationGroupID> {
  std::vector<FormationGroupID> ids;
  ids.reserve(m_groups.size());
  for (const auto& entry : m_groups) {
    ids.push_back(entry.first);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

void ArmyFormationRegistry::clear() {
  m_groups.clear();
  m_membership.clear();
  m_next_id = 1U;
}

auto ArmyFormationRegistry::to_json() const -> QJsonObject {
  QJsonObject root;
  QJsonArray groups;
  for (auto const id : group_ids()) {
    const auto* formation = find(id);
    if (formation == nullptr) {
      continue;
    }
    QJsonObject obj;
    obj["id"] = static_cast<qint64>(formation->id);
    obj["doctrine"] = QString::fromStdString(formation->doctrine);
    obj["intent"] = QString::fromLatin1(intent_to_string(formation->intent));
    obj["anchor"] = vector_to_json(formation->anchor);
    obj["facing"] = static_cast<double>(formation->facing);
    obj["frontage"] = static_cast<double>(formation->frontage);
    obj["depth"] = static_cast<double>(formation->depth);
    obj["spacing"] = static_cast<double>(formation->spacing);
    obj["slot_spacing"] = static_cast<double>(formation->slot_spacing);
    obj["phase"] = static_cast<int>(formation->phase);
    obj["cohesion"] = static_cast<double>(formation->cohesion);
    obj["cohesion_pace"] = static_cast<double>(formation->cohesion_pace);
    obj["plan_revision"] = static_cast<qint64>(formation->plan_revision);
    obj["needs_replan"] = formation->needs_replan;
    obj["moves_pending"] = formation->moves_pending;
    obj["options"] = options_to_json(formation->options);
    obj["requested_frontage"] = static_cast<double>(formation->requested_frontage);
    obj["destination_facing"] = static_cast<double>(formation->destination_facing);
    obj["compressed"] = formation->compressed;

    QJsonArray members;
    for (auto const member : formation->members) {
      members.append(static_cast<qint64>(member));
    }
    obj["members"] = members;

    QJsonArray slot_list;
    for (const auto& slot : formation->slot_list) {
      slot_list.append(slot_to_json(slot));
    }
    obj["slot_list"] = slot_list;

    QJsonArray reference_slots;
    for (const auto& slot : formation->reference_slots) {
      reference_slots.append(slot_to_json(slot));
    }
    obj["reference_slots"] = reference_slots;

    groups.append(obj);
  }
  root["groups"] = groups;
  root["next_id"] = static_cast<qint64>(m_next_id);
  return root;
}

void ArmyFormationRegistry::from_json(const QJsonObject& root) {
  clear();
  auto const groups = root["groups"].toArray();
  for (const auto value : groups) {
    auto const obj = value.toObject();
    ArmyFormation formation;
    formation.id = static_cast<FormationGroupID>(obj["id"].toVariant().toULongLong());
    formation.doctrine = obj["doctrine"].toString().toStdString();
    if (auto parsed = try_parse_intent(obj["intent"].toString())) {
      formation.intent = *parsed;
    }
    formation.anchor = vector_from_json(obj["anchor"].toArray());
    formation.facing = static_cast<float>(obj["facing"].toDouble(0.0));
    formation.frontage = static_cast<float>(obj["frontage"].toDouble(0.0));
    formation.depth = static_cast<float>(obj["depth"].toDouble(0.0));
    formation.spacing = static_cast<float>(obj["spacing"].toDouble(1.0));

    formation.slot_spacing = static_cast<float>(
        obj["slot_spacing"].toDouble(static_cast<double>(formation.spacing)));
    formation.phase = static_cast<FormationPhase>(obj["phase"].toInt(0));
    formation.cohesion = static_cast<float>(obj["cohesion"].toDouble(1.0));
    formation.cohesion_pace = static_cast<float>(obj["cohesion_pace"].toDouble(0.0));
    formation.plan_revision =
        static_cast<std::uint32_t>(obj["plan_revision"].toVariant().toUInt());
    formation.needs_replan = obj["needs_replan"].toBool(false);
    formation.moves_pending = obj["moves_pending"].toBool(false);
    formation.options = options_from_json(obj["options"].toObject());
    formation.requested_frontage =
        static_cast<float>(obj["requested_frontage"].toDouble(0.0));
    formation.destination_facing = static_cast<float>(
        obj["destination_facing"].toDouble(static_cast<double>(formation.facing)));
    formation.compressed = obj["compressed"].toBool(false);

    for (const auto member : obj["members"].toArray()) {
      formation.members.push_back(
          static_cast<EntityID>(member.toVariant().toULongLong()));
    }
    for (const auto slot : obj["slot_list"].toArray()) {
      formation.slot_list.push_back(slot_from_json(slot.toObject()));
    }
    for (const auto slot : obj["reference_slots"].toArray()) {
      formation.reference_slots.push_back(slot_from_json(slot.toObject()));
    }

    if (formation.id == k_invalid_group) {
      continue;
    }
    auto const id = formation.id;
    m_groups.emplace(id, std::move(formation));
    reindex_membership(m_groups.at(id));
  }
  m_next_id = static_cast<FormationGroupID>(root["next_id"].toVariant().toULongLong());
  if (m_next_id == 0U) {
    m_next_id = 1U;
    for (const auto& entry : m_groups) {
      m_next_id = std::max(m_next_id, entry.first + 1U);
    }
  }
}

namespace {

constexpr float k_morph_pace_share = 0.85F;
constexpr float k_morph_about_face_degrees = 135.0F;
constexpr int k_morph_samples = 12;

auto rotate_yaw(const QVector3D& local, float yaw_degrees) -> QVector3D {
  float const yaw = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  float const s = std::sin(yaw);
  float const c = std::cos(yaw);
  return {local.x() * c + local.z() * s, 0.0F, -local.x() * s + local.z() * c};
}

auto morph_point(const FormationMorph& morph, std::size_t index, float t) -> QVector3D {
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

void keep_places_in_shape(Engine::Core::World& world,
                          ArmyFormation& formation,
                          float facing_from,
                          float facing_to) {
  std::vector<std::size_t> held;
  QVector3D troop_centre;
  QVector3D slot_centre;
  for (std::size_t i = 0; i < formation.slot_list.size(); ++i) {
    const auto& slot = formation.slot_list[i];
    const auto* transform =
        world.try_get<Engine::Core::TransformComponent>(slot.occupant);
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked ||
        transform == nullptr) {
      continue;
    }
    held.push_back(i);
    troop_centre += QVector3D(transform->position.x, 0.0F, transform->position.z);
    slot_centre += QVector3D(slot.world_position.x(), 0.0F, slot.world_position.z());
  }
  if (held.size() < 2U) {
    return;
  }
  troop_centre /= static_cast<float>(held.size());
  slot_centre /= static_cast<float>(held.size());

  auto kind_of = [&](EntityID id) -> std::uint64_t {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    const auto* movement = world.try_get<Engine::Core::MovementComponent>(id);
    std::uint64_t kind =
        unit != nullptr ? static_cast<std::uint64_t>(unit->spawn_type) : 0U;
    kind = (kind << 1U) |
           ((movement != nullptr && !movement->get_can_enter_forest()) ? 1U : 0U);
    return kind;
  };

  std::vector<bool> done(held.size(), false);
  std::vector<EntityID> occupants(formation.slot_list.size(), 0U);
  for (std::size_t first = 0; first < held.size(); ++first) {
    if (done[first]) {
      continue;
    }
    auto const kind = kind_of(formation.slot_list[held[first]].occupant);
    std::vector<std::size_t> bucket;
    for (std::size_t k = first; k < held.size(); ++k) {
      if (!done[k] && kind_of(formation.slot_list[held[k]].occupant) == kind) {
        done[k] = true;
        bucket.push_back(held[k]);
      }
    }
    std::vector<std::vector<float>> cost(bucket.size(),
                                         std::vector<float>(bucket.size()));
    for (std::size_t r = 0; r < bucket.size(); ++r) {
      const auto* transform = world.try_get<Engine::Core::TransformComponent>(
          formation.slot_list[bucket[r]].occupant);
      QVector3D const place = rotate_yaw(
          QVector3D(transform->position.x, 0.0F, transform->position.z) - troop_centre,
          -facing_from);
      for (std::size_t c = 0; c < bucket.size(); ++c) {
        auto const& target = formation.slot_list[bucket[c]].world_position;
        QVector3D const shape_place = rotate_yaw(
            QVector3D(target.x(), 0.0F, target.z()) - slot_centre, -facing_to);
        cost[r][c] = (place - shape_place).length();
      }
    }
    auto const chosen = ArmyFormationPlanner::min_cost_assignment(cost);
    for (std::size_t r = 0; r < bucket.size(); ++r) {
      auto const column = chosen[r] >= 0 ? static_cast<std::size_t>(chosen[r]) : r;
      occupants[bucket[column]] = formation.slot_list[bucket[r]].occupant;
    }
  }
  for (auto const index : held) {
    formation.slot_list[index].occupant = occupants[index];
  }
  ArmyFormationRuntime::sync_membership_components(world, formation);
}

auto start_morph(Engine::Core::World& world,
                 ArmyFormation& formation,
                 std::optional<float> marching_facing) -> bool {
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
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

  std::vector<QVector3D> starts;
  std::vector<float> speeds;
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
    starts.emplace_back(transform->position.x, 0.0F, transform->position.z);
    speeds.push_back(unit->speed);
  }
  if (morph.occupants.empty()) {
    return false;
  }

  QVector3D anchor_from;
  for (std::size_t i = 0; i < starts.size(); ++i) {
    anchor_from += starts[i] - rotate_yaw(morph.local_to[i], morph.facing_from);
  }
  anchor_from /= static_cast<float>(starts.size());
  anchor_from.setY(morph.anchor_to.y());
  morph.anchor_from = anchor_from;
  for (auto const& start : starts) {
    morph.local_from.push_back(rotate_yaw(start - anchor_from, -morph.facing_from));
  }

  float duration = 0.5F;
  for (std::size_t i = 0; i < morph.occupants.size(); ++i) {
    const auto* movement =
        world.try_get<Engine::Core::MovementComponent>(morph.occupants[i]);
    auto const passability = movement != nullptr && !movement->get_can_enter_forest()
                                 ? Game::Systems::Pathfinding::Passability::Heavy
                                 : Game::Systems::Pathfinding::Passability::Light;
    float const clearance =
        movement != nullptr ? movement->get_navigation_clearance() : 0.0F;
    float length = 0.0F;
    QVector3D previous = morph_point(morph, i, 0.0F);
    for (int k = 1; k <= k_morph_samples; ++k) {
      QVector3D const next = morph_point(
          morph, i, static_cast<float>(k) / static_cast<float>(k_morph_samples));
      if (pathfinder != nullptr && !pathfinder->is_world_segment_walkable(
                                       previous, next, passability, clearance)) {
        return false;
      }
      length +=
          QVector3D(next.x() - previous.x(), 0.0F, next.z() - previous.z()).length();
      previous = next;
    }
    duration = std::max(duration, length / (speeds[i] * k_morph_pace_share));
    morph.path_speed.push_back(length);
  }
  for (auto& speed : morph.path_speed) {
    speed /= duration;
  }
  constexpr float k_rigid_tolerance = 0.75F;
  morph.rigid = true;
  for (std::size_t i = 0; i < morph.local_from.size(); ++i) {
    if ((morph.local_from[i] - morph.local_to[i]).length() > k_rigid_tolerance) {
      morph.rigid = false;
      break;
    }
  }
  morph.duration = duration;
  morph.active = true;
  formation.morph = std::move(morph);
  formation.anchor = formation.morph.anchor_from;
  formation.facing = formation.morph.facing_from;
  for (auto& slot : formation.slot_list) {
    auto const found = std::find(formation.morph.occupants.begin(),
                                 formation.morph.occupants.end(),
                                 slot.occupant);
    if (found == formation.morph.occupants.end()) {
      continue;
    }
    auto const index = static_cast<std::size_t>(
        std::distance(formation.morph.occupants.begin(), found));
    slot.world_position = morph_point(formation.morph, index, 0.0F);
    slot.facing = formation.facing;
  }
  return true;
}

auto facing_settled(const ArmyFormation& formation) -> bool {
  constexpr float k_settled_degrees = 0.5F;
  return !formation.maintains_formation() ||
         std::abs(Game::Systems::signed_yaw_delta(
             formation.facing, formation.destination_facing)) <= k_settled_degrees;
}

void hold_group_facing(Engine::Core::World& world, ArmyFormation& formation) {
  if (!formation.is_formed()) {
    return;
  }
  constexpr float k_tolerance_degrees = 4.0F;
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked) {
      continue;
    }
    const auto* movement =
        world.try_get<Engine::Core::MovementComponent>(slot.occupant);
    if (movement != nullptr && movement->get_has_target()) {
      continue;
    }
    const auto* attack = world.try_get<Engine::Core::AttackComponent>(slot.occupant);
    if (attack != nullptr && attack->in_melee_lock) {
      continue;
    }
    auto* transform = world.try_get<Engine::Core::TransformComponent>(slot.occupant);
    if (transform == nullptr || transform->has_desired_yaw) {
      continue;
    }
    float const drift =
        std::abs(Game::Systems::signed_yaw_delta(slot.facing, transform->rotation.y));
    if (drift <= k_tolerance_degrees) {
      continue;
    }
    transform->desired_yaw = slot.facing;
    transform->has_desired_yaw = true;
  }
}

} // namespace

void ArmyFormationRuntime::refresh_shape_state(Engine::Core::World& world,
                                               ArmyFormation& formation) {
  float const radius = formation.spacing * k_in_slot_radius_scale;
  float const radius_sq = radius * radius;

  int expected = 0;
  int observed = 0;
  int in_slot = 0;
  bool all_facing_aligned = true;
  bool controlled_break = false;
  float slowest_speed = std::numeric_limits<float>::max();
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked) {
      continue;
    }
    ++expected;
    auto* entity = world.get_entity(slot.occupant);
    if (entity == nullptr) {
      continue;
    }
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->speed > 0.0F) {
      slowest_speed = std::min(slowest_speed, unit->speed);
    }
    const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    controlled_break =
        controlled_break || (movement != nullptr && movement->get_has_target() &&
                             movement->get_route_lane_scale() < 0.99F);
    ++observed;
    float const off_x = transform->position.x - slot.world_position.x();
    float const off_z = transform->position.z - slot.world_position.z();
    if ((off_x * off_x) + (off_z * off_z) <= radius_sq) {
      ++in_slot;
    }
    all_facing_aligned = all_facing_aligned &&
                         std::abs(Game::Systems::signed_yaw_delta(transform->rotation.y,
                                                                  slot.facing)) <= 4.0F;
  }

  formation.cohesion_pace =
      formation.maintains_formation() && std::isfinite(slowest_speed)
          ? slowest_speed * k_maintain_speed_multiplier
          : 0.0F;
  if (expected == 0 || observed == 0) {
    formation.cohesion = 0.0F;
    formation.phase = FormationPhase::Disrupted;
    return;
  }

  formation.cohesion = static_cast<float>(in_slot) / static_cast<float>(expected);
  bool const all_in_slot = in_slot == expected;

  if (formation.cohesion <= k_disrupted_cohesion) {
    formation.phase = FormationPhase::Disrupted;
    return;
  }

  if (formation.morph.active) {
    formation.phase = formation.cohesion >= k_formed_cohesion && formation.morph.rigid
                          ? FormationPhase::Traversing
                          : FormationPhase::Reforming;
    return;
  }

  if (formation.move_plan.has_corridor()) {
    if (formation.compressed) {
      formation.phase = FormationPhase::Opening;
      return;
    }
    float const opening_distance =
        std::max(formation.spacing, 0.1F) * k_opening_progress_spacing_scale;
    bool const still_opening = controlled_break ||
                               formation.advance_progress < opening_distance ||
                               !all_in_slot || !all_facing_aligned;
    formation.phase =
        still_opening ? FormationPhase::Opening : FormationPhase::Traversing;
    return;
  }

  if (formation.has_destination) {
    if (all_in_slot && all_facing_aligned && facing_settled(formation) &&
        !formation.morph.active) {
      formation.has_destination = false;
      formation.phase = FormationPhase::Arrived;
    } else {
      formation.phase = FormationPhase::Reforming;
    }
    return;
  }

  formation.phase =
      all_in_slot && formation.phase == FormationPhase::Arrived
          ? FormationPhase::Arrived
          : (all_in_slot ? FormationPhase::Formed : FormationPhase::Reforming);
  hold_group_facing(world, formation);
}

auto ArmyFormationRuntime::damage_taken_multiplier(const Engine::Core::Entity& entity)
    -> float {
  const auto* membership =
      entity.get_component<Engine::Core::ArmyFormationMembershipComponent>();
  if (membership == nullptr || !membership->is_valid()) {
    return 1.0F;
  }
  const auto* formation = ArmyFormationRegistry::instance().find(membership->group_id);
  if (formation == nullptr) {
    return 1.0F;
  }

  if (formation->phase == FormationPhase::Disrupted) {
    return k_disrupted_damage_penalty;
  }
  if (!formation->is_formed()) {
    return 1.0F;
  }

  float const span = 1.0F - k_formed_cohesion;
  float const t =
      span <= 0.0F
          ? 1.0F
          : std::clamp((formation->cohesion - k_formed_cohesion) / span, 0.0F, 1.0F);
  return 1.0F + (k_formed_damage_floor - 1.0F) * t;
}

auto ArmyFormationRuntime::move_speed_multiplier(const Engine::Core::Entity& entity)
    -> float {
  const auto* membership =
      entity.get_component<Engine::Core::ArmyFormationMembershipComponent>();
  if (membership == nullptr || !membership->is_valid()) {
    return 1.0F;
  }
  const auto* formation = ArmyFormationRegistry::instance().find(membership->group_id);
  if (formation == nullptr || !formation->maintains_formation() ||
      formation->morph.active || !formation->move_plan.active) {
    return 1.0F;
  }
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  const auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr || unit->speed <= 0.0F) {
    return k_maintain_speed_multiplier;
  }

  float pace = formation->cohesion_pace;
  if (pace <= 0.0F) {
    pace = unit->speed * k_maintain_speed_multiplier;
  }
  QVector3D const position(
      transform->position.x, transform->position.y, transform->position.z);
  float const error = formation->slot_error(position, entity.get_id());
  float const in_slot_radius = formation->spacing * k_in_slot_radius_scale;
  float const recovery_span = std::max(formation->spacing * 4.0F, 0.1F);
  float const recovery =
      error < 0.0F ? 0.0F
                   : std::clamp((error - in_slot_radius) / recovery_span, 0.0F, 0.25F);
  float const target_speed = pace * (1.0F + recovery);

  if (!std::isfinite(target_speed)) {
    return k_maintain_speed_multiplier;
  }
  return std::clamp(target_speed / unit->speed, 0.1F, 1.0F);
}

void ArmyFormationRuntime::begin_move(Engine::Core::World& world,
                                      FormationGroupID id,
                                      const QVector3D& destination,
                                      float facing,
                                      std::optional<float> marching_facing,
                                      bool allow_morph) {
  auto& registry = ArmyFormationRegistry::instance();
  auto* formation = registry.find(id);
  if (formation == nullptr) {
    return;
  }

  formation->destination = destination;
  formation->destination_facing = facing;
  formation->has_destination = true;
  formation->facing = facing;
  formation->advance_progress = 0.0F;
  formation->move_plan.clear();
  formation->morph.clear();

  if (formation->maintains_formation() && allow_morph &&
      start_morph(world, *formation, marching_facing)) {
    formation->needs_replan = false;
    formation->moves_pending = true;
    refresh_shape_state(world, *formation);
    return;
  }

  if (!formation->maintains_formation() || allow_morph) {
    formation->anchor = destination;
    formation->needs_replan = false;
    refresh_shape_state(world, *formation);
    return;
  }

  QVector3D centroid;
  int count = 0;
  for (auto const member : formation->members) {
    auto* entity = world.get_entity(member);
    if (entity == nullptr) {
      continue;
    }
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    centroid +=
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    if (marching_facing.has_value()) {
      if (const auto* slot = formation->find_slot_for(member)) {
        float const radians = *marching_facing * std::numbers::pi_v<float> / 180.0F;
        const auto& offset = slot->local_offset;
        centroid -=
            QVector3D(offset.x() * std::cos(radians) + offset.z() * std::sin(radians),
                      0.0F,
                      -offset.x() * std::sin(radians) + offset.z() * std::cos(radians));
      }
    }
    ++count;
  }
  if (count > 0) {
    formation->anchor = centroid / static_cast<float>(count);
  }

  formation->move_plan.corridor = build_corridor(
      formation->anchor, destination, group_traversal(world, *formation));
  formation->move_plan.corridor_index = 0;
  formation->move_plan.formation_center = formation->anchor;
  formation->move_plan.active = !formation->move_plan.corridor.empty();
  {
    QVector3D heading = formation->move_plan.next_waypoint() - formation->anchor;
    heading.setY(0.0F);
    if (heading.lengthSquared() > 1.0e-4F) {
      formation->move_plan.facing_direction = heading.normalized();
    }

    if (marching_facing.has_value()) {
      formation->facing = *marching_facing;
    } else if (heading.lengthSquared() > 1.0e-4F) {
      formation->facing =
          Game::Systems::yaw_degrees_from_direction(heading.x(), heading.z());
    }
  }

  formation->needs_replan = true;
  static_cast<void>(replan(world, id));
  refresh_shape_state(world, *formation);
}

void ArmyFormationRuntime::advance_morphs(Engine::Core::World& world,
                                          float delta_time) {
  auto& registry = ArmyFormationRegistry::for_world(world);
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !formation->morph.active) {
      continue;
    }
    auto& morph = formation->morph;
    morph.elapsed += delta_time;
    float const t = morph.duration > 0.0F
                        ? std::clamp(morph.elapsed / morph.duration, 0.0F, 1.0F)
                        : 1.0F;
    formation->anchor = morph.anchor_from + (morph.anchor_to - morph.anchor_from) * t;
    formation->facing =
        morph.facing_from +
        Game::Systems::signed_yaw_delta(morph.facing_from, morph.facing_to) * t;
    for (auto& slot : formation->slot_list) {
      auto const found =
          std::find(morph.occupants.begin(), morph.occupants.end(), slot.occupant);
      if (found == morph.occupants.end()) {
        continue;
      }
      auto const index =
          static_cast<std::size_t>(std::distance(morph.occupants.begin(), found));
      slot.world_position =
          t >= 1.0F ? morph.world_to[index] : morph_point(morph, index, t);
      slot.facing = formation->facing;
    }
    formation->moves_pending = true;
    if (t >= 1.0F) {
      formation->anchor = morph.anchor_to;
      formation->facing = morph.facing_to;
      morph.active = false;
    }
  }
}

void ArmyFormationRuntime::advance_maintained_groups(Engine::Core::World& world,
                                                     float delta_time) {
  auto& registry = ArmyFormationRegistry::instance();

  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !formation->has_destination ||
        !formation->maintains_formation() || formation->morph.active ||
        !formation->move_plan.active || formation->members.empty()) {
      continue;
    }

    QVector3D centroid;
    int count = 0;
    for (auto const member : formation->members) {
      auto* entity = world.get_entity(member);
      if (entity == nullptr) {
        continue;
      }
      const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (transform == nullptr) {
        continue;
      }
      centroid += QVector3D(
          transform->position.x, transform->position.y, transform->position.z);
      if (const auto* slot = formation->find_slot_for(member)) {

        centroid -= slot->world_position - formation->anchor;
      }
      ++count;
    }
    if (count == 0) {
      continue;
    }
    centroid /= static_cast<float>(count);

    float const declared_pace = formation->cohesion_pace > 0.0F
                                    ? formation->cohesion_pace
                                    : k_maintain_speed_multiplier;

    float half_span = std::max(formation->spacing, 0.5F);
    float max_slot_error = 0.0F;
    for (const auto& slot : formation->slot_list) {
      if (slot.occupant == 0U || slot.status == SlotStatus::Blocked) {
        continue;
      }
      half_span = std::max(half_span,
                           slot.local_offset.length() +
                               std::hypot(slot.half_width, slot.half_depth));
      if (const auto* transform =
              world.try_get<Engine::Core::TransformComponent>(slot.occupant)) {
        max_slot_error =
            std::max(max_slot_error,
                     std::hypot(transform->position.x - slot.world_position.x(),
                                transform->position.z - slot.world_position.z()));
      }
    }

    if (!formation->compressed && max_slot_error > 1.0F) {
      formation->moves_pending = true;
      continue;
    }
    float const wheel_step = std::clamp(declared_pace / std::max(half_span, 0.5F) *
                                            180.0F / std::numbers::pi_v<float>,
                                        0.0F,
                                        k_max_wheel_degrees_per_second) *
                             delta_time;

    auto& plan = formation->move_plan;
    if (!plan.active) {
      plan.corridor = build_corridor(formation->anchor,
                                     formation->destination,
                                     group_traversal(world, *formation));
      plan.corridor_index = 0;
      plan.active = !plan.corridor.empty();
    }
    plan.formation_center = centroid;

    QVector3D const anchor_lead(formation->anchor.x() - centroid.x(),
                                0.0F,
                                formation->anchor.z() - centroid.z());
    if (anchor_lead.length() > k_corridor_max_anchor_lead) {
      continue;
    }

    while (plan.has_corridor()) {
      QVector3D const waypoint = plan.next_waypoint();
      QVector3D const to_waypoint(waypoint.x() - formation->anchor.x(),
                                  0.0F,
                                  waypoint.z() - formation->anchor.z());
      float const tolerance = plan.corridor_index + 1U == plan.corridor.size()
                                  ? 0.05F
                                  : k_corridor_waypoint_tolerance;
      if (to_waypoint.length() > tolerance) {
        break;
      }
      ++plan.corridor_index;
    }

    if (!plan.has_corridor()) {
      QVector3D const to_destination(formation->destination.x() - formation->anchor.x(),
                                     0.0F,
                                     formation->destination.z() -
                                         formation->anchor.z());
      if (to_destination.length() <= 0.05F) {
        plan.clear();
        if (!facing_settled(*formation)) {
          formation->facing = Game::Systems::turn_yaw_toward(
              formation->facing, formation->destination_facing, wheel_step);
          formation->needs_replan = true;
        }
        continue;
      }
      plan.corridor.push_back(formation->destination);
    }

    QVector3D heading = plan.next_waypoint() - formation->anchor;
    heading.setY(0.0F);
    float const leg = heading.length();
    if (leg <= 1.0e-4F) {
      continue;
    }
    heading /= leg;
    plan.facing_direction = heading;

    bool const final_step = plan.corridor_index + 1U >= plan.corridor.size() &&
                            leg <= k_final_sidestep_metres;
    float const target_facing =
        final_step
            ? formation->destination_facing
            : Game::Systems::yaw_degrees_from_direction(heading.x(), heading.z());
    formation->facing =
        Game::Systems::turn_yaw_toward(formation->facing, target_facing, wheel_step);
    formation->needs_replan = true;
    if (std::abs(Game::Systems::signed_yaw_delta(formation->facing, target_facing)) >
        k_wheel_in_place_degrees) {
      continue;
    }

    float const step = std::min(leg, std::max(0.05F, declared_pace * delta_time));
    formation->anchor += heading * step;
    formation->advance_progress += step;
  }
}

auto ArmyFormationRuntime::morph_target(const ArmyFormation& formation,
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
  return t >= 1.0F ? morph.world_to[index] : morph_point(morph, index, t);
}

auto ArmyFormationRuntime::morph_pace(const ArmyFormation& formation,
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
  float const pace = morph.path_speed[index] * (1.0F + k_catch_up_per_metre * behind);
  return std::clamp(pace, 0.1F, std::max(0.1F, full_speed));
}

auto ArmyFormationRuntime::reference_matches_members(const ArmyFormation& formation)
    -> bool {
  if (formation.reference_slots.empty()) {
    return false;
  }
  std::vector<EntityID> occupants;
  occupants.reserve(formation.reference_slots.size());
  for (const auto& slot : formation.reference_slots) {
    if (slot.occupant != 0U) {
      occupants.push_back(slot.occupant);
    }
  }
  std::vector<EntityID> members = formation.members;
  std::sort(occupants.begin(), occupants.end());
  std::sort(members.begin(), members.end());
  return occupants == members;
}

void ArmyFormationRuntime::sync_membership_components(Engine::Core::World& world,
                                                      const ArmyFormation& formation) {
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U) {
      continue;
    }
    auto* entity = world.get_entity(slot.occupant);
    if (entity == nullptr) {
      continue;
    }
    auto* membership = Engine::Core::get_or_add_component<
        Engine::Core::ArmyFormationMembershipComponent>(entity);
    if (membership == nullptr) {
      continue;
    }
    membership->group_id = formation.id;
    membership->slot_id = slot.id;
  }
}

void ArmyFormationRuntime::detach(Engine::Core::World& world, EntityID entity) {
  ArmyFormationRegistry::instance().remove_member(entity);
  auto* target = world.get_entity(entity);
  if (target == nullptr) {
    return;
  }
  auto* membership =
      target->get_component<Engine::Core::ArmyFormationMembershipComponent>();
  if (membership != nullptr) {
    membership->group_id = 0U;
    membership->slot_id = k_invalid_slot;
  }
}

auto ArmyFormationRuntime::replan(Engine::Core::World& world,
                                  FormationGroupID id) -> bool {
  auto& registry = ArmyFormationRegistry::instance();
  auto* formation = registry.find(id);
  if (formation == nullptr || formation->members.empty()) {
    return false;
  }

  ArmyFormationRequest request;
  request.members = formation->members;
  request.anchor = formation->anchor;
  request.facing = formation->facing;
  request.frontage = formation->requested_frontage;
  request.intent = formation->intent;
  request.doctrine = formation->doctrine;
  request.options = formation->options;
  request.spacing = formation->spacing;
  request.group_id = id;

  request.preserve_previous_slots = true;

  bool const advancing_along_corridor = formation->maintains_formation() &&
                                        formation->has_destination &&
                                        formation->move_plan.active;
  request.allow_anchor_shift = !advancing_along_corridor;
  QVector3D const advancing_anchor = formation->anchor;

  ArmyFormationPlan plan;
  bool reused_reference = false;
  if (reference_matches_members(*formation)) {
    plan = ArmyFormationPlanner::place(
        ArmyFormationPlanner::layout_from_reference(*formation), request);
    reused_reference = plan.keeps_shape();
  }
  if (!reused_reference) {
    plan = ArmyFormationPlanner::plan(world, request);
    if (plan.valid && plan.narrowed && reference_matches_members(*formation)) {
      ArmyFormationPlanner::fold_onto_reference(plan, formation->reference_slots);
    }
  }
  if (!plan.valid) {

    formation->needs_replan = false;
    refresh_shape_state(world, *formation);
    return false;
  }

  registry.apply_plan(id, plan);
  if (advancing_along_corridor) {
    if (auto* advanced = registry.find(id)) {
      advanced->anchor = advancing_anchor;
    }
  }
  auto const* updated = registry.find(id);
  if (updated == nullptr) {
    return true;
  }
  sync_membership_components(world, *updated);

  if (updated->maintains_formation() && updated->has_destination) {
    if (auto* pending = registry.find(id)) {
      pending->moves_pending = true;
    }
  }
  return true;
}

void ArmyFormationRuntime::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto& registry = ArmyFormationRegistry::instance();

  advance_morphs(*world, delta_time);

  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr) {
      continue;
    }
    std::vector<EntityID> dead;
    for (auto const member : formation->members) {
      auto* entity = world->get_entity(member);
      if (entity == nullptr) {
        dead.push_back(member);
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr || unit->health <= 0) {
        dead.push_back(member);
      }
    }
    for (auto const member : dead) {
      registry.remove_member(member);
    }
  }

  m_advance_accumulator += delta_time;
  if (m_advance_accumulator >= k_advance_interval_seconds) {
    float const elapsed = m_advance_accumulator;
    m_advance_accumulator = 0.0F;
    advance_maintained_groups(*world, elapsed);

    for (auto const id : registry.group_ids()) {
      auto* formation = registry.find(id);
      if (formation == nullptr || !formation->needs_replan ||
          !formation->has_destination || !formation->maintains_formation()) {
        continue;
      }
      static_cast<void>(replan(*world, id));
    }
  }

  m_cohesion_accumulator += delta_time;
  if (m_cohesion_accumulator >= k_cohesion_interval_seconds) {
    m_cohesion_accumulator = 0.0F;
    for (auto const id : registry.group_ids()) {
      auto* formation = registry.find(id);
      if (formation == nullptr) {
        continue;
      }
      refresh_shape_state(*world, *formation);
    }
  }

  m_replan_accumulator += delta_time;
  if (m_replan_accumulator < k_replan_interval_seconds) {
    return;
  }
  m_replan_accumulator = 0.0F;

  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !formation->needs_replan) {
      continue;
    }
    static_cast<void>(replan(*world, id));
  }
}

auto ArmyFormationRuntime::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent, TransformComponent>{},
      Writes<ArmyFormationMembershipComponent, MovementComponent, AttackComponent>{});
}

} // namespace Game::Formation
