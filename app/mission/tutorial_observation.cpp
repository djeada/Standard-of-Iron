#include "app/mission/tutorial_observation.h"

#include <QLatin1String>

#include "app/viewmodels/placement_view_model.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"

namespace App::Mission {

auto observe_tutorial_frame(const TutorialObservationInputs& inputs)
    -> Game::Mission::TutorialObservation {
  const auto& notes = inputs.notes;
  Game::Mission::TutorialObservation o;
  o.mission_running = inputs.world != nullptr && inputs.mission_running;
  o.victory = inputs.victory_state == QLatin1String("victory");
  o.defeat = inputs.victory_state == QLatin1String("defeat");
  if (!o.mission_running) {
    return o;
  }

  o.move_order_accepted = notes.move_accepted;
  o.attack_order_accepted = notes.attack_accepted;
  o.hold_order_accepted = notes.hold_accepted;
  o.guard_order_accepted = notes.guard_accepted;
  o.patrol_order_accepted = notes.patrol_accepted;
  o.gather_order_accepted = notes.gather_accepted;
  o.build_order_accepted = notes.build_accepted;
  o.last_rejection_reason = notes.last_rejection_reason;
  o.speed_changed = notes.speed_changed;
  o.camera_used = notes.camera_used;

  const int owner = inputs.local_owner_id;
  o.enemy_troops_defeated = inputs.enemy_troops_defeated;

  const auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  const auto harvested = resources.get_harvested_all(owner);
  const auto stock = resources.get_all(owner);
  o.harvested_wood = harvested.get(Game::Systems::ResourceType::Wood);
  o.harvested_stone = harvested.get(Game::Systems::ResourceType::Stone);
  o.harvested_iron = harvested.get(Game::Systems::ResourceType::Iron);
  o.wood = stock.get(Game::Systems::ResourceType::Wood);
  o.stone = stock.get(Game::Systems::ResourceType::Stone);
  o.iron = stock.get(Game::Systems::ResourceType::Iron);

  if (auto* selection = inputs.world->get_system<Game::Systems::SelectionSystem>()) {
    for (const auto id : selection->get_selected_units()) {
      const auto* entity = inputs.world->get_entity(id);
      const auto* unit = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      if (unit == nullptr || unit->owner_id != owner || unit->health <= 0) {
        continue;
      }
      if (Game::Units::is_building_spawn(unit->spawn_type)) {
        ++o.selected_building_count;
        if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
          ++o.selected_barracks_count;
        }
        continue;
      }
      if (unit->spawn_type == Game::Units::SpawnType::Builder) {
        ++o.selected_builder_count;
        continue;
      }
      if (entity->get_component<Engine::Core::CommanderComponent>() != nullptr) {
        o.commander_selected = true;
      }
      if (unit->spawn_type != Game::Units::SpawnType::Civilian) {
        ++o.selected_troop_count;
      }
    }
  }

  const auto& owners = Game::Systems::OwnerRegistry::instance();
  inputs.world->for_each_entity([&](const Engine::Core::Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      return;
    }
    const auto* commander = entity.get_component<Engine::Core::CommanderComponent>();
    if (unit->owner_id != owner) {
      if (commander != nullptr && owners.are_enemies(owner, unit->owner_id)) {
        ++o.enemy_commanders_alive;
      }
      return;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Home &&
        entity.get_component<Engine::Core::WallConstructionSiteComponent>() ==
            nullptr) {
      ++o.home_count;
      return;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      if (const auto* production =
              entity.get_component<Engine::Core::ProductionComponent>()) {
        o.barracks_manpower += production->manpower_available;
        o.production_in_progress = o.production_in_progress || production->in_progress;
      }
      return;
    }
    if (commander != nullptr) {
      o.aura_ready = o.aura_ready || commander->can_activate_aura_ability();
      o.aura_active = o.aura_active || commander->aura_ability_active ||
                      commander->aura_ability_requested;
      return;
    }
    if (Game::Units::is_troop_spawn(unit->spawn_type) &&
        unit->spawn_type != Game::Units::SpawnType::Builder &&
        unit->spawn_type != Game::Units::SpawnType::Civilian) {
      ++o.soldier_count;
    }
  });

  if (inputs.placement != nullptr) {
    o.construction_preview_active = inputs.placement->construction_preview_active();
    o.construction_preview_valid = inputs.placement->construction_preview_valid();
  }

  const QVariantMap& waves = inputs.wave_status;
  o.waves_cleared = waves.value(QStringLiteral("cleared")).toInt();
  o.wave_live = waves.value(QStringLiteral("live_enemies")).toInt() > 0;
  o.wave_pending = waves.value(QStringLiteral("active")).toBool() &&
                   waves.value(QStringLiteral("seconds_until_next")).toDouble() >= 0.0;
  return o;
}

} // namespace App::Mission
