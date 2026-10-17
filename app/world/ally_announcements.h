#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>

#include <cstdint>
#include <functional>
#include <map>

#include "game/mission/commander_message_director.h"
#include "game/systems/alliance_board.h"

namespace Game::Session {
class SessionContext;
}

namespace App::World {

[[nodiscard]] auto owner_display_name(const Game::Session::SessionContext* session,
                                      int owner_id) -> QString;

[[nodiscard]] auto is_friendly_commander(const Game::Session::SessionContext* session,
                                         int local_owner_id,
                                         int owner_id) -> bool;

[[nodiscard]] auto ally_resource_word(const QString& resource_key) -> QString;

struct AllyAnnouncementSink {
  std::function<void(const QString& text, bool positive)> exchange;
  std::function<void(const QVariantMap& card)> appeal_opened;
  std::function<void(quint32 appeal_id)> appeal_closed;
  std::function<void(const Game::Mission::CommanderMessageFact& fact)> commander_fact;
  std::function<void(const QVariantList& markers)> appeal_markers;
};

class AllyAnnouncementPresenter {
public:
  explicit AllyAnnouncementPresenter(AllyAnnouncementSink sink)
      : m_sink(std::move(sink)) {}

  void announce_all(Game::Session::SessionContext* session, int local_owner_id);

  void announce_exchanges(Game::Session::SessionContext* session,
                          int local_owner_id) const;
  void announce_calls(Game::Session::SessionContext* session, int local_owner_id) const;
  void announce_appeals(Game::Session::SessionContext* session, int local_owner_id);

private:
  struct TrackedAppeal {
    Game::Systems::AllyAppealKind kind = Game::Systems::AllyAppealKind::Resources;
    int from_ally = 0;
    float x = 0.0F;
    float z = 0.0F;
    QString name;
    bool card_open = true;
    bool pledged = false;
  };

  void close_every_card();
  void close_withdrawn_cards(const Game::Systems::AllianceBoard& board);
  void forget(quint32 appeal_id);
  void publish_markers();

  AllyAnnouncementSink m_sink;
  std::map<quint32, TrackedAppeal> m_tracked;
  std::uint32_t m_board_epoch = 0;
  bool m_markers_dirty = false;
};

} // namespace App::World
