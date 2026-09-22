#include "commander_speaker_roster.h"

#include <QDebug>

#include <algorithm>
#include <map>

#include "game/core/component_commander.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/units/commander_catalog.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace Game::Mission {

namespace {

auto default_commander_for_nation(Game::Systems::NationID nation) -> QString {
  const auto troop = Game::Units::default_commander_troop_for_nation(nation);
  const auto troop_nation = Game::Units::commander_troop_nation(troop);
  if (!troop_nation.has_value() || *troop_nation != nation) {
    return {};
  }
  return Game::Units::troop_typeToQString(troop);
}

} // namespace

auto build_commander_speaker_roster(Engine::Core::World& world,
                                    const Game::Systems::OwnerRegistry& owners,
                                    const Game::Systems::NationRegistry& nations,
                                    int local_owner_id)
    -> std::vector<CommanderSpeaker> {
  std::map<int, QString> fielded_by_owner;
  std::map<int, Game::Systems::NationID> nation_by_owner;
  for (const auto& [owner_id, nation] : nations.player_nation_assignments()) {
    nation_by_owner.insert_or_assign(owner_id, nation);
  }

  for (auto [entity_id, unit] : world.view<const Engine::Core::UnitComponent>()) {
    if (unit.health <= 0) {
      continue;
    }
    nation_by_owner.try_emplace(unit.owner_id, unit.nation_id);

    if (!world.has<Engine::Core::CommanderComponent>(entity_id)) {
      continue;
    }
    const auto troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
    if (!troop_type.has_value() || !Game::Units::is_commander_troop(*troop_type)) {
      continue;
    }
    fielded_by_owner.try_emplace(unit.owner_id,
                                 Game::Units::troop_typeToQString(*troop_type));
  }

  std::vector<CommanderSpeaker> roster;
  for (const auto& owner : owners.get_all_owners()) {
    if (owner.owner_id == local_owner_id ||
        owner.type == Game::Systems::OwnerType::Neutral) {
      continue;
    }
    QString troop_type;
    if (const auto it = fielded_by_owner.find(owner.owner_id);
        it != fielded_by_owner.end()) {
      troop_type = it->second;
    } else if (owner.type == Game::Systems::OwnerType::AI) {
      const auto nation_it = nation_by_owner.find(owner.owner_id);
      if (nation_it == nation_by_owner.end()) {
        continue;
      }
      troop_type = default_commander_for_nation(nation_it->second);
    }
    if (troop_type.isEmpty()) {
      continue;
    }
    roster.push_back({.owner_id = owner.owner_id,
                      .troop_type = troop_type,
                      .relationship = owners.are_allies(owner.owner_id, local_owner_id)
                                          ? CommanderRelationship::Ally
                                          : CommanderRelationship::Enemy});
  }

  std::sort(roster.begin(), roster.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.owner_id < rhs.owner_id;
  });
  return roster;
}

auto local_commander_speaker(Engine::Core::World& world,
                             int local_owner_id) -> std::optional<CommanderSpeaker> {
  for (auto [entity_id, unit] : world.view<const Engine::Core::UnitComponent>()) {
    if (unit.owner_id != local_owner_id || unit.health <= 0 ||
        !world.has<Engine::Core::CommanderComponent>(entity_id)) {
      continue;
    }
    const auto troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
    if (!troop_type.has_value() || !Game::Units::is_commander_troop(*troop_type)) {
      continue;
    }
    return CommanderSpeaker{.owner_id = local_owner_id,
                            .troop_type = Game::Units::troop_typeToQString(*troop_type),
                            .relationship = CommanderRelationship::Ally};
  }
  return std::nullopt;
}

} // namespace Game::Mission
