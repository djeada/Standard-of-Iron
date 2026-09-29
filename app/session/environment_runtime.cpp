#include "app/session/environment_runtime.h"

#include "app/audio/weather_audio.h"
#include "game/core/event_manager.h"
#include "game/map/environment_lighting.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/rain_manager.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/scene_renderer.h"

namespace App::Session {

EnvironmentRuntime::EnvironmentRuntime()
    : m_rain_manager(std::make_unique<Game::Systems::RainManager>())
    , m_weather_audio(std::make_unique<App::Core::WeatherAudio>())
    , m_clock(std::make_unique<Game::Map::EnvironmentClock>())
    , m_ambient(std::make_unique<AmbientStateManager>()) {
}

EnvironmentRuntime::~EnvironmentRuntime() = default;

void EnvironmentRuntime::configure_rain(const Game::Systems::LevelSnapshot& level,
                                        Render::GL::RainRenderer* rain) {
  if (m_rain_manager) {
    m_rain_manager->configure(level.rain, level.biome_seed);
  }

  if (m_weather_audio) {
    m_weather_audio->stop();
    if (level.rain.enabled) {
      m_weather_audio->preload(level.rain.type);
    }
  }

  if (rain == nullptr) {
    return;
  }

  const float world_width = static_cast<float>(level.grid_width) * level.tile_size;
  const float world_height = static_cast<float>(level.grid_height) * level.tile_size;
  rain->configure(world_width, world_height, level.biome_seed, level.rain.type);
  rain->set_enabled(level.rain.enabled);
  rain->set_wind_strength(level.rain.wind_strength);
  rain->set_wind_direction_deg(level.rain.wind_direction_deg);

  const float initial_intensity =
      m_rain_manager ? m_rain_manager->get_intensity()
                     : (level.rain.enabled ? level.rain.intensity : 0.0F);
  rain->set_intensity(initial_intensity);
}

void EnvironmentRuntime::reset_clock(const Game::Systems::LevelSnapshot& level) {
  if (m_clock) {
    m_clock->reset(level.environment);
  }
}

void EnvironmentRuntime::restore_after_load(const Game::Systems::LevelSnapshot& level,
                                            Render::GL::Renderer* renderer,
                                            Render::GL::RainRenderer* rain) {
  if (m_clock) {
    m_clock->restore(level.environment, level.environment_clock);
    if (renderer != nullptr) {
      renderer->set_environment_lighting(m_clock->lighting());
    }
  }

  if (m_rain_manager) {
    configure_rain(level, rain);
    m_rain_manager->restore(level.weather_runtime);
    if (rain != nullptr) {
      rain->set_intensity(m_rain_manager->get_intensity());
    }
  }
}

void EnvironmentRuntime::capture_into(Game::Systems::LevelSnapshot& level) const {
  if (m_clock) {
    level.environment = m_clock->definition();
    level.environment_clock = m_clock->snapshot();
  }
  if (m_rain_manager) {
    level.weather_runtime = m_rain_manager->snapshot();
  }
}

void EnvironmentRuntime::reset_ambient() {
  m_ambient = std::make_unique<AmbientStateManager>();
  Engine::Core::EventManager::instance().publish(Engine::Core::AmbientStateChangedEvent(
      Engine::Core::AmbientState::PEACEFUL, Engine::Core::AmbientState::PEACEFUL));
}

void EnvironmentRuntime::sync_scatter_world_props(
    Game::Session::SessionContext& session,
    Render::GL::TerrainScatterManager* scatter) {
  auto& terrain_service = session.terrain();
  if (scatter == nullptr || !terrain_service.is_initialized() ||
      terrain_service.get_height_map() == nullptr) {
    return;
  }

  auto const revision = terrain_service.world_props_revision();
  if (revision == m_last_world_props_revision) {
    return;
  }

  scatter->set_world_view(Render::WorldView::of(session));
  scatter->refresh_runtime_world_props(terrain_service.world_props());
  m_last_world_props_revision = revision;
}

void EnvironmentRuntime::release() {
  if (m_weather_audio) {
    m_weather_audio->stop();
  }
  m_weather_audio.reset();
  m_rain_manager.reset();
}

} // namespace App::Session
