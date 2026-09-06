#include "app/world/player_defeat_watcher.h"

#include <QCoreApplication>

#include <utility>

#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"

namespace {

constexpr float k_poll_interval_seconds = 0.5F;

auto owner_display_name(const Game::Systems::OwnerRegistry& owners,
                        const Game::Systems::NationRegistry& nations,
                        int owner_id) -> QString {
  const auto* nation = nations.get_nation_for_player(owner_id);
  if (nation != nullptr && !nation->display_name.empty()) {
    return QString::fromStdString(nation->display_name);
  }
  const std::string name = owners.get_owner_name(owner_id);
  if (!name.empty()) {
    return QString::fromStdString(name);
  }
  return QCoreApplication::translate("PlayerDefeatWatcher", "Player %1").arg(owner_id);
}

} // namespace

void PlayerDefeatWatcher::reset() {
  m_owners.clear();
  m_accumulator = 0.0F;
}

void PlayerDefeatWatcher::update(Engine::Core::World& world,
                                 int local_owner_id,
                                 float dt_seconds,
                                 const Announce& announce,
                                 const StillExpected& still_expected) {
  if (!announce) {
    return;
  }

  m_accumulator += dt_seconds;
  if (m_accumulator < k_poll_interval_seconds) {
    return;
  }
  m_accumulator = 0.0F;

  std::unordered_set<int> alive_now;
  for (auto [entity_id, unit] : world.view<Engine::Core::UnitComponent>()) {
    if (unit.health <= 0 || unit.owner_id == local_owner_id) {
      continue;
    }
    alive_now.insert(unit.owner_id);

    auto& state = m_owners[unit.owner_id];
    state.seen_alive = true;
    if (const auto* commander =
            world.try_get<Engine::Core::CommanderComponent>(entity_id);
        commander != nullptr && !commander->display_name.empty()) {

      state.commander_name = QString::fromStdString(commander->display_name);
    }
  }

  auto& owners = Game::Systems::OwnerRegistry::instance();
  const auto& nations = Game::Systems::NationRegistry::instance();

  for (auto& [owner_id, state] : m_owners) {
    if (state.announced || !state.seen_alive || alive_now.contains(owner_id)) {
      continue;
    }
    if (still_expected && still_expected(owner_id)) {
      continue;
    }
    if (owners.get_owner_type(owner_id) == Game::Systems::OwnerType::Neutral) {

      state.announced = true;
      continue;
    }

    state.announced = true;
    announce(Defeat{.owner_id = owner_id,
                    .ally = owners.are_allies(local_owner_id, owner_id),
                    .owner_name = owner_display_name(owners, nations, owner_id),
                    .commander_name = state.commander_name});
  }
}
