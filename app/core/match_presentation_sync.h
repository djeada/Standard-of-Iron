#pragma once

#include <QVariantMap>

#include <vector>

#include "game/systems/attack_range.h"
#include "game/systems/attack_targeting.h"
#include "game/systems/target_focus.h"

class CursorManager;
class HoverTracker;

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Camera;
}

namespace App::Core::PresentationSync {

struct SelectionAttackContext {
  Engine::Core::World* world = nullptr;
  const HoverTracker* hover = nullptr;
  const CursorManager* cursor = nullptr;
  const Render::GL::Camera* camera = nullptr;
  int local_owner_id = 1;
  bool spectator_mode = false;
};

struct AttackTargetingResult {
  Game::Systems::AttackTargetingHighlights highlights;

  QVariantMap hint;
};

[[nodiscard]] auto collect_attack_range_rings(const SelectionAttackContext& ctx)
    -> std::vector<Game::Systems::AttackRangeRing>;

[[nodiscard]] auto
collect_attack_targeting(const SelectionAttackContext& ctx) -> AttackTargetingResult;

} // namespace App::Core::PresentationSync
