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
#include <thread>
#include <utility>
#include <vector>

#include "audio_mastering.h"
#include "loop_seam.h"
#include "resampler.h"

namespace {

constexpr int COMMAND_WAIT_ATTEMPTS = 200;

auto sanitize_backend_volume(float volume) -> float {
  if (!std::isfinite(volume)) {
    return MiniaudioBackend::MIN_VOLUME;
  }
  return std::clamp(volume, MiniaudioBackend::MIN_VOLUME, MiniaudioBackend::MAX_VOLUME);
}

void log_resample(const QString& id, const Game::Audio::ResampleReport& report) {
  if (!report.applied) {
    return;
  }
  qInfo().nospace() << "audio resample " << id << ": " << report.rate_in << " -> "
                    << report.rate_out << " Hz, " << report.up << "/" << report.down
                    << ", " << int(report.taps_per_phase) << " taps per phase";
}

void log_loop_seam(const QString& id, const Game::Audio::LoopSeamReport& seam) {
  static constexpr float REPORTABLE_STEP = 0.02F;
  if (seam.step_before < REPORTABLE_STEP) {
    return;
  }
  qInfo().nospace() << "audio loop seam " << id << ": wrap step " << seam.step_before
                    << " -> " << seam.step_after << " over " << int(seam.fade_frames)
                    << " frames";
}

void log_mastering(const QString& id, const Game::Audio::Mastering::Report& report) {
  static constexpr float REPORTABLE_CHANGE_DB = 0.5F;
  if (report.input_peak_db < 0.0F &&
      std::abs(report.loudness_gain_db) < REPORTABLE_CHANGE_DB &&
      report.notch_count == 0 && report.limiter_reduction_db > -REPORTABLE_CHANGE_DB) {
    return;
  }
  qInfo().nospace() << "audio mastering " << id << ": peak " << report.input_peak_db
                    << " -> " << report.output_peak_db << " dBFS, loudness "
                    << report.input_lufs << " LUFS " << report.loudness_gain_db
                    << " dB, notches " << int(report.notch_count) << ", tilt "
                    << report.presence_tilt_db << '/' << report.air_tilt_db
                    << " dB, limiter " << report.limiter_reduction_db << " dB";
}

} // namespace

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS

#define MA_ENABLE_PULSEAUDIO
#define MA_ENABLE_ALSA
#define MA_ENABLE_WASAPI
#define MA_ENABLE_COREAUDIO

#define MA_ENABLE_MP3
#define MA_ENABLE_FLAC
#define MA_ENABLE_VORBIS
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
  start_worker();

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
    stop_worker();
    return false;
  }

  m_device_wrapper = std::move(wrapper);

  if (ma_device_start(m_device.get()) != MA_SUCCESS) {
    qWarning() << "MiniaudioBackend: Failed to start audio device";
    ma_device_uninit(m_device.get());
    m_device.reset();
    m_device_wrapper.reset();
    stop_worker();
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
  stop_worker();

  drain_commands();
  m_channels.clear();
  m_sound_effects.clear();

  QMutexLocker const locker(&m_registry_mutex);
  m_track_ids.clear();
  for (int slot = 0; slot < MAX_TRACKS; ++slot) {
    m_track_table[slot].store(nullptr, std::memory_order_release);
    m_track_storage[slot].reset();
    m_slot_taken[slot] = false;
  }
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

void MiniaudioBackend::start_worker() {
  QMutexLocker locker(&m_decode_mutex);
  if (m_decode_running) {
    return;
  }
  m_decode_running = true;
  locker.unlock();
  m_decode_thread = std::thread([this] { decode_worker(); });
}

void MiniaudioBackend::stop_worker() {
  {
    QMutexLocker const locker(&m_decode_mutex);
    if (!m_decode_running) {
      return;
    }
    m_decode_running = false;
    m_decode_jobs.clear();
    m_decode_bulk_jobs.clear();
    m_decode_ready.wakeAll();
  }
  if (m_decode_thread.joinable()) {
    m_decode_thread.join();
  }
}

auto MiniaudioBackend::take_next_job(DecodeJob& job) -> bool {
  if (!m_decode_jobs.empty()) {
    job = m_decode_jobs.front();
    m_decode_jobs.pop_front();
    return true;
  }
  if (!m_decode_bulk_jobs.empty()) {
    job = m_decode_bulk_jobs.front();
    m_decode_bulk_jobs.pop_front();
    return true;
  }
  return false;
}

void MiniaudioBackend::decode_worker() {
  for (;;) {
    DecodeJob job;
    {
      QMutexLocker locker(&m_decode_mutex);
      while (m_decode_running && m_decode_jobs.empty() && m_decode_bulk_jobs.empty()) {
        m_decode_ready.wait(&m_decode_mutex);
      }
      if (!m_decode_running) {
        return;
      }
      if (!take_next_job(job)) {
        continue;
      }
      ++m_decode_in_flight;
    }

    finish_job(job, decode_into_slot(job));
  }
}

void MiniaudioBackend::finish_job(const DecodeJob& job, bool decoded) {
  if (!decoded) {
    qWarning() << "MiniaudioBackend: dropping" << job.id
               << "because its audio could not be decoded";
    Game::Audio::AudioCommand command;
    command.type = Game::Audio::AudioCommand::Type::ReleaseTrack;
    command.track = static_cast<std::int16_t>(job.track);
    submit(command);
    release_slot(job.track);
  }

  QMutexLocker const locker(&m_decode_mutex);
  m_pending_slots.remove(job.track);
  --m_decode_in_flight;
  if (m_decode_jobs.empty() && m_decode_bulk_jobs.empty() && m_decode_in_flight == 0) {
    m_decode_idle.wakeAll();
  }
}

void MiniaudioBackend::release_slot(int slot) {
  if (slot < 0 || slot >= MAX_TRACKS) {
    return;
  }
  QMutexLocker const locker(&m_registry_mutex);
  for (auto it = m_track_ids.begin(); it != m_track_ids.end(); ++it) {
    if (it.value() == slot) {
      m_track_ids.erase(it);
      break;
    }
  }
  m_track_table[slot].store(nullptr, std::memory_order_release);
  m_track_storage[slot].reset();
  m_slot_taken[slot] = false;
}

void MiniaudioBackend::wait_for_track(const QString& id) {
  const int slot = find_track_slot(id);
  if (slot < 0) {
    return;
  }
  QMutexLocker locker(&m_decode_mutex);
  while (m_decode_running && m_pending_slots.contains(slot)) {
    m_decode_idle.wait(&m_decode_mutex);
  }
}

void MiniaudioBackend::wait_for_decodes() {
  QMutexLocker locker(&m_decode_mutex);
  while (m_decode_running && (!m_decode_jobs.empty() || !m_decode_bulk_jobs.empty() ||
                              m_decode_in_flight > 0)) {
    m_decode_idle.wait(&m_decode_mutex);
  }
}

auto MiniaudioBackend::claim_track_slot(const QString& id) -> int {
  QMutexLocker const locker(&m_registry_mutex);
  const auto existing = m_track_ids.constFind(id);
  if (existing != m_track_ids.constEnd()) {
    return existing.value();
  }
  for (int slot = 0; slot < MAX_TRACKS; ++slot) {
    if (!m_slot_taken[slot]) {
      m_slot_taken[slot] = true;
      m_track_ids.insert(id, slot);
      return slot;
    }
  }
  return -1;
}

auto MiniaudioBackend::find_track_slot(const QString& id) const -> int {
  QMutexLocker const locker(&m_registry_mutex);
  const auto found = m_track_ids.constFind(id);
  return found == m_track_ids.constEnd() ? -1 : found.value();
}

auto MiniaudioBackend::is_track_ready(const QString& id) const -> bool {
  const int slot = find_track_slot(id);
  return slot >= 0 && m_track_table[slot].load(std::memory_order_acquire) != nullptr;
}

auto MiniaudioBackend::decode_into_slot(const DecodeJob& job) -> bool {
  QFile file(job.path);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "miniaudio: QFile open failed for" << job.path;
    return false;
  }

  const QByteArray data = file.readAll();
  if (data.isEmpty()) {
    qWarning() << "miniaudio: empty track data" << job.path;
    return false;
  }

  const ma_decoder_config decoder_config =
      ma_decoder_config_init(ma_format_f32, DEFAULT_OUTPUT_CHANNELS, 0);
  ma_decoder decoder;
  if (ma_decoder_init_memory(data.constData(),
                             static_cast<size_t>(data.size()),
                             &decoder_config,
                             &decoder) != MA_SUCCESS) {
    qWarning() << "miniaudio: decoder init failed for" << job.path;
    return false;
  }

  std::vector<float> pcm;
  ma_uint64 expected_frames = 0;
  if (ma_decoder_get_length_in_pcm_frames(&decoder, &expected_frames) == MA_SUCCESS &&
      expected_frames > 0) {
    pcm.reserve(static_cast<std::size_t>(expected_frames) * DEFAULT_OUTPUT_CHANNELS);
  }

  std::array<float, DECODE_BUFFER_FRAMES * DEFAULT_OUTPUT_CHANNELS> buffer{};
  for (;;) {
    ma_uint64 frames_read = 0;
    ma_result const result = ma_decoder_read_pcm_frames(
        &decoder, buffer.data(), DECODE_BUFFER_FRAMES, &frames_read);
    if (frames_read > 0) {
      const auto samples =
          static_cast<std::size_t>(frames_read) * DEFAULT_OUTPUT_CHANNELS;
      pcm.insert(pcm.end(),
                 buffer.begin(),
                 buffer.begin() + static_cast<std::ptrdiff_t>(samples));
    }
    if (result == MA_AT_END) {
      break;
    }
    if (result != MA_SUCCESS) {
      ma_decoder_uninit(&decoder);
      qWarning() << "miniaudio: decode failed for" << job.path << "result:" << result;
      return false;
    }
  }
  const ma_uint32 source_rate = decoder.outputSampleRate;
  ma_decoder_uninit(&decoder);

  if (pcm.empty()) {
    qWarning() << "miniaudio: decode produced no PCM for" << job.path;
    return false;
  }

  const Game::Audio::ResampleReport resampled = Game::Audio::resample_to(
      pcm, DEFAULT_OUTPUT_CHANNELS, source_rate, m_sample_rate);
  log_resample(job.id, resampled);

  auto frame_count = pcm.size() / DEFAULT_OUTPUT_CHANNELS;
  const Game::Audio::Mastering::Analysis analysis = Game::Audio::Mastering::analyse(
      pcm.data(), frame_count, DEFAULT_OUTPUT_CHANNELS, m_sample_rate);
  const Game::Audio::Mastering::Report report =
      Game::Audio::Mastering::apply(pcm.data(),
                                    frame_count,
                                    DEFAULT_OUTPUT_CHANNELS,
                                    m_sample_rate,
                                    Game::Audio::Mastering::profile_for(job.material),
                                    analysis);
  log_mastering(job.id, report);

  const bool loops = job.material == Game::Audio::Mastering::Material::Music ||
                     job.material == Game::Audio::Mastering::Material::Ambience;
  if (loops) {
    const Game::Audio::LoopSeamReport seam = Game::Audio::seal_loop(
        pcm.data(), frame_count, DEFAULT_OUTPUT_CHANNELS, m_sample_rate);
    if (seam.loop_frames > 0) {
      log_loop_seam(job.id, seam);
      frame_count = seam.loop_frames;
      pcm.resize(frame_count * DEFAULT_OUTPUT_CHANNELS);
    }
  }

  auto track = std::make_unique<DecodedTrack>();
  track->frames = static_cast<unsigned>(frame_count);
  if (analysis.channels_identical) {
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
      pcm[frame] = pcm[frame * DEFAULT_OUTPUT_CHANNELS];
    }
    pcm.resize(frame_count);
    pcm.shrink_to_fit();
    track->channels = 1;
  } else {
    track->channels = DEFAULT_OUTPUT_CHANNELS;
  }
  track->pcm = std::move(pcm);

  QMutexLocker const locker(&m_registry_mutex);
  if (job.track < 0 || job.track >= MAX_TRACKS ||
      m_track_ids.value(job.id, -1) != job.track) {
    return false;
  }
  m_track_storage[job.track] = std::move(track);
  m_track_table[job.track].store(m_track_storage[job.track].get(),
                                 std::memory_order_release);
  return true;
}

auto MiniaudioBackend::request_track(const QString& id,
                                     const QString& path,
                                     Game::Audio::Mastering::Material material)
    -> bool {
  const int slot = claim_track_slot(id);
  if (slot < 0) {
    qWarning() << "MiniaudioBackend: no free track slot for" << id;
    return false;
  }
  if (m_track_table[slot].load(std::memory_order_acquire) != nullptr) {
    return true;
  }

  DecodeJob job;
  job.id = id;
  job.path = path;
  job.track = slot;
  job.material = material;

  QMutexLocker locker(&m_decode_mutex);
  if (!m_decode_running) {
    locker.unlock();
    return decode_into_slot(job);
  }
  m_pending_slots.insert(slot);
  const bool is_bed = material == Game::Audio::Mastering::Material::Music ||
                      material == Game::Audio::Mastering::Material::Ambience;
  if (is_bed) {
    m_decode_bulk_jobs.push_back(std::move(job));
  } else {
    m_decode_jobs.push_back(std::move(job));
  }
  m_decode_ready.wakeOne();
  return true;
}

void MiniaudioBackend::unload(const QString& id) {
  wait_for_track(id);

  int slot = -1;
  {
    QMutexLocker const locker(&m_registry_mutex);
    const auto found = m_track_ids.constFind(id);
    if (found == m_track_ids.constEnd()) {
      return;
    }
    slot = found.value();
    m_track_ids.erase(found);
  }

  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::ReleaseTrack;
  command.track = static_cast<std::int16_t>(slot);
  submit_and_wait(command);

  QMutexLocker const locker(&m_registry_mutex);
  m_track_table[slot].store(nullptr, std::memory_order_release);
  m_track_storage[slot].reset();
  m_slot_taken[slot] = false;
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
  const int slot = find_track_slot(id);
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

void MiniaudioBackend::play_sound(const QString& id, float volume, bool loop) {
  const int slot = find_track_slot(id);
  if (slot < 0 || m_track_table[slot].load(std::memory_order_acquire) == nullptr) {
    qWarning() << "MiniaudioBackend: Sound not ready:" << id;
    return;
  }
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::PlaySound;
  command.track = static_cast<std::int16_t>(slot);
  command.volume = sanitize_backend_volume(volume);
  command.loop = loop;
  submit(command);
}

void MiniaudioBackend::set_sound_volume(const QString& id, float volume, int fade_ms) {
  const int slot = find_track_slot(id);
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
  const int slot = find_track_slot(id);
  if (slot < 0) {
    return;
  }
  Game::Audio::AudioCommand command;
  command.type = Game::Audio::AudioCommand::Type::StopSound;
  command.track = static_cast<std::int16_t>(slot);
  submit(command);
}

auto MiniaudioBackend::is_sound_active(const QString& id) const -> bool {
  const int slot = find_track_slot(id);
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

void MiniaudioBackend::apply_command(const Game::Audio::AudioCommand& command) {
  using Type = Game::Audio::AudioCommand::Type;
  const auto channel_index = static_cast<std::size_t>(command.channel);
  const bool channel_valid = command.channel >= 0 && channel_index < m_channels.size();

  switch (command.type) {
  case Type::Play: {
    if (!channel_valid) {
      return;
    }
    Channel& channel = m_channels[channel_index];
    channel.track = command.track;
    channel.frame_pos = 0;
    channel.looping = command.loop;
    channel.paused = false;
    channel.active = true;
    channel.target_volume = command.volume;
    channel.current_volume = MIN_VOLUME;
    channel.fade_samples = std::max(1U, command.fade_samples);
    channel.volume_step =
        (channel.target_volume - channel.current_volume) / float(channel.fade_samples);
    return;
  }
  case Type::Stop: {
    if (!channel_valid || !m_channels[channel_index].active) {
      return;
    }
    Channel& channel = m_channels[channel_index];
    channel.target_volume = MIN_VOLUME;
    channel.fade_samples = std::max(1U, command.fade_samples);
    channel.volume_step =
        (channel.target_volume - channel.current_volume) / float(channel.fade_samples);
    channel.looping = false;
    return;
  }
  case Type::Pause:
    if (channel_valid) {
      m_channels[channel_index].paused = true;
    }
    return;
  case Type::Resume:
    if (channel_valid) {
      m_channels[channel_index].paused = false;
    }
    return;
  case Type::SetVolume: {
    if (!channel_valid || !m_channels[channel_index].active) {
      return;
    }
    Channel& channel = m_channels[channel_index];
    channel.target_volume = command.volume;
    channel.fade_samples = std::max(1U, command.fade_samples);
    channel.volume_step =
        (channel.target_volume - channel.current_volume) / float(channel.fade_samples);
    return;
  }
  case Type::StopAll:
    for (Channel& channel : m_channels) {
      if (!channel.active) {
        continue;
      }
      channel.target_volume = MIN_VOLUME;
      channel.fade_samples = std::max(1U, command.fade_samples);
      channel.volume_step = (channel.target_volume - channel.current_volume) /
                            float(channel.fade_samples);
      channel.looping = false;
    }
    return;
  case Type::SetMasterVolume:
    m_master_volume = command.volume;
    return;
  case Type::PlaySound: {
    for (SoundEffect& effect : m_sound_effects) {
      if (effect.active) {
        continue;
      }
      effect.track = command.track;
      effect.frame_pos = 0;
      effect.volume = command.volume;
      effect.target_volume = command.volume;
      effect.volume_step = 0.0F;
      effect.fade_samples = 0;
      effect.looping = command.loop;
      effect.active = true;
      return;
    }
    return;
  }
  case Type::SetSoundVolume:
    for (SoundEffect& effect : m_sound_effects) {
      if (!effect.active || effect.track != command.track) {
        continue;
      }
      effect.target_volume = command.volume;
      effect.fade_samples = std::max(1U, command.fade_samples);
      effect.volume_step =
          (effect.target_volume - effect.volume) / float(effect.fade_samples);
    }
    return;
  case Type::StopSound:
    for (SoundEffect& effect : m_sound_effects) {
      if (effect.active && effect.track == command.track) {
        effect = SoundEffect{};
      }
    }
    return;
  case Type::ReleaseTrack:
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

void MiniaudioBackend::on_audio(float* output, unsigned frames) {
  static constexpr int STEREO_CHANNELS = 2;

  const unsigned samples = frames * STEREO_CHANNELS;
  std::memset(output, 0, samples * sizeof(float));

  drain_commands();

  const float master = m_master_volume;

  for (Channel& channel : m_channels) {
    if (!channel.active || channel.paused || channel.track < 0) {
      continue;
    }
    const DecodedTrack* track =
        m_track_table[static_cast<std::size_t>(channel.track)].load(
            std::memory_order_acquire);
    if (track == nullptr || track->frames == 0) {
      continue;
    }

    const float* const pcm = track->pcm.data();
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
      const float* source = pcm + (static_cast<std::size_t>(position) * stride);

      const unsigned fading = std::min(run, channel.fade_samples);
      for (unsigned i = 0; i < fading; ++i) {
        const float volume = channel.current_volume * master;
        const float left = source[0] * volume;
        destination[0] += left;
        destination[1] += (stride == 1) ? left : source[1] * volume;
        destination += STEREO_CHANNELS;
        source += stride;
        channel.current_volume += channel.volume_step;
        if (--channel.fade_samples == 0) {
          channel.current_volume = channel.target_volume;
        }
      }
      const unsigned steady = run - fading;
      if (steady > 0) {
        const float volume = channel.current_volume * master;
        if (stride == 1) {
          for (unsigned i = 0; i < steady; ++i) {
            const float value = source[i] * volume;
            destination[0] += value;
            destination[1] += value;
            destination += STEREO_CHANNELS;
          }
          source += steady;
        } else {
          for (unsigned i = 0; i < steady; ++i) {
            destination[0] += source[0] * volume;
            destination[1] += source[1] * volume;
            destination += STEREO_CHANNELS;
            source += STEREO_CHANNELS;
          }
        }
      }
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

  for (SoundEffect& effect : m_sound_effects) {
    if (!effect.active || effect.track < 0) {
      continue;
    }
    const DecodedTrack* track =
        m_track_table[static_cast<std::size_t>(effect.track)].load(
            std::memory_order_acquire);
    if (track == nullptr || track->frames == 0) {
      effect = SoundEffect{};
      continue;
    }

    const float* const pcm = track->pcm.data();
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
      const float* source = pcm + (static_cast<std::size_t>(position) * stride);

      const unsigned fading = std::min(run, effect.fade_samples);
      for (unsigned i = 0; i < fading; ++i) {
        const float volume = effect.volume * master;
        const float left = source[0] * volume;
        destination[0] += left;
        destination[1] += (stride == 1) ? left : source[1] * volume;
        destination += STEREO_CHANNELS;
        source += stride;
        effect.volume += effect.volume_step;
        if (--effect.fade_samples == 0) {
          effect.volume = effect.target_volume;
        }
      }
      const unsigned steady = run - fading;
      if (steady > 0) {
        const float volume = effect.volume * master;
        if (stride == 1) {
          for (unsigned i = 0; i < steady; ++i) {
            const float value = source[i] * volume;
            destination[0] += value;
            destination[1] += value;
            destination += STEREO_CHANNELS;
          }
        } else {
          for (unsigned i = 0; i < steady; ++i) {
            destination[0] += source[0] * volume;
            destination[1] += source[1] * volume;
            destination += STEREO_CHANNELS;
            source += STEREO_CHANNELS;
          }
        }
      }
      position += run;
      frames_left -= run;
    }

    effect.frame_pos = position;
    if (!effect.active) {
      effect = SoundEffect{};
    }
  }

  publish_state();

  if (m_bus_limiter.is_ready()) {
    m_bus_limiter.process(output, frames);
    return;
  }

  for (unsigned i = 0; i < samples; ++i) {
    output[i] = std::clamp(output[i], -MAX_OUTPUT_SAMPLE, MAX_OUTPUT_SAMPLE);
  }
}
