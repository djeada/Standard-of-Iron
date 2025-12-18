#pragma once

#include "../map/map_definition.h"
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>
#include <cstdint>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

struct LevelSnapshot {
  QString map_path;
  QString map_name;
  Engine::Core::EntityID player_unit_id = 0;
  float cam_fov = 45.0F;
  float cam_near = 0.1F;
  float cam_far = 1000.0F;
  int max_troops_per_player = 500;
  int grid_width = 50;
  int grid_height = 50;
  float tile_size = 1.0F;
  bool is_spectator_mode = false;
  Game::Map::RainSettings rain;
  std::uint32_t biome_seed = 0;
};

struct RuntimeSnapshot {
  bool paused = false;
  float time_scale = 1.0F;
  int local_owner_id = 1;
  QString victory_state = "";
  int cursor_mode = 0;
  int selected_player_id = 1;
  bool follow_selection = false;
};

class GameStateSerializer {
public:
  static auto build_metadata(const Engine::Core::World &world,
                            const Render::GL::Camera *camera,
                            const LevelSnapshot &level,
                            const RuntimeSnapshot &runtime) -> QJsonObject;

  static void restore_camera_from_metadata(const QJsonObject &metadata,
                                        Render::GL::Camera *camera,
                                        int viewport_width,
                                        int viewport_height);

  static void restore_runtime_from_metadata(const QJsonObject &metadata,
                                         RuntimeSnapshot &runtime);

  static void restore_level_from_metadata(const QJsonObject &metadata,
                                       LevelSnapshot &level);
};

} // namespace Game::Systems
