#include "audio_system_proxy.h"
#include "game/audio/AudioSystem.h"

namespace App {
namespace Models {

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

float AudioSystemProxy::getMasterVolume() const {
  return AudioSystem::getInstance().getMasterVolume();
}

float AudioSystemProxy::getMusicVolume() const {
  return AudioSystem::getInstance().getMusicVolume();
}

float AudioSystemProxy::getSoundVolume() const {
  return AudioSystem::getInstance().getSoundVolume();
}

float AudioSystemProxy::getVoiceVolume() const {
  return AudioSystem::getInstance().getVoiceVolume();
}

} 
} 
