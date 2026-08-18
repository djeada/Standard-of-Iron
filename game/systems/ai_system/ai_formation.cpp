#include "ai_formation.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>

#include "../../formation/army_formation_planner.h"
#include "../../formation/unit_layout_resolver.h"
#include "../nation_registry.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;

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
  QVector3D forward = anchor - (sum / static_cast<float>(members.size()));
  forward.setY(0.0F);
  if (forward.lengthSquared() <= 1.0e-4F) {
    return 0.0F;
  }
  forward.normalize();
  return std::atan2(forward.x(), forward.z()) * k_rad_to_deg;
}

} // namespace

auto doctrine_for_nation(const Game::Systems::Nation* nation)
    -> Game::Formation::FormationDoctrineId {
  if (nation != nullptr && !nation->doctrine.empty()) {
    return nation->doctrine;
  }
  return Game::Formation::k_neutral_doctrine;
}

auto plan_ai_formation(const AIFormationRequest& request,
                       const std::vector<const EntitySnapshot*>& units)
    -> std::vector<QVector3D> {
  auto const doctrine = doctrine_for_nation(request.nation);
  auto const members = build_members(units, doctrine);

  Game::Formation::ArmyFormationRequest plan_request;
  plan_request.anchor = request.anchor;
  plan_request.facing = facing_towards(members, request.anchor);
  plan_request.intent = request.intent;
  plan_request.doctrine = doctrine;
  plan_request.spacing = request.spacing;
  plan_request.resolve_terrain = request.resolve_terrain;
  plan_request.preserve_previous_slots = false;
  plan_request.options.movement_policy = request.movement;
  plan_request.members.reserve(members.size());
  for (const auto& member : members) {
    plan_request.members.push_back(member.entity_id);
  }

  auto planned =
      Game::Formation::ArmyFormationService::positions_for(members, plan_request);

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

  std::vector<QVector3D> out;
  out.reserve(units.size());
  std::unordered_map<Engine::Core::EntityID, QVector3D> by_id;
  by_id.reserve(members.size());
  for (std::size_t i = 0; i < members.size() && i < planned.size(); ++i) {
    by_id.emplace(members[i].entity_id, planned[i]);
  }
  for (const auto* unit : units) {
    if (unit == nullptr) {
      out.push_back(request.anchor);
      continue;
    }
    auto it = by_id.find(unit->id);
    out.push_back(it == by_id.end() ? request.anchor : it->second);
  }
  return out;
}

auto plan_ai_formation(const AIFormationRequest& request,
                       const std::vector<Engine::Core::EntityID>& unit_ids,
                       const AISnapshot& snapshot) -> std::vector<QVector3D> {
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
