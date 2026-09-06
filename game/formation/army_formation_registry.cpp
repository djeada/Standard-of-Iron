#include "army_formation_registry.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "../core/ambient_session.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/nav_grid.h"
#include "../systems/pathfinding.h"
#include "../systems/route_corridor_planner.h"
#include "army_formation_planner.h"

namespace Game::Formation {

namespace {

constexpr float k_replan_interval_seconds = 0.5F;
constexpr float k_advance_interval_seconds = 0.25F;
constexpr float k_maintain_speed_multiplier = 0.55F;
constexpr float k_stage_arrival_tolerance = 2.5F;

constexpr float k_corridor_waypoint_tolerance = 1.25F;
constexpr float k_corridor_max_anchor_lead = 6.0F;
constexpr float k_corridor_min_leg_length = 1.5F;

auto build_corridor(const QVector3D& start,
                    const QVector3D& destination) -> std::vector<QVector3D> {
  std::vector<QVector3D> corridor;
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    corridor.push_back(destination);
    return corridor;
  }

  auto const planned = Game::Systems::RouteCorridorPlanner::plan(
      *pathfinder,
      start,
      destination,
      Game::Systems::Pathfinding::Passability::Light,
      0.0F);
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
  formation->slot_list = plan.slot_list;
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
    obj["phase"] = static_cast<int>(formation->phase);
    obj["cohesion"] = static_cast<double>(formation->cohesion);
    obj["cohesion_pace"] = static_cast<double>(formation->cohesion_pace);
    obj["plan_revision"] = static_cast<qint64>(formation->plan_revision);
    obj["needs_replan"] = formation->needs_replan;
    obj["moves_pending"] = formation->moves_pending;
    obj["options"] = options_to_json(formation->options);

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
    formation.phase = static_cast<FormationPhase>(obj["phase"].toInt(0));
    formation.cohesion = static_cast<float>(obj["cohesion"].toDouble(1.0));
    formation.cohesion_pace = static_cast<float>(obj["cohesion_pace"].toDouble(0.0));
    formation.plan_revision =
        static_cast<std::uint32_t>(obj["plan_revision"].toVariant().toUInt());
    formation.needs_replan = obj["needs_replan"].toBool(false);
    formation.moves_pending = obj["moves_pending"].toBool(false);
    formation.options = options_from_json(obj["options"].toObject());

    for (const auto member : obj["members"].toArray()) {
      formation.members.push_back(
          static_cast<EntityID>(member.toVariant().toULongLong()));
    }
    for (const auto slot : obj["slot_list"].toArray()) {
      formation.slot_list.push_back(slot_from_json(slot.toObject()));
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

void ArmyFormationRuntime::refresh_shape_state(Engine::Core::World& world,
                                               ArmyFormation& formation) {
  float const radius = formation.spacing * k_in_slot_radius_scale;
  float const radius_sq = radius * radius;

  int expected = 0;
  int observed = 0;
  int in_slot = 0;
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

  if (formation.move_plan.has_corridor()) {
    float const opening_distance =
        std::max(formation.spacing, 0.1F) * k_opening_progress_spacing_scale;
    bool const still_opening = controlled_break ||
                               formation.advance_progress < opening_distance ||
                               formation.cohesion < k_formed_cohesion;
    formation.phase =
        still_opening ? FormationPhase::Opening : FormationPhase::Traversing;
    return;
  }

  if (formation.has_destination) {
    if (all_in_slot) {
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
  if (formation == nullptr || !formation->maintains_formation()) {
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
  return std::clamp(target_speed / unit->speed, 0.1F, 1.0F);
}

void ArmyFormationRuntime::begin_move(Engine::Core::World& world,
                                      FormationGroupID id,
                                      const QVector3D& destination,
                                      float facing) {
  auto& registry = ArmyFormationRegistry::instance();
  auto* formation = registry.find(id);
  if (formation == nullptr) {
    return;
  }

  formation->destination = destination;
  formation->has_destination = true;
  formation->facing = facing;
  formation->advance_progress = 0.0F;
  formation->move_plan.clear();

  if (!formation->maintains_formation()) {
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
    ++count;
  }
  if (count > 0) {
    formation->anchor = centroid / static_cast<float>(count);
  }

  formation->move_plan.corridor = build_corridor(formation->anchor, destination);
  formation->move_plan.corridor_index = 0;
  formation->move_plan.formation_center = formation->anchor;
  formation->move_plan.active = !formation->move_plan.corridor.empty();
  {
    QVector3D heading = formation->move_plan.next_waypoint() - formation->anchor;
    heading.setY(0.0F);
    if (heading.lengthSquared() > 1.0e-4F) {
      formation->move_plan.facing_direction = heading.normalized();
    }
  }

  formation->needs_replan = true;
  static_cast<void>(replan(world, id));
  refresh_shape_state(world, *formation);
}

void ArmyFormationRuntime::advance_maintained_groups(Engine::Core::World& world,
                                                     float delta_time) {
  auto& registry = ArmyFormationRegistry::instance();

  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !formation->has_destination ||
        !formation->maintains_formation() || formation->members.empty()) {
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
      ++count;
    }
    if (count == 0) {
      continue;
    }
    centroid /= static_cast<float>(count);

    auto& plan = formation->move_plan;
    if (!plan.active) {
      plan.corridor = build_corridor(formation->anchor, formation->destination);
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
      if (to_waypoint.length() > k_corridor_waypoint_tolerance) {
        break;
      }
      ++plan.corridor_index;
    }

    if (!plan.has_corridor()) {
      QVector3D const to_destination(formation->destination.x() - formation->anchor.x(),
                                     0.0F,
                                     formation->destination.z() -
                                         formation->anchor.z());
      if (to_destination.length() <= k_stage_arrival_tolerance * 0.5F) {
        plan.clear();
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

    float const declared_pace = formation->cohesion_pace > 0.0F
                                    ? formation->cohesion_pace
                                    : k_maintain_speed_multiplier;
    float const step = std::min(leg, std::max(0.05F, declared_pace * delta_time));
    formation->anchor += heading * step;
    formation->facing = static_cast<float>(
        std::atan2(static_cast<double>(heading.x()), static_cast<double>(heading.z())) *
        180.0 / std::numbers::pi);
    formation->advance_progress += step;
    formation->needs_replan = true;
  }
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
  request.frontage = formation->frontage;
  request.intent = formation->intent;
  request.doctrine = formation->doctrine;
  request.options = formation->options;
  request.spacing = formation->spacing;
  request.group_id = id;

  request.preserve_previous_slots = true;

  bool const advancing_along_corridor = formation->maintains_formation() &&
                                        formation->has_destination &&
                                        formation->move_plan.active;
  QVector3D const advancing_anchor = formation->anchor;

  auto const plan = ArmyFormationPlanner::plan(world, request);
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
