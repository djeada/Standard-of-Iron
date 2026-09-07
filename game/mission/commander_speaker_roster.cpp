#include "commander_speaker_roster.h"

#include <QDebug>

#include <algorithm>
#include <map>

#include "game/core/component_commander.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/mission/mission_commander_setup.h"
#include "game/systems/nation_id.h"
#include "game/systems/owner_registry.h"
#include "game/units/commander_catalog.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace Game::Mission {

auto build_commander_speaker_roster(Engine::Core::World& world,
                                    const Game::Systems::OwnerRegistry& owners,
                                    int local_owner_id)
    -> std::vector<CommanderSpeaker> {
  struct Fielded {
    QString troop_type;
    Game::Systems::NationID nation = Game::Systems::NationID::RomanRepublic;
  };
  std::map<int, Fielded> fielded_by_owner;
  std::map<int, Game::Systems::NationID> nation_by_owner;

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
    fielded_by_owner.try_emplace(
        unit.owner_id,
        Fielded{
            .troop_type = Game::Units::troop_typeToQString(*troop_type),
            .nation = unit.nation_id,
        });
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
      troop_type = it->second.troop_type;
    } else if (owner.type == Game::Systems::OwnerType::AI) {
      const auto nation_it = nation_by_owner.find(owner.owner_id);
      const auto nation = nation_it != nation_by_owner.end()
                              ? nation_it->second
                              : Game::Systems::NationID::RomanRepublic;
      troop_type = resolve_commander_troop(Game::Systems::nation_id_to_qstring(nation),
                                           std::nullopt);
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

} // namespace Game::Mission
