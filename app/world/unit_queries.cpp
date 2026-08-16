#include "app/world/unit_queries.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/nation_id.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"

namespace App::World {
namespace {

auto find_unit(const Engine::Core::World* world,
               Engine::Core::EntityID id) -> Engine::Core::Entity* {
  if (world == nullptr) {
    return nullptr;
  }
  return const_cast<Engine::Core::World*>(world)->get_entity(id);
}

} // namespace

auto describe_unit(const Engine::Core::World* world,
                   Engine::Core::EntityID id,
                   UnitDescription& out) -> bool {
  auto* entity = find_unit(world, id);
  if (entity == nullptr) {
    return false;
  }

  out.is_building = entity->has_component<Engine::Core::BuildingComponent>();

  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    out.name = QStringLiteral("Entity");
    out.health = 0;
    out.max_health = 0;
    out.alive = true;
    out.nation.clear();
    return true;
  }

  if (const auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type);
      troop_type.has_value()) {
    const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
        unit->nation_id, *troop_type);
    out.name = Game::Util::tr_asset(Game::Util::k_units_context, profile.display_name);
  } else {
    out.name =
        QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type));
  }
  out.health = unit->health;
  out.max_health = unit->max_health;
  out.alive = unit->health > 0;
  out.nation = Game::Systems::nation_id_to_qstring(unit->nation_id);
  return true;
}

auto unit_type_key(const Engine::Core::World* world,
                   Engine::Core::EntityID id,
                   QString& out) -> bool {
  out.clear();
  auto* entity = find_unit(world, id);
  if (entity == nullptr) {
    return false;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return false;
  }
  out = Game::Units::spawn_typeToQString(unit->spawn_type);
  return true;
}

auto describe_unit_stamina(const Engine::Core::World* world,
                           Engine::Core::EntityID id,
                           UnitStamina& out) -> bool {
  out = UnitStamina{};

  auto* entity = find_unit(world, id);
  if (entity == nullptr) {
    return false;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return false;
  }

  out.can_run = Game::Units::can_use_run_mode(unit->spawn_type);
  if (const auto* stamina = entity->get_component<Engine::Core::StaminaComponent>()) {
    out.ratio = stamina->get_stamina_ratio();
    out.is_running = stamina->is_running;
  }
  return true;
}

auto unit_activity(const Engine::Core::World* world,
                   Engine::Core::EntityID id) -> Game::Systems::UnitActivity {
  if (world == nullptr) {
    return {};
  }
  return Game::Systems::classify_unit_activity(*const_cast<Engine::Core::World*>(world),
                                               id);
}

} // namespace App::World
