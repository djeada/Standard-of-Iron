#pragma once

#include <QVector3D>

#include <vector>

#include "game/formation/army_formation_planner.h"

namespace Engine::Core {
class World;
}

namespace App::Controllers {

class FormationPreviewCache {
public:
  enum class Refresh : std::uint8_t {
    Unchanged,
    Changed
  };

  [[nodiscard]] auto plan() const -> const Game::Formation::ArmyFormationPlan& {
    return m_plan;
  }

  [[nodiscard]] auto refresh(Engine::Core::World* world,
                             Game::Formation::ArmyFormationRequest request,
                             bool placing) -> Refresh;

  void invalidate_layout();
  void mark_dirty() { m_dirty = true; }
  void clear();

private:
  Game::Formation::ArmyFormationPlan m_plan;
  std::vector<Game::Formation::ArmyFormationMember> m_members;
  Game::Formation::ArmyFormationLayout m_layout;
  bool m_layout_valid = false;
  bool m_dirty = true;
  QVector3D m_previewed_anchor;
  float m_previewed_facing = 0.0F;
};

} // namespace App::Controllers
