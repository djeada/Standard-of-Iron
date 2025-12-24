#include "builder_behavior.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Game::Systems::AI {

namespace {

constexpr const char *BUILDING_TYPE_HOME = "home";
constexpr const char *BUILDING_TYPE_DEFENSE_TOWER = "defense_tower";
constexpr const char *BUILDING_TYPE_BARRACKS = "barracks";
constexpr const char *BUILDING_TYPE_CATAPULT = "catapult";

constexpr int MIN_HOMES = 2;
constexpr int MAX_HOMES = 20;
constexpr int MIN_DEFENSE_TOWERS = 1;
constexpr int MAX_DEFENSE_TOWERS = 10;
constexpr int MAX_CATAPULTS = 5;

constexpr float DEFENSE_TOWER_CLOSE_RADIUS = 25.0F;
} // namespace

void BuilderBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                              float delta_time,
                              std::vector<AICommand> &outCommands) {
  m_construction_timer += delta_time;
  if (m_construction_timer < 3.0F) {
    return;
  }
  m_construction_timer = 0.0F;

  std::vector<Engine::Core::EntityID> available_builders;
  for (const auto &entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }

    if (entity.builder_production.has_component &&
        entity.builder_production.has_construction_site) {
      continue;
    }

    if (entity.movement.has_component && !entity.movement.has_target) {
      available_builders.push_back(entity.id);
    }
  }

  if (available_builders.empty()) {
    return;
  }

  std::string building_to_construct;
  
  int catapult_count = 0;
  for (const auto &entity : snapshot.friendly_units) {
    if (entity.spawn_type == Game::Units::SpawnType::Catapult) {
      catapult_count++;
    }
  }

  if (context.home_count < MIN_HOMES) {
    building_to_construct = BUILDING_TYPE_HOME;
  } else if (context.defense_tower_count < MIN_DEFENSE_TOWERS) {
    building_to_construct = BUILDING_TYPE_DEFENSE_TOWER;
  } else if (context.home_count < MAX_HOMES && context.defense_tower_count < MAX_DEFENSE_TOWERS) {
    int const target_defense_towers = context.home_count / 2;
    if (context.defense_tower_count < target_defense_towers) {
      building_to_construct = BUILDING_TYPE_DEFENSE_TOWER;
    } else {
      building_to_construct = BUILDING_TYPE_HOME;
    }
  } else if (context.home_count < MAX_HOMES) {
    building_to_construct = BUILDING_TYPE_HOME;
  } else if (context.defense_tower_count < MAX_DEFENSE_TOWERS) {
    building_to_construct = BUILDING_TYPE_DEFENSE_TOWER;
  } else if (catapult_count < MAX_CATAPULTS) {
    building_to_construct = BUILDING_TYPE_CATAPULT;
  }

  if (building_to_construct.empty()) {
    return;
  }

  float construction_x = context.base_pos_x;
  float construction_z = context.base_pos_z;

  if (context.primary_barracks != 0) {
    float angle = m_construction_counter * 0.8F;
    float radius = 15.0F + (m_construction_counter % 3) * 5.0F;
    
    if (building_to_construct == BUILDING_TYPE_DEFENSE_TOWER) {
      radius = std::min(radius, DEFENSE_TOWER_CLOSE_RADIUS);
    }
    
    construction_x += radius * std::cos(angle);
    construction_z += radius * std::sin(angle);
  }

  if (!available_builders.empty()) {
    AICommand command;
    command.type = AICommandType::StartBuilderConstruction;
    command.units.push_back(available_builders[0]);
    command.construction_type = building_to_construct;
    command.construction_site_x = construction_x;
    command.construction_site_z = construction_z;
    outCommands.push_back(std::move(command));

    m_construction_counter++;
  }
}

auto BuilderBehavior::should_execute(const AISnapshot &snapshot,
                                     const AIContext &context) const -> bool {
  (void)snapshot;

  if (context.builder_count == 0) {
    return false;
  }

  return context.home_count < MAX_HOMES ||
         context.defense_tower_count < MAX_DEFENSE_TOWERS;
}

} // namespace Game::Systems::AI
