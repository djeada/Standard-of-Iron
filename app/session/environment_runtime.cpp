#include "app/session/environment_runtime.h"

#include <QMatrix4x4>

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "app/audio/weather_audio.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/map/environment_lighting.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/rain_manager.h"
#include "render/entity/home_renderer_common.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/tree_renderer.h"
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

namespace {

struct LocalTree {
  float x;
  float z;
  float scale;
  float y{0.0F};
};

constexpr std::array<LocalTree, 2> k_punic_barracks_palms{
    {{1.96F, 1.66F, 0.42F}, {-1.86F, -0.50F, 0.50F}}};
constexpr LocalTree k_punic_roof_cypress{0.62F, 0.62F, 0.17F, 1.33F};
constexpr int k_punic_light_well_variant = 4;

auto building_is_standing(const Engine::Core::UnitComponent& unit) -> bool {
  return unit.health > 0;
}

void anchor(std::vector<Render::GL::AnchoredTree>& out,
            const Engine::Core::TransformComponent& transform,
            const LocalTree& local,
            Game::Map::TreeSpecies species,
            std::uint32_t seed) {
  QMatrix4x4 model;
  model.translate(transform.position.x, transform.position.y, transform.position.z);
  model.rotate(transform.rotation.y, 0.0F, 1.0F, 0.0F);
  model.scale(transform.scale.x, transform.scale.y, transform.scale.z);
  const QVector3D world = model.map(QVector3D(local.x, local.y, local.z));
  const float rotation = static_cast<float>(seed % 360U);
  out.push_back(Render::GL::AnchoredTree{.species = species,
                                         .position = world,
                                         .scale = local.scale * transform.scale.y,
                                         .rotation = rotation,
                                         .seed = seed,
                                         .grounded = local.y <= 0.0F});
}

auto collect_building_trees(Game::Session::SessionContext& session)
    -> std::vector<Render::GL::AnchoredTree> {
  std::vector<Render::GL::AnchoredTree> trees;
  auto& world = session.world();
  const auto& biome = session.terrain().biome_settings();
  const bool cold = biome.ground_type == Game::Map::GroundType::AlpineMix ||
                    biome.snow_coverage >= 0.30F;
  for (auto [entity_id, unit] : world.view<Engine::Core::UnitComponent>()) {
    if (unit.nation_id != Game::Systems::NationID::Carthage ||
        !building_is_standing(unit)) {
      continue;
    }
    const bool barracks = unit.spawn_type == Game::Units::SpawnType::Barracks;
    const bool home = unit.spawn_type == Game::Units::SpawnType::Home;
    if (!barracks && !home) {
      continue;
    }
    const auto* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    if (transform == nullptr) {
      continue;
    }
    const auto id = static_cast<std::uint32_t>(entity_id);
    if (barracks) {
      std::uint32_t salt = 0;
      for (const LocalTree& palm : k_punic_barracks_palms) {
        anchor(trees,
               *transform,
               palm,
               cold ? Game::Map::TreeSpecies::Pine : Game::Map::TreeSpecies::Palm,
               id * 2654435761U + (++salt));
      }
      continue;
    }
    const auto seed = Render::GL::home_placement_seed(
        transform->position.x, transform->position.z, id);
    if (Render::GL::home_visual_variant(seed) == k_punic_light_well_variant) {
      anchor(trees,
             *transform,
             k_punic_roof_cypress,
             Game::Map::TreeSpecies::Cypress,
             seed);
    }
  }
  return trees;
}

} // namespace

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
  if (revision != m_last_world_props_revision) {
    scatter->set_world_view(Render::WorldView::of(session));
    scatter->refresh_runtime_world_props(terrain_service.world_props());
    m_last_world_props_revision = revision;
  }

  std::vector<Render::GL::AnchoredTree> trees = collect_building_trees(session);
  if (!m_anchored_trees_synced || trees != m_anchored_trees) {
    scatter->set_world_view(Render::WorldView::of(session));
    scatter->set_anchored_trees(trees);
    m_anchored_trees = std::move(trees);
    m_anchored_trees_synced = true;
  }
}

void EnvironmentRuntime::release() {
  if (m_weather_audio) {
    m_weather_audio->stop();
  }
  m_weather_audio.reset();
  m_rain_manager.reset();
}

} // namespace App::Session
