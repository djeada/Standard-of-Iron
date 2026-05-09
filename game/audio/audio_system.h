#pragma once

#include "audio_constants.h"
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
  std::string resource_id;
  float volume = AudioConstants::DEFAULT_VOLUME;
  bool loop = false;
  int priority = AudioConstants::DEFAULT_PRIORITY;
  AudioCategory category = AudioCategory::SFX;

  AudioEvent(AudioEventType t, std::string id = "",
             float vol = AudioConstants::DEFAULT_VOLUME, bool l = false,
             int p = AudioConstants::DEFAULT_PRIORITY,
             AudioCategory cat = AudioCategory::SFX)
      : type(t), resource_id(std::move(id)), volume(vol), loop(l), priority(p),
        category(cat) {}
};

class AudioSystem {
public:
  static auto get_instance() -> AudioSystem &;

  auto initialize() -> bool;
  void shutdown();

  void play_sound(const std::string &sound_id,
                 float volume = AudioConstants::DEFAULT_VOLUME,
                 bool loop = false,
                 int priority = AudioConstants::DEFAULT_PRIORITY,
                 AudioCategory category = AudioCategory::SFX);
  void play_music(const std::string &music_id,
                 float volume = AudioConstants::DEFAULT_VOLUME,
                 bool crossfade = true);
  void stop_sound(const std::string &sound_id);
  void stop_music();
  void set_master_volume(float volume);
  void set_sound_volume(float volume);
  void set_music_volume(float volume);
  void set_voice_volume(float volume);
  void pause_all();
  void resume_all();

  auto load_sound(const std::string &sound_id, const std::string &filePath,
                 AudioCategory category = AudioCategory::SFX) -> bool;
  auto load_music(const std::string &music_id,
                 const std::string &filePath) -> bool;
  void unload_sound(const std::string &sound_id);
  void unload_music(const std::string &music_id);
  void unload_all_sounds();
  void unload_all_music();

  void set_max_channels(size_t maxChannels);
  auto get_active_channel_count() const -> size_t;

  auto get_master_volume() const -> float { return masterVolume; }
  auto get_sound_volume() const -> float { return soundVolume; }
  auto get_music_volume() const -> float { return musicVolume; }
  auto get_voice_volume() const -> float { return voiceVolume; }

private:
  AudioSystem();
  ~AudioSystem();

  AudioSystem(const AudioSystem &) = delete;
  auto operator=(const AudioSystem &) -> AudioSystem & = delete;

  void audio_thread_func();
  void process_event(const AudioEvent &event);
  void cleanup_inactive_sounds();
  auto can_play_sound(int priority) -> bool;
  void evict_lowest_priority_sound();
  void evict_lowest_priority_sound_locked();
  auto get_effective_volume(AudioCategory category,
                          float eventVolume) const -> float;

  std::unordered_map<std::string, std::unique_ptr<Sound>> sounds;
  std::unordered_map<std::string, AudioCategory> soundCategories;
  std::unordered_set<std::string> activeResources;
  mutable std::mutex resourceMutex;

  Game::Audio::MusicPlayer *m_musicPlayer{nullptr};

  std::thread audioThread;
  std::queue<AudioEvent> eventQueue;
  mutable std::mutex queueMutex;
  std::condition_variable queue_condition;
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
