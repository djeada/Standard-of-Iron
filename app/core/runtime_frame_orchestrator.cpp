#include "runtime_frame_orchestrator.h"

#include "ambient_state_manager.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/systems/rain_manager.h"
#include "game/systems/selection_system.h"
#include "game/systems/victory_service.h"
#include "minimap_manager.h"
#include "render/gl/camera.h"
#include "render/ground/rain_renderer.h"
#include "render/scene_renderer.h"
#include "visibility_coordinator.h"

namespace {
constexpr int k_selection_refresh_interval = 15;
}

void RuntimeFrameOrchestrator::update(const AppSceneContext& scene,
                                      RuntimeFrameState& state,
                                      EntityCache& entity_cache,
                                      AmbientStateManager* ambient_state_manager,
                                      const QString& victory_state,
                                      float dt,
                                      const FrameUpdateCallbacks& callbacks,
                                      const SimulationStep& simulation_step) const {
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

  if (scene.world != nullptr) {

    simulation_step(dt);

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
      scene.minimap_manager->update_units(
          scene.world, selection_system, state.local_owner_id);
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
    if (scene.rain != nullptr) {
      scene.rain->set_enabled(scene.rain_manager->is_enabled());
      scene.rain->set_intensity(scene.rain_manager->get_intensity());
      scene.rain->set_weather_type(scene.rain_manager->get_weather_type());
      scene.rain->set_wind_strength(scene.rain_manager->get_wind_strength());
      if (scene.active_camera != nullptr) {
        scene.rain->set_camera_position(scene.active_camera->get_position());
      }
    }
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
