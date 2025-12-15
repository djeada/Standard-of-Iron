#pragma once
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

struct ma_device;
struct DeviceWrapper;

class MiniaudioBackend : public QObject {
  Q_OBJECT
public:
  static constexpr int DEFAULT_SAMPLE_RATE = 48000;
  static constexpr int DEFAULT_OUTPUT_CHANNELS = 2;
  static constexpr int DEFAULT_MUSIC_CHANNELS = 4;
  static constexpr int DEFAULT_SOUND_EFFECT_SLOTS = 32;
  static constexpr int DECODE_BUFFER_FRAMES = 4096;
  static constexpr int MIN_SAMPLE_RATE = 22050;
  static constexpr float MIN_VOLUME = 0.0F;
  static constexpr float MAX_VOLUME = 1.0F;
  static constexpr float DEFAULT_VOLUME = 1.0F;

  explicit MiniaudioBackend(QObject *parent = nullptr);
  ~MiniaudioBackend() override;

  auto initialize(int device_rate, int output_channels,
                  int music_channels) -> bool;
  void shutdown();

  auto predecode(const QString &id, const QString &path) -> bool;

  void play(int channel, const QString &id, float volume, bool loop,
            int fade_ms);
  void stop(int channel, int fade_ms);
  void pause(int channel);
  void resume(int channel);
  void set_volume(int channel, float volume, int fade_ms);
  void stop_all(int fade_ms);
  void set_master_volume(float volume, int fade_ms);

  auto any_channel_playing() const -> bool;
  auto channel_playing(int channel) const -> bool;

  void play_sound(const QString &id, float volume, bool loop = false);

  void on_audio(float *output, unsigned frames);

private:
  struct DecodedTrack {
    QVector<float> pcm;
    unsigned frames = 0;
  };

  struct Channel {
    const DecodedTrack *track = nullptr;
    unsigned frame_pos = 0;
    float current_volume = 0.0F;
    float target_volume = DEFAULT_VOLUME;
    float volume_step = 0.0F;
    unsigned fade_samples = 0;
    bool looping = false;
    bool paused = false;
    bool active = false;
  };

  struct SoundEffect {
    const DecodedTrack *track = nullptr;
    unsigned frame_pos = 0;
    float volume = DEFAULT_VOLUME;
    bool looping = false;
    bool active = false;
  };

  void start_device();
  void stop_device();
  auto find_free_sound_slot() const -> int;

  std::unique_ptr<ma_device> m_device{nullptr};
  std::unique_ptr<DeviceWrapper> m_device_wrapper{nullptr};
  int m_sample_rate{DEFAULT_SAMPLE_RATE};
  int m_output_channels{DEFAULT_OUTPUT_CHANNELS};

  mutable QMutex m_mutex;
  QMap<QString, DecodedTrack> m_tracks;
  QVector<Channel> m_channels;
  QVector<SoundEffect> m_sound_effects;
  float m_master_volume{DEFAULT_VOLUME};
};
