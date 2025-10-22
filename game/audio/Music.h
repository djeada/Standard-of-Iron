#pragma once

#include <QPointer>
#include <atomic>
#include <memory>
#include <string>

class QMediaPlayer;
class QAudioOutput;
class QThread;

class Music {
public:
  Music(const std::string &filePath);
  ~Music();

  bool isLoaded() const;
  void play(float volume = 1.0f, bool loop = true);
  void stop();
  void pause();
  void resume();
  void setVolume(float volume);
  void fadeOut(); // Safer than immediate stop/pause

private:
  void cleanupPlayer();

  QPointer<QMediaPlayer> player;
  QAudioOutput *audioOutput;
  QThread *mainThread;
  std::string filepath;
  bool loaded;
  bool playing;
  std::atomic<bool> markedForDeletion;
};
