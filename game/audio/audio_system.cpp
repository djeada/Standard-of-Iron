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
    : isRunning(false), masterVolume(AudioConstants::DEFAULT_VOLUME),
      soundVolume(AudioConstants::DEFAULT_VOLUME),
      musicVolume(AudioConstants::DEFAULT_VOLUME),
      voiceVolume(AudioConstants::DEFAULT_VOLUME) {}

AudioSystem::~AudioSystem() { shutdown(); }

auto AudioSystem::get_instance() -> AudioSystem & {
  static AudioSystem instance;
  return instance;
}

auto AudioSystem::initialize() -> bool {
  if (isRunning) {
    return true;
  }

  m_musicPlayer = &Game::Audio::MusicPlayer::get_instance();
  if (!m_musicPlayer->initialize()) {
    qWarning() << "Failed to initialize MusicPlayer";
    return false;
  }

  isRunning = true;
  audioThread = std::thread(&AudioSystem::audio_thread_func, this);

  return true;
}

void AudioSystem::shutdown() {
  if (!isRunning) {
    return;
  }

  {
    std::lock_guard<std::mutex> const lock(queueMutex);
    eventQueue.emplace(AudioEventType::SHUTDOWN);
  }
  queue_condition.notify_one();

  if (audioThread.joinable()) {
    audioThread.join();
  }

  if (m_musicPlayer != nullptr) {
    m_musicPlayer->shutdown();
    m_musicPlayer = nullptr;
  }

  sounds.clear();
  soundCategories.clear();
  activeResources.clear();

  {
    std::lock_guard<std::mutex> const lock(activeSoundsMutex);
    activeSounds.clear();
  }
}

void AudioSystem::play_sound(const std::string &sound_id, float volume,
                            bool loop, int priority, AudioCategory category) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::PLAY_SOUND, sound_id, volume, loop,
                     priority, category);
  queue_condition.notify_one();
}

void AudioSystem::play_music(const std::string &music_id, float volume, bool) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::PLAY_MUSIC, music_id, volume);
  queue_condition.notify_one();
}

void AudioSystem::stop_sound(const std::string &sound_id) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::STOP_SOUND, sound_id);
  queue_condition.notify_one();
}

void AudioSystem::stop_music() {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::STOP_MUSIC);
  queue_condition.notify_one();
}

void AudioSystem::set_master_volume(float volume) {
  masterVolume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                            AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    AudioCategory const category =
        (it != soundCategories.end()) ? it->second : AudioCategory::SFX;
    sound.second->set_volume(
        get_effective_volume(category, AudioConstants::DEFAULT_VOLUME));
  }

  if (m_musicPlayer != nullptr) {
    m_musicPlayer->set_volume(masterVolume * musicVolume);
  }
}

void AudioSystem::set_sound_volume(float volume) {
  soundVolume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                           AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    if (it != soundCategories.end() && it->second == AudioCategory::SFX) {
      sound.second->set_volume(get_effective_volume(
          AudioCategory::SFX, AudioConstants::DEFAULT_VOLUME));
    }
  }
}

void AudioSystem::set_music_volume(float volume) {
  musicVolume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                           AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resourceMutex);
  if (m_musicPlayer != nullptr) {
    m_musicPlayer->set_volume(masterVolume * musicVolume);
  }
}

void AudioSystem::set_voice_volume(float volume) {
  voiceVolume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                           AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    if (it != soundCategories.end() && it->second == AudioCategory::VOICE) {
      sound.second->set_volume(get_effective_volume(
          AudioCategory::VOICE, AudioConstants::DEFAULT_VOLUME));
    }
  }
}

void AudioSystem::pause_all() {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::PAUSE);
  queue_condition.notify_one();
}

void AudioSystem::resume_all() {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::RESUME);
  queue_condition.notify_one();
}

auto AudioSystem::load_sound(const std::string &sound_id,
                            const std::string &filePath,
                            AudioCategory category) -> bool {
  std::lock_guard<std::mutex> const lock(resourceMutex);
  if (sounds.find(sound_id) != sounds.end()) {
    return true;
  }

  MiniaudioBackend *backend =
      (m_musicPlayer != nullptr) ? m_musicPlayer->get_backend() : nullptr;
  auto sound = std::make_unique<Sound>(filePath, backend);
  if (!sound || !sound->is_loaded()) {
    return false;
  }

  sounds[sound_id] = std::move(sound);
  soundCategories[sound_id] = category;
  activeResources.insert(sound_id);
  return true;
}

auto AudioSystem::load_music(const std::string &music_id,
                            const std::string &filePath) -> bool {
  std::lock_guard<std::mutex> const lock(resourceMutex);

  if (m_musicPlayer == nullptr) {
    qWarning() << "MusicPlayer not initialized";
    return false;
  }

  m_musicPlayer->register_track(music_id, filePath);
  activeResources.insert(music_id);
  return true;
}

void AudioSystem::unload_sound(const std::string &sound_id) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::UNLOAD_RESOURCE, sound_id);
  queue_condition.notify_one();
}

void AudioSystem::unload_music(const std::string &music_id) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::UNLOAD_RESOURCE, music_id);
  queue_condition.notify_one();
}

void AudioSystem::unload_all_sounds() {
  std::lock_guard<std::mutex> const lock(queueMutex);
  for (const auto &sound : sounds) {
    eventQueue.emplace(AudioEventType::UNLOAD_RESOURCE, sound.first);
  }
  queue_condition.notify_one();
}

void AudioSystem::unload_all_music() {
  std::lock_guard<std::mutex> const lock(resourceMutex);

  if (m_musicPlayer != nullptr) {
    m_musicPlayer->stop();
  }

  std::vector<std::string> music_resources;
  for (const auto &res : activeResources) {

    if (sounds.find(res) == sounds.end()) {
      music_resources.push_back(res);
    }
  }
  for (const auto &res : music_resources) {
    activeResources.erase(res);
  }
}

void AudioSystem::set_max_channels(size_t channels) {
  maxChannels = std::max(AudioConstants::MIN_CHANNELS, channels);
}

auto AudioSystem::get_active_channel_count() const -> size_t {
  std::lock_guard<std::mutex> const lock(activeSoundsMutex);
  return activeSounds.size();
}

void AudioSystem::audio_thread_func() {
  while (isRunning) {
    std::unique_lock<std::mutex> lock(queueMutex);
    queue_condition.wait(lock, [this] { return !eventQueue.empty(); });

    while (!eventQueue.empty()) {
      AudioEvent const event = eventQueue.front();
      eventQueue.pop();
      lock.unlock();

      process_event(event);

      if (event.type == AudioEventType::SHUTDOWN) {
        isRunning = false;
        return;
      }

      lock.lock();
    }
  }
}

void AudioSystem::process_event(const AudioEvent &event) {
  switch (event.type) {
  case AudioEventType::PLAY_SOUND: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    auto it = sounds.find(event.resource_id);
    if (it != sounds.end()) {
      if (!can_play_sound(event.priority)) {
        evict_lowest_priority_sound_locked();
      }

      float const effective_vol =
          get_effective_volume(event.category, event.volume);
      it->second->play(effective_vol, event.loop);

      {
        std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);
        activeSounds.push_back({event.resource_id, event.priority, event.loop,
                                event.category,
                                std::chrono::steady_clock::now()});
      }
    }
    break;
  }
  case AudioEventType::PLAY_MUSIC: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    if (m_musicPlayer != nullptr) {
      float const effective_volume = masterVolume * musicVolume * event.volume;
      m_musicPlayer->play(event.resource_id, effective_volume, event.loop);
    }
    break;
  }
  case AudioEventType::STOP_SOUND: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    auto it = sounds.find(event.resource_id);
    if (it != sounds.end()) {
      it->second->stop();

      std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);
      activeSounds.erase(std::remove_if(activeSounds.begin(),
                                        activeSounds.end(),
                                        [&](const ActiveSound &as) {
                                          return as.id == event.resource_id;
                                        }),
                         activeSounds.end());
    }
    break;
  }
  case AudioEventType::STOP_MUSIC: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    if (m_musicPlayer != nullptr) {
      m_musicPlayer->stop();
    }
    break;
  }
  case AudioEventType::PAUSE: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    if (m_musicPlayer != nullptr) {
      m_musicPlayer->pause();
    }
    break;
  }
  case AudioEventType::RESUME: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    if (m_musicPlayer != nullptr) {
      m_musicPlayer->resume();
    }
    break;
  }
  case AudioEventType::UNLOAD_RESOURCE: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    auto sound_it = sounds.find(event.resource_id);
    if (sound_it != sounds.end()) {
      sound_it->second->stop();

      std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);
      activeSounds.erase(std::remove_if(activeSounds.begin(),
                                        activeSounds.end(),
                                        [&](const ActiveSound &as) {
                                          return as.id == event.resource_id;
                                        }),
                         activeSounds.end());

      sounds.erase(sound_it);
      soundCategories.erase(event.resource_id);
      activeResources.erase(event.resource_id);
    }

    activeResources.erase(event.resource_id);
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
  std::lock_guard<std::mutex> const lock(activeSoundsMutex);
  return activeSounds.size() < maxChannels;
}

void AudioSystem::evict_lowest_priority_sound() {
  std::string sound_id_to_stop;

  {
    std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);

    if (activeSounds.empty()) {
      return;
    }

    auto lowest_it =
        std::min_element(activeSounds.begin(), activeSounds.end(),
                         [](const ActiveSound &a, const ActiveSound &b) {
                           if (a.priority != b.priority) {
                             return a.priority < b.priority;
                           }
                           return a.startTime < b.startTime;
                         });

    if (lowest_it != activeSounds.end()) {
      sound_id_to_stop = lowest_it->id;
      activeSounds.erase(lowest_it);
    }
  }

  if (!sound_id_to_stop.empty()) {
    std::lock_guard<std::mutex> const resource_lock(resourceMutex);
    auto it = sounds.find(sound_id_to_stop);
    if (it != sounds.end()) {
      it->second->stop();
    }
  }
}

void AudioSystem::evict_lowest_priority_sound_locked() {
  std::string sound_id_to_stop;

  {
    std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);

    if (activeSounds.empty()) {
      return;
    }

    auto lowest_it =
        std::min_element(activeSounds.begin(), activeSounds.end(),
                         [](const ActiveSound &a, const ActiveSound &b) {
                           if (a.priority != b.priority) {
                             return a.priority < b.priority;
                           }
                           return a.startTime < b.startTime;
                         });

    if (lowest_it != activeSounds.end()) {
      sound_id_to_stop = lowest_it->id;
      activeSounds.erase(lowest_it);
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
  std::lock_guard<std::mutex> const resource_lock(resourceMutex);
  std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);

  activeSounds.erase(std::remove_if(activeSounds.begin(), activeSounds.end(),
                                    [this](const ActiveSound &as) {
                                      if (as.loop) {
                                        return false;
                                      }

                                      auto it = sounds.find(as.id);
                                      return it == sounds.end();
                                    }),
                     activeSounds.end());
}

auto AudioSystem::get_effective_volume(AudioCategory category,
                                     float eventVolume) const -> float {
  float category_volume = NAN;
  switch (category) {
  case AudioCategory::SFX:
    category_volume = soundVolume;
    break;
  case AudioCategory::VOICE:
    category_volume = voiceVolume;
    break;
  case AudioCategory::MUSIC:
    category_volume = musicVolume;
    break;
  default:
    category_volume = soundVolume;
    break;
  }

  return masterVolume * category_volume * eventVolume;
}
