#include "app/orders/order_cues.h"

#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/units/spawn_type.h"

namespace App::Orders {

auto selection_mounts(Engine::Core::World& world,
                      const std::vector<Engine::Core::EntityID>& units)
    -> Game::Audio::Cue::SelectionMounts {
  Game::Audio::Cue::SelectionMounts mounts;
  for (const auto id : units) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    if (unit == nullptr) {
      continue;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Elephant) {
      ++mounts.elephants;
    } else if (Game::Units::is_cavalry(unit->spawn_type)) {
      ++mounts.cavalry;
    } else {
      ++mounts.foot;
    }
  }
  return mounts;
}

auto charge_cue(const Game::Audio::Cue::SelectionMounts& mounts) -> const char* {
  if (mounts.elephants > 0) {
    return Game::Audio::Cue::k_combat_charge_elephant;
  }
  if (mounts.cavalry > 0) {
    return Game::Audio::Cue::k_combat_charge_cavalry;
  }
  return Game::Audio::Cue::k_combat_charge;
}

auto move_order_cue(const Game::Audio::Cue::SelectionMounts& mounts) -> const char* {
  const bool all_mounted =
      mounts.cavalry > 0 && mounts.foot == 0 && mounts.elephants == 0;
  return all_mounted ? Game::Audio::Cue::k_order_move_mounted
                     : Game::Audio::Cue::k_order_move;
}

} // namespace App::Orders
