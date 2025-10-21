#pragma once

#include <memory>
#include <string>

class QSoundEffect;

class Sound {
public:
  Sound(const std::string &filePath);
  ~Sound();

  bool isLoaded() const;
  void play(float volume = 1.0f, bool loop = false);
  void stop();
  void setVolume(float volume);

private:
  std::unique_ptr<QSoundEffect> soundEffect;
  std::string filepath;
  bool loaded;
};
