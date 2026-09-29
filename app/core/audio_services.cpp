#include "app/core/audio_services.h"

#include <QDebug>

#include "app/audio/audio_coordinator.h"
#include "app/audio/audio_resource_loader.h"
#include "app/audio/audio_system_proxy.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/audio_system.h"
#include "game/audio/cue_trace.h"

namespace App::Core {

AudioServices::AudioServices() = default;

AudioServices::~AudioServices() = default;

void AudioServices::initialize_audio_system(QObject* proxy_parent) {
  if (AudioSystem::get_instance().initialize()) {
    AudioResourceLoader::load_audio_resources();
    AudioResourceLoader::load_audio_cues();
  } else {
    qWarning() << "Failed to initialize AudioSystem";
  }

  m_proxy = std::make_unique<App::Models::AudioSystemProxy>(proxy_parent);
}

void AudioServices::create_event_handling(Engine::Core::World* world,
                                          Game::Systems::NationRegistry& nations) {
  m_event_handler = std::make_unique<Game::Audio::AudioEventHandler>(world);
  m_coordinator = std::make_unique<AudioCoordinator>(m_event_handler.get(), nations);
}

void AudioServices::initialize_event_handler(int local_owner_id) {
  if (m_event_handler->initialize()) {
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Screen);
    m_coordinator->configure_audio_manifest_mappings(local_owner_id);

    qInfo() << "Audio mappings configured";
  } else {
    qWarning() << "Failed to initialize AudioEventHandler";
  }
}

auto AudioServices::proxy() -> QObject* {
  return m_proxy.get();
}

void AudioServices::set_frontend_context(const QString& context) {
  const QString normalized = context.trimmed().toLower();
  if (m_frontend_context == normalized) {
    return;
  }

  m_frontend_context = normalized;
  m_coordinator->apply_frontend_music_context(normalized);
}

void AudioServices::shutdown(const std::string& map_path) {
  if (m_event_handler) {
    m_event_handler->shutdown();
  }
  Game::Audio::CueTrace::instance().write_requested_summary(map_path);
  AudioSystem::get_instance().shutdown();
  qInfo() << "AudioSystem shut down";
}

} // namespace App::Core
