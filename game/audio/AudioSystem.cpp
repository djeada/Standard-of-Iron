#include "AudioSystem.h"
#include "Music.h"
#include "Sound.h"
#include <algorithm>

AudioSystem::AudioSystem()
    : isRunning(false), masterVolume(1.0f), soundVolume(1.0f),
      musicVolume(1.0f) {}

AudioSystem::~AudioSystem() { shutdown(); }

AudioSystem &AudioSystem::getInstance() {
  static AudioSystem instance;
  return instance;
}

bool AudioSystem::initialize() {
  if (isRunning) {
    return true;
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

  sounds.clear();
  music.clear();
  activeSounds.clear();
}

void AudioSystem::playSound(const std::string &soundId, float volume, bool loop,
                            int priority) {
  std::lock_guard<std::mutex> lock(queueMutex);
  eventQueue.push(
      AudioEvent(AudioEventType::PLAY_SOUND, soundId, volume, loop, priority));
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
  for (auto &sound : sounds) {
    sound.second->setVolume(masterVolume * soundVolume);
  }
  for (auto &musicTrack : music) {
    musicTrack.second->setVolume(masterVolume * musicVolume);
  }
}

void AudioSystem::setSoundVolume(float volume) {
  soundVolume = std::clamp(volume, 0.0f, 1.0f);
  for (auto &sound : sounds) {
    sound.second->setVolume(masterVolume * soundVolume);
  }
}

void AudioSystem::setMusicVolume(float volume) {
  musicVolume = std::clamp(volume, 0.0f, 1.0f);
  for (auto &musicTrack : music) {
    musicTrack.second->setVolume(masterVolume * musicVolume);
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
                            const std::string &filePath) {
  auto sound = std::make_unique<Sound>(filePath);
  if (!sound->isLoaded()) {
    return false;
  }
  sounds[soundId] = std::move(sound);
  return true;
}

bool AudioSystem::loadMusic(const std::string &musicId,
                            const std::string &filePath) {
  auto musicTrack = std::make_unique<Music>(filePath);
  if (!musicTrack->isLoaded()) {
    return false;
  }
  music[musicId] = std::move(musicTrack);
  return true;
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
    auto it = sounds.find(event.resourceId);
    if (it != sounds.end()) {
      it->second->play(masterVolume * soundVolume * event.volume, event.loop);
      activeSounds.push_back({event.resourceId, event.priority, event.loop});
    }
    break;
  }
  case AudioEventType::PLAY_MUSIC: {
    auto it = music.find(event.resourceId);
    if (it != music.end()) {
      for (auto &musicTrack : music) {
        musicTrack.second->stop();
      }
      it->second->play(masterVolume * musicVolume * event.volume, event.loop);
    }
    break;
  }
  case AudioEventType::STOP_SOUND: {
    auto it = sounds.find(event.resourceId);
    if (it != sounds.end()) {
      it->second->stop();
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
    for (auto &musicTrack : music) {
      musicTrack.second->stop();
    }
    break;
  }
  case AudioEventType::PAUSE: {
    for (auto &musicTrack : music) {
      musicTrack.second->pause();
    }
    break;
  }
  case AudioEventType::RESUME: {
    for (auto &musicTrack : music) {
      musicTrack.second->resume();
    }
    break;
  }
  case AudioEventType::SET_VOLUME:
  case AudioEventType::SHUTDOWN:
    break;
  }
}
