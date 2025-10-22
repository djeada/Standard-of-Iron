#pragma once

#include <memory>
#include <string>

class QMediaPlayer;
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

private:
  void cleanupPlayer();

  std::unique_ptr<QMediaPlayer> player;
  QThread *mainThread;
  std::string filepath;
  bool loaded;
};
