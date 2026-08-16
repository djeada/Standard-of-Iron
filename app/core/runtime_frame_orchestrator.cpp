#include "runtime_frame_orchestrator.h"

#include <algorithm>
#include <cmath>

#include "ambient_state_manager.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/environment_lighting.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/rain_manager.h"
#include "game/systems/selection_system.h"
#include "game/systems/victory_service.h"
#include "game/wildlife/wildlife_system.h"
#include "minimap_manager.h"
#include "render/ground/rain_renderer.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "visibility_coordinator.h"
#include "weather_audio.h"

namespace {
constexpr int k_selection_refresh_interval = 15;
constexpr float k_minimap_unit_update_interval = 0.05F;
constexpr int k_max_simulation_steps_per_frame = 8;
constexpr int k_simulation_step_ceiling = 64;

auto simulation_step_budget(double time_scale) -> int {
  const double scaled =
      std::ceil(static_cast<double>(k_max_simulation_steps_per_frame) *
                std::max(1.0, time_scale));
  return std::clamp(static_cast<int>(scaled),
                    k_max_simulation_steps_per_frame,
                    k_simulation_step_ceiling);
}
} // namespace

void RuntimeFrameOrchestrator::update(const AppSceneContext& scene,
                                      RuntimeFrameState& state,
                                      EntityCache& entity_cache,
                                      AmbientStateManager* ambient_state_manager,
                                      const QString& victory_state,
                                      float real_dt,
                                      const FrameUpdateCallbacks& callbacks,
                                      const SimulationStep& simulation_step) const {
  const double time_scale =
      std::max(0.0, static_cast<double>(state.simulation_time_scale));
  const float frame_seconds = std::max(real_dt, 0.0F);
  const float dt = static_cast<float>(static_cast<double>(frame_seconds) * time_scale);

  state.dropped_simulation_ticks = 0;

  if (scene.world != nullptr && ambient_state_manager != nullptr) {
    ambient_state_manager->update(
        dt, scene.world, state.local_owner_id, entity_cache, victory_state);
  }

  if (scene.renderer != nullptr) {
    scene.renderer->update_animation_time(dt);
  }

  if (scene.active_camera != nullptr) {
    scene.active_camera->update(dt);
  }

  if (scene.world != nullptr && scene.active_camera != nullptr) {
    if (auto* wildlife = scene.world->get_system<Game::Wildlife::WildlifeSystem>()) {
      const QVector3D focus = scene.active_camera->get_target();
      wildlife->set_focus(focus.x(), focus.z());
    }
  }

  if (scene.world != nullptr) {

    Game::Session::SessionContext& session =
        scene.session != nullptr ? *scene.session
                                 : Game::Session::SessionContext::active();

    session.clock().set_time_scale(time_scale);
    session.advance(static_cast<double>(frame_seconds),
                    simulation_step_budget(time_scale),
                    [&](float step) {
                      simulation_step(step);
                      if (scene.environment_clock != nullptr) {
                        scene.environment_clock->update(step, false);
                      }
                    });
    state.dropped_simulation_ticks = session.clock().consume_dropped_ticks();

    if (scene.visibility_coordinator != nullptr) {
      scene.visibility_coordinator->update(
          *scene.world,
          state.local_owner_id,
          dt,
          Game::GameConfig::instance().gameplay().visibility_update_interval,
          state.spectator_mode);
    }

    if (scene.minimap_manager != nullptr) {
      auto* selection_system =
          scene.world->get_system<Game::Systems::SelectionSystem>();
      state.minimap_unit_update_accumulator += std::max(dt, 0.0F);
      const bool unit_update_due =
          state.minimap_unit_update_accumulator >= k_minimap_unit_update_interval;
      if (unit_update_due || scene.minimap_manager->unit_overlay_stale()) {
        scene.minimap_manager->update_units(
            scene.world, selection_system, state.local_owner_id);
        if (unit_update_due) {
          state.minimap_unit_update_accumulator = std::fmod(
              state.minimap_unit_update_accumulator, k_minimap_unit_update_interval);
        }
      }
      scene.minimap_manager->update_camera_viewport(
          scene.active_camera,
          static_cast<float>(state.viewport_width),
          static_cast<float>(state.viewport_height));

      if (scene.minimap_manager->consume_dirty_flag() &&
          callbacks.on_minimap_image_changed) {
        callbacks.on_minimap_image_changed();
      }
    }
  }

  if (scene.rain_manager != nullptr) {
    scene.rain_manager->update(dt);
    if (scene.weather_audio != nullptr) {
      scene.weather_audio->update(scene.rain_manager);
    }
    if (scene.rain != nullptr) {
      scene.rain->set_enabled(scene.rain_manager->is_enabled());
      scene.rain->set_intensity(scene.rain_manager->get_intensity());
      scene.rain->set_weather_type(scene.rain_manager->get_weather_type());
      scene.rain->set_wind_strength(scene.rain_manager->get_wind_strength());
      scene.rain->set_wind_direction_deg(scene.rain_manager->get_wind_direction_deg());
      if (scene.active_camera != nullptr) {
        scene.rain->set_camera_position(scene.active_camera->get_position());
      }
    }
  }

  if (scene.environment_clock != nullptr && scene.renderer != nullptr) {
    Game::Map::WeatherLightingInput weather;
    if (scene.rain_manager != nullptr && scene.rain_manager->is_enabled()) {
      const float intensity = scene.rain_manager->get_intensity();
      if (scene.rain_manager->get_weather_type() == Game::Map::WeatherType::Snow) {
        weather.snow = intensity;
      } else {
        weather.rain = intensity;
        weather.storm = std::clamp((intensity - 0.65F) / 0.35F, 0.0F, 1.0F);
      }
    }
    scene.renderer->set_environment_lighting(
        scene.environment_clock->lighting(weather));
  }

  if (scene.victory_service != nullptr && scene.world != nullptr) {
    scene.victory_service->update(*scene.world, dt);
  }

  if (!state.selection_refresh_enabled || scene.world == nullptr) {
    return;
  }

  auto* selection_system = scene.world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr || selection_system->get_selected_units().empty()) {
    return;
  }

  ++state.selection_refresh_counter;
  if (state.selection_refresh_counter >= k_selection_refresh_interval) {
    state.selection_refresh_counter = 0;
    if (callbacks.on_selected_units_data_changed) {
      callbacks.on_selected_units_data_changed();
    }
  }
}
