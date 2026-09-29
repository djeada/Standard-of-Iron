#pragma once

#include <QVariantMap>

#include <vector>

#include "app/world/focus_target.h"
#include "game/systems/target_focus.h"

class VisibilityCoordinator;

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SessionContext;
}

namespace App::ViewModels {
class ActivityViewModel;
}

namespace App::World {

struct FocusInputs {
  Engine::Core::World* world = nullptr;
  Game::Session::SessionContext* session = nullptr;
  const VisibilityCoordinator* visibility = nullptr;
  App::ViewModels::ActivityViewModel* activity = nullptr;
  int local_owner_id = 1;
  bool spectator_mode = false;
};

class FocusTracker {
public:
  void sync_focus_targets(const FocusInputs& inputs);
  void sync_target_focus_markers(const FocusInputs& inputs);

  [[nodiscard]] auto
  markers() const -> const std::vector<Game::Systems::TargetFocusMarker>& {
    return m_markers;
  }

private:
  [[nodiscard]] static auto
  describe(const FocusInputs& inputs,
           Engine::Core::EntityID id) -> App::Core::FocusTargetInfo;

  std::vector<Game::Systems::TargetFocusMarker> m_markers;
  QVariantMap m_inspect_target;
  QVariantMap m_selection_target;
};

} // namespace App::World
