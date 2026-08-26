#pragma once
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QWaitCondition>

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <thread>
#include <vector>

#include "audio_commands.h"
#include "audio_mastering.h"
#include "bus_limiter.h"

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
  static constexpr int MAX_TRACKS = 512;
  static constexpr std::size_t COMMAND_CAPACITY = 256;
  static constexpr float MIN_VOLUME = 0.0F;
  static constexpr float MAX_VOLUME = 2.0F;
  static constexpr float DEFAULT_VOLUME = 1.0F;
  static constexpr float MAX_OUTPUT_SAMPLE = 1.0F;

  explicit MiniaudioBackend(QObject* parent = nullptr);
  ~MiniaudioBackend() override;

  auto initialize(int device_rate,
                  int output_channels,
                  int music_channels,
                  bool open_device = true) -> bool;
  void shutdown();

  auto request_track(const QString& id,
                     const QString& path,
                     Game::Audio::Mastering::Material material) -> bool;
  void wait_for_decodes();
  void wait_for_track(const QString& id);
  void unload(const QString& id);

  void play(int channel, const QString& id, float volume, bool loop, int fade_ms);
  void stop(int channel, int fade_ms);
  void pause(int channel);
  void resume(int channel);
  void set_volume(int channel, float volume, int fade_ms);
  void stop_all(int fade_ms);
  void set_master_volume(float volume, int fade_ms);

  auto any_channel_playing() const -> bool;
  auto channel_playing(int channel) const -> bool;

  void play_sound(const QString& id, float volume, bool loop = false);
  void stop_sound(const QString& id);
  void set_sound_volume(const QString& id, float volume, int fade_ms);
  auto is_sound_active(const QString& id) const -> bool;

  auto is_track_ready(const QString& id) const -> bool;

  // How many clips have actually had the mastering analysis run on them. It
  // only rises when a clip is seen for the first time; a reload re-applies what
  // is already known.
  [[nodiscard]] auto mastering_analyses_computed() const -> std::uint64_t {
    return m_analyses_computed.load(std::memory_order_relaxed);
  }
  auto is_track_decode_pending(const QString& id) const -> bool;

  void on_audio(float* output, unsigned frames);

private:
  struct DecodedTrack {
    std::vector<float> pcm;
    unsigned frames = 0;
    unsigned channels = DEFAULT_OUTPUT_CHANNELS;
  };

  struct Channel {
    int track = -1;
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
    int track = -1;
    unsigned frame_pos = 0;
    float volume = DEFAULT_VOLUME;
    float target_volume = DEFAULT_VOLUME;
    float volume_step = 0.0F;
    unsigned fade_samples = 0;
    bool looping = false;
    bool active = false;
  };

  struct DecodeJob {
    QString id;
    QString path;
    int track = -1;
    Game::Audio::Mastering::Material material =
        Game::Audio::Mastering::Material::Effect;
  };

  void stop_device();
  auto claim_track_slot(const QString& id) -> int;
  auto find_track_slot(const QString& id) const -> int;
  auto decode_into_slot(const DecodeJob& job) -> bool;
  auto analysis_for(const QString& id,
                    const float* pcm,
                    std::size_t frames) -> Game::Audio::Mastering::Analysis;
  [[nodiscard]] auto take_next_job(DecodeJob& job) -> bool;
  void finish_job(const DecodeJob& job, bool decoded);
  void release_slot(int slot);
  void decode_worker();
  void start_worker();
  void stop_worker();
  void submit(const Game::Audio::AudioCommand& command);
  void submit_and_wait(const Game::Audio::AudioCommand& command);
  void apply_command(const Game::Audio::AudioCommand& command);
  void drain_commands();
  [[nodiscard]] auto fade_samples_for(int fade_ms) const -> unsigned;
  void publish_state();

  std::unique_ptr<ma_device> m_device{nullptr};
  std::unique_ptr<DeviceWrapper> m_device_wrapper{nullptr};
  int m_sample_rate{DEFAULT_SAMPLE_RATE};
  int m_output_channels{DEFAULT_OUTPUT_CHANNELS};
  std::atomic<bool> m_device_running{false};

  mutable QMutex m_registry_mutex;
  QHash<QString, int> m_track_ids;
  std::array<bool, MAX_TRACKS> m_slot_taken{};
  std::array<std::unique_ptr<DecodedTrack>, MAX_TRACKS> m_track_storage;
  std::array<std::atomic<const DecodedTrack*>, MAX_TRACKS> m_track_table{};

  Game::Audio::CommandRing<COMMAND_CAPACITY> m_commands;

  std::vector<Channel> m_channels;
  std::vector<SoundEffect> m_sound_effects;
  float m_master_volume{DEFAULT_VOLUME};
  Game::Audio::BusLimiter m_bus_limiter;

  std::atomic<std::uint32_t> m_active_channel_mask{0};
  std::array<std::atomic<int>, DEFAULT_SOUND_EFFECT_SLOTS> m_active_sfx_tracks{};

  mutable QMutex m_decode_mutex;
  QWaitCondition m_decode_ready;
  QWaitCondition m_decode_idle;
  std::deque<DecodeJob> m_decode_jobs;
  std::deque<DecodeJob> m_decode_bulk_jobs;
  QSet<int> m_pending_slots;

  // A looping bed asked for while its own audio is still being decoded. The
  // request used to be dropped on the floor, which is how a whole match could
  // run with no weather ambience: the load asks for the rain the moment the map
  // is up, the decoder is still working through ninety-seven clips, and nothing
  // ever asks again. One-shots are not held - a combat cue that missed its
  // moment is better lost than played late.
  struct DeferredLoop {
    float volume = 1.0F;
  };
  QHash<int, DeferredLoop> m_deferred_loops;

  // Returning to the menu unloads the mission's audio, so the next match
  // decodes and masters the same ninety-seven clips again from scratch. The
  // unload is deliberate and worth keeping - the PCM is the memory it was for -
  // but the analysis behind the mastering is not: a loudness sweep, two band
  // filters over every sample, and up to sixty-four FFT windows, all of which
  // produce exactly the same numbers for the same file every time. Keep those
  // for the life of the process and re-apply them on reload.
  struct CachedAnalysis {
    std::size_t frames = 0;
    Game::Audio::Mastering::Analysis analysis;
  };
  mutable QMutex m_analysis_cache_mutex;
  QHash<QString, CachedAnalysis> m_analysis_cache;
  std::atomic<std::uint64_t> m_analyses_computed{0};

  std::thread m_decode_thread;
  bool m_decode_running{false};
  int m_decode_in_flight{0};
};
