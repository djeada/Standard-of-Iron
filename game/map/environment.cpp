#include "environment.h"
#include "../../render/gl/camera.h"
#include "../../render/scene_renderer.h"
#include "../game_config.h"
#include <algorithm>

namespace Game {
namespace Map {

void Environment::apply(const MapDefinition &def,
                        Render::GL::Renderer &renderer,
                        Render::GL::Camera &camera) {

  camera.setRTSView(def.camera.center, def.camera.distance, def.camera.tiltDeg,
                    def.camera.yawDeg);
  camera.setPerspective(def.camera.fovY, 16.0f / 9.0f, def.camera.nearPlane,
                        def.camera.farPlane);
  Render::GL::Renderer::GridParams gp;
  gp.cellSize = def.grid.tileSize;
  gp.extent =
      std::max(def.grid.width, def.grid.height) * def.grid.tileSize * 0.5f;
  renderer.setGridParams(gp);
}

void Environment::applyDefault(Render::GL::Renderer &renderer,
                               Render::GL::Camera &camera) {
  const auto &cameraConfig = Game::GameConfig::instance().camera();
  camera.setRTSView(QVector3D(0, 0, 0), 15.0f, cameraConfig.defaultPitch,
                    cameraConfig.defaultYaw);

  camera.setPerspective(45.0f, 16.0f / 9.0f, 1.0f, 200.0f);
  Render::GL::Renderer::GridParams gp;
  gp.cellSize = 1.0f;
  gp.extent = 50.0f;
  renderer.setGridParams(gp);
}

} // namespace Map
} // namespace Game
