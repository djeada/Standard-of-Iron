#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class Sound;
class Music;

enum class AudioEventType {
  PLAY_SOUND,
  PLAY_MUSIC,
  STOP_SOUND,
  STOP_MUSIC,
  SET_VOLUME,
  PAUSE,
  RESUME,
  SHUTDOWN
};

struct AudioEvent {
  AudioEventType type;
  std::string resourceId;
  float volume = 1.0f;
  bool loop = false;
  int priority = 0;

  AudioEvent(AudioEventType t, const std::string &id = "", float vol = 1.0f,
             bool l = false, int p = 0)
      : type(t), resourceId(id), volume(vol), loop(l), priority(p) {}
};

class AudioSystem {
public:
  static AudioSystem &getInstance();

  bool initialize();
  void shutdown();

  void playSound(const std::string &soundId, float volume = 1.0f,
                 bool loop = false, int priority = 0);
  void playMusic(const std::string &musicId, float volume = 1.0f,
                 bool crossfade = true);
  void stopSound(const std::string &soundId);
  void stopMusic();
  void setMasterVolume(float volume);
  void setSoundVolume(float volume);
  void setMusicVolume(float volume);
  void pauseAll();
  void resumeAll();

  bool loadSound(const std::string &soundId, const std::string &filePath);
  bool loadMusic(const std::string &musicId, const std::string &filePath);

private:
  AudioSystem();
  ~AudioSystem();

  AudioSystem(const AudioSystem &) = delete;
  AudioSystem &operator=(const AudioSystem &) = delete;

  void audioThreadFunc();
  void processEvent(const AudioEvent &event);

  std::unordered_map<std::string, std::unique_ptr<Sound>> sounds;
  std::unordered_map<std::string, std::unique_ptr<Music>> music;

  std::thread audioThread;
  std::queue<AudioEvent> eventQueue;
  std::mutex queueMutex;
  std::condition_variable queueCondition;
  std::atomic<bool> isRunning;

  float masterVolume;
  float soundVolume;
  float musicVolume;

  struct ActiveSound {
    std::string id;
    int priority;
    bool loop;
  };
  std::vector<ActiveSound> activeSounds;
};
