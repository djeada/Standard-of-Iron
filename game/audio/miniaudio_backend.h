#pragma once
#include <QObject>
#include <QString>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "audio_commands.h"
#include "audio_mastering.h"
#include "bus_limiter.h"
#include "decode_worker.h"
#include "decoded_track.h"
#include "track_decoder.h"
#include "track_store.h"

struct ma_device;
struct DeviceWrapper;

class MiniaudioBackend : public QObject {
  Q_OBJECT
public:
  static constexpr int DEFAULT_SAMPLE_RATE = 48000;
  static constexpr int DEFAULT_OUTPUT_CHANNELS = 2;
  static constexpr int DEFAULT_MUSIC_CHANNELS = 4;
  static constexpr int DEFAULT_SOUND_EFFECT_SLOTS = 32;
  static constexpr int DECODE_BUFFER_FRAMES =
      Game::Audio::TrackDecoder::DECODE_BUFFER_FRAMES;
  static constexpr int MIN_SAMPLE_RATE = 22050;
  static constexpr int MAX_TRACKS = Game::Audio::TrackStore::MAX_TRACKS;
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

  void play_sound(const QString& id,
                  float volume,
                  bool loop = false,
                  float pan = 0.0F,
                  Game::Audio::MixBus bus = Game::Audio::MixBus::Unmixed,
                  int priority = 0);
  void set_listening_preset(Game::Audio::ListeningPreset preset) {
    m_listening_preset.store(preset, std::memory_order_relaxed);
  }
  void stop_sound(const QString& id);
  void set_sound_volume(const QString& id, float volume, int fade_ms);
  auto is_sound_active(const QString& id) const -> bool;

  auto is_track_ready(const QString& id) const -> bool;

  [[nodiscard]] auto resident_pcm_bytes() const -> std::uint64_t {
    return m_tracks.resident_bytes();
  }

  [[nodiscard]] auto peak_pcm_bytes() const -> std::uint64_t {
    return m_tracks.peak_bytes();
  }

  [[nodiscard]] auto pcm_budget_bytes() const -> std::uint64_t {
    return m_tracks.budget_bytes();
  }

  void set_pcm_budget_bytes(std::uint64_t budget) { m_tracks.set_budget_bytes(budget); }

  [[nodiscard]] auto pcm_budget_overruns() const -> std::uint64_t {
    return m_tracks.budget_overruns();
  }

  [[nodiscard]] auto mastering_analyses_computed() const -> std::uint64_t {
    return m_decoder.analyses_computed();
  }
  auto is_track_decode_pending(const QString& id) const -> bool;
  [[nodiscard]] auto has_pending_decodes() const -> bool;

  void on_audio(float* output, unsigned frames);
  void set_offline_render(bool enabled) { m_offline_render = enabled; }

private:
  using DecodedTrack = Game::Audio::DecodedTrack;

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
    Game::Audio::MixBus mix_bus = Game::Audio::MixBus::Unmixed;
    int priority = 0;
    int track = -1;
    unsigned frame_pos = 0;
    float volume = DEFAULT_VOLUME;
    float target_volume = DEFAULT_VOLUME;
    float volume_step = 0.0F;
    float gain_left = DEFAULT_VOLUME;
    float gain_right = DEFAULT_VOLUME;
    unsigned fade_samples = 0;
    bool looping = false;
    bool active = false;
  };

  void stop_device();
  void submit(const Game::Audio::AudioCommand& command);
  void submit_and_wait(const Game::Audio::AudioCommand& command);
  void apply_command(const Game::Audio::AudioCommand& command);
  [[nodiscard]] auto channel_for(const Game::Audio::AudioCommand& command) -> Channel*;
  void apply_play(const Game::Audio::AudioCommand& command);
  void apply_stop(const Game::Audio::AudioCommand& command);
  void apply_stop_all(const Game::Audio::AudioCommand& command);
  void apply_play_sound(const Game::Audio::AudioCommand& command);
  void apply_set_sound_volume(const Game::Audio::AudioCommand& command);
  void apply_release_track(const Game::Audio::AudioCommand& command);
  void drain_commands();
  [[nodiscard]] auto fade_samples_for(int fade_ms) const -> unsigned;
  void publish_state();

  struct BlockGains;
  struct EffectLoad;
  [[nodiscard]] auto measure_effect_load(float master) const -> EffectLoad;
  void mix_music_channels(float* output,
                          unsigned frames,
                          float master,
                          const BlockGains& gains);
  void mix_sound_effects(float* output,
                         unsigned frames,
                         float master,
                         const BlockGains& gains);
  static void render_music_run(Channel& channel,
                               const std::int16_t* source,
                               unsigned stride,
                               unsigned run,
                               float*& destination,
                               float master,
                               const BlockGains& gains);
  static void render_effect_run(SoundEffect& effect,
                                const std::int16_t* source,
                                unsigned stride,
                                unsigned run,
                                float*& destination,
                                float master,
                                const BlockGains& gains);

  std::unique_ptr<ma_device> m_device{nullptr};
  std::unique_ptr<DeviceWrapper> m_device_wrapper{nullptr};
  int m_sample_rate{DEFAULT_SAMPLE_RATE};
  int m_output_channels{DEFAULT_OUTPUT_CHANNELS};
  std::atomic<bool> m_device_running{false};

  Game::Audio::TrackStore m_tracks;
  Game::Audio::TrackDecoder m_decoder;
  Game::Audio::DecodeWorker m_worker{
      m_tracks, m_decoder, [this](const Game::Audio::AudioCommand& command) {
        submit(command);
      }};

  Game::Audio::CommandRing<COMMAND_CAPACITY> m_commands;

  std::vector<Channel> m_channels;
  std::vector<SoundEffect> m_sound_effects;
  float m_master_volume{DEFAULT_VOLUME};
  Game::Audio::BusLimiter m_bus_limiter;
  Game::Audio::GameplayMix m_gameplay_mix;
  std::atomic<Game::Audio::ListeningPreset> m_listening_preset{
      Game::Audio::ListeningPreset::Speakers};

  std::atomic<std::uint32_t> m_active_channel_mask{0};
  std::array<std::atomic<int>, DEFAULT_SOUND_EFFECT_SLOTS> m_active_sfx_tracks{};

  bool m_offline_render{false};
};
