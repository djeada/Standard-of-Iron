#pragma once
#include "MiniaudioBackend.h"
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QVector>
#include <string>
#include <unordered_map>

namespace Game::Audio {

class MusicPlayer final : public QObject {
  Q_OBJECT
public:
  static constexpr int DEFAULT_MUSIC_CHANNELS = 4;
  static constexpr int DEFAULT_SAMPLE_RATE = 48000;
  static constexpr int DEFAULT_OUTPUT_CHANNELS = 2;
  static constexpr int DEFAULT_FADE_IN_MS = 250;
  static constexpr int DEFAULT_FADE_OUT_MS = 150;
  static constexpr int NO_FADE_MS = 0;
  static constexpr float DEFAULT_VOLUME = 1.0F;
  
  static auto getInstance() -> MusicPlayer &;

  auto initialize(int musicChannels = DEFAULT_MUSIC_CHANNELS) -> bool;
  void shutdown();

  void registerTrack(const std::string &trackId, const std::string &filePath);

  void play(const std::string &trackId, float volume = DEFAULT_VOLUME, bool loop = true);
  void stop();
  void pause();
  void resume();
  void setVolume(float volume);

  auto play(const std::string &trackId, float volume, bool loop, int channel,
            int fadeMs) -> int;
  void stop(int channel, int fadeMs = DEFAULT_FADE_OUT_MS);
  void pause(int channel);
  void resume(int channel);
  void setVolume(int channel, float volume, int fadeMs = NO_FADE_MS);

  void stopAll(int fadeMs = DEFAULT_FADE_OUT_MS);
  void setMasterVolume(float volume, int fadeMs = NO_FADE_MS);

  auto isPlaying() const -> bool;
  auto isPlaying(int channel) const -> bool;

  auto getBackend() -> MiniaudioBackend * { return m_backend.data(); }

private:
  explicit MusicPlayer();
  ~MusicPlayer() override;

  static void ensureOnGuiThread(const char *where);

  void play_gui(const std::string &trackId, float volume, bool loop,
                int channel, int fadeMs);
  void stop_gui(int channel, int fadeMs);
  void pause_gui(int channel);
  void resume_gui(int channel);
  void setVolume_gui(int channel, float volume, int fadeMs);
  void setMasterVolume_gui(float volume, int fadeMs);
  void stopAll_gui(int fadeMs);
  auto findFreeChannel() const -> int;

  QPointer<MiniaudioBackend> m_backend;
  std::unordered_map<std::string, QString> m_tracks;
  int m_channelCount{0};
  int m_defaultChannel{0};
  bool m_initialized{false};
};

} // namespace Game::Audio
