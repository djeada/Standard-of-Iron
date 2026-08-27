#include "ai_commander_doctrine.h"

#include <QString>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../core/world.h"
#include "../../units/commander_catalog.h"
#include "../../units/spawn_type.h"
#include "ai_doctrine_catalog.h"
#include "ai_strategy.h"

namespace Game::Systems::AI {

auto doctrine_profile_for_troop(Game::Units::TroopType troop_type)
    -> std::optional<AIPlayerProfile> {
  const auto* definition = Game::Units::commander_definition(troop_type);
  if (definition == nullptr) {
    return std::nullopt;
  }

  ensure_ai_doctrine_catalog_loaded();

  if (const auto* authored = authored_doctrine(definition->id)) {
    AIPlayerProfile profile;
    profile.strategy =
        AIStrategyFactory::parse_strategy(QString::fromStdString(authored->strategy));
    profile.posture = AIStrategyFactory::parse_posture(
        QString::fromStdString(authored->posture), AIPosture::Field);
    profile.personality.aggression = authored->aggression;
    profile.personality.defense = authored->defense;
    profile.personality.harassment = authored->harassment;
    profile.doctrine = authored;
    return profile;
  }

  if (!definition->doctrine.is_authored()) {
    return std::nullopt;
  }

  const auto& doctrine = definition->doctrine;
  AIPlayerProfile profile;
  profile.strategy =
      AIStrategyFactory::parse_strategy(QString::fromStdString(doctrine.ai_strategy));
  profile.posture = AIStrategyFactory::parse_posture(
      QString::fromStdString(doctrine.ai_posture), AIPosture::Field);
  profile.personality.aggression = doctrine.aggression;
  profile.personality.defense = doctrine.defense;
  profile.personality.harassment = doctrine.harassment;
  return profile;
}

auto doctrine_profile_for_owner(Engine::Core::World& world,
                                int owner_id) -> std::optional<AIPlayerProfile> {
  for (auto* entity : world.collect_entities_with<Engine::Core::CommanderComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->owner_id != owner_id || unit->health <= 0) {
      continue;
    }
    const auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type);
    if (!troop_type.has_value()) {
      continue;
    }
    if (auto profile = doctrine_profile_for_troop(*troop_type)) {
      return profile;
    }
  }
  return std::nullopt;
}

} // namespace Game::Systems::AI
