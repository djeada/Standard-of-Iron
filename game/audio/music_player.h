#pragma once
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QVector>

#include <string>
#include <unordered_map>

#include "audio_constants.h"
#include "miniaudio_backend.h"

namespace Game::Audio {

class MusicPlayer final : public QObject {
  Q_OBJECT
public:
  static auto get_instance() -> MusicPlayer&;

  auto initialize(int music_channels = AudioConstants::DEFAULT_MUSIC_CHANNELS) -> bool;
  void shutdown();

  void register_track(const std::string& track_id, const std::string& file_path);

  void play(const std::string& track_id,
            float volume = AudioConstants::DEFAULT_VOLUME,
            bool loop = true);
  auto
  play(const std::string& track_id, float volume, bool loop, bool crossfade) -> int;
  void stop();
  void pause();
  void resume();
  void set_volume(float volume);

  auto play(const std::string& track_id,
            float volume,
            bool loop,
            int channel,
            int fade_ms) -> int;
  void stop(int channel, int fade_ms = AudioConstants::DEFAULT_FADE_OUT_MS);
  void pause(int channel);
  void resume(int channel);
  void set_volume(int channel, float volume, int fade_ms = AudioConstants::NO_FADE_MS);

  void stop_all(int fade_ms = AudioConstants::DEFAULT_FADE_OUT_MS);
  void set_master_volume(float volume, int fade_ms = AudioConstants::NO_FADE_MS);

  auto is_playing() const -> bool;
  auto is_playing(int channel) const -> bool;

  auto get_backend() -> MiniaudioBackend* { return m_backend.data(); }

private:
  explicit MusicPlayer();
  ~MusicPlayer() override;

  static void ensure_on_gui_thread(const char* where);

  void play_gui(
      const std::string& track_id, float volume, bool loop, int channel, int fade_ms);
  void stop_gui(int channel, int fade_ms);
  void pause_gui(int channel);
  void resume_gui(int channel);
  void setVolume_gui(int channel, float volume, int fade_ms);
  void setMasterVolume_gui(float volume, int fade_ms);
  void stopAll_gui(int fade_ms);
  auto find_free_channel() const -> int;
  auto find_free_channel_excluding(int excluded_channel) const -> int;

  QPointer<MiniaudioBackend> m_backend;
  std::unordered_map<std::string, QString> m_tracks;
  int m_channel_count{0};
  int m_default_channel{0};
  int m_current_music_channel{-1};
  bool m_initialized{false};
};

} // namespace Game::Audio
