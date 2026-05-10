#include "audio_system.h"

#include "miniaudio_backend.h"
#include "music_player.h"
#include "sound.h"
#include <QDebug>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <qglobal.h>
#include <string>
#include <utility>
#include <vector>

AudioSystem::AudioSystem()
    : is_running(false), master_volume(AudioConstants::DEFAULT_VOLUME),
      sound_volume(AudioConstants::DEFAULT_VOLUME),
      music_volume(AudioConstants::DEFAULT_VOLUME),
      voice_volume(AudioConstants::DEFAULT_VOLUME) {}

AudioSystem::~AudioSystem() { shutdown(); }

auto AudioSystem::get_instance() -> AudioSystem & {
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
  sound_categories.clear();
  active_resources.clear();

  {
    std::lock_guard<std::mutex> const lock(active_sounds_mutex);
    active_sounds.clear();
  }
}

void AudioSystem::play_sound(const std::string &sound_id, float volume,
                             bool loop, int priority, AudioCategory category) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::PLAY_SOUND, sound_id, volume, loop,
                      priority, category);
  queue_condition.notify_one();
}

void AudioSystem::play_music(const std::string &music_id, float volume, bool) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::PLAY_MUSIC, music_id, volume);
  queue_condition.notify_one();
}

void AudioSystem::stop_sound(const std::string &sound_id) {
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
  master_volume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                             AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resource_mutex);
  for (auto &sound : sounds) {
    auto it = sound_categories.find(sound.first);
    AudioCategory const category =
        (it != sound_categories.end()) ? it->second : AudioCategory::SFX;
    sound.second->set_volume(
        get_effective_volume(category, AudioConstants::DEFAULT_VOLUME));
  }

  if (m_music_player != nullptr) {
    m_music_player->set_volume(master_volume * music_volume);
  }
}

void AudioSystem::set_sound_volume(float volume) {
  sound_volume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                            AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resource_mutex);
  for (auto &sound : sounds) {
    auto it = sound_categories.find(sound.first);
    if (it != sound_categories.end() && it->second == AudioCategory::SFX) {
      sound.second->set_volume(get_effective_volume(
          AudioCategory::SFX, AudioConstants::DEFAULT_VOLUME));
    }
  }
}

void AudioSystem::set_music_volume(float volume) {
  music_volume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                            AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resource_mutex);
  if (m_music_player != nullptr) {
    m_music_player->set_volume(master_volume * music_volume);
  }
}

void AudioSystem::set_voice_volume(float volume) {
  voice_volume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                            AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resource_mutex);
  for (auto &sound : sounds) {
    auto it = sound_categories.find(sound.first);
    if (it != sound_categories.end() && it->second == AudioCategory::VOICE) {
      sound.second->set_volume(get_effective_volume(
          AudioCategory::VOICE, AudioConstants::DEFAULT_VOLUME));
    }
  }
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

auto AudioSystem::load_sound(const std::string &sound_id,
                             const std::string &file_path,
                             AudioCategory category) -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  if (sounds.find(sound_id) != sounds.end()) {
    return true;
  }

  MiniaudioBackend *backend =
      (m_music_player != nullptr) ? m_music_player->get_backend() : nullptr;
  auto sound = std::make_unique<Sound>(file_path, backend);
  if (!sound || !sound->is_loaded()) {
    return false;
  }

  sounds[sound_id] = std::move(sound);
  sound_categories[sound_id] = category;
  active_resources.insert(sound_id);
  return true;
}

auto AudioSystem::load_music(const std::string &music_id,
                             const std::string &file_path) -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);

  if (m_music_player == nullptr) {
    qWarning() << "MusicPlayer not initialized";
    return false;
  }

  m_music_player->register_track(music_id, file_path);
  active_resources.insert(music_id);
  return true;
}

void AudioSystem::unload_sound(const std::string &sound_id) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::UNLOAD_RESOURCE, sound_id);
  queue_condition.notify_one();
}

void AudioSystem::unload_music(const std::string &music_id) {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.emplace(AudioEventType::UNLOAD_RESOURCE, music_id);
  queue_condition.notify_one();
}

void AudioSystem::unload_all_sounds() {
  std::lock_guard<std::mutex> const lock(queue_mutex);
  for (const auto &sound : sounds) {
    event_queue.emplace(AudioEventType::UNLOAD_RESOURCE, sound.first);
  }
  queue_condition.notify_one();
}

void AudioSystem::unload_all_music() {
  std::lock_guard<std::mutex> const lock(resource_mutex);

  if (m_music_player != nullptr) {
    m_music_player->stop();
  }

  std::vector<std::string> music_resources;
  for (const auto &res : active_resources) {

    if (sounds.find(res) == sounds.end()) {
      music_resources.push_back(res);
    }
  }
  for (const auto &res : music_resources) {
    active_resources.erase(res);
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

void AudioSystem::process_event(const AudioEvent &event) {
  switch (event.type) {
  case AudioEventType::PLAY_SOUND: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    auto it = sounds.find(event.resource_id);
    if (it != sounds.end()) {
      if (!can_play_sound(event.priority)) {
        evict_lowest_priority_sound_locked();
      }

      float const effective_vol =
          get_effective_volume(event.category, event.volume);
      it->second->play(effective_vol, event.loop);

      {
        std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
        active_sounds.push_back({event.resource_id, event.priority, event.loop,
                                 event.category,
                                 std::chrono::steady_clock::now()});
      }
    }
    break;
  }
  case AudioEventType::PLAY_MUSIC: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    if (m_music_player != nullptr) {
      float const effective_volume =
          master_volume * music_volume * event.volume;
      m_music_player->play(event.resource_id, effective_volume, event.loop);
    }
    break;
  }
  case AudioEventType::STOP_SOUND: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    auto it = sounds.find(event.resource_id);
    if (it != sounds.end()) {
      it->second->stop();

      std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
      active_sounds.erase(std::remove_if(active_sounds.begin(),
                                         active_sounds.end(),
                                         [&](const ActiveSound &as) {
                                           return as.id == event.resource_id;
                                         }),
                          active_sounds.end());
    }
    break;
  }
  case AudioEventType::STOP_MUSIC: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    if (m_music_player != nullptr) {
      m_music_player->stop();
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
    auto sound_it = sounds.find(event.resource_id);
    if (sound_it != sounds.end()) {
      sound_it->second->stop();

      std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
      active_sounds.erase(std::remove_if(active_sounds.begin(),
                                         active_sounds.end(),
                                         [&](const ActiveSound &as) {
                                           return as.id == event.resource_id;
                                         }),
                          active_sounds.end());

      sounds.erase(sound_it);
      sound_categories.erase(event.resource_id);
      active_resources.erase(event.resource_id);
    }

    active_resources.erase(event.resource_id);
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

auto AudioSystem::can_play_sound(int) -> bool {
  std::lock_guard<std::mutex> const lock(active_sounds_mutex);
  return active_sounds.size() < max_channels;
}

void AudioSystem::evict_lowest_priority_sound() {
  std::string sound_id_to_stop;

  {
    std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);

    if (active_sounds.empty()) {
      return;
    }

    auto lowest_it =
        std::min_element(active_sounds.begin(), active_sounds.end(),
                         [](const ActiveSound &a, const ActiveSound &b) {
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
    std::lock_guard<std::mutex> const resource_lock(resource_mutex);
    auto it = sounds.find(sound_id_to_stop);
    if (it != sounds.end()) {
      it->second->stop();
    }
  }
}

void AudioSystem::evict_lowest_priority_sound_locked() {
  std::string sound_id_to_stop;

  {
    std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);

    if (active_sounds.empty()) {
      return;
    }

    auto lowest_it =
        std::min_element(active_sounds.begin(), active_sounds.end(),
                         [](const ActiveSound &a, const ActiveSound &b) {
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
  std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);

  active_sounds.erase(std::remove_if(active_sounds.begin(), active_sounds.end(),
                                     [this](const ActiveSound &as) {
                                       if (as.loop) {
                                         return false;
                                       }

                                       auto it = sounds.find(as.id);
                                       return it == sounds.end();
                                     }),
                      active_sounds.end());
}

auto AudioSystem::get_effective_volume(AudioCategory category,
                                       float event_volume) const -> float {
  float category_volume = NAN;
  switch (category) {
  case AudioCategory::SFX:
    category_volume = sound_volume;
    break;
  case AudioCategory::VOICE:
    category_volume = voice_volume;
    break;
  case AudioCategory::MUSIC:
    category_volume = music_volume;
    break;
  default:
    category_volume = sound_volume;
    break;
  }

  return master_volume * category_volume * event_volume;
}
