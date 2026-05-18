#pragma once

#include <queue>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "audio_constants.h"

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

enum class AudioCategory {
  SFX,
  VOICE,
  AMBIENCE,
  MUSIC
};

struct AudioResourceConfig {
  AudioCategory category = AudioCategory::SFX;
  float volume = AudioConstants::DEFAULT_VOLUME;
  int priority = AudioConstants::DEFAULT_PRIORITY;
  int cooldown_ms = 0;
  size_t max_instances = 0;
};

struct AudioEvent {
  AudioEventType type;
  std::string resource_id;
  float volume = AudioConstants::DEFAULT_VOLUME;
  bool loop = false;
  int priority = AudioConstants::DEFAULT_PRIORITY;
  AudioCategory category = AudioCategory::SFX;
  bool crossfade = false;

  AudioEvent(AudioEventType t,
             std::string id = "",
             float vol = AudioConstants::DEFAULT_VOLUME,
             bool l = false,
             int p = AudioConstants::DEFAULT_PRIORITY,
             AudioCategory cat = AudioCategory::SFX,
             bool cf = false)
      : type(t)
      , resource_id(std::move(id))
      , volume(vol)
      , loop(l)
      , priority(p)
      , category(cat)
      , crossfade(cf) {}
};

class AudioSystem {
public:
  static auto get_instance() -> AudioSystem&;

  auto initialize() -> bool;
  void shutdown();

  void play_sound(const std::string& sound_id,
                  float volume = AudioConstants::DEFAULT_VOLUME,
                  bool loop = false,
                  int priority = AudioConstants::DEFAULT_PRIORITY,
                  AudioCategory category = AudioCategory::SFX);
  void play_music(const std::string& music_id,
                  float volume = AudioConstants::DEFAULT_VOLUME,
                  bool crossfade = true);
  void stop_sound(const std::string& sound_id);
  void stop_music();
  void set_master_volume(float volume);
  void set_sound_volume(float volume);
  void set_music_volume(float volume);
  void set_voice_volume(float volume);
  void set_ambience_volume(float volume);
  void pause_all();
  void resume_all();

  auto load_sound(const std::string& sound_id,
                  const std::string& file_path,
                  const AudioResourceConfig& config = {}) -> bool;
  auto load_music(const std::string& music_id,
                  const std::string& file_path,
                  const AudioResourceConfig& config = {}) -> bool;
  void register_alias(const std::string& alias_id, const std::string& resource_id);
  auto has_resource(const std::string& resource_id) const -> bool;
  void unload_sound(const std::string& sound_id);
  void unload_music(const std::string& music_id);
  void unload_all_sounds();
  void unload_all_music();

  void set_max_channels(size_t max_channels);
  auto get_active_channel_count() const -> size_t;

  auto get_master_volume() const -> float { return master_volume; }
  auto get_sound_volume() const -> float { return sound_volume; }
  auto get_music_volume() const -> float { return music_volume; }
  auto get_voice_volume() const -> float { return voice_volume; }
  auto get_ambience_volume() const -> float { return ambience_volume; }

private:
  AudioSystem();
  ~AudioSystem();

  AudioSystem(const AudioSystem&) = delete;
  auto operator=(const AudioSystem&) -> AudioSystem& = delete;

  void audio_thread_func();
  void process_event(const AudioEvent& event);
  void cleanup_inactive_sounds();
  void cleanup_inactive_sounds_locked();
  auto resolve_resource_id_locked(const std::string& resource_id) const -> std::string;
  auto should_accept_sound_locked(int priority) -> bool;
  auto get_active_instance_count_locked(const std::string& resource_id) const -> size_t;
  auto is_sound_on_cooldown_locked(const std::string& resource_id,
                                   int cooldown_ms,
                                   std::chrono::steady_clock::time_point now) const
      -> bool;
  void mark_sound_played_locked(const std::string& resource_id,
                                std::chrono::steady_clock::time_point now);
  auto get_resource_config_locked(const std::string& resource_id) const
      -> AudioResourceConfig;
  void evict_lowest_priority_sound_locked();
  auto get_effective_volume(AudioCategory category, float event_volume) const -> float;

  std::unordered_map<std::string, std::unique_ptr<Sound>> sounds;
  std::unordered_map<std::string, AudioResourceConfig> resource_configs;
  std::unordered_map<std::string, std::string> resource_aliases;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      resource_last_played_at;
  std::unordered_set<std::string> active_resources;
  mutable std::mutex resource_mutex;

  Game::Audio::MusicPlayer* m_music_player{nullptr};

  std::thread audio_thread;
  std::queue<AudioEvent> event_queue;
  mutable std::mutex queue_mutex;
  std::condition_variable queue_condition;
  std::atomic<bool> is_running;

  std::atomic<float> master_volume;
  std::atomic<float> sound_volume;
  std::atomic<float> music_volume;
  std::atomic<float> voice_volume;
  std::atomic<float> ambience_volume;

  size_t max_channels{AudioConstants::DEFAULT_MAX_CHANNELS};

  struct ActiveSound {
    std::string id;
    int priority;
    bool loop;
    AudioCategory category;
    std::chrono::steady_clock::time_point start_time;
  };
  std::vector<ActiveSound> active_sounds;
  mutable std::mutex active_sounds_mutex;
};
