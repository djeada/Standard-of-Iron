#pragma once

#include <QString>
#include <QVariantList>

#include <functional>
#include <optional>

#include "app/session/replay_view_track.h"
#include "game/command/replay.h"

namespace Game::Session {
class SessionContext;
}

namespace Render::GL {
class Camera;
}

namespace App::Session {

struct ReplayLaunch {
  QString kind;
  QString reference;
  QVariantList player_configs;
  QString difficulty;
};

struct ReplayStarters {
  std::function<void(const QString& reference, const QString& difficulty)>
      campaign_mission;
  std::function<void(const QString& reference, const QString& difficulty)> mission_file;
  std::function<void(const QString& reference, const QVariantList& player_configs)>
      skirmish;
};

struct ReplayPlaybackResult {
  enum class Failure {
    None,
    LoadFailed,
    UnknownKind
  };
  Failure failure = Failure::None;
  QString detail;

  [[nodiscard]] auto ok() const -> bool { return failure == Failure::None; }
};

class ReplayCoordinator {
public:
  void set_record_path(const QString& path) { m_record_path = path; }
  void set_verify_exit(bool enabled) { m_verify_exit = enabled; }
  void note_launch(ReplayLaunch launch) { m_launch = std::move(launch); }

  [[nodiscard]] auto begin_playback(const QString& path, const ReplayStarters& starters)
      -> ReplayPlaybackResult;

  void arm_for_started_match(Game::Session::SessionContext* session);

  void finish_verification_if_done(Game::Session::SessionContext* session);

  void present_view(Game::Session::SessionContext* session, Render::GL::Camera* camera);

  [[nodiscard]] auto view_track() -> ReplayViewTrack& { return m_view; }

private:
  bool m_verify_exit = false;
  ReplayLaunch m_launch;
  QString m_record_path;
  QString m_playback_path;
  std::optional<Game::Command::ReplayFile> m_pending;
  ReplayViewTrack m_view;
};

} // namespace App::Session
