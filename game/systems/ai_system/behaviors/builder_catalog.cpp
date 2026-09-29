#include "builder_catalog.h"

#include <string_view>

#include "../ai_doctrine_catalog.h"

namespace Game::Systems::AI {

auto building_type_name(const std::string& name) -> const char* {
  if (name == BUILDING_TYPE_HOME) {
    return BUILDING_TYPE_HOME;
  }
  if (name == BUILDING_TYPE_DEFENSE_TOWER) {
    return BUILDING_TYPE_DEFENSE_TOWER;
  }
  if (name == BUILDING_TYPE_WALL_SEGMENT) {
    return BUILDING_TYPE_WALL_SEGMENT;
  }
  if (name == BUILDING_TYPE_WALL_GATE) {
    return BUILDING_TYPE_WALL_GATE;
  }
  if (name == BUILDING_TYPE_BARRACKS) {
    return BUILDING_TYPE_BARRACKS;
  }
  if (name == BUILDING_TYPE_MARKETPLACE) {
    return BUILDING_TYPE_MARKETPLACE;
  }
  if (name == BUILDING_TYPE_CATAPULT) {
    return BUILDING_TYPE_CATAPULT;
  }
  if (name == BUILDING_TYPE_BALLISTA) {
    return BUILDING_TYPE_BALLISTA;
  }
  if (name == BUILDING_TYPE_FARM) {
    return BUILDING_TYPE_FARM;
  }
  return nullptr;
}

auto preferred_siege_engine(const AIContext& context) -> const char* {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr) {
    return BUILDING_TYPE_CATAPULT;
  }
  for (const auto& name : doctrine->recruitment.preferred) {
    if (name == BUILDING_TYPE_BALLISTA) {
      return BUILDING_TYPE_BALLISTA;
    }
    if (name == BUILDING_TYPE_CATAPULT) {
      return BUILDING_TYPE_CATAPULT;
    }
  }
  if (doctrine->town_plan != nullptr) {
    for (const auto& step : doctrine->town_plan->steps) {
      if (step.building == BUILDING_TYPE_BALLISTA) {
        return BUILDING_TYPE_BALLISTA;
      }
      if (step.building == BUILDING_TYPE_CATAPULT) {
        return BUILDING_TYPE_CATAPULT;
      }
    }
  }
  return BUILDING_TYPE_CATAPULT;
}

auto is_fortification(const char* building_type) -> bool {
  if (building_type == nullptr) {
    return false;
  }
  const std::string_view type(building_type);
  return type == BUILDING_TYPE_WALL_SEGMENT || type == BUILDING_TYPE_WALL_GATE;
}

auto is_fortification_or_tower(const std::string& building) -> bool {
  return building == BUILDING_TYPE_WALL_SEGMENT ||
         building == BUILDING_TYPE_WALL_GATE || building == BUILDING_TYPE_DEFENSE_TOWER;
}

} // namespace Game::Systems::AI
