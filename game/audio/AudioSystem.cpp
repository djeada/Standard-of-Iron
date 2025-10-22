#include "AudioSystem.h"
#include "MusicPlayer.h"
#include "Sound.h"
#include "MiniaudioBackend.h"
#include <QDebug>
#include <algorithm>

AudioSystem::AudioSystem()
    : isRunning(false), masterVolume(1.0f), soundVolume(1.0f),
      musicVolume(1.0f), voiceVolume(1.0f), maxChannels(32), m_musicPlayer(nullptr) {}

AudioSystem::~AudioSystem() { shutdown(); }

AudioSystem &AudioSystem::getInstance() {
  static AudioSystem instance;
  return instance;
}

bool AudioSystem::initialize() {
  if (isRunning) {
    return true;
  }

  // Initialize the singleton music player
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
    std::lock_guard<std::mutex> lock(queueMutex);
    eventQueue.push(AudioEvent(AudioEventType::SHUTDOWN));
  }
  queueCondition.notify_one();

  if (audioThread.joinable()) {
    audioThread.join();
  }

  // Shutdown music player
  if (m_musicPlayer) {
    m_musicPlayer->shutdown();
    m_musicPlayer = nullptr;
  }

  sounds.clear();
  soundCategories.clear();
  activeResources.clear();

  {
    std::lock_guard<std::mutex> lock(activeSoundsMutex);
    activeSounds.clear();
  }
}

void AudioSystem::playSound(const std::string &soundId, float volume, bool loop,
                            int priority, AudioCategory category) {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(AudioEvent(AudioEventType::PLAY_SOUND, soundId, volume, loop,
                              priority, category));
  queueCondition.notify_one();
}

void AudioSystem::playMusic(const std::string &musicId, float volume,
                            bool crossfade) {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(AudioEvent(AudioEventType::PLAY_MUSIC, musicId, volume));
  queueCondition.notify_one();
}

void AudioSystem::stopSound(const std::string &soundId) {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(AudioEvent(AudioEventType::STOP_SOUND, soundId));
  queueCondition.notify_one();
}

void AudioSystem::stopMusic() {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(AudioEvent(AudioEventType::STOP_MUSIC));
  queueCondition.notify_one();
}

void AudioSystem::setMasterVolume(float volume) {
  masterVolume = std::clamp(volume, 0.0f, 1.0f);

  std::lock_guard<std::mutex> lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    AudioCategory category =
        (it != soundCategories.end()) ? it->second : AudioCategory::SFX;
    sound.second->setVolume(getEffectiveVolume(category, 1.0f));
  }
  // Update music player volume
  if (m_musicPlayer) {
    m_musicPlayer->setVolume(masterVolume * musicVolume);
  }
}

void AudioSystem::setSoundVolume(float volume) {
  soundVolume = std::clamp(volume, 0.0f, 1.0f);

  std::lock_guard<std::mutex> lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    if (it != soundCategories.end() && it->second == AudioCategory::SFX) {
      sound.second->setVolume(getEffectiveVolume(AudioCategory::SFX, 1.0f));
    }
  }
}

void AudioSystem::setMusicVolume(float volume) {
  musicVolume = std::clamp(volume, 0.0f, 1.0f);

  std::lock_guard<std::mutex> lock(resourceMutex);
  if (m_musicPlayer) {
    m_musicPlayer->setVolume(masterVolume * musicVolume);
  }
}

void AudioSystem::setVoiceVolume(float volume) {
  voiceVolume = std::clamp(volume, 0.0f, 1.0f);

  std::lock_guard<std::mutex> lock(resourceMutex);
  for (auto &sound : sounds) {
    auto it = soundCategories.find(sound.first);
    if (it != soundCategories.end() && it->second == AudioCategory::VOICE) {
      sound.second->setVolume(getEffectiveVolume(AudioCategory::VOICE, 1.0f));
    }
  }
}

void AudioSystem::pauseAll() {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(AudioEvent(AudioEventType::PAUSE));
  queueCondition.notify_one();
}

void AudioSystem::resumeAll() {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(AudioEvent(AudioEventType::RESUME));
  queueCondition.notify_one();
}

bool AudioSystem::loadSound(const std::string &soundId,
                            const std::string &filePath,
                            AudioCategory category) {
  std::lock_guard<std::mutex> lock(resourceMutex);
  if (sounds.find(soundId) != sounds.end()) {
    return true;
  }

  // Get backend from MusicPlayer for sound effects
  MiniaudioBackend* backend = m_musicPlayer ? m_musicPlayer->getBackend() : nullptr;
  auto sound = std::make_unique<Sound>(filePath, backend);
  if (!sound || !sound->isLoaded()) {
    return false;
  }

  sounds[soundId] = std::move(sound);
  soundCategories[soundId] = category;
  activeResources.insert(soundId);
  return true;
}

bool AudioSystem::loadMusic(const std::string &musicId,
                            const std::string &filePath) {
  std::lock_guard<std::mutex> lock(resourceMutex);
  
  if (!m_musicPlayer) {
    qWarning() << "MusicPlayer not initialized";
    return false;
  }

  // Register the track with the music player
  m_musicPlayer->registerTrack(musicId, filePath);
  activeResources.insert(musicId);
  return true;
}

void AudioSystem::unloadSound(const std::string &soundId) {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(AudioEvent(AudioEventType::UNLOAD_RESOURCE, soundId));
  queueCondition.notify_one();
}

void AudioSystem::unloadMusic(const std::string &musicId) {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(AudioEvent(AudioEventType::UNLOAD_RESOURCE, musicId));
  queueCondition.notify_one();
}

void AudioSystem::unloadAllSounds() {
  std::lock_guard<std::mutex> lock(queueMutex);
  for (const auto &sound : sounds) {
    eventQueue.push(AudioEvent(AudioEventType::UNLOAD_RESOURCE, sound.first));
  }
  queueCondition.notify_one();
}

void AudioSystem::unloadAllMusic() {
  std::lock_guard<std::mutex> lock(resourceMutex);
  // Stop the music player and clear all registered tracks
  if (m_musicPlayer) {
    m_musicPlayer->stop();
  }
  // Clear music-related resources from active set
  std::vector<std::string> musicResources;
  for (const auto& res : activeResources) {
    // Heuristic: if it's not in sounds map, it's likely a music resource
    if (sounds.find(res) == sounds.end()) {
      musicResources.push_back(res);
    }
  }
  for (const auto& res : musicResources) {
    activeResources.erase(res);
  }
}

void AudioSystem::setMaxChannels(size_t channels) {
  maxChannels = std::max(size_t(1), channels);
}

size_t AudioSystem::getActiveChannelCount() const {
  std::lock_guard<std::mutex> lock(activeSoundsMutex);
  return activeSounds.size();
}

void AudioSystem::audioThreadFunc() {
  while (isRunning) {
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCondition.wait(lock, [this] { return !eventQueue.empty(); });

    while (!eventQueue.empty()) {
      AudioEvent event = eventQueue.front();
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
    std::lock_guard<std::mutex> lock(resourceMutex);
    auto it = sounds.find(event.resourceId);
    if (it != sounds.end()) {
      if (!canPlaySound(event.priority)) {
        evictLowestPrioritySoundLocked();
      }

      float effectiveVol = getEffectiveVolume(event.category, event.volume);
      it->second->play(effectiveVol, event.loop);

      {
        std::lock_guard<std::mutex> activeLock(activeSoundsMutex);
        activeSounds.push_back({event.resourceId, event.priority, event.loop,
                                event.category,
                                std::chrono::steady_clock::now()});
      }
    }
    break;
  }
  case AudioEventType::PLAY_MUSIC: {
    std::lock_guard<std::mutex> lock(resourceMutex);
    if (m_musicPlayer) {
      float effectiveVolume = masterVolume * musicVolume * event.volume;
      m_musicPlayer->play(event.resourceId, effectiveVolume, event.loop);
    }
    break;
  }
  case AudioEventType::STOP_SOUND: {
    std::lock_guard<std::mutex> lock(resourceMutex);
    auto it = sounds.find(event.resourceId);
    if (it != sounds.end()) {
      it->second->stop();

      std::lock_guard<std::mutex> activeLock(activeSoundsMutex);
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
    std::lock_guard<std::mutex> lock(resourceMutex);
    if (m_musicPlayer) {
      m_musicPlayer->stop();
    }
    break;
  }
  case AudioEventType::PAUSE: {
    std::lock_guard<std::mutex> lock(resourceMutex);
    if (m_musicPlayer) {
      m_musicPlayer->pause();
    }
    break;
  }
  case AudioEventType::RESUME: {
    std::lock_guard<std::mutex> lock(resourceMutex);
    if (m_musicPlayer) {
      m_musicPlayer->resume();
    }
    break;
  }
  case AudioEventType::UNLOAD_RESOURCE: {
    std::lock_guard<std::mutex> lock(resourceMutex);
    auto soundIt = sounds.find(event.resourceId);
    if (soundIt != sounds.end()) {
      soundIt->second->stop();

      std::lock_guard<std::mutex> activeLock(activeSoundsMutex);
      activeSounds.erase(std::remove_if(activeSounds.begin(),
                                        activeSounds.end(),
                                        [&](const ActiveSound &as) {
                                          return as.id == event.resourceId;
                                        }),
                         activeSounds.end());

      sounds.erase(soundIt);
      soundCategories.erase(event.resourceId);
      activeResources.erase(event.resourceId);
    }

    // Music tracks are just registered IDs, nothing to unload from player
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

bool AudioSystem::canPlaySound(int priority) {
  std::lock_guard<std::mutex> lock(activeSoundsMutex);
  return activeSounds.size() < maxChannels;
}

void AudioSystem::evictLowestPrioritySound() {
  std::string soundIdToStop;
  
  {
    std::lock_guard<std::mutex> activeLock(activeSoundsMutex);

    if (activeSounds.empty()) {
      return;
    }

    auto lowestIt = std::min_element(
        activeSounds.begin(), activeSounds.end(),
        [](const ActiveSound &a, const ActiveSound &b) {
          if (a.priority != b.priority) {
            return a.priority < b.priority;
          }
          return a.startTime < b.startTime;
        });

    if (lowestIt != activeSounds.end()) {
      soundIdToStop = lowestIt->id;
      activeSounds.erase(lowestIt);
    }
  }
  
  if (!soundIdToStop.empty()) {
    std::lock_guard<std::mutex> resourceLock(resourceMutex);
    auto it = sounds.find(soundIdToStop);
    if (it != sounds.end()) {
      it->second->stop();
    }
  }
}

void AudioSystem::evictLowestPrioritySoundLocked() {
  std::string soundIdToStop;
  
  {
    std::lock_guard<std::mutex> activeLock(activeSoundsMutex);

    if (activeSounds.empty()) {
      return;
    }

    auto lowestIt = std::min_element(
        activeSounds.begin(), activeSounds.end(),
        [](const ActiveSound &a, const ActiveSound &b) {
          if (a.priority != b.priority) {
            return a.priority < b.priority;
          }
          return a.startTime < b.startTime;
        });

    if (lowestIt != activeSounds.end()) {
      soundIdToStop = lowestIt->id;
      activeSounds.erase(lowestIt);
    }
  }
  
  if (!soundIdToStop.empty()) {
    auto it = sounds.find(soundIdToStop);
    if (it != sounds.end()) {
      it->second->stop();
    }
  }
}

void AudioSystem::cleanupInactiveSounds() {
  std::lock_guard<std::mutex> resourceLock(resourceMutex);
  std::lock_guard<std::mutex> activeLock(activeSoundsMutex);

  activeSounds.erase(
      std::remove_if(activeSounds.begin(), activeSounds.end(),
                     [this](const ActiveSound &as) {
                       if (as.loop) {
                         return false;
                       }

                       auto it = sounds.find(as.id);
                       if (it == sounds.end()) {
                         return true;
                       }

                       return false;
                     }),
      activeSounds.end());
}

float AudioSystem::getEffectiveVolume(AudioCategory category,
                                      float eventVolume) const {
  float categoryVolume;
  switch (category) {
  case AudioCategory::SFX:
    categoryVolume = soundVolume;
    break;
  case AudioCategory::VOICE:
    categoryVolume = voiceVolume;
    break;
  case AudioCategory::MUSIC:
    categoryVolume = musicVolume;
    break;
  default:
    categoryVolume = soundVolume;
    break;
  }

  return masterVolume * categoryVolume * eventVolume;
}
