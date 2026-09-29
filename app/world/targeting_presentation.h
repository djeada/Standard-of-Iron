#pragma once

#include <QPointF>
#include <QVariantMap>
#include <QVector3D>

#include <functional>
#include <vector>

#include "app/core/match_presentation_sync.h"
#include "game/systems/attack_range.h"
#include "game/systems/attack_targeting.h"
#include "game/systems/interaction_targeting.h"

class ProductionManager;

namespace Game::Session {
class SessionContext;
}

namespace App::ViewModels {
class ActivityViewModel;
}

namespace App::World {

using ScreenToGround = std::function<bool(const QPointF& screen, QVector3D& ground)>;

struct TargetingInputs {
  App::Core::PresentationSync::SelectionAttackContext attack;
  Game::Session::SessionContext* session = nullptr;
  const ProductionManager* production = nullptr;
  App::ViewModels::ActivityViewModel* activity = nullptr;
  bool commander_active = false;
  QPointF cursor_screen;
  ScreenToGround screen_to_ground;
};

class TargetingPresentation {
public:
  void sync_attack_targeting(const TargetingInputs& inputs);
  void sync_interaction_targeting(float delta_time, const TargetingInputs& inputs);
  void sync_attack_range_rings(const TargetingInputs& inputs);

  void reset_interaction();

  [[nodiscard]] auto
  attack_targeting() const -> const Game::Systems::AttackTargetingHighlights& {
    return m_attack_targeting;
  }
  [[nodiscard]] auto interaction_targeting() const
      -> const Game::Systems::InteractionTargetingHighlights& {
    return m_interaction_targeting;
  }
  [[nodiscard]] auto
  attack_range_rings() const -> const std::vector<Game::Systems::AttackRangeRing>& {
    return m_attack_range_rings;
  }

private:
  [[nodiscard]] auto collect_interaction(const TargetingInputs& inputs,
                                         QVariantMap& hint)
      -> Game::Systems::InteractionTargetingHighlights;

  Game::Systems::AttackTargetingHighlights m_attack_targeting;
  Game::Systems::InteractionTargetingHighlights m_interaction_targeting;
  float m_interaction_accumulator = 0.0F;
  QVariantMap m_interaction_hint;
  QVariantMap m_attack_hint;
  std::vector<Game::Systems::AttackRangeRing> m_attack_range_rings;
};

} // namespace App::World
