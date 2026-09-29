#pragma once

#include <QString>
#include <QVariantMap>
#include <QtGlobal>

#include <functional>

#include "game/mission/commander_message_director.h"

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
};

class AllyAnnouncementPresenter {
public:
  explicit AllyAnnouncementPresenter(AllyAnnouncementSink sink)
      : m_sink(std::move(sink)) {}

  void announce_all(Game::Session::SessionContext* session, int local_owner_id) const;

  void announce_exchanges(Game::Session::SessionContext* session,
                          int local_owner_id) const;
  void announce_calls(Game::Session::SessionContext* session, int local_owner_id) const;
  void announce_appeals(Game::Session::SessionContext* session,
                        int local_owner_id) const;

private:
  AllyAnnouncementSink m_sink;
};

} // namespace App::World
