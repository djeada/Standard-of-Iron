#pragma once

#include <cstdint>
#include <memory>

#include "app/world/ambient_state_manager.h"
#include "game/systems/match_snapshot.h"

namespace Game::Session {
class SessionContext;
}

namespace Game::Systems {
class RainManager;
}

namespace Game::Map {
class EnvironmentClock;
}

namespace Render::GL {
class Renderer;
class RainRenderer;
class TerrainScatterManager;
} // namespace Render::GL

namespace App::Core {
class WeatherAudio;
}

struct EntityCache;

namespace App::Session {

class EnvironmentRuntime {
public:
  EnvironmentRuntime();
  ~EnvironmentRuntime();
  EnvironmentRuntime(const EnvironmentRuntime&) = delete;
  auto operator=(const EnvironmentRuntime&) -> EnvironmentRuntime& = delete;
  EnvironmentRuntime(EnvironmentRuntime&&) = delete;
  auto operator=(EnvironmentRuntime&&) -> EnvironmentRuntime& = delete;

  [[nodiscard]] auto rain_manager() const -> Game::Systems::RainManager* {
    return m_rain_manager.get();
  }
  [[nodiscard]] auto weather_audio() const -> App::Core::WeatherAudio* {
    return m_weather_audio.get();
  }
  [[nodiscard]] auto clock() const -> Game::Map::EnvironmentClock* {
    return m_clock.get();
  }
  [[nodiscard]] auto ambient() const -> AmbientStateManager* { return m_ambient.get(); }

  void configure_rain(const Game::Systems::LevelSnapshot& level,
                      Render::GL::RainRenderer* rain);

  void reset_clock(const Game::Systems::LevelSnapshot& level);

  void restore_after_load(const Game::Systems::LevelSnapshot& level,
                          Render::GL::Renderer* renderer,
                          Render::GL::RainRenderer* rain);

  void capture_into(Game::Systems::LevelSnapshot& level) const;

  void reset_ambient();

  void sync_scatter_world_props(Game::Session::SessionContext& session,
                                Render::GL::TerrainScatterManager* scatter);

  void release();

private:
  std::unique_ptr<Game::Systems::RainManager> m_rain_manager;
  std::unique_ptr<App::Core::WeatherAudio> m_weather_audio;
  std::unique_ptr<Game::Map::EnvironmentClock> m_clock;
  std::unique_ptr<AmbientStateManager> m_ambient;
  std::uint64_t m_last_world_props_revision = 0;
};

} // namespace App::Session
