#include "ai_formation.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <unordered_map>

#include "../../formation/army_formation_planner.h"
#include "../../formation/unit_layout_resolver.h"
#include "../nation_registry.h"
#include "ai_doctrine_catalog.h"

namespace Game::Systems::AI {

namespace {

auto formation_ai_logger() -> QLoggingCategory& {
  static QLoggingCategory category("soi.ai.formation", QtWarningMsg);
  return category;
}

auto build_members(const std::vector<const EntitySnapshot*>& units,
                   const Game::Formation::FormationDoctrineId& doctrine)
    -> std::vector<Game::Formation::ArmyFormationMember> {
  std::vector<Game::Formation::ArmyFormationMember> members;
  members.reserve(units.size());
  for (const auto* unit : units) {
    if (unit == nullptr) {
      continue;
    }
    auto const troop = Game::Units::spawn_typeToTroopType(unit->spawn_type);
    if (!troop.has_value()) {
      continue;
    }
    members.push_back(Game::Formation::ArmyFormationPlanner::make_member(
        unit->id, *troop, QVector3D(unit->pos_x, unit->pos_y, unit->pos_z), doctrine));
  }
  return members;
}

auto facing_towards(const std::vector<Game::Formation::ArmyFormationMember>& members,
                    const QVector3D& anchor) -> float {
  if (members.empty()) {
    return 0.0F;
  }
  QVector3D sum(0.0F, 0.0F, 0.0F);
  for (const auto& member : members) {
    sum += member.current_position;
  }
  return Game::Formation::ArmyFormationService::facing_from(
      sum / static_cast<float>(members.size()), anchor);
}

} // namespace

auto doctrine_for_nation(const Game::Systems::Nation* nation)
    -> Game::Formation::FormationDoctrineId {
  if (nation != nullptr && !nation->doctrine.empty()) {
    return nation->doctrine;
  }
  return Game::Formation::k_neutral_doctrine;
}

namespace {

auto to_plan_request(const AIFormationRequest& request,
                     const std::vector<Game::Formation::ArmyFormationMember>& members,
                     const Game::Formation::FormationDoctrineId& doctrine)
    -> Game::Formation::ArmyFormationRequest {
  Game::Formation::ArmyFormationRequest plan_request;
  plan_request.anchor = request.anchor;
  plan_request.facing =
      request.facing.value_or(facing_towards(members, request.anchor));
  plan_request.intent = request.intent;
  plan_request.doctrine = doctrine;
  plan_request.spacing = request.spacing;
  plan_request.resolve_terrain = request.resolve_terrain;
  plan_request.preserve_previous_slots = false;
  plan_request.options.movement_policy = request.movement;
  plan_request.options.preserve_member_order = request.preserve_member_order;
  plan_request.members.reserve(members.size());
  for (const auto& member : members) {
    plan_request.members.push_back(member.entity_id);
  }
  return plan_request;
}

} // namespace

auto muster_footprint(const AIFormationRequest& request,
                      const std::vector<const EntitySnapshot*>& units)
    -> MusterFootprint {
  auto const doctrine = doctrine_for_nation(request.nation);
  auto const members = build_members(units, doctrine);
  if (members.empty()) {
    return {};
  }
  auto plan_request = to_plan_request(request, members, doctrine);
  plan_request.resolve_terrain = false;
  auto const layout =
      Game::Formation::ArmyFormationPlanner::build_layout(members, plan_request);
  if (layout.valid) {
    return {layout.frontage, layout.depth, true};
  }
  auto const scattered = Game::Formation::ArmyFormationPlanner::scatter_layout(
      members, std::max(0.5F, request.spacing));
  return {scattered.frontage, scattered.depth, scattered.valid};
}

auto plan_ai_formation(const AIFormationRequest& request,
                       const std::vector<const EntitySnapshot*>& units)
    -> std::vector<AIFormationSlot> {
  auto const doctrine = doctrine_for_nation(request.nation);
  auto const members = build_members(units, doctrine);
  auto const plan_request = to_plan_request(request, members, doctrine);

  auto planned =
      Game::Formation::ArmyFormationService::placements_for(members, plan_request);

  if (formation_ai_logger().isDebugEnabled()) {
    auto const plan =
        Game::Formation::ArmyFormationPlanner::plan(members, plan_request);
    qCDebug(formation_ai_logger())
        << "player" << request.player_id << "doctrine"
        << QString::fromStdString(plan.doctrine) << "intent"
        << Game::Formation::intent_to_string(plan.intent) << "members"
        << static_cast<int>(members.size()) << "anchor" << plan_request.anchor
        << "facing" << plan_request.facing << "frontage" << plan.frontage << "depth"
        << plan.depth << "blocked" << plan.blocked_count << "adjusted"
        << plan.adjusted_count
        << (plan.valid ? "" : QString::fromStdString(plan.rejection_reason));
  }

  std::vector<AIFormationSlot> out;
  out.reserve(units.size());
  std::unordered_map<Engine::Core::EntityID, AIFormationSlot> by_id;
  by_id.reserve(members.size());
  for (std::size_t i = 0; i < members.size() && i < planned.size(); ++i) {
    by_id.emplace(
        members[i].entity_id,
        AIFormationSlot{planned[i].position,
                        planned[i].status != Game::Formation::SlotStatus::Blocked});
  }
  for (const auto* unit : units) {
    if (unit == nullptr) {
      out.push_back({request.anchor, false});
      continue;
    }
    auto it = by_id.find(unit->id);
    out.push_back(it == by_id.end() ? AIFormationSlot{request.anchor, false}
                                    : it->second);
  }
  return out;
}

auto plan_ai_formation(const AIFormationRequest& request,
                       const std::vector<Engine::Core::EntityID>& unit_ids,
                       const AISnapshot& snapshot) -> std::vector<AIFormationSlot> {
  std::unordered_map<Engine::Core::EntityID, const EntitySnapshot*> lookup;
  lookup.reserve(snapshot.friendly_units.size());
  for (const auto& unit : snapshot.friendly_units) {
    lookup.emplace(unit.id, &unit);
  }

  std::vector<const EntitySnapshot*> units;
  units.reserve(unit_ids.size());
  for (auto const id : unit_ids) {
    auto it = lookup.find(id);
    units.push_back(it == lookup.end() ? nullptr : it->second);
  }
  return plan_ai_formation(request, units);
}

auto move_to_slots(const std::vector<Engine::Core::EntityID>& unit_ids,
                   const std::vector<AIFormationSlot>& slot_list) -> AICommand {
  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units.reserve(unit_ids.size());
  command.move_target_x.reserve(unit_ids.size());
  command.move_target_y.reserve(unit_ids.size());
  command.move_target_z.reserve(unit_ids.size());
  for (std::size_t i = 0; i < unit_ids.size() && i < slot_list.size(); ++i) {
    if (!slot_list[i].usable) {
      continue;
    }
    command.units.push_back(unit_ids[i]);
    command.move_target_x.push_back(slot_list[i].position.x());
    command.move_target_y.push_back(slot_list[i].position.y());
    command.move_target_z.push_back(slot_list[i].position.z());
  }
  return command;
}

namespace {

auto doctrine_intent(const AIContext& context)
    -> std::optional<Game::Formation::ArmyFormationIntent> {
  using Game::Formation::ArmyFormationIntent;
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->formation.empty()) {
    return std::nullopt;
  }
  const auto& name = doctrine->formation;
  if (name == "line") {
    return ArmyFormationIntent::Line;
  }
  if (name == "column") {
    return ArmyFormationIntent::Column;
  }
  if (name == "defensive" || name == "shield_wall") {
    return ArmyFormationIntent::Defensive;
  }
  if (name == "assault" || name == "wedge") {
    return ArmyFormationIntent::Assault;
  }
  if (name == "encirclement" || name == "envelop") {
    return ArmyFormationIntent::Encirclement;
  }
  if (name == "siege_escort") {
    return ArmyFormationIntent::SiegeEscort;
  }
  if (name == "faction_default") {
    return ArmyFormationIntent::FactionDefault;
  }
  qCWarning(formation_ai_logger())
      << "commander doctrine names unknown formation" << QString::fromStdString(name)
      << "; using the situational choice";
  return std::nullopt;
}

} // namespace

auto select_ai_intent(const AISnapshot& snapshot,
                      const AIContext& context,
                      bool defensive_posture,
                      bool escorting_siege) -> Game::Formation::ArmyFormationIntent {
  using Game::Formation::ArmyFormationIntent;

  if (escorting_siege) {
    return ArmyFormationIntent::SiegeEscort;
  }
  if (defensive_posture) {
    return ArmyFormationIntent::Defensive;
  }
  if (const auto authored = doctrine_intent(context)) {
    return *authored;
  }

  auto const friendly = static_cast<int>(snapshot.friendly_units.size());
  auto const enemies = static_cast<int>(snapshot.visible_enemies.size());
  if (enemies > 0 && friendly >= enemies * 2 && friendly >= 6) {
    return ArmyFormationIntent::Encirclement;
  }
  if (context.strategy_config.personality.aggression > 0.6F) {
    return ArmyFormationIntent::Assault;
  }
  return ArmyFormationIntent::FactionDefault;
}

} // namespace Game::Systems::AI
