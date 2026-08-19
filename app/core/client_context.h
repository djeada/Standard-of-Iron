#pragma once

#include <QString>
#include <QVariantList>

#include <mutex>

#include "app/input/cursor_mode.h"

class CampaignManager;
class CursorManager;
class InputCommandHandler;
class MinimapManager;
class ProductionManager;
class QQuickWindow;
class RtsCameraController;
class VisibilityCoordinator;
struct ViewportState;

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SessionContext;
}

namespace Game::Map {
class MapCatalog;
}

namespace Game::Systems {
class PickingService;
class SaveLoadService;
class SelectionController;
struct LevelSnapshot;
} // namespace Game::Systems

namespace Render::GL {
class Camera;
class Renderer;
} // namespace Render::GL

namespace App::Controllers {
class CommandController;
}

namespace App::Core {

struct ClientContext {
  Game::Session::SessionContext* session = nullptr;
  Engine::Core::World* world = nullptr;
  const Game::Systems::LevelSnapshot* level = nullptr;
  int local_owner_id = 1;

  Render::GL::Renderer* renderer = nullptr;
  Render::GL::Camera* active_camera = nullptr;
  Render::GL::Camera* rts_camera = nullptr;
  Render::GL::Camera* commander_camera = nullptr;

  Game::Systems::PickingService* picking = nullptr;
  Game::Systems::SelectionController* selection = nullptr;

  RtsCameraController* camera_controller = nullptr;
  MinimapManager* minimap = nullptr;
  VisibilityCoordinator* visibility = nullptr;
  InputCommandHandler* input = nullptr;
  App::Controllers::CommandController* commands = nullptr;
  ProductionManager* production = nullptr;
  CursorManager* cursor = nullptr;

  CampaignManager* campaign = nullptr;
  Game::Map::MapCatalog* map_catalog = nullptr;
  Game::Systems::SaveLoadService* saves = nullptr;

  const ViewportState* viewport = nullptr;
  QQuickWindow* window = nullptr;
};

struct MatchLaunch {

  QString kind;
  QString reference;
  QString map_path;
  QVariantList player_configs;
  bool set_skirmish_context = false;
};

class ClientHost {
public:
  ClientHost() = default;
  ClientHost(const ClientHost&) = delete;
  ClientHost(ClientHost&&) = delete;
  auto operator=(const ClientHost&) -> ClientHost& = delete;
  auto operator=(ClientHost&&) -> ClientHost& = delete;
  virtual ~ClientHost() = default;

  virtual void ensure_initialized() = 0;

  [[nodiscard]] virtual auto lock_frame() -> std::unique_lock<std::recursive_mutex> = 0;

  virtual void set_cursor_mode(CursorMode mode) = 0;
};

} // namespace App::Core
