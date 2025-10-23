#pragma once
#include <QObject>
#include <QPointer>
#include <QVector>
#include <QString>
#include <QThread>
#include <unordered_map>
#include <string>
#include "MiniaudioBackend.h"  // Include instead of forward-declare

namespace Game {
namespace Audio {

class MusicPlayer final : public QObject {
  Q_OBJECT
public:
  static MusicPlayer& getInstance();

  // Call from GUI thread after Q(Gui)Application exists
  bool initialize(int musicChannels = 4);
  void shutdown();

  // Track registry (id -> file path)
  void registerTrack(const std::string& trackId, const std::string& filePath);

  // Back-compat (channel 0)
  void play(const std::string& trackId, float volume = 1.0f, bool loop = true);
  void stop();
  void pause();
  void resume();
  void setVolume(float volume);

  // Channel-aware (parallel layers)
  int  play(const std::string& trackId, float volume, bool loop, int channel, int fadeMs);
  void stop(int channel, int fadeMs = 150);
  void pause(int channel);
  void resume(int channel);
  void setVolume(int channel, float volume, int fadeMs = 0);

  // Global
  void stopAll(int fadeMs = 150);
  void setMasterVolume(float volume, int fadeMs = 0);

  // Queries
  bool isPlaying() const;
  bool isPlaying(int channel) const;
  
  // Access to backend for sound effects
  MiniaudioBackend* getBackend() { return m_backend.data(); }

private:
  explicit MusicPlayer();
  ~MusicPlayer() override;

  void ensureOnGuiThread(const char* where) const;

  // GUI-thread impls
  void play_gui(const std::string& trackId, float volume, bool loop, int channel, int fadeMs);
  void stop_gui(int channel, int fadeMs);
  void pause_gui(int channel);
  void resume_gui(int channel);
  void setVolume_gui(int channel, float volume, int fadeMs);
  void setMasterVolume_gui(float volume, int fadeMs);
  void stopAll_gui(int fadeMs);
  int  findFreeChannel() const;

private:
  QPointer<MiniaudioBackend> m_backend; // heavy lifter
  std::unordered_map<std::string, QString> m_tracks; // id -> absolute path
  int   m_channelCount {0};
  int   m_defaultChannel {0};
  bool  m_initialized {false};
};

} // namespace Audio
} // namespace Game
