#include "AudioSystem.h"

#include "MiniaudioBackend.h"
#include "MusicPlayer.h"
#include "Sound.h"
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

auto AudioSystem::getInstance() -> AudioSystem & {
  static AudioSystem instance;
  return instance;
}

auto AudioSystem::initialize() -> bool {
  if (isRunning) {
    return true;
  }

  m_musicPlayer = &Game::Audio::MusicPlayer::getInstance();
  if (!m_musicPlayer->initialize()) {
    qWarning() << "Failed to initialize MusicPlayer";
    return false;
  }

  isRunning = true;
  audioThread = std::thread(&AudioSystem::audioThreadFunc, this);

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
  queueCondition.notify_one();

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

void AudioSystem::playSound(const std::string &soundId, float volume, bool loop,
                            int priority, AudioCategory category) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::PLAY_SOUND, soundId, volume, loop,
                     priority, category);
  queueCondition.notify_one();
}

void AudioSystem::playMusic(const std::string &musicId, float volume, bool) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::PLAY_MUSIC, musicId, volume);
  queueCondition.notify_one();
}

void AudioSystem::stopSound(const std::string &soundId) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::STOP_SOUND, soundId);
  queueCondition.notify_one();
}

void AudioSystem::stopMusic() {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::STOP_MUSIC);
  queueCondition.notify_one();
}

void AudioSystem::setMasterVolume(float volume) {
  masterVolume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                            AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    AudioCategory const category =
        (it != soundCategories.end()) ? it->second : AudioCategory::SFX;
    sound.second->set_volume(
        getEffectiveVolume(category, AudioConstants::DEFAULT_VOLUME));
  }

  if (m_musicPlayer != nullptr) {
    m_musicPlayer->setVolume(masterVolume * musicVolume);
  }
}

void AudioSystem::setSoundVolume(float volume) {
  soundVolume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                           AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    if (it != soundCategories.end() && it->second == AudioCategory::SFX) {
      sound.second->set_volume(getEffectiveVolume(
          AudioCategory::SFX, AudioConstants::DEFAULT_VOLUME));
    }
  }
}

void AudioSystem::setMusicVolume(float volume) {
  musicVolume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                           AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resourceMutex);
  if (m_musicPlayer != nullptr) {
    m_musicPlayer->setVolume(masterVolume * musicVolume);
  }
}

void AudioSystem::setVoiceVolume(float volume) {
  voiceVolume = std::clamp(volume, AudioConstants::MIN_VOLUME,
                           AudioConstants::MAX_VOLUME);

  std::lock_guard<std::mutex> const lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    if (it != soundCategories.end() && it->second == AudioCategory::VOICE) {
      sound.second->set_volume(getEffectiveVolume(
          AudioCategory::VOICE, AudioConstants::DEFAULT_VOLUME));
    }
  }
}

void AudioSystem::pauseAll() {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::PAUSE);
  queueCondition.notify_one();
}

void AudioSystem::resumeAll() {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::RESUME);
  queueCondition.notify_one();
}

auto AudioSystem::loadSound(const std::string &soundId,
                            const std::string &filePath,
                            AudioCategory category) -> bool {
  std::lock_guard<std::mutex> const lock(resourceMutex);
  if (sounds.find(soundId) != sounds.end()) {
    return true;
  }

  MiniaudioBackend *backend =
      (m_musicPlayer != nullptr) ? m_musicPlayer->getBackend() : nullptr;
  auto sound = std::make_unique<Sound>(filePath, backend);
  if (!sound || !sound->is_loaded()) {
    return false;
  }

  sounds[soundId] = std::move(sound);
  soundCategories[soundId] = category;
  activeResources.insert(soundId);
  return true;
}

auto AudioSystem::loadMusic(const std::string &musicId,
                            const std::string &filePath) -> bool {
  std::lock_guard<std::mutex> const lock(resourceMutex);

  if (m_musicPlayer == nullptr) {
    qWarning() << "MusicPlayer not initialized";
    return false;
  }

  m_musicPlayer->registerTrack(musicId, filePath);
  activeResources.insert(musicId);
  return true;
}

void AudioSystem::unloadSound(const std::string &soundId) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::UNLOAD_RESOURCE, soundId);
  queueCondition.notify_one();
}

void AudioSystem::unloadMusic(const std::string &musicId) {
  std::lock_guard<std::mutex> const lock(queueMutex);
  eventQueue.emplace(AudioEventType::UNLOAD_RESOURCE, musicId);
  queueCondition.notify_one();
}

void AudioSystem::unloadAllSounds() {
  std::lock_guard<std::mutex> const lock(queueMutex);
  for (const auto &sound : sounds) {
    eventQueue.emplace(AudioEventType::UNLOAD_RESOURCE, sound.first);
  }
  queueCondition.notify_one();
}

void AudioSystem::unloadAllMusic() {
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

void AudioSystem::setMaxChannels(size_t channels) {
  maxChannels = std::max(AudioConstants::MIN_CHANNELS, channels);
}

auto AudioSystem::getActiveChannelCount() const -> size_t {
  std::lock_guard<std::mutex> const lock(activeSoundsMutex);
  return activeSounds.size();
}

void AudioSystem::audioThreadFunc() {
  while (isRunning) {
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCondition.wait(lock, [this] { return !eventQueue.empty(); });

    while (!eventQueue.empty()) {
      AudioEvent const event = eventQueue.front();
      eventQueue.pop();
      lock.unlock();

      processEvent(event);

      if (event.type == AudioEventType::SHUTDOWN) {
        isRunning = false;
        return;
      }

      lock.lock();
    }
  }
}

void AudioSystem::processEvent(const AudioEvent &event) {
  switch (event.type) {
  case AudioEventType::PLAY_SOUND: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    auto it = sounds.find(event.resourceId);
    if (it != sounds.end()) {
      if (!canPlaySound(event.priority)) {
        evictLowestPrioritySoundLocked();
      }

      float const effective_vol =
          getEffectiveVolume(event.category, event.volume);
      it->second->play(effective_vol, event.loop);

      {
        std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);
        activeSounds.push_back({event.resourceId, event.priority, event.loop,
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
      m_musicPlayer->play(event.resourceId, effective_volume, event.loop);
    }
    break;
  }
  case AudioEventType::STOP_SOUND: {
    std::lock_guard<std::mutex> const lock(resourceMutex);
    auto it = sounds.find(event.resourceId);
    if (it != sounds.end()) {
      it->second->stop();

      std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);
      activeSounds.erase(std::remove_if(activeSounds.begin(),
                                        activeSounds.end(),
                                        [&](const ActiveSound &as) {
                                          return as.id == event.resourceId;
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
    auto sound_it = sounds.find(event.resourceId);
    if (sound_it != sounds.end()) {
      sound_it->second->stop();

      std::lock_guard<std::mutex> const active_lock(activeSoundsMutex);
      activeSounds.erase(std::remove_if(activeSounds.begin(),
                                        activeSounds.end(),
                                        [&](const ActiveSound &as) {
                                          return as.id == event.resourceId;
                                        }),
                         activeSounds.end());

      sounds.erase(sound_it);
      soundCategories.erase(event.resourceId);
      activeResources.erase(event.resourceId);
    }

    activeResources.erase(event.resourceId);
    break;
  }
  case AudioEventType::CLEANUP_INACTIVE: {
    cleanupInactiveSounds();
    break;
  }
  case AudioEventType::SET_VOLUME:
  case AudioEventType::SHUTDOWN:
    break;
  }
}

auto AudioSystem::canPlaySound(int) -> bool {
  std::lock_guard<std::mutex> const lock(activeSoundsMutex);
  return activeSounds.size() < maxChannels;
}

void AudioSystem::evictLowestPrioritySound() {
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

void AudioSystem::evictLowestPrioritySoundLocked() {
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

void AudioSystem::cleanupInactiveSounds() {
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

auto AudioSystem::getEffectiveVolume(AudioCategory category,
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
