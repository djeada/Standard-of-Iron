#include "MiniaudioBackend.h"
#include <QDebug>
#include <cmath>
#include <cstring>

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
#include "third_party/miniaudio.h"

struct DeviceWrapper {
  MiniaudioBackend *self;
};

static void audioCallback(ma_device *device, void *pOutput, const void *,
                          ma_uint32 frameCount) {
  auto *w = reinterpret_cast<DeviceWrapper *>(device->pUserData);
  if (!w || !w->self) {
    std::memset(pOutput, 0, frameCount * 2 * sizeof(float));
    return;
  }
  w->self->onAudio(reinterpret_cast<float *>(pOutput), frameCount);
}

MiniaudioBackend::MiniaudioBackend(QObject *parent) : QObject(parent) {}
MiniaudioBackend::~MiniaudioBackend() { shutdown(); }

bool MiniaudioBackend::initialize(int deviceRate, int outChannels,
                                  int musicChannels) {
  m_rate = std::max(22050, deviceRate);
  m_outCh = 2;

  ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
  cfg.playback.format = ma_format_f32;
  cfg.playback.channels = m_outCh;
  cfg.sampleRate = m_rate;
  cfg.dataCallback = audioCallback;

  auto *wrap = new DeviceWrapper{this};
  cfg.pUserData = wrap;

  m_device = new ma_device();
  if (ma_device_init(nullptr, &cfg, m_device) != MA_SUCCESS) {
    qWarning() << "MiniaudioBackend: Failed to initialize audio device";
    qWarning() << "  Requested sample rate:" << m_rate;
    qWarning() << "  Requested channels:" << m_outCh;
    qWarning() << "  This may indicate no audio device is available";
    delete m_device;
    m_device = nullptr;
    delete wrap;
    return false;
  }

  m_channels.resize(std::max(1, musicChannels));
  for (auto &ch : m_channels)
    ch = Channel{};

  m_soundEffects.resize(32);
  for (auto &sfx : m_soundEffects)
    sfx = SoundEffect{};

  if (ma_device_start(m_device) != MA_SUCCESS) {
    qWarning() << "MiniaudioBackend: Failed to start audio device";
    ma_device_uninit(m_device);
    delete m_device;
    m_device = nullptr;
    delete wrap;
    return false;
  }

  qInfo() << "MiniaudioBackend: Initialized successfully";
  qInfo() << "  Sample rate:" << m_rate;
  qInfo() << "  Channels:" << m_outCh;
  qInfo() << "  Music channels:" << musicChannels;
  return true;
}

void MiniaudioBackend::shutdown() {
  QMutexLocker lk(&m_mutex);
  stopDevice();
  m_tracks.clear();
  m_channels.clear();
}

void MiniaudioBackend::stopDevice() {
  if (!m_device)
    return;
  auto *wrap = reinterpret_cast<DeviceWrapper *>(m_device->pUserData);
  ma_device_stop(m_device);
  ma_device_uninit(m_device);
  delete m_device;
  m_device = nullptr;
  delete wrap;
}

bool MiniaudioBackend::predecode(const QString &id, const QString &path) {

  ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, m_outCh, m_rate);
  ma_decoder dec;
  if (ma_decoder_init_file(path.toUtf8().constData(), &dc, &dec) !=
      MA_SUCCESS) {
    qWarning() << "miniaudio: cannot open" << path;
    return false;
  }

  QVector<float> pcm;
  float buffer[4096 * 2];
  for (;;) {
    ma_uint64 framesRead = 0;
    ma_result r = ma_decoder_read_pcm_frames(&dec, buffer, 4096, &framesRead);
    if (framesRead > 0) {
      const size_t samples = size_t(framesRead) * 2;
      const size_t old = pcm.size();
      pcm.resize(old + samples);
      std::memcpy(pcm.data() + old, buffer, samples * sizeof(float));
    }
    if (r == MA_AT_END)
      break;
    if (r != MA_SUCCESS) {
      ma_decoder_uninit(&dec);
      return false;
    }
  }
  ma_decoder_uninit(&dec);

  QMutexLocker lk(&m_mutex);
  DecodedTrack t;
  t.frames = pcm.size() / 2;
  t.pcm = std::move(pcm);
  m_tracks[id] = std::move(t);
  return true;
}

void MiniaudioBackend::play(int channel, const QString &id, float volume,
                            bool loop, int fadeMs) {
  QMutexLocker lk(&m_mutex);
  if (channel < 0 || channel >= m_channels.size())
    return;
  auto it = m_tracks.find(id);
  if (it == m_tracks.end()) {
    lk.unlock();

    predecode(id, id);
    lk.relock();
    it = m_tracks.find(id);
    if (it == m_tracks.end()) {
      qWarning() << "MiniaudioBackend: unknown track" << id;
      return;
    }
  }

  auto &ch = m_channels[channel];
  ch.track = &it.value();
  ch.framePos = 0;
  ch.looping = loop;
  ch.paused = false;
  ch.active = true;
  ch.tgtVol = std::clamp(volume, 0.0f, 1.0f);
  ch.curVol = 0.0f;

  const unsigned fadeSamples = std::max(1u, unsigned((fadeMs * m_rate) / 1000));
  ch.fadeSamples = fadeSamples;
  ch.volStep = (ch.tgtVol - ch.curVol) / float(fadeSamples);
}

void MiniaudioBackend::stop(int channel, int fadeMs) {
  QMutexLocker lk(&m_mutex);
  if (channel < 0 || channel >= m_channels.size())
    return;
  auto &ch = m_channels[channel];
  if (!ch.active)
    return;
  const unsigned fadeSamples = std::max(1u, unsigned((fadeMs * m_rate) / 1000));
  ch.tgtVol = 0.0f;
  ch.fadeSamples = fadeSamples;
  ch.volStep = (ch.tgtVol - ch.curVol) / float(fadeSamples);
  ch.looping = false;
}

void MiniaudioBackend::pause(int channel) {
  QMutexLocker lk(&m_mutex);
  if (channel >= 0 && channel < m_channels.size())
    m_channels[channel].paused = true;
}
void MiniaudioBackend::resume(int channel) {
  QMutexLocker lk(&m_mutex);
  if (channel >= 0 && channel < m_channels.size())
    m_channels[channel].paused = false;
}

void MiniaudioBackend::setVolume(int channel, float volume, int fadeMs) {
  QMutexLocker lk(&m_mutex);
  if (channel < 0 || channel >= m_channels.size())
    return;
  auto &ch = m_channels[channel];
  if (!ch.active)
    return;
  ch.tgtVol = std::clamp(volume, 0.0f, 1.0f);
  const unsigned fadeSamples = std::max(1u, unsigned((fadeMs * m_rate) / 1000));
  ch.fadeSamples = fadeSamples;
  ch.volStep = (ch.tgtVol - ch.curVol) / float(fadeSamples);
}

void MiniaudioBackend::stopAll(int fadeMs) {
  QMutexLocker lk(&m_mutex);
  const unsigned fadeSamples = std::max(1u, unsigned((fadeMs * m_rate) / 1000));
  for (auto &ch : m_channels) {
    if (!ch.active)
      continue;
    ch.tgtVol = 0.0f;
    ch.fadeSamples = fadeSamples;
    ch.volStep = (ch.tgtVol - ch.curVol) / float(fadeSamples);
    ch.looping = false;
  }
}

void MiniaudioBackend::setMasterVolume(float volume, int) {
  QMutexLocker lk(&m_mutex);
  m_masterVol = std::clamp(volume, 0.0f, 1.0f);
}

bool MiniaudioBackend::anyChannelPlaying() const {
  QMutexLocker lk(&m_mutex);
  for (const auto &ch : m_channels)
    if (ch.active && !ch.paused)
      return true;
  return false;
}
bool MiniaudioBackend::channelPlaying(int channel) const {
  QMutexLocker lk(&m_mutex);
  if (channel < 0 || channel >= m_channels.size())
    return false;
  const auto &ch = m_channels[channel];
  return ch.active && !ch.paused;
}

void MiniaudioBackend::playSound(const QString &id, float volume, bool loop) {
  QMutexLocker lk(&m_mutex);

  auto it = m_tracks.find(id);
  if (it == m_tracks.end()) {
    qWarning() << "MiniaudioBackend: Sound not preloaded:" << id;
    return;
  }

  int slot = findFreeSoundSlot();
  if (slot < 0) {
    qWarning() << "MiniaudioBackend: No free sound slots available";
    return;
  }

  auto &sfx = m_soundEffects[slot];
  sfx.track = &it.value();
  sfx.framePos = 0;
  sfx.volume = std::clamp(volume, 0.0f, 1.0f);
  sfx.looping = loop;
  sfx.active = true;
}

int MiniaudioBackend::findFreeSoundSlot() const {
  for (int i = 0; i < m_soundEffects.size(); ++i) {
    if (!m_soundEffects[i].active)
      return i;
  }
  return -1;
}

void MiniaudioBackend::onAudio(float *out, unsigned frames) {

  const unsigned samples = frames * 2;
  std::memset(out, 0, samples * sizeof(float));

  QMutexLocker lk(&m_mutex);

  for (auto &ch : m_channels) {
    if (!ch.active || ch.paused || ch.track == nullptr)
      continue;

    const auto *pcm = ch.track->pcm.constData();
    unsigned framesLeft = frames;
    unsigned pos = ch.framePos;

    float *dst = out;

    while (framesLeft > 0) {
      if (pos >= ch.track->frames) {
        if (ch.looping)
          pos = 0;
        else
          break;
      }
      const unsigned canCopy = std::min(framesLeft, ch.track->frames - pos);
      const float *src = pcm + pos * 2;

      for (unsigned i = 0; i < canCopy; ++i) {
        const float vol = ch.curVol * m_masterVol;
        dst[0] += src[0] * vol;
        dst[1] += src[1] * vol;
        dst += 2;
        src += 2;

        if (ch.fadeSamples > 0) {
          ch.curVol += ch.volStep;
          --ch.fadeSamples;
          if (ch.fadeSamples == 0)
            ch.curVol = ch.tgtVol;
        }
      }
      pos += canCopy;
      framesLeft -= canCopy;
    }

    ch.framePos = pos;

    if (!ch.looping && ch.framePos >= ch.track->frames) {
      ch.active = false;
      ch.curVol = ch.tgtVol = 0.0f;
      ch.fadeSamples = 0;
    }
    if (ch.fadeSamples == 0 && ch.curVol == 0.0f && ch.tgtVol == 0.0f &&
        !ch.looping) {
      ch.active = false;
    }
  }

  for (auto &sfx : m_soundEffects) {
    if (!sfx.active || sfx.track == nullptr)
      continue;

    const auto *pcm = sfx.track->pcm.constData();
    unsigned framesLeft = frames;
    unsigned pos = sfx.framePos;
    float *dst = out;

    while (framesLeft > 0) {
      if (pos >= sfx.track->frames) {
        if (sfx.looping) {
          pos = 0;
        } else {
          sfx.active = false;
          break;
        }
      }

      const unsigned canCopy = std::min(framesLeft, sfx.track->frames - pos);
      const float *src = pcm + pos * 2;

      for (unsigned i = 0; i < canCopy; ++i) {
        const float vol = sfx.volume * m_masterVol;
        dst[0] += src[0] * vol;
        dst[1] += src[1] * vol;
        dst += 2;
        src += 2;
      }

      pos += canCopy;
      framesLeft -= canCopy;
    }

    sfx.framePos = pos;
  }

  for (unsigned i = 0; i < samples; ++i) {
    if (out[i] > 1.0f)
      out[i] = 1.0f;
    else if (out[i] < -1.0f)
      out[i] = -1.0f;
  }
}
