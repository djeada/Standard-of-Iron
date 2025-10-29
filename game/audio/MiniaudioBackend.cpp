#include "MiniaudioBackend.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <qglobal.h>
#include <qhashfunctions.h>
#include <qmutex.h>
#include <qobject.h>
#include <utility>

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
#include <miniaudio.h>

struct DeviceWrapper {
  MiniaudioBackend *self;
};

static void audioCallback(ma_device *device, void *output_buffer, const void *,
                          ma_uint32 frame_count) {
  auto *wrapper = reinterpret_cast<DeviceWrapper *>(device->pUserData);
  if ((wrapper == nullptr) || (wrapper->self == nullptr)) {
    std::memset(output_buffer, 0,
                static_cast<unsigned long>(frame_count * MiniaudioBackend::DEFAULT_OUTPUT_CHANNELS) * sizeof(float));
    return;
  }
  wrapper->self->on_audio(reinterpret_cast<float *>(output_buffer), frame_count);
}

MiniaudioBackend::MiniaudioBackend(QObject *parent) : QObject(parent) {}
MiniaudioBackend::~MiniaudioBackend() { shutdown(); }

auto MiniaudioBackend::initialize(int device_rate, int,
                                  int music_channels) -> bool {
  m_sample_rate = std::max(MIN_SAMPLE_RATE, device_rate);
  m_output_channels = DEFAULT_OUTPUT_CHANNELS;

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = m_output_channels;
  config.sampleRate = m_sample_rate;
  config.dataCallback = audioCallback;

  auto *wrapper = new DeviceWrapper{this};
  config.pUserData = wrapper;

  m_device = new ma_device();
  if (ma_device_init(nullptr, &config, m_device) != MA_SUCCESS) {
    qWarning() << "MiniaudioBackend: Failed to initialize audio device";
    qWarning() << "  Requested sample rate:" << m_sample_rate;
    qWarning() << "  Requested channels:" << m_output_channels;
    qWarning() << "  This may indicate no audio device is available";
    delete m_device;
    m_device = nullptr;
    delete wrapper;
    return false;
  }

  m_channels.resize(std::max(1, music_channels));
  for (auto &channel : m_channels) {
    channel = Channel{};
  }

  m_sound_effects.resize(DEFAULT_SOUND_EFFECT_SLOTS);
  for (auto &sfx : m_sound_effects) {
    sfx = SoundEffect{};
  }

  if (ma_device_start(m_device) != MA_SUCCESS) {
    qWarning() << "MiniaudioBackend: Failed to start audio device";
    ma_device_uninit(m_device);
    delete m_device;
    m_device = nullptr;
    delete wrapper;
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
  auto *wrapper = reinterpret_cast<DeviceWrapper *>(m_device->pUserData);
  ma_device_stop(m_device);
  ma_device_uninit(m_device);
  delete m_device;
  m_device = nullptr;
  delete wrapper;
}

auto MiniaudioBackend::predecode(const QString &id,
                                 const QString &path) -> bool {

  ma_decoder_config const decoder_config =
      ma_decoder_config_init(ma_format_f32, m_output_channels, m_sample_rate);
  ma_decoder decoder;
  if (ma_decoder_init_file(path.toUtf8().constData(), &decoder_config, &decoder) !=
      MA_SUCCESS) {
    qWarning() << "miniaudio: cannot open" << path;
    return false;
  }

  QVector<float> pcm;
  float buffer[DECODE_BUFFER_FRAMES * DEFAULT_OUTPUT_CHANNELS];
  for (;;) {
    ma_uint64 frames_read = 0;
    ma_result const result =
        ma_decoder_read_pcm_frames(&decoder, buffer, DECODE_BUFFER_FRAMES, &frames_read);
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
      return false;
    }
  }
  ma_decoder_uninit(&decoder);

  QMutexLocker const locker(&m_mutex);
  DecodedTrack track;
  track.frames = pcm.size() / DEFAULT_OUTPUT_CHANNELS;
  track.pcm = std::move(pcm);
  m_tracks[id] = std::move(track);
  return true;
}

void MiniaudioBackend::play(int channel, const QString &id, float volume,
                            bool loop, int fade_ms) {
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

  auto &ch = m_channels[channel];
  ch.track = &it.value();
  ch.frame_pos = 0;
  ch.looping = loop;
  ch.paused = false;
  ch.active = true;
  ch.target_volume = std::clamp(volume, MIN_VOLUME, MAX_VOLUME);
  ch.current_volume = MIN_VOLUME;

  const unsigned fade_samples =
      std::max(unsigned(MIN_FADE_MS), unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
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
  auto &ch = m_channels[channel];
  if (!ch.active) {
    return;
  }
  const unsigned fade_samples =
      std::max(unsigned(MIN_FADE_MS), unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
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
  auto &ch = m_channels[channel];
  if (!ch.active) {
    return;
  }
  ch.target_volume = std::clamp(volume, MIN_VOLUME, MAX_VOLUME);
  const unsigned fade_samples =
      std::max(unsigned(MIN_FADE_MS), unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
  ch.fade_samples = fade_samples;
  ch.volume_step = (ch.target_volume - ch.current_volume) / float(fade_samples);
}

void MiniaudioBackend::stop_all(int fade_ms) {
  static constexpr int MIN_FADE_MS = 1;
  static constexpr int MS_PER_SECOND = 1000;
  
  QMutexLocker const locker(&m_mutex);
  const unsigned fade_samples =
      std::max(unsigned(MIN_FADE_MS), unsigned((fade_ms * m_sample_rate) / MS_PER_SECOND));
  for (auto &ch : m_channels) {
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
  m_master_volume = std::clamp(volume, MIN_VOLUME, MAX_VOLUME);
}

auto MiniaudioBackend::any_channel_playing() const -> bool {
  QMutexLocker const locker(&m_mutex);
  for (const auto &ch : m_channels) {
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
  const auto &ch = m_channels[channel];
  return ch.active && !ch.paused;
}

void MiniaudioBackend::play_sound(const QString &id, float volume, bool loop) {
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

  auto &sfx = m_sound_effects[slot];
  sfx.track = &it.value();
  sfx.frame_pos = 0;
  sfx.volume = std::clamp(volume, MIN_VOLUME, MAX_VOLUME);
  sfx.looping = loop;
  sfx.active = true;
}

auto MiniaudioBackend::find_free_sound_slot() const -> int {
  for (int i = 0; i < m_sound_effects.size(); ++i) {
    if (!m_sound_effects[i].active) {
      return i;
    }
  }
  return -1;
}

void MiniaudioBackend::on_audio(float *output, unsigned frames) {
  static constexpr int STEREO_CHANNELS = 2;

  const unsigned samples = frames * STEREO_CHANNELS;
  std::memset(output, 0, samples * sizeof(float));

  QMutexLocker const locker(&m_mutex);

  for (auto &ch : m_channels) {
    if (!ch.active || ch.paused || ch.track == nullptr) {
      continue;
    }

    const auto *pcm = ch.track->pcm.constData();
    unsigned frames_left = frames;
    unsigned pos = ch.frame_pos;

    float *dst = output;

    while (frames_left > 0) {
      if (pos >= ch.track->frames) {
        if (ch.looping) {
          pos = 0;
        } else {
          break;
        }
      }
      const unsigned can_copy = std::min(frames_left, ch.track->frames - pos);
      const float *src = pcm + static_cast<size_t>(pos * STEREO_CHANNELS);

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
    if (ch.fade_samples == 0 && ch.current_volume == MIN_VOLUME && ch.target_volume == MIN_VOLUME &&
        !ch.looping) {
      ch.active = false;
    }
  }

  for (auto &sfx : m_sound_effects) {
    if (!sfx.active || sfx.track == nullptr) {
      continue;
    }

    const auto *pcm = sfx.track->pcm.constData();
    unsigned frames_left = frames;
    unsigned pos = sfx.frame_pos;
    float *dst = output;

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
      const float *src = pcm + static_cast<size_t>(pos * STEREO_CHANNELS);

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
