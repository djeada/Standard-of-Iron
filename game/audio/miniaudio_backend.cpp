#include "miniaudio_backend.h"

#include <QDebug>
#include <QFile>
#include <qglobal.h>
#include <qhashfunctions.h>
#include <qmutex.h>
#include <qobject.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "spatial.h"

namespace {

constexpr float k_pcm_scale_down = 1.0F / 32768.0F;

constexpr int COMMAND_WAIT_ATTEMPTS = 200;

auto sanitize_backend_volume(float volume) -> float {
  if (!std::isfinite(volume)) {
    return MiniaudioBackend::MIN_VOLUME;
  }
  return std::clamp(volume, MiniaudioBackend::MIN_VOLUME, MiniaudioBackend::MAX_VOLUME);
}

} // namespace

#include "miniaudio_config.h"

#define MINIAUDIO_IMPLEMENTATION
#pragma push_macro("TRUE")
#pragma push_macro("FALSE")
#pragma push_macro("L")
#pragma push_macro("C")
#pragma push_macro("R")
#include <stb_vorbis.h>
#pragma pop_macro("R")
#pragma pop_macro("C")
#pragma pop_macro("L")
#pragma pop_macro("FALSE")
#pragma pop_macro("TRUE")
#define STB_VORBIS_INCLUDE_STB_VORBIS_H
#include <miniaudio.h>
#ifdef PlaySound
#undef PlaySound
#endif

struct DeviceWrapper {
  MiniaudioBackend* self;
};

static void audioCallback(ma_device* device,
                          void* output_buffer,
                          const void*,
                          ma_uint32 frame_count) {
  auto* wrapper = reinterpret_cast<DeviceWrapper*>(device->pUserData);
  if ((wrapper == nullptr) || (wrapper->self == nullptr)) {
    std::memset(output_buffer,
                0,
                static_cast<unsigned long>(frame_count *
                                           MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS) *
                    sizeof(float));
    return;
  }
  wrapper->self->on_audio(reinterpret_cast<float*>(output_buffer), frame_count);
}

MiniaudioBackend::MiniaudioBackend(QObject* parent)
    : QObject(parent) {
}
MiniaudioBackend::~MiniaudioBackend() {
  shutdown();
}

auto MiniaudioBackend::initialize(int device_rate,
                                  int,
                                  int music_channels,
                                  bool open_device) -> bool {
  m_sample_rate = std::max(MIN_SAMPLE_RATE, device_rate);
  m_output_channels = DEFAULT_OUTPUT_CHANNELS;

  m_channels.assign(static_cast<std::size_t>(std::max(1, music_channels)), Channel{});
  m_sound_effects.assign(static_cast<std::size_t>(DEFAULT_SOUND_EFFECT_SLOTS),
                         SoundEffect{});
  for (auto& slot : m_active_sfx_tracks) {
    slot.store(-1, std::memory_order_relaxed);
  }
  m_active_channel_mask.store(0, std::memory_order_relaxed);
  m_bus_limiter.prepare(m_sample_rate, m_output_channels);
  m_gameplay_mix.prepare(m_sample_rate);
  m_decoder.set_sample_rate(m_sample_rate);
  m_worker.start();

  if (!open_device) {
    qInfo() << "MiniaudioBackend: mixer ready without a playback device";
    return true;
  }

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = m_output_channels;
  config.sampleRate = m_sample_rate;
  config.dataCallback = audioCallback;

  auto wrapper = std::make_unique<DeviceWrapper>(DeviceWrapper{this});
  config.pUserData = wrapper.get();

  m_device = std::make_unique<ma_device>();
  if (ma_device_init(nullptr, &config, m_device.get()) != MA_SUCCESS) {
    qWarning() << "MiniaudioBackend: Failed to initialize audio device";
    qWarning() << "  Requested sample rate:" << m_sample_rate;
    qWarning() << "  Requested channels:" << m_output_channels;
    qWarning() << "  This may indicate no audio device is available";
    m_device.reset();
    m_worker.stop();
    return false;
  }

  m_device_wrapper = std::move(wrapper);

  if (ma_device_start(m_device.get()) != MA_SUCCESS) {
    qWarning() << "MiniaudioBackend: Failed to start audio device";
    ma_device_uninit(m_device.get());
    m_device.reset();
    m_device_wrapper.reset();
    m_worker.stop();
    return false;
  }
  m_device_running.store(true, std::memory_order_release);

  qInfo() << "MiniaudioBackend: Initialized successfully";
  qInfo() << "  Sample rate:" << m_sample_rate;
  qInfo() << "  Channels:" << m_output_channels;
  qInfo() << "  Music channels:" << music_channels;
  return true;
}

void MiniaudioBackend::shutdown() {
  stop_device();
  m_worker.stop();

  drain_commands();
  m_channels.clear();
  m_sound_effects.clear();

  m_tracks.clear();
}

void MiniaudioBackend::stop_device() {
  if (m_device == nullptr) {
    return;
  }
  m_device_running.store(false, std::memory_order_release);
  ma_device_stop(m_device.get());
  ma_device_uninit(m_device.get());
  m_device.reset();
  m_device_wrapper.reset();
}

void MiniaudioBackend::wait_for_track(const QString& id) {
  const int slot = m_tracks.find_slot(id);
  if (slot < 0) {
    return;
  }
  m_worker.wait_for_slot(slot);
}

void MiniaudioBackend::wait_for_decodes() {
  m_worker.wait_for_all();
}

auto MiniaudioBackend::is_track_ready(const QString& id) const -> bool {
  return m_tracks.is_ready(id);
}

auto MiniaudioBackend::is_track_decode_pending(const QString& id) const -> bool {
  const int slot = m_tracks.find_slot(id);
  if (slot < 0) {
    return false;
  }
  return m_worker.is_pending(slot);
}

auto MiniaudioBackend::has_pending_decodes() const -> bool {
  return m_worker.has_pending();
}

auto MiniaudioBackend::request_track(const QString& id,
                                     const QString& path,
                                     Game::Audio::Mastering::Material material)
    -> bool {
  const int slot = m_tracks.claim_slot(id);
  if (slot < 0) {
    qWarning() << "MiniaudioBackend: no free track slot for" << id;
    return false;
  }
  if (m_tracks.track(static_cast<std::size_t>(slot)) != nullptr) {
    return true;
  }
  return m_worker.request(Game::Audio::DecodeJob{id, path, slot, material});
}

void MiniaudioBackend::unload(const QString& id) {
  const int queued_slot = m_tracks.find_slot(id);
  if (queued_slot >= 0) {
    m_worker.cancel(queued_slot);
  }
  wait_for_track(id);

  const int slot = m_tracks.detach(id);
  if (slot < 0) {
    return;
  }

  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::ReleaseTrack;
  command.track = static_cast<std::int16_t>(slot);
  submit_and_wait(command);

  m_tracks.free_slot(slot);
}

void MiniaudioBackend::submit(const Game::Audio::AudioCommand& command) {
  if (m_commands.push(command) ==
      Game::Audio::CommandRing<COMMAND_CAPACITY>::REJECTED) {
    qWarning() << "MiniaudioBackend: command queue full, dropping request";
    return;
  }
  if (!m_device_running.load(std::memory_order_acquire)) {
    drain_commands();
  }
}

void MiniaudioBackend::submit_and_wait(const Game::Audio::AudioCommand& command) {
  const std::int64_t sequence = m_commands.push(command);
  if (sequence == Game::Audio::CommandRing<COMMAND_CAPACITY>::REJECTED) {
    qWarning() << "MiniaudioBackend: command queue full, track release dropped";
    return;
  }
  if (!m_device_running.load(std::memory_order_acquire)) {
    drain_commands();
    return;
  }
  const auto target = static_cast<std::uint64_t>(sequence) + 1;
  for (int spin = 0; spin < COMMAND_WAIT_ATTEMPTS; ++spin) {
    if (m_commands.processed() >= target) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  qWarning() << "MiniaudioBackend: timed out waiting for the mixer to release a track";
}

auto MiniaudioBackend::fade_samples_for(int fade_ms) const -> unsigned {
  static constexpr int MIN_FADE_MS = 1;
  static constexpr int MS_PER_SECOND = 1000;
  return std::max(unsigned(MIN_FADE_MS),
                  unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
}

void MiniaudioBackend::play(
    int channel, const QString& id, float volume, bool loop, int fade_ms) {
  const int slot = m_tracks.find_slot(id);
  if (slot < 0) {
    qWarning() << "MiniaudioBackend: track not registered:" << id;
    return;
  }
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::Play;
  command.channel = static_cast<std::int16_t>(channel);
  command.track = static_cast<std::int16_t>(slot);
  command.volume = sanitize_backend_volume(volume);
  command.loop = loop;
  command.fade_samples = fade_samples_for(fade_ms);
  submit(command);
}

void MiniaudioBackend::stop(int channel, int fade_ms) {
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::Stop;
  command.channel = static_cast<std::int16_t>(channel);
  command.fade_samples = fade_samples_for(fade_ms);
  submit(command);
}

void MiniaudioBackend::pause(int channel) {
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::Pause;
  command.channel = static_cast<std::int16_t>(channel);
  submit(command);
}

void MiniaudioBackend::resume(int channel) {
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::Resume;
  command.channel = static_cast<std::int16_t>(channel);
  submit(command);
}

void MiniaudioBackend::set_volume(int channel, float volume, int fade_ms) {
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::SetVolume;
  command.channel = static_cast<std::int16_t>(channel);
  command.volume = sanitize_backend_volume(volume);
  command.fade_samples = fade_samples_for(fade_ms);
  submit(command);
}

void MiniaudioBackend::stop_all(int fade_ms) {
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::StopAll;
  command.fade_samples = fade_samples_for(fade_ms);
  submit(command);
}

void MiniaudioBackend::set_master_volume(float volume, int) {
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::SetMasterVolume;
  command.volume = sanitize_backend_volume(volume);
  submit(command);
}

auto MiniaudioBackend::any_channel_playing() const -> bool {
  return m_active_channel_mask.load(std::memory_order_acquire) != 0U;
}

auto MiniaudioBackend::channel_playing(int channel) const -> bool {
  if (channel < 0 || channel >= int(sizeof(std::uint32_t) * 8)) {
    return false;
  }
  const std::uint32_t mask = m_active_channel_mask.load(std::memory_order_acquire);
  return (mask & (1U << static_cast<unsigned>(channel))) != 0U;
}

void MiniaudioBackend::play_sound(const QString& id,
                                  float volume,
                                  bool loop,
                                  float pan,
                                  Game::Audio::MixBus bus,
                                  int priority) {
  if (m_offline_render && is_track_decode_pending(id)) {
    wait_for_track(id);
  }
  const int slot = m_tracks.find_slot(id);
  if (slot < 0) {
    qWarning() << "MiniaudioBackend: Sound not ready:" << id;
    return;
  }
  if (m_tracks.track(static_cast<std::size_t>(slot)) == nullptr) {
    const Game::Audio::DeferredLoop deferred{
        sanitize_backend_volume(volume), pan, bus, priority};
    if (m_worker.defer_if_pending(slot, loop, deferred)) {
      if (!loop) {
        qDebug() << "MiniaudioBackend: Sound still decoding, skipping play:" << id;
      }
      return;
    }
    if (m_tracks.track(static_cast<std::size_t>(slot)) == nullptr) {
      qWarning() << "MiniaudioBackend: Sound not ready:" << id;
      return;
    }
  }
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::PlaySound;
  command.track = static_cast<std::int16_t>(slot);
  command.volume = sanitize_backend_volume(volume);
  command.pan = std::isfinite(pan) ? std::clamp(pan, -1.0F, 1.0F) : 0.0F;
  command.mix_bus = bus;
  command.priority = priority;
  command.loop = loop;
  submit(command);
}

void MiniaudioBackend::set_sound_volume(const QString& id, float volume, int fade_ms) {
  const int slot = m_tracks.find_slot(id);
  if (slot < 0) {
    return;
  }
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::SetSoundVolume;
  command.track = static_cast<std::int16_t>(slot);
  command.volume = sanitize_backend_volume(volume);
  command.fade_samples = fade_samples_for(fade_ms);
  submit(command);
}

void MiniaudioBackend::stop_sound(const QString& id) {
  const int slot = m_tracks.find_slot(id);
  if (slot < 0) {
    return;
  }
  m_worker.discard_deferred(slot);
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::StopSound;
  command.track = static_cast<std::int16_t>(slot);
  submit(command);
}

auto MiniaudioBackend::is_sound_active(const QString& id) const -> bool {
  const int slot = m_tracks.find_slot(id);
  if (slot < 0) {
    return false;
  }
  for (const auto& active : m_active_sfx_tracks) {
    if (active.load(std::memory_order_acquire) == slot) {
      return true;
    }
  }
  return false;
}

namespace {

template <typename Fader>
void begin_channel_fade(Fader& channel, float target, unsigned fade_samples) {
  channel.target_volume = target;
  channel.fade_samples = std::max(1U, fade_samples);
  channel.volume_step =
      (channel.target_volume - channel.current_volume) / float(channel.fade_samples);
}

} // namespace

auto MiniaudioBackend::channel_for(const Game::Audio::AudioCommand& command)
    -> Channel* {
  const auto index = static_cast<std::size_t>(command.channel);
  return command.channel >= 0 && index < m_channels.size() ? &m_channels[index]
                                                           : nullptr;
}

void MiniaudioBackend::apply_play(const Game::Audio::AudioCommand& command) {
  Channel* channel = channel_for(command);
  if (channel == nullptr) {
    return;
  }
  channel->track = command.track;
  channel->frame_pos = 0;
  channel->looping = command.loop;
  channel->paused = false;
  channel->active = true;
  channel->current_volume = MIN_VOLUME;
  begin_channel_fade(*channel, command.volume, command.fade_samples);
}

void MiniaudioBackend::apply_stop(const Game::Audio::AudioCommand& command) {
  Channel* channel = channel_for(command);
  if (channel == nullptr || !channel->active) {
    return;
  }
  begin_channel_fade(*channel, MIN_VOLUME, command.fade_samples);
  channel->looping = false;
}

void MiniaudioBackend::apply_stop_all(const Game::Audio::AudioCommand& command) {
  for (Channel& channel : m_channels) {
    if (!channel.active) {
      continue;
    }
    begin_channel_fade(channel, MIN_VOLUME, command.fade_samples);
    channel.looping = false;
  }
}

void MiniaudioBackend::apply_play_sound(const Game::Audio::AudioCommand& command) {
  for (SoundEffect& effect : m_sound_effects) {
    if (effect.active) {
      continue;
    }
    effect.mix_bus = command.mix_bus;
    effect.priority = command.priority;
    effect.track = command.track;
    effect.frame_pos = 0;
    effect.volume = command.volume;
    effect.target_volume = command.volume;
    effect.volume_step = 0.0F;
    const auto gains = Game::Audio::pan_gains(command.pan);
    effect.gain_left = gains.first;
    effect.gain_right = gains.second;
    effect.fade_samples = 0;
    effect.looping = command.loop;
    effect.active = true;
    return;
  }
}

void MiniaudioBackend::apply_set_sound_volume(
    const Game::Audio::AudioCommand& command) {
  for (SoundEffect& effect : m_sound_effects) {
    if (!effect.active || effect.track != command.track) {
      continue;
    }
    effect.target_volume = command.volume;
    effect.fade_samples = std::max(1U, command.fade_samples);
    effect.volume_step =
        (effect.target_volume - effect.volume) / float(effect.fade_samples);
  }
}

void MiniaudioBackend::apply_release_track(const Game::Audio::AudioCommand& command) {
  for (Channel& channel : m_channels) {
    if (channel.track == command.track) {
      channel = Channel{};
    }
  }
  for (SoundEffect& effect : m_sound_effects) {
    if (effect.track == command.track) {
      effect = SoundEffect{};
    }
  }
}

void MiniaudioBackend::apply_command(const Game::Audio::AudioCommand& command) {
  using Type = Game::Audio::AudioCommand::Type;
  switch (command.type) {
  case Type::Play:
    apply_play(command);
    return;
  case Type::Stop:
    apply_stop(command);
    return;
  case Type::Pause:
    if (Channel* channel = channel_for(command)) {
      channel->paused = true;
    }
    return;
  case Type::Resume:
    if (Channel* channel = channel_for(command)) {
      channel->paused = false;
    }
    return;
  case Type::SetVolume:
    if (Channel* channel = channel_for(command);
        channel != nullptr && channel->active) {
      begin_channel_fade(*channel, command.volume, command.fade_samples);
    }
    return;
  case Type::StopAll:
    apply_stop_all(command);
    return;
  case Type::SetMasterVolume:
    m_master_volume = command.volume;
    return;
  case Type::PlaySound:
    apply_play_sound(command);
    return;
  case Type::SetSoundVolume:
    apply_set_sound_volume(command);
    return;
  case Type::StopSound:
    for (SoundEffect& effect : m_sound_effects) {
      if (effect.active && effect.track == command.track) {
        effect = SoundEffect{};
      }
    }
    return;
  case Type::ReleaseTrack:
    apply_release_track(command);
    return;
  case Type::None:
    return;
  }
}

void MiniaudioBackend::drain_commands() {
  m_commands.drain(
      [this](const Game::Audio::AudioCommand& command) { apply_command(command); });
}

void MiniaudioBackend::publish_state() {
  std::uint32_t mask = 0;
  for (std::size_t index = 0; index < m_channels.size() && index < 32; ++index) {
    const Channel& channel = m_channels[index];
    if (channel.active && !channel.paused) {
      mask |= 1U << static_cast<unsigned>(index);
    }
  }
  m_active_channel_mask.store(mask, std::memory_order_release);

  for (std::size_t index = 0; index < m_active_sfx_tracks.size(); ++index) {
    const int track = (index < m_sound_effects.size() && m_sound_effects[index].active)
                          ? m_sound_effects[index].track
                          : -1;
    m_active_sfx_tracks[index].store(track, std::memory_order_release);
  }
}

namespace {

constexpr int STEREO_CHANNELS = 2;
constexpr unsigned k_mix_block = 256;

} // namespace

struct MiniaudioBackend::BlockGains {
  const float* origin = nullptr;
  std::array<Game::Audio::MixGains, k_mix_block> values;

  [[nodiscard]] auto at(const float* destination,
                        Game::Audio::MixBus bus) const -> float {
    return values[static_cast<std::size_t>(destination - origin) / STEREO_CHANNELS]
                 [Game::Audio::mix_index(bus)];
  }
};

struct MiniaudioBackend::EffectLoad {
  Game::Audio::MixCounts counts{};
  bool voice = false;
  bool critical = false;
};

auto MiniaudioBackend::measure_effect_load(float master) const -> EffectLoad {
  EffectLoad load;
  for (const SoundEffect& effect : m_sound_effects) {
    if (!effect.active || effect.volume * master <= 0.001F) {
      continue;
    }
    const auto* track = effect.track < 0
                            ? nullptr
                            : m_tracks.track(static_cast<std::size_t>(effect.track));
    if (track == nullptr || track->frames == 0 ||
        (!effect.looping && effect.frame_pos >= track->frames)) {
      continue;
    }
    ++load.counts[Game::Audio::mix_index(effect.mix_bus)];
    load.voice = load.voice || effect.mix_bus == Game::Audio::MixBus::Voice;
    load.critical = load.critical || (effect.priority >= 7 &&
                                      (effect.mix_bus == Game::Audio::MixBus::Voice ||
                                       effect.mix_bus == Game::Audio::MixBus::Alert));
  }
  return load;
}

void MiniaudioBackend::render_music_run(Channel& channel,
                                        const std::int16_t* source,
                                        unsigned stride,
                                        unsigned run,
                                        float*& destination,
                                        float master,
                                        const BlockGains& gains) {
  const unsigned fading = std::min(run, channel.fade_samples);
  for (unsigned i = 0; i < fading; ++i) {
    const float volume = channel.current_volume * master * k_pcm_scale_down *
                         gains.at(destination, Game::Audio::MixBus::Music);
    const float left = static_cast<float>(source[0]) * volume;
    destination[0] += left;
    destination[1] += (stride == 1) ? left : static_cast<float>(source[1]) * volume;
    destination += STEREO_CHANNELS;
    source += stride;
    channel.current_volume += channel.volume_step;
    if (--channel.fade_samples == 0) {
      channel.current_volume = channel.target_volume;
    }
  }
  const unsigned steady = run - fading;
  if (steady == 0) {
    return;
  }
  if (stride == 1) {
    for (unsigned i = 0; i < steady; ++i) {
      const float volume = channel.current_volume * master * k_pcm_scale_down *
                           gains.at(destination, Game::Audio::MixBus::Music);
      const float value = static_cast<float>(source[i]) * volume;
      destination[0] += value;
      destination[1] += value;
      destination += STEREO_CHANNELS;
    }
    return;
  }
  for (unsigned i = 0; i < steady; ++i) {
    const float volume = channel.current_volume * master * k_pcm_scale_down *
                         gains.at(destination, Game::Audio::MixBus::Music);
    destination[0] += static_cast<float>(source[0]) * volume;
    destination[1] += static_cast<float>(source[1]) * volume;
    destination += STEREO_CHANNELS;
    source += STEREO_CHANNELS;
  }
}

void MiniaudioBackend::mix_music_channels(float* output,
                                          unsigned frames,
                                          float master,
                                          const BlockGains& gains) {
  for (Channel& channel : m_channels) {
    if (!channel.active || channel.paused || channel.track < 0) {
      continue;
    }
    const DecodedTrack* track = m_tracks.track(static_cast<std::size_t>(channel.track));
    if (track == nullptr || track->frames == 0) {
      continue;
    }

    const std::int16_t* const pcm = track->pcm.data();
    const unsigned stride = track->channels;
    unsigned frames_left = frames;
    unsigned position = channel.frame_pos;
    float* destination = output;

    while (frames_left > 0) {
      if (position >= track->frames) {
        if (!channel.looping) {
          break;
        }
        position = 0;
      }
      const unsigned run = std::min(frames_left, track->frames - position);
      const std::int16_t* source = pcm + (static_cast<std::size_t>(position) * stride);
      render_music_run(channel, source, stride, run, destination, master, gains);
      position += run;
      frames_left -= run;
    }

    channel.frame_pos = position;

    if (!channel.looping && channel.frame_pos >= track->frames) {
      channel = Channel{};
      continue;
    }
    if (channel.fade_samples == 0 && channel.current_volume <= MIN_VOLUME &&
        channel.target_volume <= MIN_VOLUME && !channel.looping) {
      channel = Channel{};
    }
  }
}

void MiniaudioBackend::render_effect_run(SoundEffect& effect,
                                         const std::int16_t* source,
                                         unsigned stride,
                                         unsigned run,
                                         float*& destination,
                                         float master,
                                         const BlockGains& gains) {
  const unsigned fading = std::min(run, effect.fade_samples);
  for (unsigned i = 0; i < fading; ++i) {
    const float volume = effect.volume * master * k_pcm_scale_down *
                         gains.at(destination, effect.mix_bus);
    const float left = static_cast<float>(source[0]) * volume;
    destination[0] += left * effect.gain_left;
    destination[1] += ((stride == 1) ? left : static_cast<float>(source[1]) * volume) *
                      effect.gain_right;
    destination += STEREO_CHANNELS;
    source += stride;
    effect.volume += effect.volume_step;
    if (--effect.fade_samples == 0) {
      effect.volume = effect.target_volume;
    }
  }
  const unsigned steady = run - fading;
  if (steady == 0) {
    return;
  }
  if (stride == 1) {
    for (unsigned i = 0; i < steady; ++i) {
      const float volume = effect.volume * master * k_pcm_scale_down *
                           gains.at(destination, effect.mix_bus);
      const float value = static_cast<float>(source[i]) * volume;
      destination[0] += value * effect.gain_left;
      destination[1] += value * effect.gain_right;
      destination += STEREO_CHANNELS;
    }
    return;
  }
  for (unsigned i = 0; i < steady; ++i) {
    const float volume = effect.volume * master * k_pcm_scale_down *
                         gains.at(destination, effect.mix_bus);
    destination[0] += static_cast<float>(source[0]) * volume * effect.gain_left;
    destination[1] += static_cast<float>(source[1]) * volume * effect.gain_right;
    destination += STEREO_CHANNELS;
    source += STEREO_CHANNELS;
  }
}

void MiniaudioBackend::mix_sound_effects(float* output,
                                         unsigned frames,
                                         float master,
                                         const BlockGains& gains) {
  for (SoundEffect& effect : m_sound_effects) {
    if (!effect.active || effect.track < 0) {
      continue;
    }
    const DecodedTrack* track = m_tracks.track(static_cast<std::size_t>(effect.track));
    if (track == nullptr || track->frames == 0) {
      effect = SoundEffect{};
      continue;
    }

    const std::int16_t* const pcm = track->pcm.data();
    const unsigned stride = track->channels;
    unsigned frames_left = frames;
    unsigned position = effect.frame_pos;
    float* destination = output;

    while (frames_left > 0) {
      if (position >= track->frames) {
        if (!effect.looping) {
          effect.active = false;
          break;
        }
        position = 0;
      }
      const unsigned run = std::min(frames_left, track->frames - position);
      const std::int16_t* source = pcm + (static_cast<std::size_t>(position) * stride);
      render_effect_run(effect, source, stride, run, destination, master, gains);
      position += run;
      frames_left -= run;
    }

    effect.frame_pos = position;
    if (!effect.active) {
      effect = SoundEffect{};
    }
  }
}

void MiniaudioBackend::on_audio(float* output, unsigned frames) {
  if (frames > k_mix_block) {
    for (unsigned offset = 0; offset < frames; offset += k_mix_block) {
      on_audio(output + offset * STEREO_CHANNELS,
               std::min(k_mix_block, frames - offset));
    }
    return;
  }
  const unsigned samples = frames * STEREO_CHANNELS;
  std::memset(output, 0, samples * sizeof(float));

  drain_commands();

  const float master = m_master_volume;
  const EffectLoad load = measure_effect_load(master);
  const auto preset = m_listening_preset.load(std::memory_order_relaxed);
  m_gameplay_mix.target(load.counts, load.voice, load.critical, preset);
  BlockGains gains;
  gains.origin = output;
  for (unsigned i = 0; i < frames; ++i) {
    gains.values[i] = m_gameplay_mix.next();
  }

  mix_music_channels(output, frames, master, gains);
  mix_sound_effects(output, frames, master, gains);

  publish_state();

  if (m_bus_limiter.is_ready()) {
    m_bus_limiter.set_listening_preset(preset);
    m_bus_limiter.process(output, frames);
    return;
  }

  for (unsigned i = 0; i < samples; ++i) {
    output[i] = std::clamp(output[i], -MAX_OUTPUT_SAMPLE, MAX_OUTPUT_SAMPLE);
  }
}
