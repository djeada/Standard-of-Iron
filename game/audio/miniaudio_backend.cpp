#include "miniaudio_backend.h"

#include <QDebug>
#include <QFile>
#include <qglobal.h>
#include <qhashfunctions.h>
#include <qmutex.h>
#include <qobject.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>

namespace {

struct BiquadCoefficients {
  float b0 = 1.0F;
  float b1 = 0.0F;
  float b2 = 0.0F;
  float a1 = 0.0F;
  float a2 = 0.0F;
};

struct BiquadState {
  float x1 = 0.0F;
  float x2 = 0.0F;
  float y1 = 0.0F;
  float y2 = 0.0F;

  auto process(const BiquadCoefficients& coefficients, float input) -> float {
    const float output = (coefficients.b0 * input) + (coefficients.b1 * x1) +
                         (coefficients.b2 * x2) - (coefficients.a1 * y1) -
                         (coefficients.a2 * y2);
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;
    return output;
  }
};

struct AudioSofteningProfile {
  float intensity = 1.0F;
  float output_gain_db = 0.4F;
  float high_band_cutoff_hz = 5600.0F;
  float high_band_threshold = 0.075F;
  float high_band_ratio = 2.8F;
  float high_band_compression_blend = 0.55F;
  float warmth_drive = 0.035F;
};

auto decibels_to_amplitude(float decibels) -> float {
  return std::pow(10.0F, decibels / 20.0F);
}

auto sanitize_backend_volume(float volume) -> float {
  if (!std::isfinite(volume)) {
    return MiniaudioBackend::MIN_VOLUME;
  }
  return std::clamp(volume, MiniaudioBackend::MIN_VOLUME, MiniaudioBackend::MAX_VOLUME);
}

auto clamp_filter_frequency(float frequency_hz, int sample_rate) -> float {
  const float nyquist = 0.5F * static_cast<float>(sample_rate);
  return std::clamp(frequency_hz, 20.0F, nyquist * 0.9F);
}

auto make_peaking_eq(float frequency_hz,
                     float q,
                     float gain_db,
                     int sample_rate) -> BiquadCoefficients {
  static constexpr float PI = 3.14159265358979323846F;

  const float frequency = clamp_filter_frequency(frequency_hz, sample_rate);
  const float omega = 2.0F * PI * frequency / static_cast<float>(sample_rate);
  const float sin_omega = std::sin(omega);
  const float cos_omega = std::cos(omega);
  const float alpha = sin_omega / (2.0F * q);
  const float amplitude = std::pow(10.0F, gain_db / 40.0F);

  const float b0 = 1.0F + (alpha * amplitude);
  const float b1 = -2.0F * cos_omega;
  const float b2 = 1.0F - (alpha * amplitude);
  const float a0 = 1.0F + (alpha / amplitude);
  const float a1 = -2.0F * cos_omega;
  const float a2 = 1.0F - (alpha / amplitude);

  return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

auto make_high_shelf(float frequency_hz,
                     float slope,
                     float gain_db,
                     int sample_rate) -> BiquadCoefficients {
  static constexpr float PI = 3.14159265358979323846F;

  const float frequency = clamp_filter_frequency(frequency_hz, sample_rate);
  const float omega = 2.0F * PI * frequency / static_cast<float>(sample_rate);
  const float sin_omega = std::sin(omega);
  const float cos_omega = std::cos(omega);
  const float amplitude = std::pow(10.0F, gain_db / 40.0F);
  const float alpha =
      (sin_omega * 0.5F) *
      std::sqrt(((amplitude + (1.0F / amplitude)) * ((1.0F / slope) - 1.0F)) + 2.0F);
  const float two_sqrt_amplitude_alpha = 2.0F * std::sqrt(amplitude) * alpha;

  const float b0 = amplitude * ((amplitude + 1.0F) + ((amplitude - 1.0F) * cos_omega) +
                                two_sqrt_amplitude_alpha);
  const float b1 =
      -2.0F * amplitude * ((amplitude - 1.0F) + ((amplitude + 1.0F) * cos_omega));
  const float b2 = amplitude * ((amplitude + 1.0F) + ((amplitude - 1.0F) * cos_omega) -
                                two_sqrt_amplitude_alpha);
  const float a0 =
      (amplitude + 1.0F) - ((amplitude - 1.0F) * cos_omega) + two_sqrt_amplitude_alpha;
  const float a1 = 2.0F * ((amplitude - 1.0F) - ((amplitude + 1.0F) * cos_omega));
  const float a2 =
      (amplitude + 1.0F) - ((amplitude - 1.0F) * cos_omega) - two_sqrt_amplitude_alpha;

  return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

auto compress_high_band(float high_band, float threshold, float ratio) -> float {
  const float magnitude = std::abs(high_band);
  if (magnitude <= threshold) {
    return high_band;
  }

  const float compressed_magnitude = threshold + ((magnitude - threshold) / ratio);
  return std::copysign(compressed_magnitude, high_band);
}

auto make_softening_profile(const QString& id,
                            const QString& path) -> AudioSofteningProfile {
  AudioSofteningProfile profile;
  const QString key = (id + QStringLiteral(" ") + path).toLower();

  if (key.contains(QStringLiteral("music/"))) {
    profile.intensity = 0.45F;
    profile.output_gain_db = 0.15F;
    profile.high_band_threshold = 0.095F;
    profile.high_band_compression_blend = 0.30F;
    profile.warmth_drive = 0.015F;
  } else if (key.contains(QStringLiteral("ambience/"))) {
    profile.intensity = 0.60F;
    profile.output_gain_db = 0.2F;
    profile.high_band_threshold = 0.09F;
    profile.high_band_compression_blend = 0.35F;
    profile.warmth_drive = 0.018F;
  } else if (key.contains(QStringLiteral("voices/"))) {
    profile.intensity = 0.85F;
    profile.output_gain_db = 0.45F;
    profile.high_band_cutoff_hz = 5200.0F;
    profile.high_band_threshold = 0.07F;
    profile.high_band_compression_blend = 0.50F;
    profile.warmth_drive = 0.025F;
  } else if (key.contains(QStringLiteral("ui.")) ||
             key.contains(QStringLiteral("low_resources_click"))) {
    profile.intensity = 1.30F;
    profile.output_gain_db = -0.2F;
    profile.high_band_cutoff_hz = 4600.0F;
    profile.high_band_threshold = 0.045F;
    profile.high_band_ratio = 3.6F;
    profile.high_band_compression_blend = 0.78F;
    profile.warmth_drive = 0.060F;
  } else if (key.contains(QStringLiteral("sfx/combat/"))) {
    profile.intensity = 1.10F;
    profile.output_gain_db = 0.15F;
    profile.high_band_cutoff_hz = 5000.0F;
    profile.high_band_threshold = 0.06F;
    profile.high_band_ratio = 3.2F;
    profile.high_band_compression_blend = 0.66F;
    profile.warmth_drive = 0.045F;
  }

  return profile;
}

auto estimate_high_band_ratio(const QVector<float>& pcm, int sample_rate) -> float {
  static constexpr float ANALYSIS_CUTOFF_HZ = 5200.0F;

  if (pcm.isEmpty() || sample_rate <= 0) {
    return 0.0F;
  }

  std::array<float, MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS> low{};
  const float alpha =
      1.0F - std::exp(-2.0F * 3.14159265358979323846F * ANALYSIS_CUTOFF_HZ /
                      static_cast<float>(sample_rate));

  double full_band_energy = 0.0;
  double high_band_energy = 0.0;
  int sample_count = 0;
  for (int i = 0; i + 1 < pcm.size(); i += MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS) {
    for (int channel = 0; channel < MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS;
         ++channel) {
      const float sample = pcm[i + channel];
      low[channel] += alpha * (sample - low[channel]);
      const float high_band = sample - low[channel];
      full_band_energy += double(sample) * double(sample);
      high_band_energy += double(high_band) * double(high_band);
      ++sample_count;
    }
  }

  if (sample_count == 0 || full_band_energy <= 0.0) {
    return 0.0F;
  }

  return static_cast<float>(std::sqrt(high_band_energy / full_band_energy));
}

void adapt_profile_to_content(AudioSofteningProfile& profile, float high_band_ratio) {
  if (high_band_ratio > 0.58F) {
    profile.intensity *= 1.25F;
    profile.high_band_threshold *= 0.82F;
    profile.high_band_compression_blend += 0.08F;
  } else if (high_band_ratio > 0.44F) {
    profile.intensity *= 1.10F;
    profile.high_band_threshold *= 0.92F;
  } else if (high_band_ratio < 0.24F) {
    profile.intensity *= 0.68F;
    profile.high_band_threshold *= 1.18F;
    profile.high_band_compression_blend -= 0.08F;
  }

  profile.intensity = std::clamp(profile.intensity, 0.25F, 1.55F);
  profile.high_band_threshold = std::clamp(profile.high_band_threshold, 0.035F, 0.13F);
  profile.high_band_compression_blend =
      std::clamp(profile.high_band_compression_blend, 0.18F, 0.88F);
}

auto soften_peak_shape(float sample, float drive_amount) -> float {
  const float drive = 1.0F + drive_amount;
  const float normalizer = std::tanh(drive);
  if (normalizer <= 0.0F) {
    return sample;
  }
  return std::tanh(sample * drive) / normalizer;
}

void apply_decode_softening_filter(QVector<float>& pcm,
                                   int sample_rate,
                                   const QString& id,
                                   const QString& path) {
  static constexpr float DC_BLOCKER_DECAY = 0.995F;

  if (pcm.isEmpty() || sample_rate <= 0) {
    return;
  }

  AudioSofteningProfile profile = make_softening_profile(id, path);
  adapt_profile_to_content(profile, estimate_high_band_ratio(pcm, sample_rate));

  const std::array<BiquadCoefficients, 3> filters = {
      make_peaking_eq(3600.0F, 1.05F, -1.6F * profile.intensity, sample_rate),
      make_peaking_eq(6200.0F, 1.35F, -2.2F * profile.intensity, sample_rate),
      make_high_shelf(9200.0F, 0.75F, -1.1F * profile.intensity, sample_rate),
  };
  std::array<std::array<BiquadState, 3>, MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS>
      filter_states{};
  std::array<float, MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS> previous_input{};
  std::array<float, MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS> previous_dc_output{};
  std::array<float, MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS> high_band_low{};

  const float high_band_alpha =
      1.0F - std::exp(-2.0F * 3.14159265358979323846F * profile.high_band_cutoff_hz /
                      static_cast<float>(sample_rate));
  const float output_gain = decibels_to_amplitude(profile.output_gain_db);
  const float warmth_drive = profile.warmth_drive * profile.intensity;

  for (int i = 0; i + 1 < pcm.size(); i += MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS) {
    for (int channel = 0; channel < MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS;
         ++channel) {
      const int sample_index = i + channel;
      const float input = pcm[sample_index];
      float sample = input - previous_input[channel] +
                     (DC_BLOCKER_DECAY * previous_dc_output[channel]);
      previous_input[channel] = input;
      previous_dc_output[channel] = sample;

      for (size_t filter_index = 0; filter_index < filters.size(); ++filter_index) {
        sample =
            filter_states[channel][filter_index].process(filters[filter_index], sample);
      }

      high_band_low[channel] += high_band_alpha * (sample - high_band_low[channel]);
      const float high_band = sample - high_band_low[channel];
      const float compressed_high_band = compress_high_band(
          high_band, profile.high_band_threshold, profile.high_band_ratio);
      sample = high_band_low[channel] +
               (high_band * (1.0F - profile.high_band_compression_blend)) +
               (compressed_high_band * profile.high_band_compression_blend);
      sample = soften_peak_shape(sample, warmth_drive);

      pcm[sample_index] = std::clamp(sample * output_gain, -1.0F, 1.0F);
    }
  }
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
#include <stb_vorbis.h>
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

auto MiniaudioBackend::initialize(int device_rate, int, int music_channels) -> bool {
  m_sample_rate = std::max(MIN_SAMPLE_RATE, device_rate);
  m_output_channels = DEFAULT_OUTPUT_CHANNELS;

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
    return false;
  }

  m_device_wrapper = std::move(wrapper);

  m_channels.resize(std::max(1, music_channels));
  for (auto& channel : m_channels) {
    channel = Channel{};
  }

  m_sound_effects.resize(DEFAULT_SOUND_EFFECT_SLOTS);
  for (auto& sfx : m_sound_effects) {
    sfx = SoundEffect{};
  }

  if (ma_device_start(m_device.get()) != MA_SUCCESS) {
    qWarning() << "MiniaudioBackend: Failed to start audio device";
    ma_device_uninit(m_device.get());
    m_device.reset();
    m_device_wrapper.reset();
    return false;
  }

  qInfo() << "MiniaudioBackend: Initialized successfully";
  qInfo() << "  Sample rate:" << m_sample_rate;
  qInfo() << "  Channels:" << m_output_channels;
  qInfo() << "  Music channels:" << music_channels;
  return true;
}

void MiniaudioBackend::shutdown() {
  QMutexLocker const locker(&m_mutex);
  stop_device();
  m_tracks.clear();
  m_channels.clear();
}

void MiniaudioBackend::stop_device() {
  if (m_device == nullptr) {
    return;
  }
  ma_device_stop(m_device.get());
  ma_device_uninit(m_device.get());
  m_device.reset();
  m_device_wrapper.reset();
}

auto MiniaudioBackend::predecode(const QString& id, const QString& path) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "miniaudio: QFile open failed for" << path;
    return false;
  }

  const QByteArray data = file.readAll();
  if (data.isEmpty()) {
    qWarning() << "miniaudio: empty track data" << path;
    return false;
  }

  QVector<float> pcm;
  ma_decoder_config const decoder_config =
      ma_decoder_config_init(ma_format_f32, DEFAULT_OUTPUT_CHANNELS, m_sample_rate);
  ma_decoder decoder;
  ma_result const decoder_result = ma_decoder_init_memory(
      data.constData(), static_cast<size_t>(data.size()), &decoder_config, &decoder);
  if (decoder_result != MA_SUCCESS) {
    qWarning() << "miniaudio: decoder init failed for" << path << "size:" << data.size()
               << "result:" << decoder_result;
    return false;
  }

  float buffer[DECODE_BUFFER_FRAMES * DEFAULT_OUTPUT_CHANNELS];
  for (;;) {
    ma_uint64 frames_read = 0;
    ma_result const result = ma_decoder_read_pcm_frames(
        &decoder, buffer, DECODE_BUFFER_FRAMES, &frames_read);
    if (frames_read > 0) {
      const size_t samples = size_t(frames_read) * DEFAULT_OUTPUT_CHANNELS;
      const size_t old_size = pcm.size();
      pcm.resize(old_size + samples);
      std::memcpy(pcm.data() + old_size, buffer, samples * sizeof(float));
    }
    if (result == MA_AT_END) {
      break;
    }
    if (result != MA_SUCCESS) {
      ma_decoder_uninit(&decoder);
      qWarning() << "miniaudio: decode failed for" << path << "result:" << result;
      return false;
    }
  }
  ma_decoder_uninit(&decoder);

  if (pcm.isEmpty()) {
    qWarning() << "miniaudio: decode produced no PCM for" << path;
    return false;
  }

  apply_decode_softening_filter(pcm, m_sample_rate, id, path);

  QMutexLocker const locker(&m_mutex);
  DecodedTrack track;
  track.frames = pcm.size() / DEFAULT_OUTPUT_CHANNELS;
  track.pcm = std::move(pcm);
  m_tracks[id] = std::move(track);
  return true;
}

void MiniaudioBackend::play(
    int channel, const QString& id, float volume, bool loop, int fade_ms) {
  static constexpr int MIN_FADE_MS = 1;
  static constexpr int MS_PER_SECOND = 1000;

  QMutexLocker locker(&m_mutex);
  if (channel < 0 || channel >= m_channels.size()) {
    return;
  }
  auto it = m_tracks.find(id);
  if (it == m_tracks.end()) {
    locker.unlock();

    predecode(id, id);
    locker.relock();
    it = m_tracks.find(id);
    if (it == m_tracks.end()) {
      qWarning() << "MiniaudioBackend: unknown track" << id;
      return;
    }
  }

  auto& ch = m_channels[channel];
  ch.track = &it.value();
  ch.frame_pos = 0;
  ch.looping = loop;
  ch.paused = false;
  ch.active = true;
  ch.target_volume = sanitize_backend_volume(volume);
  ch.current_volume = MIN_VOLUME;

  const unsigned fade_samples = std::max(
      unsigned(MIN_FADE_MS), unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
  ch.fade_samples = fade_samples;
  ch.volume_step = (ch.target_volume - ch.current_volume) / float(fade_samples);
}

void MiniaudioBackend::stop(int channel, int fade_ms) {
  static constexpr int MIN_FADE_MS = 1;
  static constexpr int MS_PER_SECOND = 1000;

  QMutexLocker const locker(&m_mutex);
  if (channel < 0 || channel >= m_channels.size()) {
    return;
  }
  auto& ch = m_channels[channel];
  if (!ch.active) {
    return;
  }
  const unsigned fade_samples = std::max(
      unsigned(MIN_FADE_MS), unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
  ch.target_volume = MIN_VOLUME;
  ch.fade_samples = fade_samples;
  ch.volume_step = (ch.target_volume - ch.current_volume) / float(fade_samples);
  ch.looping = false;
}

void MiniaudioBackend::pause(int channel) {
  QMutexLocker const locker(&m_mutex);
  if (channel >= 0 && channel < m_channels.size()) {
    m_channels[channel].paused = true;
  }
}
void MiniaudioBackend::resume(int channel) {
  QMutexLocker const locker(&m_mutex);
  if (channel >= 0 && channel < m_channels.size()) {
    m_channels[channel].paused = false;
  }
}

void MiniaudioBackend::set_volume(int channel, float volume, int fade_ms) {
  static constexpr int MIN_FADE_MS = 1;
  static constexpr int MS_PER_SECOND = 1000;

  QMutexLocker const locker(&m_mutex);
  if (channel < 0 || channel >= m_channels.size()) {
    return;
  }
  auto& ch = m_channels[channel];
  if (!ch.active) {
    return;
  }
  ch.target_volume = sanitize_backend_volume(volume);
  const unsigned fade_samples = std::max(
      unsigned(MIN_FADE_MS), unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
  ch.fade_samples = fade_samples;
  ch.volume_step = (ch.target_volume - ch.current_volume) / float(fade_samples);
}

void MiniaudioBackend::stop_all(int fade_ms) {
  static constexpr int MIN_FADE_MS = 1;
  static constexpr int MS_PER_SECOND = 1000;

  QMutexLocker const locker(&m_mutex);
  const unsigned fade_samples = std::max(
      unsigned(MIN_FADE_MS), unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
  for (auto& ch : m_channels) {
    if (!ch.active) {
      continue;
    }
    ch.target_volume = MIN_VOLUME;
    ch.fade_samples = fade_samples;
    ch.volume_step = (ch.target_volume - ch.current_volume) / float(fade_samples);
    ch.looping = false;
  }
}

void MiniaudioBackend::set_master_volume(float volume, int) {
  QMutexLocker const locker(&m_mutex);
  m_master_volume = sanitize_backend_volume(volume);
}

auto MiniaudioBackend::any_channel_playing() const -> bool {
  QMutexLocker const locker(&m_mutex);
  for (const auto& ch : m_channels) {
    if (ch.active && !ch.paused) {
      return true;
    }
  }
  return false;
}
auto MiniaudioBackend::channel_playing(int channel) const -> bool {
  QMutexLocker const locker(&m_mutex);
  if (channel < 0 || channel >= m_channels.size()) {
    return false;
  }
  const auto& ch = m_channels[channel];
  return ch.active && !ch.paused;
}

void MiniaudioBackend::play_sound(const QString& id, float volume, bool loop) {
  QMutexLocker const locker(&m_mutex);

  auto it = m_tracks.find(id);
  if (it == m_tracks.end()) {
    qWarning() << "MiniaudioBackend: Sound not preloaded:" << id;
    return;
  }

  int const slot = find_free_sound_slot();
  if (slot < 0) {
    qWarning() << "MiniaudioBackend: No free sound slots available";
    return;
  }

  auto& sfx = m_sound_effects[slot];
  sfx.track = &it.value();
  sfx.frame_pos = 0;
  sfx.volume = sanitize_backend_volume(volume);
  sfx.looping = loop;
  sfx.active = true;
}

void MiniaudioBackend::stop_sound(const QString& id) {
  QMutexLocker const locker(&m_mutex);

  auto it = m_tracks.find(id);
  if (it == m_tracks.end()) {
    return;
  }

  const DecodedTrack* const track = &it.value();
  for (auto& sfx : m_sound_effects) {
    if (sfx.active && sfx.track == track) {
      sfx.track = nullptr;
      sfx.frame_pos = 0;
      sfx.volume = DEFAULT_VOLUME;
      sfx.looping = false;
      sfx.active = false;
    }
  }
}

auto MiniaudioBackend::is_sound_active(const QString& id) const -> bool {
  QMutexLocker const locker(&m_mutex);

  auto it = m_tracks.find(id);
  if (it == m_tracks.end()) {
    return false;
  }

  const DecodedTrack* const track = &it.value();
  for (const auto& sfx : m_sound_effects) {
    if (sfx.active && sfx.track == track) {
      return true;
    }
  }
  return false;
}

auto MiniaudioBackend::find_free_sound_slot() const -> int {
  for (int i = 0; i < m_sound_effects.size(); ++i) {
    if (!m_sound_effects[i].active) {
      return i;
    }
  }
  return -1;
}

void MiniaudioBackend::on_audio(float* output, unsigned frames) {
  static constexpr int STEREO_CHANNELS = 2;

  const unsigned samples = frames * STEREO_CHANNELS;
  std::memset(output, 0, samples * sizeof(float));

  QMutexLocker const locker(&m_mutex);

  for (auto& ch : m_channels) {
    if (!ch.active || ch.paused || ch.track == nullptr) {
      continue;
    }

    const auto* pcm = ch.track->pcm.constData();
    unsigned frames_left = frames;
    unsigned pos = ch.frame_pos;

    float* dst = output;

    while (frames_left > 0) {
      if (pos >= ch.track->frames) {
        if (ch.looping) {
          pos = 0;
        } else {
          break;
        }
      }
      const unsigned can_copy = std::min(frames_left, ch.track->frames - pos);
      const float* src = pcm + static_cast<size_t>(pos * STEREO_CHANNELS);

      for (unsigned i = 0; i < can_copy; ++i) {
        const float vol = ch.current_volume * m_master_volume;
        dst[0] += src[0] * vol;
        dst[1] += src[1] * vol;
        dst += STEREO_CHANNELS;
        src += STEREO_CHANNELS;

        if (ch.fade_samples > 0) {
          ch.current_volume += ch.volume_step;
          --ch.fade_samples;
          if (ch.fade_samples == 0) {
            ch.current_volume = ch.target_volume;
          }
        }
      }
      pos += can_copy;
      frames_left -= can_copy;
    }

    ch.frame_pos = pos;

    if (!ch.looping && ch.frame_pos >= ch.track->frames) {
      ch.active = false;
      ch.current_volume = ch.target_volume = MIN_VOLUME;
      ch.fade_samples = 0;
    }
    if (ch.fade_samples == 0 && ch.current_volume == MIN_VOLUME &&
        ch.target_volume == MIN_VOLUME && !ch.looping) {
      ch.active = false;
    }
  }

  for (auto& sfx : m_sound_effects) {
    if (!sfx.active || sfx.track == nullptr) {
      continue;
    }

    const auto* pcm = sfx.track->pcm.constData();
    unsigned frames_left = frames;
    unsigned pos = sfx.frame_pos;
    float* dst = output;

    while (frames_left > 0) {
      if (pos >= sfx.track->frames) {
        if (sfx.looping) {
          pos = 0;
        } else {
          sfx.active = false;
          break;
        }
      }

      const unsigned can_copy = std::min(frames_left, sfx.track->frames - pos);
      const float* src = pcm + static_cast<size_t>(pos * STEREO_CHANNELS);

      for (unsigned i = 0; i < can_copy; ++i) {
        const float vol = sfx.volume * m_master_volume;
        dst[0] += src[0] * vol;
        dst[1] += src[1] * vol;
        dst += STEREO_CHANNELS;
        src += STEREO_CHANNELS;
      }

      pos += can_copy;
      frames_left -= can_copy;
    }

    sfx.frame_pos = pos;
  }

  for (unsigned i = 0; i < samples; ++i) {
    if (output[i] > MAX_VOLUME) {
      output[i] = MAX_VOLUME;
    } else if (output[i] < -MAX_VOLUME) {
      output[i] = -MAX_VOLUME;
    }
  }
}
