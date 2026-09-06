#pragma once

#include "../../core/component_combat.h"

namespace Game::Systems::CombatActions {
struct CombatActionDefinition;
}

namespace Game::Systems::RpgCombat {

struct BowDrawTick {
  float allowed_delta{0.0F};

  bool at_full_draw{false};

  bool started_draw{false};

  bool reached_full_draw{false};

  bool started_straining{false};

  bool loosed{false};

  bool relaxed{false};

  float shot_power{0.0F};
};

[[nodiscard]] auto aim_spread_degrees(const Engine::Core::RpgCommanderAimComponent& aim,
                                      float stamina_ratio) -> float;

[[nodiscard]] auto
update_bow_draw(Engine::Core::RpgCommanderAimComponent& aim,
                const Engine::Core::RpgCommanderActionComponent& action,
                const Game::Systems::CombatActions::CombatActionDefinition& definition,
                float stamina_ratio,
                float delta_time) -> BowDrawTick;

} // namespace Game::Systems::RpgCombat
