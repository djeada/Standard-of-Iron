#pragma once

#include <QElapsedTimer>
#include <QString>

#include <memory>
#include <string>

class AudioCoordinator;
class QObject;
class QTimer;

namespace Engine::Core {
class World;
}

namespace Game::Audio {
class AudioEventHandler;
}

namespace Game::Systems {
class NationRegistry;
}

namespace App::Models {
class AudioSystemProxy;
}

namespace App::Core {

class AudioServices {
public:
  AudioServices();
  ~AudioServices();
  AudioServices(const AudioServices&) = delete;
  auto operator=(const AudioServices&) -> AudioServices& = delete;
  AudioServices(AudioServices&&) = delete;
  auto operator=(AudioServices&&) -> AudioServices& = delete;

  void initialize_audio_system(QObject* proxy_parent);

  void create_event_handling(Engine::Core::World* world,
                             Game::Systems::NationRegistry& nations);

  void initialize_event_handler(int local_owner_id);

  [[nodiscard]] auto coordinator() -> AudioCoordinator& { return *m_coordinator; }
  [[nodiscard]] auto proxy() -> QObject*;

  void set_frontend_context(const QString& context);

  void shutdown(const std::string& map_path);

private:
  void start_frontend_playlist(const QString& track_id, int playlist_size);
  void check_frontend_playlist();

  std::unique_ptr<Game::Audio::AudioEventHandler> m_event_handler;
  std::unique_ptr<AudioCoordinator> m_coordinator;
  std::unique_ptr<App::Models::AudioSystemProxy> m_proxy;
  QString m_frontend_context;

  std::unique_ptr<QTimer> m_playlist_timer;
  QString m_playlist_track;
  QElapsedTimer m_playlist_clock;
};

} // namespace App::Core
