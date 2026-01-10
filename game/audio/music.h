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
  static constexpr float DEFAULT_VOLUME = 1.0F;

  Music(const std::string &file_path);
  ~Music();

  bool is_loaded() const;
  void play(float volume = DEFAULT_VOLUME, bool loop = true);
  void stop();
  void pause();
  void resume();
  void set_volume(float volume);
  void fade_out();

private:
  void cleanup_player();

  QPointer<QMediaPlayer> player;
  QAudioOutput *audio_output;
  QThread *main_thread;
  std::string file_path;
  bool loaded;
  bool playing;
  std::atomic<bool> marked_for_deletion;
};
