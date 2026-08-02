#include "audio_system_proxy.h"

#include <QDebug>
#include <qobject.h>

#include "game/audio/audio_cues.h"
#include "game/audio/audio_system.h"

namespace App::Models {

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

void AudioSystemProxy::play_cue(const QString& cue_id) {
  Game::Audio::play_cue(cue_id.toStdString());
}

void AudioSystemProxy::play_ui_hover() {
  Game::Audio::play_cue(Game::Audio::Cue::k_ui_hover);
}

void AudioSystemProxy::play_ui_click() {
  Game::Audio::play_cue(Game::Audio::Cue::k_ui_click);
}

void AudioSystemProxy::play_ui_back() {
  Game::Audio::play_cue(Game::Audio::Cue::k_ui_back);
}

void AudioSystemProxy::play_ui_tab_switch() {
  Game::Audio::play_cue(Game::Audio::Cue::k_ui_tab_switch);
}

void AudioSystemProxy::play_ui_panel_open() {
  Game::Audio::play_cue(Game::Audio::Cue::k_ui_panel_open);
}

void AudioSystemProxy::play_ui_panel_close() {
  Game::Audio::play_cue(Game::Audio::Cue::k_ui_panel_close);
}

void AudioSystemProxy::play_ui_toggle() {
  Game::Audio::play_cue(Game::Audio::Cue::k_ui_toggle);
}

void AudioSystemProxy::play_ui_error() {
  Game::Audio::play_cue(Game::Audio::Cue::k_ui_error);
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
