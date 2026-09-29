#pragma once

#include <QJsonObject>
#include <QString>

#include "game/mission/commander_message_director.h"
#include "game/mission/commander_voice_bank.h"
#include "game/mission/commander_voice_observer.h"

class CampaignManager;

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SessionContext;
}

namespace Game::Systems {
struct LevelSnapshot;
}

namespace App::ViewModels {
class CommanderMessageViewModel;
}

namespace App::Mission {

struct CommanderMessageBinding {
  Engine::Core::World* world = nullptr;
  Game::Session::SessionContext* session = nullptr;
  CampaignManager* campaign = nullptr;
  const Game::Systems::LevelSnapshot* level = nullptr;
  int local_owner_id = 1;
};

struct CommanderMessageFrame {
  bool may_release_start_cue = false;
  bool match_decided = false;
};

class CommanderMessageRuntime {
public:
  explicit CommanderMessageRuntime(
      App::ViewModels::CommanderMessageViewModel* view_model);

  [[nodiscard]] auto director() -> Game::Mission::CommanderMessageDirector& {
    return m_director;
  }
  [[nodiscard]] auto speaker_ids() const -> const QStringList& {
    return m_director.speaker_ids();
  }
  [[nodiscard]] auto has_messages() const -> bool { return m_director.has_messages(); }

  void configure(const CommanderMessageBinding& binding);
  void clear();
  void arm_start_cue() { m_start_cue_pending = m_director.has_messages(); }

  void update(const CommanderMessageBinding& binding,
              const CommanderMessageFrame& frame,
              float dt);

  void publish(const CommanderMessageBinding& binding);
  void dismiss_active(const CommanderMessageBinding& binding);

  void notify_outcome(const QString& state);

  [[nodiscard]] auto serialize() const -> QJsonObject;
  void restore(const CommanderMessageBinding& binding, const QJsonObject& state);

private:
  [[nodiscard]] auto voices() -> const Game::Mission::CommanderVoiceLibrary&;
  void release_pending_start_cue(bool allowed);

  App::ViewModels::CommanderMessageViewModel* m_view_model;
  Game::Mission::CommanderMessageDirector m_director;
  Game::Mission::CommanderVoiceLibrary m_voices;
  Game::Mission::CommanderVoiceObserver m_observer;
  bool m_voices_loaded = false;
  bool m_start_cue_pending = false;
};

} // namespace App::Mission
