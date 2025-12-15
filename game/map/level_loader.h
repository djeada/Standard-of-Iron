#pragma once

#include "map_definition.h"
#include <QString>
#include <cstdint>
#include <memory>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Renderer;
class Camera;
} // namespace Render::GL

namespace Game::Map {

struct LevelLoadResult {
  bool ok = false;
  QString map_name;
  QString errorMessage;
  Engine::Core::EntityID player_unit_id = 0;
  float cam_fov = 45.0F;
  float cam_near = 0.1F;
  float cam_far = 1000.0F;
  int grid_width = 50;
  int grid_height = 50;
  float tile_size = 1.0F;
  int max_troops_per_player = 500;
  VictoryConfig victoryConfig;
  RainSettings rainSettings;
  std::uint32_t biome_seed = 0;
};

class LevelLoader {
public:
  static auto loadFromAssets(const QString &map_path,
                             Engine::Core::World &world,
                             Render::GL::Renderer &renderer,
                             Render::GL::Camera &camera) -> LevelLoadResult;
};

} // namespace Game::Map
