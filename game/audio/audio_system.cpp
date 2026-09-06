#include "audio_system.h"

#include <QDebug>
#include <qglobal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "audio_settings.h"
#include "cue_trace.h"
#include "miniaudio_backend.h"
#include "music_player.h"
#include "sound.h"

namespace {

auto sanitize_volume(float volume) -> float {
  if (!std::isfinite(volume)) {
    return AudioConstants::MIN_VOLUME;
  }
  return std::clamp(volume, AudioConstants::MIN_VOLUME, AudioConstants::MAX_VOLUME);
}

auto is_effectively_muted(float volume) -> bool {
  static constexpr float MUTED_EPSILON = 0.0001F;
  return volume <= MUTED_EPSILON;
}

void trace_cue_result(const std::string& cue_id,
                      const std::string& resource_id,
                      Game::Audio::CueOutcome outcome,
                      const std::string& source) {
  Game::Audio::CueTrace::instance().record(cue_id, resource_id, outcome, source);
}

} // namespace

AudioSystem::AudioSystem()
    : is_running(false)
    , master_volume(Game::Audio::Settings::k_first_run_master_volume)
    , sound_volume(Game::Audio::Settings::k_first_run_sound_volume)
    , music_volume(Game::Audio::Settings::k_first_run_music_volume)
    , voice_volume(Game::Audio::Settings::k_first_run_voice_volume)
    , ambience_volume(Game::Audio::Settings::k_first_run_ambience_volume) {
  load_persisted_volumes();
}

AudioSystem::~AudioSystem() {
  shutdown();
}

auto AudioSystem::get_instance() -> AudioSystem& {
  static AudioSystem instance;
  return instance;
}

auto AudioSystem::initialize() -> bool {
  if (is_running) {
    return true;
  }

  m_music_player = &Game::Audio::MusicPlayer::get_instance();
  if (!m_music_player->initialize()) {
    qWarning() << "Failed to initialize MusicPlayer";
    return false;
  }

  m_music_player->set_volume(master_volume.load() * music_volume.load());

  m_music_player->get_backend()->set_listening_preset(
      Game::Audio::preset_from_int(listening_preset.load()));
  is_running = true;
  audio_thread = std::thread(&AudioSystem::audio_thread_func, this);

  return true;
}

void AudioSystem::render_offline(float* interleaved_stereo, unsigned frames) {
  if (m_music_player == nullptr) {
    std::fill_n(interleaved_stereo, static_cast<std::size_t>(frames) * 2U, 0.0F);
    return;
  }
  m_music_player->render_offline(interleaved_stereo, frames);
}

void AudioSystem::shutdown() {
  Game::Audio::CueTrace::instance().write_requested_summary();

  if (!is_running) {
    return;
  }

  {
    std::lock_guard<std::mutex> const lock(queue_mutex);
    event_queue.emplace(AudioEventType::SHUTDOWN);
  }
  queue_condition.notify_one();

  if (audio_thread.joinable()) {
    audio_thread.join();
  }

  if (m_music_player != nullptr) {
    m_music_player->shutdown();
    m_music_player = nullptr;
  }

  sounds.clear();
  resource_configs.clear();
  resource_aliases.clear();
  resource_last_played_at.clear();
  active_resources.clear();

  {
    std::lock_guard<std::mutex> const lock(active_sounds_mutex);
    active_sounds.clear();
  }
}

void AudioSystem::enqueue(AudioEvent&& event) {

  if (!is_running) {
    return;
  }

  std::lock_guard<std::mutex> const lock(queue_mutex);
  event_queue.push(std::move(event));
  queue_condition.notify_one();
}

void AudioSystem::play_sound(const std::string& sound_id,
                             float volume,
                             bool loop,
                             int priority,
                             AudioCategory category,
                             std::string cue_id,
                             std::string cue_source,
                             const Game::Audio::WorldPoint* position) {
  if (!is_running) {
    trace_cue_result(
        cue_id, sound_id, Game::Audio::CueOutcome::SystemStopped, cue_source);
    return;
  }

  AudioEvent event(AudioEventType::PLAY_SOUND,
                   sound_id,
                   volume,
                   loop,
                   priority,
                   category,
                   false,
                   std::move(cue_id),
                   std::move(cue_source));
  if (position != nullptr) {
    event.position = *position;
    event.positioned = true;
  }
  enqueue(std::move(event));
}

void AudioSystem::set_listener(const Game::Audio::AudioListener& incoming) {
  std::lock_guard<std::mutex> const lock(listener_mutex);
  m_listener = incoming;
}

auto AudioSystem::listener() const -> Game::Audio::AudioListener {
  std::lock_guard<std::mutex> const lock(listener_mutex);
  return m_listener;
}

void AudioSystem::play_music(const std::string& music_id,
                             float volume,
                             Game::Audio::MusicTransition transition) {
  enqueue(AudioEvent(AudioEventType::PLAY_MUSIC,
                     music_id,
                     volume,
                     true,
                     AudioConstants::DEFAULT_PRIORITY,
                     AudioCategory::MUSIC,
                     transition == Game::Audio::MusicTransition::Crossfade));
}

auto AudioSystem::is_sound_playing(const std::string& sound_id) const -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  const std::string resource_id = resolve_resource_id_locked(sound_id);
  auto it = sounds.find(resource_id);
  return it != sounds.end() && it->second->is_playing();
}

void AudioSystem::set_playing_sound_volume(const std::string& sound_id, float volume) {
  enqueue(AudioEvent(AudioEventType::SET_SOUND_LEVEL, sound_id, volume));
}

void AudioSystem::stop_sound(const std::string& sound_id) {
  enqueue(AudioEvent(AudioEventType::STOP_SOUND, sound_id));
}

void AudioSystem::stop_music() {
  enqueue(AudioEvent(AudioEventType::STOP_MUSIC));
}

void AudioSystem::set_listening_preset(int preset) {
  const auto selected = Game::Audio::preset_from_int(preset);
  listening_preset = static_cast<int>(selected);
  Game::Audio::Settings::save_listening_preset(listening_preset.load());
  if (m_music_player != nullptr && m_music_player->get_backend() != nullptr) {
    m_music_player->get_backend()->set_listening_preset(selected);
  }
}

void AudioSystem::set_master_volume(float volume) {
  master_volume = sanitize_volume(volume);
  Game::Audio::Settings::save_master_volume(master_volume.load());

  if (m_music_player != nullptr) {
    m_music_player->set_volume(master_volume.load() * music_volume.load());
  }
}

void AudioSystem::set_sound_volume(float volume) {
  sound_volume = sanitize_volume(volume);
  Game::Audio::Settings::save_sound_volume(sound_volume.load());
}

void AudioSystem::set_music_volume(float volume) {
  music_volume = sanitize_volume(volume);
  Game::Audio::Settings::save_music_volume(music_volume.load());
  if (m_music_player != nullptr) {
    m_music_player->set_volume(master_volume.load() * music_volume.load());
  }
}

void AudioSystem::set_voice_volume(float volume) {
  voice_volume = sanitize_volume(volume);
  Game::Audio::Settings::save_voice_volume(voice_volume.load());
}

void AudioSystem::set_ambience_volume(float volume) {
  ambience_volume = sanitize_volume(volume);
  Game::Audio::Settings::save_ambience_volume(ambience_volume.load());
}

void AudioSystem::load_persisted_volumes() {
  listening_preset = Game::Audio::Settings::load_listening_preset();
  const auto volumes = Game::Audio::Settings::load_volumes();
  master_volume = volumes.master;
  sound_volume = volumes.sound;
  music_volume = volumes.music;
  voice_volume = volumes.voice;
  ambience_volume = volumes.ambience;
}

namespace {

auto mastering_material(AudioCategory category, const std::string& resource_id)
    -> Game::Audio::Mastering::Material {
  switch (category) {
  case AudioCategory::MUSIC:
    return Game::Audio::Mastering::Material::Music;
  case AudioCategory::VOICE:
    return Game::Audio::Mastering::Material::Voice;
  case AudioCategory::AMBIENCE:
    return Game::Audio::Mastering::Material::Ambience;
  case AudioCategory::SFX:
    break;
  }
  return Game::Audio::Mastering::effect_material(resource_id);
}

} // namespace

auto AudioSystem::load_sound(const std::string& sound_id,
                             const std::string& file_path,
                             const AudioResourceConfig& config) -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  if (sounds.find(sound_id) != sounds.end()) {
    cancel_pending_unload_locked(sound_id);
    return true;
  }

  MiniaudioBackend* backend =
      (m_music_player != nullptr) ? m_music_player->get_backend() : nullptr;
  auto sound = std::make_unique<Sound>(
      sound_id, file_path, mastering_material(config.category, sound_id), backend);
  if (!sound->is_registered()) {
    return false;
  }

  sounds[sound_id] = std::move(sound);
  resource_configs[sound_id] = config;
  cancel_pending_unload_locked(sound_id);
  active_resources.insert(sound_id);
  return true;
}

auto AudioSystem::load_music(const std::string& music_id,
                             const std::string& file_path,
                             const AudioResourceConfig& config) -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);

  if (m_music_player == nullptr) {
    qWarning() << "MusicPlayer not initialized";
    return false;
  }

  m_music_player->register_track(music_id, file_path);
  AudioResourceConfig music_config = config;
  music_config.category = AudioCategory::MUSIC;
  resource_configs[music_id] = music_config;
  cancel_pending_unload_locked(music_id);
  active_resources.insert(music_id);
  return true;
}

void AudioSystem::register_alias(const std::string& alias_id,
                                 const std::string& resource_id) {
  if (alias_id.empty() || resource_id.empty() || alias_id == resource_id) {
    return;
  }

  std::lock_guard<std::mutex> const lock(resource_mutex);
  resource_aliases[alias_id] = resolve_resource_id_locked(resource_id);
  active_resources.insert(alias_id);
}

auto AudioSystem::has_resource(const std::string& resource_id) const -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  const std::string resolved_id = resolve_resource_id_locked(resource_id);
  if (pending_unloads.find(resolved_id) != pending_unloads.end()) {
    return false;
  }
  return sounds.find(resolved_id) != sounds.end() ||
         resource_configs.find(resolved_id) != resource_configs.end();
}

auto AudioSystem::is_resource_ready(const std::string& resource_id) const -> bool {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  const std::string resolved_id = resolve_resource_id_locked(resource_id);
  if (sounds.find(resolved_id) == sounds.end()) {
    return false;
  }

  const AudioResourceConfig config = get_resource_config_locked(resolved_id);
  const auto now = std::chrono::steady_clock::now();
  if (is_sound_on_cooldown_locked(resolved_id, config.cooldown_ms, now)) {
    return false;
  }
  return config.max_instances == 0 ||
         get_active_instance_count_locked(resolved_id) < config.max_instances;
}

auto AudioSystem::resource_cooldown_ms(const std::string& resource_id) const -> int {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  return get_resource_config_locked(resolve_resource_id_locked(resource_id))
      .cooldown_ms;
}

void AudioSystem::reset_playback_throttles() {
  std::lock_guard<std::mutex> const lock(resource_mutex);
  resource_last_played_at.clear();
}

void AudioSystem::unload_sound(const std::string& sound_id) {
  enqueue_unload(sound_id);
}

void AudioSystem::unload_music(const std::string& music_id) {
  enqueue_unload(music_id);
}

void AudioSystem::enqueue_unload(const std::string& resource_id) {
  AudioEvent event(AudioEventType::UNLOAD_RESOURCE, resource_id);
  {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    event.load_serial = current_load_serial_locked(resource_id);
    pending_unloads.insert(resolve_resource_id_locked(resource_id));
  }
  enqueue(std::move(event));
}

void AudioSystem::cancel_pending_unload_locked(const std::string& resource_id) {
  ++resource_load_serials[resource_id];
  pending_unloads.erase(resource_id);
}

auto AudioSystem::current_load_serial_locked(const std::string& resource_id) const
    -> std::uint64_t {
  const auto it = resource_load_serials.find(resolve_resource_id_locked(resource_id));
  return it == resource_load_serials.end() ? 0U : it->second;
}

auto AudioSystem::get_active_channel_count() const -> size_t {
  std::lock_guard<std::mutex> const lock(active_sounds_mutex);
  return active_sounds.size();
}

void AudioSystem::audio_thread_func() {
  while (is_running) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    queue_condition.wait(lock, [this] { return !event_queue.empty(); });

    while (!event_queue.empty()) {
      AudioEvent const event = event_queue.front();
      event_queue.pop();
      lock.unlock();

      process_event(event);

      if (event.type == AudioEventType::SHUTDOWN) {
        is_running = false;
        return;
      }

      lock.lock();
    }
  }
}

void AudioSystem::process_event(const AudioEvent& event) {
  switch (event.type) {
  case AudioEventType::PLAY_SOUND: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    cleanup_inactive_sounds_locked();
    const std::string resource_id = resolve_resource_id_locked(event.resource_id);
    auto it = sounds.find(resource_id);
    if (it != sounds.end()) {
      auto const now = std::chrono::steady_clock::now();
      AudioResourceConfig const config = get_resource_config_locked(resource_id);
      AudioCategory const category = config.category;
      int const effective_priority = std::max(event.priority, config.priority);
      float const requested_volume = event.volume * config.volume;

      if (config.max_instances > 0 &&
          get_active_instance_count_locked(resource_id) >= config.max_instances) {
        trace_cue_result(event.cue_id,
                         resource_id,
                         Game::Audio::CueOutcome::InstanceLimit,
                         event.cue_source);
        break;
      }

      if (is_sound_on_cooldown_locked(resource_id, config.cooldown_ms, now)) {
        trace_cue_result(event.cue_id,
                         resource_id,
                         Game::Audio::CueOutcome::ResourceCooldown,
                         event.cue_source);
        break;
      }

      Game::Audio::SpatialGain spatial;
      if (event.positioned) {
        spatial = Game::Audio::spatialize(listener(), event.position);
      }

      float const effective_vol =
          get_effective_volume(category, requested_volume) * spatial.volume_scale;
      if (is_effectively_muted(effective_vol)) {
        trace_cue_result(event.cue_id,
                         resource_id,
                         Game::Audio::CueOutcome::Muted,
                         event.cue_source);
        break;
      }

      if (!should_accept_sound_locked(effective_priority)) {
        trace_cue_result(event.cue_id,
                         resource_id,
                         Game::Audio::CueOutcome::GlobalPriority,
                         event.cue_source);
        break;
      }

      if (!make_room_in_category_locked(category, effective_priority)) {
        trace_cue_result(event.cue_id,
                         resource_id,
                         Game::Audio::CueOutcome::CategoryPriority,
                         event.cue_source);
        break;
      }

      if (get_active_channel_count() >= max_channels) {
        evict_lowest_priority_sound_locked();
      }

      using Game::Audio::MixBus;
      const MixBus fallback =
          category == AudioCategory::VOICE
              ? MixBus::Voice
              : (category == AudioCategory::AMBIENCE ? MixBus::Ambience
                                                     : MixBus::Combat);
      // Category wins for voice resources bound to generic feedback cues.
      const MixBus bus =
          category == AudioCategory::VOICE
              ? MixBus::Voice
              : Game::Audio::mix_bus_for(
                    event.cue_id, Game::Audio::mix_bus_for(resource_id, fallback));
      it->second->play(effective_vol, event.loop, spatial.pan, bus, effective_priority);
      mark_sound_played_locked(resource_id, now);
      trace_cue_result(event.cue_id,
                       resource_id,
                       Game::Audio::CueOutcome::Accepted,
                       event.cue_source);
      if (event.cue_id.empty() && Game::Audio::CueTrace::logging_enabled()) {

        qInfo().noquote() << QStringLiteral("audio play %1 (%2)")
                                 .arg(QString::fromStdString(resource_id),
                                      event.loop ? QStringLiteral("loop")
                                                 : QStringLiteral("one-shot"));
      }

      {
        std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
        active_sounds.push_back(
            {resource_id, effective_priority, event.loop, category, now});
      }
    } else {
      trace_cue_result(event.cue_id,
                       resource_id,
                       Game::Audio::CueOutcome::ResourceNotLoaded,
                       event.cue_source);
    }
    break;
  }
  case AudioEventType::PLAY_MUSIC: {
    if (m_music_player == nullptr) {
      break;
    }

    std::string resource_id;
    float effective_volume = AudioConstants::MIN_VOLUME;
    {
      std::lock_guard<std::mutex> const lock(resource_mutex);
      resource_id = resolve_resource_id_locked(event.resource_id);
      AudioResourceConfig const config = get_resource_config_locked(resource_id);
      effective_volume =
          get_effective_volume(AudioCategory::MUSIC, event.volume * config.volume);
    }

    (void)m_music_player->play(resource_id,
                               effective_volume,
                               true,
                               event.crossfade
                                   ? Game::Audio::MusicTransition::Crossfade
                                   : Game::Audio::MusicTransition::Immediate);
    break;
  }
  case AudioEventType::SET_SOUND_LEVEL: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    const std::string resource_id = resolve_resource_id_locked(event.resource_id);
    auto it = sounds.find(resource_id);
    if (it != sounds.end()) {
      const AudioResourceConfig config = get_resource_config_locked(resource_id);
      const float effective =
          get_effective_volume(AudioCategory::AMBIENCE, event.volume * config.volume);
      it->second->set_playing_volume(effective, AudioConstants::DEFAULT_FADE_OUT_MS);
    }
    break;
  }
  case AudioEventType::STOP_SOUND: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    const std::string resource_id = resolve_resource_id_locked(event.resource_id);
    auto it = sounds.find(resource_id);
    if (it != sounds.end()) {
      it->second->stop();

      std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
      active_sounds.erase(
          std::remove_if(active_sounds.begin(),
                         active_sounds.end(),
                         [&](const ActiveSound& as) { return as.id == resource_id; }),
          active_sounds.end());
    }
    break;
  }
  case AudioEventType::STOP_MUSIC: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    if (m_music_player != nullptr) {
      m_music_player->stop_all(AudioConstants::NO_FADE_MS);
    }
    break;
  }
  case AudioEventType::UNLOAD_RESOURCE: {
    std::lock_guard<std::mutex> const lock(resource_mutex);
    const std::string resource_id = resolve_resource_id_locked(event.resource_id);
    if (current_load_serial_locked(resource_id) != event.load_serial) {
      break;
    }
    const AudioResourceConfig config = get_resource_config_locked(resource_id);
    auto sound_it = sounds.find(resource_id);
    if (sound_it != sounds.end()) {
      sound_it->second->stop();

      {
        std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
        active_sounds.erase(
            std::remove_if(active_sounds.begin(),
                           active_sounds.end(),
                           [&](const ActiveSound& as) { return as.id == resource_id; }),
            active_sounds.end());
      }

      MiniaudioBackend* const backend =
          (m_music_player != nullptr) ? m_music_player->get_backend() : nullptr;
      if (backend != nullptr) {
        backend->unload(sound_it->second->track_id());
      }
      sounds.erase(sound_it);
    }

    if (config.category == AudioCategory::MUSIC && m_music_player != nullptr) {
      m_music_player->stop_all(AudioConstants::NO_FADE_MS);
      m_music_player->unregister_track(resource_id);
    }

    resource_configs.erase(resource_id);
    resource_load_serials.erase(resource_id);
    pending_unloads.erase(resource_id);
    resource_last_played_at.erase(resource_id);
    active_resources.erase(resource_id);

    active_resources.erase(event.resource_id);
    for (auto alias_it = resource_aliases.begin();
         alias_it != resource_aliases.end();) {
      if (alias_it->first == event.resource_id || alias_it->second == resource_id) {
        active_resources.erase(alias_it->first);
        resource_last_played_at.erase(alias_it->first);
        alias_it = resource_aliases.erase(alias_it);
      } else {
        ++alias_it;
      }
    }
    break;
  }
  case AudioEventType::SHUTDOWN:
    break;
  }
}

auto AudioSystem::should_accept_sound_locked(int priority) -> bool {
  std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
  if (active_sounds.size() < max_channels) {
    return true;
  }

  auto lowest_it = std::min_element(active_sounds.begin(),
                                    active_sounds.end(),
                                    [](const ActiveSound& a, const ActiveSound& b) {
                                      if (a.priority != b.priority) {
                                        return a.priority < b.priority;
                                      }
                                      return a.start_time < b.start_time;
                                    });
  return lowest_it != active_sounds.end() && priority > lowest_it->priority;
}

auto AudioSystem::get_active_instance_count_locked(const std::string& resource_id) const
    -> size_t {
  std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
  return static_cast<size_t>(std::count_if(
      active_sounds.begin(), active_sounds.end(), [&](const ActiveSound& sound) {
        return sound.id == resource_id;
      }));
}

auto AudioSystem::is_sound_on_cooldown_locked(
    const std::string& resource_id,
    int cooldown_ms,
    std::chrono::steady_clock::time_point now) const -> bool {
  if (cooldown_ms <= 0) {
    return false;
  }

  auto it = resource_last_played_at.find(resource_id);
  if (it == resource_last_played_at.end()) {
    return false;
  }

  auto const elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
  return elapsed < cooldown_ms;
}

void AudioSystem::mark_sound_played_locked(const std::string& resource_id,
                                           std::chrono::steady_clock::time_point now) {
  resource_last_played_at[resource_id] = now;
}

auto AudioSystem::get_resource_config_locked(const std::string& resource_id) const
    -> AudioResourceConfig {
  auto it = resource_configs.find(resource_id);
  if (it != resource_configs.end()) {
    return it->second;
  }
  return {};
}

auto AudioSystem::category_channel_cap(AudioCategory category) -> size_t {
  switch (category) {
  case AudioCategory::VOICE:
    return AudioConstants::MAX_CONCURRENT_VOICE;
  case AudioCategory::AMBIENCE:
    return AudioConstants::MAX_CONCURRENT_AMBIENCE;
  case AudioCategory::SFX:
    return AudioConstants::MAX_CONCURRENT_SFX;
  default:
    return AudioConstants::DEFAULT_MAX_CHANNELS;
  }
}

auto AudioSystem::make_room_in_category_locked(AudioCategory category,
                                               int priority) -> bool {
  const size_t cap = category_channel_cap(category);
  std::string sound_id_to_stop;

  {
    std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);
    const size_t in_category = static_cast<size_t>(std::count_if(
        active_sounds.begin(),
        active_sounds.end(),
        [category](const ActiveSound& sound) { return sound.category == category; }));
    if (in_category < cap) {
      return true;
    }

    auto lowest_it = active_sounds.end();
    for (auto it = active_sounds.begin(); it != active_sounds.end(); ++it) {
      if (it->category != category) {
        continue;
      }
      if (lowest_it == active_sounds.end() || it->priority < lowest_it->priority ||
          (it->priority == lowest_it->priority &&
           it->start_time < lowest_it->start_time)) {
        lowest_it = it;
      }
    }
    if (lowest_it == active_sounds.end() || priority <= lowest_it->priority) {
      return false;
    }
    sound_id_to_stop = lowest_it->id;
    active_sounds.erase(lowest_it);
  }

  if (!sound_id_to_stop.empty()) {
    if (auto it = sounds.find(sound_id_to_stop); it != sounds.end()) {
      it->second->stop();
    }
  }
  return true;
}

void AudioSystem::evict_lowest_priority_sound_locked() {
  std::string sound_id_to_stop;

  {
    std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);

    if (active_sounds.empty()) {
      return;
    }

    auto lowest_it = std::min_element(active_sounds.begin(),
                                      active_sounds.end(),
                                      [](const ActiveSound& a, const ActiveSound& b) {
                                        if (a.priority != b.priority) {
                                          return a.priority < b.priority;
                                        }
                                        return a.start_time < b.start_time;
                                      });

    if (lowest_it != active_sounds.end()) {
      sound_id_to_stop = lowest_it->id;
      active_sounds.erase(lowest_it);
    }
  }

  if (!sound_id_to_stop.empty()) {
    auto it = sounds.find(sound_id_to_stop);
    if (it != sounds.end()) {
      it->second->stop();
    }
  }
}

void AudioSystem::cleanup_inactive_sounds_locked() {
  std::lock_guard<std::mutex> const active_lock(active_sounds_mutex);

  const auto now = std::chrono::steady_clock::now();
  active_sounds.erase(std::remove_if(active_sounds.begin(),
                                     active_sounds.end(),
                                     [this, now](const ActiveSound& as) {
                                       auto it = sounds.find(as.id);
                                       if (it == sounds.end()) {
                                         return true;
                                       }
                                       if (it->second->is_playing()) {
                                         return false;
                                       }
                                       if (!as.loop) {
                                         return true;
                                       }
                                       return (now - as.start_time) >
                                              k_loop_start_grace;
                                     }),
                      active_sounds.end());
}

auto AudioSystem::resolve_resource_id_locked(const std::string& resource_id) const
    -> std::string {
  auto it = resource_aliases.find(resource_id);
  if (it == resource_aliases.end()) {
    return resource_id;
  }
  return it->second;
}

auto AudioSystem::get_effective_volume(AudioCategory category,
                                       float event_volume) const -> float {
  static constexpr float VOICE_GAIN = 1.0F;
  if (!std::isfinite(event_volume)) {
    return AudioConstants::MIN_VOLUME;
  }

  float category_volume = NAN;
  switch (category) {
  case AudioCategory::SFX:
    category_volume = sound_volume;
    break;
  case AudioCategory::VOICE:
    category_volume = voice_volume;
    break;
  case AudioCategory::AMBIENCE:
    category_volume = ambience_volume;
    break;
  case AudioCategory::MUSIC:
    category_volume = music_volume;
    break;
  default:
    category_volume = sound_volume;
    break;
  }

  const float category_gain =
      (category == AudioCategory::VOICE) ? VOICE_GAIN : AudioConstants::DEFAULT_VOLUME;
  const float effective_volume =
      master_volume.load() * category_volume * category_gain * event_volume;
  if (!std::isfinite(effective_volume)) {
    return AudioConstants::MIN_VOLUME;
  }
  return std::clamp(
      effective_volume, AudioConstants::MIN_VOLUME, MiniaudioBackend::MAX_VOLUME);
}
