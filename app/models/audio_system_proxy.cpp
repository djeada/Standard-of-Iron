#include "audio_system_proxy.h"

#include <QDebug>
#include <qobject.h>

#include "app/core/audio_resource_loader.h"
#include "game/audio/audio_system.h"

namespace App::Models {
namespace {

void play_ui_sound(const QString& sound_id, float volume, int priority) {
  if (!AudioResourceLoader::ensure_audio_resource_loaded(sound_id)) {
    qWarning() << "UI audio resource unavailable:" << sound_id;
    return;
  }

  AudioSystem::get_instance().play_sound(sound_id.toStdString(),
                                         volume,
                                         false,
                                         priority,
                                         AudioCategory::SFX);
}

} // namespace

AudioSystemProxy::AudioSystemProxy(QObject* parent)
    : QObject(parent) {
}

void AudioSystemProxy::set_master_volume(float volume) {
  AudioSystem::get_instance().set_master_volume(volume);
}

void AudioSystemProxy::set_music_volume(float volume) {
  AudioSystem::get_instance().set_music_volume(volume);
}

void AudioSystemProxy::set_sound_volume(float volume) {
  AudioSystem::get_instance().set_sound_volume(volume);
}

void AudioSystemProxy::set_voice_volume(float volume) {
  AudioSystem::get_instance().set_voice_volume(volume);
}

void AudioSystemProxy::set_ambience_volume(float volume) {
  AudioSystem::get_instance().set_ambience_volume(volume);
}

void AudioSystemProxy::play_ui_hover() {
  static const QString sound_id = QStringLiteral("ui.hover.soft");
  play_ui_sound(sound_id, 1.0F, AudioConstants::DEFAULT_PRIORITY);
}

void AudioSystemProxy::play_ui_click() {
  static const QString sound_id = QStringLiteral("ui.click.primary");
  play_ui_sound(sound_id, 1.0F, AudioConstants::DEFAULT_PRIORITY);
}

auto AudioSystemProxy::get_master_volume() -> float {
  return AudioSystem::get_instance().get_master_volume();
}

auto AudioSystemProxy::get_music_volume() -> float {
  return AudioSystem::get_instance().get_music_volume();
}

auto AudioSystemProxy::get_sound_volume() -> float {
  return AudioSystem::get_instance().get_sound_volume();
}

auto AudioSystemProxy::get_voice_volume() -> float {
  return AudioSystem::get_instance().get_voice_volume();
}

auto AudioSystemProxy::get_ambience_volume() -> float {
  return AudioSystem::get_instance().get_ambience_volume();
}

} // namespace App::Models
