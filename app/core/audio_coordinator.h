#pragma once

#include <QString>

namespace Game::Audio {
class AudioEventHandler;
}

namespace Game::Mission {
struct MissionDefinition;
}

class AudioCoordinator {
public:
  explicit AudioCoordinator(Game::Audio::AudioEventHandler* event_handler);

  void apply_frontend_music_context(const QString& context);
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
  QString m_current_ambient_sound_id;
};
