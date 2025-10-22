#pragma once
#include "MiniaudioBackend.h"
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QVector>
#include <string>
#include <unordered_map>

namespace Game {
namespace Audio {

class MusicPlayer final : public QObject {
  Q_OBJECT
public:
  static MusicPlayer &getInstance();

  bool initialize(int musicChannels = 4);
  void shutdown();

  void registerTrack(const std::string &trackId, const std::string &filePath);

  void play(const std::string &trackId, float volume = 1.0f, bool loop = true);
  void stop();
  void pause();
  void resume();
  void setVolume(float volume);

  int play(const std::string &trackId, float volume, bool loop, int channel,
           int fadeMs);
  void stop(int channel, int fadeMs = 150);
  void pause(int channel);
  void resume(int channel);
  void setVolume(int channel, float volume, int fadeMs = 0);

  void stopAll(int fadeMs = 150);
  void setMasterVolume(float volume, int fadeMs = 0);

  bool isPlaying() const;
  bool isPlaying(int channel) const;

  MiniaudioBackend *getBackend() { return m_backend.data(); }

private:
  explicit MusicPlayer();
  ~MusicPlayer() override;

  void ensureOnGuiThread(const char *where) const;

  void play_gui(const std::string &trackId, float volume, bool loop,
                int channel, int fadeMs);
  void stop_gui(int channel, int fadeMs);
  void pause_gui(int channel);
  void resume_gui(int channel);
  void setVolume_gui(int channel, float volume, int fadeMs);
  void setMasterVolume_gui(float volume, int fadeMs);
  void stopAll_gui(int fadeMs);
  int findFreeChannel() const;

private:
  QPointer<MiniaudioBackend> m_backend;
  std::unordered_map<std::string, QString> m_tracks;
  int m_channelCount{0};
  int m_defaultChannel{0};
  bool m_initialized{false};
};

} // namespace Audio
} // namespace Game
