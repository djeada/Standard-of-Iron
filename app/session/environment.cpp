#include "app/session/environment.h"

#include <algorithm>

#include "game/game_config.h"
#include "game/map/map_definition.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace App::Core {

using namespace Game::Map;

void Environment::apply(const MapDefinition& def,
                        Render::GL::Renderer& renderer,
                        Render::GL::Camera& camera) {
  EnvironmentClock initial_clock(def.environment);
  renderer.set_environment_lighting(initial_clock.lighting(
      {.rain = def.rain.enabled && def.rain.type == WeatherType::Rain
                   ? def.rain.intensity
                   : 0.0F,
       .snow = def.rain.enabled && def.rain.type == WeatherType::Snow
                   ? def.rain.intensity
                   : 0.0F}));
  Game::GameConfig::instance().set_authored_camera({.distance = def.camera.distance,
                                                    .pitch = def.camera.tilt_deg,
                                                    .yaw = def.camera.yaw_deg});
  camera.set_rts_view(
      def.camera.center, def.camera.distance, def.camera.tilt_deg, def.camera.yaw_deg);
  camera.set_perspective(
      def.camera.fov_y, 16.0F / 9.0F, def.camera.near_plane, def.camera.far_plane);
  Render::GL::Renderer::GridParams gp;
  gp.cell_size = def.grid.tile_size;
  gp.extent = std::max(def.grid.width, def.grid.height) * def.grid.tile_size * 0.5F;
  renderer.set_grid_params(gp);
}

void Environment::apply_default(Render::GL::Renderer& renderer,
                                Render::GL::Camera& camera) {
  renderer.set_environment_lighting(lighting_for_hour(13.0F));
  const auto& camera_config = Game::GameConfig::instance().camera();
  constexpr float k_mapless_distance = 15.0F;
  Game::GameConfig::instance().set_authored_camera(
      {.distance = k_mapless_distance,
       .pitch = camera_config.default_pitch,
       .yaw = camera_config.default_yaw});
  camera.set_rts_view(QVector3D(0, 0, 0),
                      k_mapless_distance,
                      camera_config.default_pitch,
                      camera_config.default_yaw);

  camera.set_perspective(45.0F, 16.0F / 9.0F, 1.0F, 200.0F);
  Render::GL::Renderer::GridParams gp;
  gp.cell_size = 1.0F;
  gp.extent = 50.0F;
  renderer.set_grid_params(gp);
}

} // namespace App::Core
