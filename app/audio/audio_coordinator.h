#pragma once

#include <QString>

namespace Game::Audio {
class AudioEventHandler;
}

namespace Game::Mission {
struct MissionDefinition;
}

namespace Game::Systems {
class NationRegistry;
}

class AudioCoordinator {
public:
  AudioCoordinator(Game::Audio::AudioEventHandler* event_handler,
                   Game::Systems::NationRegistry& nations);

  struct FrontendMusic {
    QString track_id;
    int playlist_size = 0;
  };

  // Starts the next track of the screen's playlist (round robin) and reports
  // what it chose, so the caller can move on when the track has played through.
  auto apply_frontend_music_context(const QString& context) -> FrontendMusic;
  void configure_audio_manifest_mappings(int local_owner_id);
  void configure_audio_voice_mappings();
  void configure_audio_ambient_mappings(int local_owner_id);
  void ensure_result_audio_ready(const QString& state, int local_owner_id);
  void apply_mission_ambience(const Game::Mission::MissionDefinition* mission,
                              const QString& map_path,
                              int local_owner_id);
  void stop_mission_ambience();

private:
  Game::Audio::AudioEventHandler* m_event_handler = nullptr;
  Game::Systems::NationRegistry& m_nations;
  QString m_current_ambient_sound_id;
};
