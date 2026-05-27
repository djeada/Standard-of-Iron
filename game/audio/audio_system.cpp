#include "audio_system.h"

#include <QDebug>
#include <qglobal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "app/core/user_settings.h"
#include "miniaudio_backend.h"
#include "music_player.h"
#include "sound.h"

namespace {

auto sanitize_volume(float volume) -> float {
  if (!std::isfinite(volume)) {
    return AudioConstants::MIN_VOLUME;
  }
  return std::clamp(volume, AudioConstants::MIN_VOLUME, AudioConstants::MAX_VOLUME);
}

auto is_effectively_muted(float volume) -> bool {
  static constexpr float MUTED_EPSILON = 0.0001F;
  return volume <= MUTED_EPSILON;
}

} // namespace

AudioSystem::AudioSystem()
    : is_running(false)
    , master_volume(AudioConstants::DEFAULT_VOLUME)
    , sound_volume(AudioConstants::DEFAULT_VOLUME)
    , music_volume(AudioConstants::DEFAULT_VOLUME)
    , voice_volume(AudioConstants::DEFAULT_VOLUME)
    , ambience_volume(AudioConstants::DEFAULT_VOLUME) {
  load_persisted_volumes();
}

AudioSystem::~AudioSystem() {
  shutdown();
}

auto AudioSystem::get_instance() -> AudioSystem& {
  static AudioSystem instance;
  return instance;
}

auto AudioSystem::initialize() -> bool {
  if (is_running) {
    return true;
  }

  m_music_player = &Game::Audio::MusicPlayer::get_instance();
  if (!m_music_player->initialize()) {
    qWarning() << "Failed to initialize MusicPlayer";
    return false;
  }

  m_music_player->set_volume(master_volume.load() * music_volume.load());

  is_running = true;
  audio_thread = std::thread(&AudioSystem::audio_thread_func, this);

  return true;
}

void AudioSystem::shutdown() {
  if (!is_running) {
    return;
  }

  {
    std::lock_guard<std::mutex> const lock(queue_mutex);
    event_queue.emplace(AudioEventType::SHUTDOWN);
  }
  queue_condition.notify_one();

  if (audio_thread.joinable()) {
    audio_thread.join();
  }

  if (m_music_player != nullptr) {
    m_music_player->shutdown();
    m_music_player = nullptr;
  }

  sounds.clear();
  resource_configs.clear();
  resource_aliases.clear();
  resource_last_played_at.clear();
  active_resources.clear();

  {
    std::lock_guard<std::mutex> const lock(active_sounds_mutex);
    active_sounds.clear();
  }
}

void AudioSystem::play_sound(const std::string& sound_id,
                             float volume,
                             bool loop,
                             int priority,
                             AudioCategory category) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(
      AudioEventType::PLAY_SOUND, sound_id, volume, loop, priority, category);
  queue_condition.notify_one();
}

void AudioSystem::play_music(const std::string& music_id,
                             float volume,
                             Game::Audio::MusicTransition transition) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::PLAY_MUSIC,
                      music_id,
                      volume,
                      true,
                      AudioConstants::DEFAULT_PRIORITY,
                      AudioCategory::MUSIC,
                      transition == Game::Audio::MusicTransition::Crossfade);
  queue_condition.notify_one();
}

void AudioSystem::stop_sound(const std::string& sound_id) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::STOP_SOUND, sound_id);
  queue_condition.notify_one();
}

void AudioSystem::stop_music() {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::STOP_MUSIC);
  queue_condition.notify_one();
}

void AudioSystem::set_master_volume(float volume) {
  master_volume = sanitize_volume(volume);
  App::Core::UserSettings::save_master_volume(master_volume.load());

  if (m_music_player != nullptr) {
    m_music_player->set_volume(master_volume.load() * music_volume.load());
  }
}

void AudioSystem::set_sound_volume(float volume) {
  sound_volume = sanitize_volume(volume);
  App::Core::UserSettings::save_sound_volume(sound_volume.load());
}

void AudioSystem::set_music_volume(float volume) {
  music_volume = sanitize_volume(volume);
  App::Core::UserSettings::save_music_volume(music_volume.load());
  if (m_music_player != nullptr) {
    m_music_player->set_volume(master_volume.load() * music_volume.load());
  }
}

void AudioSystem::set_voice_volume(float volume) {
  voice_volume = sanitize_volume(volume);
  App::Core::UserSettings::save_voice_volume(voice_volume.load());
}

void AudioSystem::set_ambience_volume(float volume) {
  ambience_volume = sanitize_volume(volume);
  App::Core::UserSettings::save_ambience_volume(ambience_volume.load());
}

void AudioSystem::load_persisted_volumes() {
  const auto volumes = App::Core::UserSettings::load_audio_volumes();
  master_volume = volumes.master;
  sound_volume = volumes.sound;
  music_volume = volumes.music;
  voice_volume = volumes.voice;
  ambience_volume = volumes.ambience;
}

void AudioSystem::pause_all() {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::PAUSE);
  queue_condition.notify_one();
}

void AudioSystem::resume_all() {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::RESUME);
  queue_condition.notify_one();
}

auto AudioSystem::load_sound(const std::string& sound_id,
                             const std::string& file_path,
                             const AudioResourceConfig& config) -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  if (sounds.find(sound_id) != sounds.end()) {
    return true;
  }

  MiniaudioBackend* backend =
      (m_music_player != nullptr) ? m_music_player->get_backend() : nullptr;
  auto sound = std::make_unique<Sound>(file_path, backend);
  if (!sound || !sound->is_loaded()) {
    return false;
  }

  sounds[sound_id] = std::move(sound);
  resource_configs[sound_id] = config;
  active_resources.insert(sound_id);
  return true;
}

auto AudioSystem::load_music(const std::string& music_id,
                             const std::string& file_path,
                             const AudioResourceConfig& config) -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);

  if (m_music_player == nullptr) {
    qWarning() << "MusicPlayer not initialized";
    return false;
  }

  m_music_player->register_track(music_id, file_path);
  AudioResourceConfig music_config = config;
  music_config.category = AudioCategory::MUSIC;
  resource_configs[music_id] = music_config;
  active_resources.insert(music_id);
  return true;
}

void AudioSystem::register_alias(const std::string& alias_id,
                                 const std::string& resource_id) {
  if (alias_id.empty() || resource_id.empty() || alias_id == resource_id) {
    return;
  }

  std::lock_guard<std::mutex> const lock(resource_mutex);
  resource_aliases[alias_id] = resolve_resource_id_locked(resource_id);
  active_resources.insert(alias_id);
}

auto AudioSystem::has_resource(const std::string& resource_id) const -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  const std::string resolved_id = resolve_resource_id_locked(resource_id);
  return sounds.find(resolved_id) != sounds.end() ||
         resource_configs.find(resolved_id) != resource_configs.end();
}

void AudioSystem::unload_sound(const std::string& sound_id) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::UNLOAD_RESOURCE, sound_id);
  queue_condition.notify_one();
}

void AudioSystem::unload_music(const std::string& music_id) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::UNLOAD_RESOURCE, music_id);
  queue_condition.notify_one();
}

void AudioSystem::unload_all_sounds() {
  std::vector<std::string> sound_resources;
  {
    std::lock_guard<std::mutex> const resource_lock(resource_mutex);
    sound_resources.reserve(sounds.size());
    for (const auto& sound : sounds) {
      sound_resources.push_back(sound.first);
    }
  }

  std::lock_guard<std::mutex> const queue_lock(queue_mutex);
  for (const auto& sound_id : sound_resources) {
    event_queue.emplace(AudioEventType::UNLOAD_RESOURCE, sound_id);
  }
  queue_condition.notify_one();
}

void AudioSystem::unload_all_music() {
  std::lock_guard<std::mutex> const lock(resource_mutex);

  if (m_music_player != nullptr) {
    m_music_player->stop_all(AudioConstants::NO_FADE_MS);
  }

  std::vector<std::string> music_resources;
  for (const auto& res : active_resources) {

    if (sounds.find(res) == sounds.end()) {
      music_resources.push_back(res);
    }
  }
  for (const auto& res : music_resources) {
    active_resources.erase(res);
    resource_configs.erase(res);
    resource_last_played_at.erase(res);
  }
}

void AudioSystem::set_max_channels(size_t channels) {
  max_channels = std::max(AudioConstants::MIN_CHANNELS, channels);
}

auto AudioSystem::get_active_channel_count() const -> size_t {
  std::lock_guard<std::mutex> const lock(active_sounds_mutex);
  return active_sounds.size();
}

void AudioSystem::audio_thread_func() {
  while (is_running) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    queue_condition.wait(lock, [this] { return !event_queue.empty(); });

    while (!event_queue.empty()) {
      AudioEvent const event = event_queue.front();
      event_queue.pop();
      lock.unlock();

      process_event(event);

      if (event.type == AudioEventType::SHUTDOWN) {
        is_running = false;
        return;
      }

      lock.lock();
    }
  }
}

void AudioSystem::process_event(const AudioEvent& event) {
  switch (event.type) {
  case AudioEventType::PLAY_SOUND: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    cleanup_inactive_sounds_locked();
    const std::string resource_id = resolve_resource_id_locked(event.resource_id);
    auto it = sounds.find(resource_id);
    if (it != sounds.end()) {
      auto const now = std::chrono::steady_clock::now();
      AudioResourceConfig const config = get_resource_config_locked(resource_id);
      AudioCategory const category = config.category;
      int const effective_priority = std::max(event.priority, config.priority);
      float const requested_volume = event.volume * config.volume;

      if (config.max_instances > 0 &&
          get_active_instance_count_locked(resource_id) >= config.max_instances) {
        break;
      }

      if (is_sound_on_cooldown_locked(resource_id, config.cooldown_ms, now)) {
        break;
      }

      if (!should_accept_sound_locked(effective_priority)) {
        break;
      }

      if (get_active_channel_count() >= max_channels) {
        evict_lowest_priority_sound_locked();
      }

      float const effective_vol = get_effective_volume(category, requested_volume);
      if (is_effectively_muted(effective_vol)) {
        break;
      }
      it->second->play(effective_vol, event.loop);
      mark_sound_played_locked(resource_id, now);

      {
        std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
        active_sounds.push_back(
            {resource_id, effective_priority, event.loop, category, now});
      }
    }
    break;
  }
  case AudioEventType::PLAY_MUSIC: {
    if (m_music_player == nullptr) {
      break;
    }

    std::string resource_id;
    float effective_volume = AudioConstants::MIN_VOLUME;
    {
      std::lock_guard<std::mutex> const lock(resource_mutex);
      resource_id = resolve_resource_id_locked(event.resource_id);
      AudioResourceConfig const config = get_resource_config_locked(resource_id);
      effective_volume =
          get_effective_volume(AudioCategory::MUSIC, event.volume * config.volume);
    }

    (void)m_music_player->play(resource_id,
                               effective_volume,
                               true,
                               event.crossfade
                                   ? Game::Audio::MusicTransition::Crossfade
                                   : Game::Audio::MusicTransition::Immediate);
    break;
  }
  case AudioEventType::STOP_SOUND: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    const std::string resource_id = resolve_resource_id_locked(event.resource_id);
    auto it = sounds.find(resource_id);
    if (it != sounds.end()) {
      it->second->stop();

      std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
      active_sounds.erase(
          std::remove_if(active_sounds.begin(),
                         active_sounds.end(),
                         [&](const ActiveSound& as) { return as.id == resource_id; }),
          active_sounds.end());
    }
    break;
  }
  case AudioEventType::STOP_MUSIC: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    if (m_music_player != nullptr) {
      m_music_player->stop_all(AudioConstants::NO_FADE_MS);
    }
    break;
  }
  case AudioEventType::PAUSE: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    if (m_music_player != nullptr) {
      m_music_player->pause();
    }
    break;
  }
  case AudioEventType::RESUME: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    if (m_music_player != nullptr) {
      m_music_player->resume();
    }
    break;
  }
  case AudioEventType::UNLOAD_RESOURCE: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    const std::string resource_id = resolve_resource_id_locked(event.resource_id);
    const AudioResourceConfig config = get_resource_config_locked(resource_id);
    auto sound_it = sounds.find(resource_id);
    if (sound_it != sounds.end()) {
      sound_it->second->stop();

      std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
      active_sounds.erase(
          std::remove_if(active_sounds.begin(),
                         active_sounds.end(),
                         [&](const ActiveSound& as) { return as.id == resource_id; }),
          active_sounds.end());

      sounds.erase(sound_it);
    }

    if (config.category == AudioCategory::MUSIC && m_music_player != nullptr) {
      m_music_player->stop_all(AudioConstants::NO_FADE_MS);
    }

    resource_configs.erase(resource_id);
    resource_last_played_at.erase(resource_id);
    active_resources.erase(resource_id);

    active_resources.erase(event.resource_id);
    for (auto alias_it = resource_aliases.begin();
         alias_it != resource_aliases.end();) {
      if (alias_it->first == event.resource_id || alias_it->second == resource_id) {
        active_resources.erase(alias_it->first);
        resource_last_played_at.erase(alias_it->first);
        alias_it = resource_aliases.erase(alias_it);
      } else {
        ++alias_it;
      }
    }
    break;
  }
  case AudioEventType::CLEANUP_INACTIVE: {
    cleanup_inactive_sounds();
    break;
  }
  case AudioEventType::SET_VOLUME:
  case AudioEventType::SHUTDOWN:
    break;
  }
}

auto AudioSystem::should_accept_sound_locked(int priority) -> bool {
  std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
  if (active_sounds.size() < max_channels) {
    return true;
  }

  auto lowest_it = std::min_element(active_sounds.begin(),
                                    active_sounds.end(),
                                    [](const ActiveSound& a, const ActiveSound& b) {
                                      if (a.priority != b.priority) {
                                        return a.priority < b.priority;
                                      }
                                      return a.start_time < b.start_time;
                                    });
  return lowest_it != active_sounds.end() && priority > lowest_it->priority;
}

auto AudioSystem::get_active_instance_count_locked(const std::string& resource_id) const
    -> size_t {
  std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
  return static_cast<size_t>(std::count_if(
      active_sounds.begin(), active_sounds.end(), [&](const ActiveSound& sound) {
        return sound.id == resource_id;
      }));
}

auto AudioSystem::is_sound_on_cooldown_locked(
    const std::string& resource_id,
    int cooldown_ms,
    std::chrono::steady_clock::time_point now) const -> bool {
  if (cooldown_ms <= 0) {
    return false;
  }

  auto it = resource_last_played_at.find(resource_id);
  if (it == resource_last_played_at.end()) {
    return false;
  }

  auto const elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
  return elapsed < cooldown_ms;
}

void AudioSystem::mark_sound_played_locked(const std::string& resource_id,
                                           std::chrono::steady_clock::time_point now) {
  resource_last_played_at[resource_id] = now;
}

auto AudioSystem::get_resource_config_locked(const std::string& resource_id) const
    -> AudioResourceConfig {
  auto it = resource_configs.find(resource_id);
  if (it != resource_configs.end()) {
    return it->second;
  }
  return {};
}

void AudioSystem::evict_lowest_priority_sound_locked() {
  std::string sound_id_to_stop;

  {
    std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);

    if (active_sounds.empty()) {
      return;
    }

    auto lowest_it = std::min_element(active_sounds.begin(),
                                      active_sounds.end(),
                                      [](const ActiveSound& a, const ActiveSound& b) {
                                        if (a.priority != b.priority) {
                                          return a.priority < b.priority;
                                        }
                                        return a.start_time < b.start_time;
                                      });

    if (lowest_it != active_sounds.end()) {
      sound_id_to_stop = lowest_it->id;
      active_sounds.erase(lowest_it);
    }
  }

  if (!sound_id_to_stop.empty()) {
    auto it = sounds.find(sound_id_to_stop);
    if (it != sounds.end()) {
      it->second->stop();
    }
  }
}

void AudioSystem::cleanup_inactive_sounds() {
  std::lock_guard<std::mutex> const resource_lock(resource_mutex);
  cleanup_inactive_sounds_locked();
}

void AudioSystem::cleanup_inactive_sounds_locked() {
  std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);

  active_sounds.erase(std::remove_if(active_sounds.begin(),
                                     active_sounds.end(),
                                     [this](const ActiveSound& as) {
                                       auto it = sounds.find(as.id);
                                       if (it == sounds.end()) {
                                         return true;
                                       }
                                       return (!as.loop) && !it->second->is_playing();
                                     }),
                      active_sounds.end());
}

auto AudioSystem::resolve_resource_id_locked(const std::string& resource_id) const
    -> std::string {
  auto it = resource_aliases.find(resource_id);
  if (it == resource_aliases.end()) {
    return resource_id;
  }
  return it->second;
}

auto AudioSystem::get_effective_volume(AudioCategory category,
                                       float event_volume) const -> float {
  static constexpr float VOICE_GAIN = 2.0F;
  if (!std::isfinite(event_volume)) {
    return AudioConstants::MIN_VOLUME;
  }

  float category_volume = NAN;
  switch (category) {
  case AudioCategory::SFX:
    category_volume = sound_volume;
    break;
  case AudioCategory::VOICE:
    category_volume = voice_volume;
    break;
  case AudioCategory::AMBIENCE:
    category_volume = ambience_volume;
    break;
  case AudioCategory::MUSIC:
    category_volume = music_volume;
    break;
  default:
    category_volume = sound_volume;
    break;
  }

  const float category_gain =
      (category == AudioCategory::VOICE) ? VOICE_GAIN : AudioConstants::DEFAULT_VOLUME;
  const float effective_volume =
      master_volume.load() * category_volume * category_gain * event_volume;
  if (!std::isfinite(effective_volume)) {
    return AudioConstants::MIN_VOLUME;
  }
  return std::clamp(
      effective_volume, AudioConstants::MIN_VOLUME, MiniaudioBackend::MAX_VOLUME);
}
