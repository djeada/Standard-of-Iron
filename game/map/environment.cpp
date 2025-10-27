#include "environment.h"
#include "../../render/gl/camera.h"
#include "../../render/scene_renderer.h"
#include "../game_config.h"
#include "map/map_definition.h"
#include <algorithm>

namespace Game::Map {

void Environment::apply(const MapDefinition &def,
                        Render::GL::Renderer &renderer,
                        Render::GL::Camera &camera) {

  camera.setRTSView(def.camera.center, def.camera.distance, def.camera.tiltDeg,
                    def.camera.yaw_deg);
  camera.setPerspective(def.camera.fovY, 16.0F / 9.0F, def.camera.near_plane,
                        def.camera.far_plane);
  Render::GL::Renderer::GridParams gp;
  gp.cellSize = def.grid.tile_size;
  gp.extent =
      std::max(def.grid.width, def.grid.height) * def.grid.tile_size * 0.5F;
  renderer.setGridParams(gp);
}

void Environment::applyDefault(Render::GL::Renderer &renderer,
                               Render::GL::Camera &camera) {
  const auto &camera_config = Game::GameConfig::instance().camera();
  camera.setRTSView(QVector3D(0, 0, 0), 15.0F, camera_config.defaultPitch,
                    camera_config.defaultYaw);

  camera.setPerspective(45.0F, 16.0F / 9.0F, 1.0F, 200.0F);
  Render::GL::Renderer::GridParams gp;
  gp.cellSize = 1.0F;
  gp.extent = 50.0F;
  renderer.setGridParams(gp);
}

} // namespace Game::Map
