#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

#include "app/world/player_defeat_watcher.h"
#include "game/core/event_manager.h"

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SessionContext;
}

namespace App::World {

[[nodiscard]] auto
format_defeat_announcement(const PlayerDefeatWatcher::Defeat& defeat) -> QString;

class BattleStats {
public:
  [[nodiscard]] static auto player_stats(Game::Session::SessionContext& session,
                                         int owner_id) -> QVariantMap;

  [[nodiscard]] auto enemy_troops_defeated() const -> int { return m_troops_defeated; }
  [[nodiscard]] auto enemy_units_defeated() const -> int { return m_units_defeated; }

  [[nodiscard]] auto reset() -> bool;

  [[nodiscard]] auto note_unit_died(const Engine::Core::UnitDiedEvent& event,
                                    Engine::Core::World* world,
                                    int local_owner_id) -> bool;

  void announce_defeats(Engine::Core::World& world,
                        int local_owner_id,
                        float dt,
                        const PlayerDefeatWatcher::Announce& announce,
                        const PlayerDefeatWatcher::StillExpected& still_expected);

  [[nodiscard]] auto
  serialize(Game::Session::SessionContext* session) const -> QJsonObject;
  [[nodiscard]] auto restore(const QJsonObject& state,
                             Game::Session::SessionContext* session) -> bool;

private:
  int m_troops_defeated = 0;
  int m_units_defeated = 0;
  PlayerDefeatWatcher m_defeat_watcher;
};

} // namespace App::World
