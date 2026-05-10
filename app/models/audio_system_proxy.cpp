#include "audio_system_proxy.h"
#include "game/audio/audio_system.h"
#include <qobject.h>

namespace App::Models {

AudioSystemProxy::AudioSystemProxy(QObject *parent) : QObject(parent) {}

void AudioSystemProxy::setMasterVolume(float volume) {
  AudioSystem::get_instance().set_master_volume(volume);
}

void AudioSystemProxy::setMusicVolume(float volume) {
  AudioSystem::get_instance().set_music_volume(volume);
}

void AudioSystemProxy::setSoundVolume(float volume) {
  AudioSystem::get_instance().set_sound_volume(volume);
}

void AudioSystemProxy::setVoiceVolume(float volume) {
  AudioSystem::get_instance().set_voice_volume(volume);
}

auto AudioSystemProxy::getMasterVolume() -> float {
  return AudioSystem::get_instance().get_master_volume();
}

auto AudioSystemProxy::getMusicVolume() -> float {
  return AudioSystem::get_instance().get_music_volume();
}

auto AudioSystemProxy::getSoundVolume() -> float {
  return AudioSystem::get_instance().get_sound_volume();
}

auto AudioSystemProxy::getVoiceVolume() -> float {
  return AudioSystem::get_instance().get_voice_volume();
}

} // namespace App::Models
