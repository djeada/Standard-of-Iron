#include "audio_system_proxy.h"
#include "game/audio/audio_system.h"
#include <qobject.h>

namespace App::Models {

AudioSystemProxy::AudioSystemProxy(QObject *parent) : QObject(parent) {}

void AudioSystemProxy::setMasterVolume(float volume) {
  AudioSystem::getInstance().setMasterVolume(volume);
}

void AudioSystemProxy::setMusicVolume(float volume) {
  AudioSystem::getInstance().setMusicVolume(volume);
}

void AudioSystemProxy::setSoundVolume(float volume) {
  AudioSystem::getInstance().setSoundVolume(volume);
}

void AudioSystemProxy::setVoiceVolume(float volume) {
  AudioSystem::getInstance().setVoiceVolume(volume);
}

auto AudioSystemProxy::getMasterVolume() -> float {
  return AudioSystem::getInstance().getMasterVolume();
}

auto AudioSystemProxy::getMusicVolume() -> float {
  return AudioSystem::getInstance().getMusicVolume();
}

auto AudioSystemProxy::getSoundVolume() -> float {
  return AudioSystem::getInstance().getSoundVolume();
}

auto AudioSystemProxy::getVoiceVolume() -> float {
  return AudioSystem::getInstance().getVoiceVolume();
}

} // namespace App::Models
