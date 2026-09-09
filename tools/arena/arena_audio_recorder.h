#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "game/core/event_manager.h"

namespace Engine::Core {
class World;
}
namespace Game::Audio {
class AudioEventHandler;
}
namespace Game::Systems {
class NationRegistry;
}
class AudioCoordinator;

namespace Arena::Promo {

class AudioRecorder {
public:
  static constexpr int k_sample_rate = 48000;

  AudioRecorder();
  ~AudioRecorder();

  AudioRecorder(const AudioRecorder&) = delete;
  auto operator=(const AudioRecorder&) -> AudioRecorder& = delete;

  [[nodiscard]] auto start(Engine::Core::World* world,
                           Game::Systems::NationRegistry& nations) -> bool;

  void advance(float seconds, bool record);

  void play_music_bed(const QString& track_id, float volume);

  void play_one_shot(const QString& track_id, float volume);

  void begin_clip();
  [[nodiscard]] auto write_clip(const QString& wav_path) -> bool;
  [[nodiscard]] auto clip_seconds() const -> float;

  void stop();

  [[nodiscard]] static auto mux(const QString& clip_path,
                                const QString& wav_path,
                                float gain_db,
                                QString* error) -> bool;

  [[nodiscard]] static auto measure_loudness(const QStringList& wav_paths,
                                             QString* error) -> std::optional<float>;

  [[nodiscard]] static auto measure_true_peak(const QString& media_path,
                                              QString* error) -> std::optional<float>;
  [[nodiscard]] static auto delivered_peak_ceiling_dbfs() -> float;

private:
  void update_ambient_state(float seconds);
  void rotate_score(float seconds);
  [[nodiscard]] auto any_side_in_combat() const -> bool;
  [[nodiscard]] auto has_opposing_forces() const -> bool;
  [[nodiscard]] auto resting_state() const -> Engine::Core::AmbientState;

  Engine::Core::World* m_world{nullptr};
  std::unique_ptr<Game::Audio::AudioEventHandler> m_handler;
  std::unique_ptr<AudioCoordinator> m_coordinator;
  Engine::Core::AmbientState m_ambient_state{Engine::Core::AmbientState::PEACEFUL};
  float m_ambient_timer{0.0F};
  float m_score_timer{0.0F};
  bool m_scored_bed{false};
  double m_sample_carry{0.0};
  std::vector<float> m_scratch;
  std::vector<float> m_clip;
  bool m_running{false};
};

} // namespace Arena::Promo
