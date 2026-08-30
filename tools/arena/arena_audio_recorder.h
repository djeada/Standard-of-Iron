#pragma once

#include <QString>

#include <cstdint>
#include <memory>
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

  void begin_clip();
  [[nodiscard]] auto write_clip(const QString& wav_path) -> bool;
  [[nodiscard]] auto clip_seconds() const -> float;

  void stop();

  [[nodiscard]] static auto
  mux(const QString& clip_path, const QString& wav_path, QString* error) -> bool;

private:
  void update_ambient_state(float seconds);
  [[nodiscard]] auto any_side_in_combat() const -> bool;

  Engine::Core::World* m_world{nullptr};
  std::unique_ptr<Game::Audio::AudioEventHandler> m_handler;
  std::unique_ptr<AudioCoordinator> m_coordinator;
  Engine::Core::AmbientState m_ambient_state{Engine::Core::AmbientState::PEACEFUL};
  float m_ambient_timer{0.0F};
  double m_sample_carry{0.0};
  std::vector<float> m_scratch;
  std::vector<float> m_clip;
  bool m_running{false};
};

} // namespace Arena::Promo
