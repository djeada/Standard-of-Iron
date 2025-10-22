#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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
  SHUTDOWN,
  UNLOAD_RESOURCE,
  CLEANUP_INACTIVE
};

enum class AudioCategory { SFX, VOICE, MUSIC };

struct AudioEvent {
  AudioEventType type;
  std::string resourceId;
  float volume = 1.0f;
  bool loop = false;
  int priority = 0;
  AudioCategory category = AudioCategory::SFX;

  AudioEvent(AudioEventType t, const std::string &id = "", float vol = 1.0f,
             bool l = false, int p = 0,
             AudioCategory cat = AudioCategory::SFX)
      : type(t), resourceId(id), volume(vol), loop(l), priority(p),
        category(cat) {}
};

class AudioSystem {
public:
  static AudioSystem &getInstance();

  bool initialize();
  void shutdown();

  void playSound(const std::string &soundId, float volume = 1.0f,
                 bool loop = false, int priority = 0,
                 AudioCategory category = AudioCategory::SFX);
  void playMusic(const std::string &musicId, float volume = 1.0f,
                 bool crossfade = true);
  void stopSound(const std::string &soundId);
  void stopMusic();
  void setMasterVolume(float volume);
  void setSoundVolume(float volume);
  void setMusicVolume(float volume);
  void setVoiceVolume(float volume);
  void pauseAll();
  void resumeAll();

  bool loadSound(const std::string &soundId, const std::string &filePath,
                 AudioCategory category = AudioCategory::SFX);
  bool loadMusic(const std::string &musicId, const std::string &filePath);
  void unloadSound(const std::string &soundId);
  void unloadMusic(const std::string &musicId);
  void unloadAllSounds();
  void unloadAllMusic();

  void setMaxChannels(size_t maxChannels);
  size_t getActiveChannelCount() const;

  float getMasterVolume() const { return masterVolume; }
  float getSoundVolume() const { return soundVolume; }
  float getMusicVolume() const { return musicVolume; }
  float getVoiceVolume() const { return voiceVolume; }

private:
  AudioSystem();
  ~AudioSystem();

  AudioSystem(const AudioSystem &) = delete;
  AudioSystem &operator=(const AudioSystem &) = delete;

  void audioThreadFunc();
  void processEvent(const AudioEvent &event);
  void cleanupInactiveSounds();
  bool canPlaySound(int priority);
  void evictLowestPrioritySound();
  float getEffectiveVolume(AudioCategory category, float eventVolume) const;

  std::unordered_map<std::string, std::unique_ptr<Sound>> sounds;
  std::unordered_map<std::string, std::unique_ptr<Music>> music;
  std::unordered_map<std::string, AudioCategory> soundCategories;
  std::unordered_set<std::string> activeResources;

  std::thread audioThread;
  std::queue<AudioEvent> eventQueue;
  mutable std::mutex queueMutex;
  std::condition_variable queueCondition;
  std::atomic<bool> isRunning;

  std::atomic<float> masterVolume;
  std::atomic<float> soundVolume;
  std::atomic<float> musicVolume;
  std::atomic<float> voiceVolume;

  size_t maxChannels;

  struct ActiveSound {
    std::string id;
    int priority;
    bool loop;
    AudioCategory category;
    std::chrono::steady_clock::time_point startTime;
  };
  std::vector<ActiveSound> activeSounds;
  mutable std::mutex activeSoundsMutex;
};
