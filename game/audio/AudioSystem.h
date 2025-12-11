#pragma once

#include "AudioConstants.h"
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
#include <utility>
#include <vector>

class Sound;

namespace Game::Audio {
class MusicPlayer;
}

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
  float volume = AudioConstants::DEFAULT_VOLUME;
  bool loop = false;
  int priority = AudioConstants::DEFAULT_PRIORITY;
  AudioCategory category = AudioCategory::SFX;

  AudioEvent(AudioEventType t, std::string id = "",
             float vol = AudioConstants::DEFAULT_VOLUME, bool l = false,
             int p = AudioConstants::DEFAULT_PRIORITY,
             AudioCategory cat = AudioCategory::SFX)
      : type(t), resourceId(std::move(id)), volume(vol), loop(l), priority(p),
        category(cat) {}
};

class AudioSystem {
public:
  static auto getInstance() -> AudioSystem &;

  auto initialize() -> bool;
  void shutdown();

  void playSound(const std::string &sound_id,
                 float volume = AudioConstants::DEFAULT_VOLUME,
                 bool loop = false,
                 int priority = AudioConstants::DEFAULT_PRIORITY,
                 AudioCategory category = AudioCategory::SFX);
  void playMusic(const std::string &music_id,
                 float volume = AudioConstants::DEFAULT_VOLUME,
                 bool crossfade = true);
  void stopSound(const std::string &sound_id);
  void stopMusic();
  void setMasterVolume(float volume);
  void setSoundVolume(float volume);
  void setMusicVolume(float volume);
  void setVoiceVolume(float volume);
  void pauseAll();
  void resumeAll();

  auto loadSound(const std::string &sound_id, const std::string &filePath,
                 AudioCategory category = AudioCategory::SFX) -> bool;
  auto loadMusic(const std::string &music_id,
                 const std::string &filePath) -> bool;
  void unloadSound(const std::string &sound_id);
  void unloadMusic(const std::string &music_id);
  void unloadAllSounds();
  void unloadAllMusic();

  void setMaxChannels(size_t maxChannels);
  auto getActiveChannelCount() const -> size_t;

  auto getMasterVolume() const -> float { return masterVolume; }
  auto getSoundVolume() const -> float { return soundVolume; }
  auto getMusicVolume() const -> float { return musicVolume; }
  auto getVoiceVolume() const -> float { return voiceVolume; }

private:
  AudioSystem();
  ~AudioSystem();

  AudioSystem(const AudioSystem &) = delete;
  auto operator=(const AudioSystem &) -> AudioSystem & = delete;

  void audioThreadFunc();
  void processEvent(const AudioEvent &event);
  void cleanupInactiveSounds();
  auto canPlaySound(int priority) -> bool;
  void evictLowestPrioritySound();
  void evictLowestPrioritySoundLocked();
  auto getEffectiveVolume(AudioCategory category,
                          float eventVolume) const -> float;

  std::unordered_map<std::string, std::unique_ptr<Sound>> sounds;
  std::unordered_map<std::string, AudioCategory> soundCategories;
  std::unordered_set<std::string> activeResources;
  mutable std::mutex resourceMutex;

  Game::Audio::MusicPlayer *m_musicPlayer{nullptr};

  std::thread audioThread;
  std::queue<AudioEvent> eventQueue;
  mutable std::mutex queueMutex;
  std::condition_variable queueCondition;
  std::atomic<bool> isRunning;

  std::atomic<float> masterVolume;
  std::atomic<float> soundVolume;
  std::atomic<float> musicVolume;
  std::atomic<float> voiceVolume;

  size_t maxChannels{AudioConstants::DEFAULT_MAX_CHANNELS};

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
