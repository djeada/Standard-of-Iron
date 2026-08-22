#pragma once

#include "../../core/entity.h"
#include "../../core/melee_intent.h"
#include "combat_action_definition.h"

namespace Game::Systems::CombatActions {

struct MeleeIntentInputs {

  float aim_delta_x{0.0F};
  float aim_delta_y{0.0F};

  float aim_rate{0.0F};

  int move_right_axis{0};
  int move_forward_axis{0};

  float held_duration{0.0F};

  float view_pitch_degrees{0.0F};

  float reach{1.5F};

  bool has_rest{false};
  float rest_dir_x{0.80F};
  float rest_dir_y{0.60F};

  bool prefer_thrust{false};
};

inline constexpr float k_melee_full_sweep_drag = 0.85F;

inline constexpr float k_melee_full_charge_seconds = 0.45F;

[[nodiscard]] auto resolve_melee_intent(const MeleeIntentInputs& inputs) noexcept
    -> Engine::Core::MeleeIntent;

[[nodiscard]] auto
steer_melee_intent(const Engine::Core::MeleeIntent& current,
                   const Engine::Core::MeleeIntent& desired,
                   float authority,
                   float max_delta) noexcept -> Engine::Core::MeleeIntent;

[[nodiscard]] auto select_melee_action(const Engine::Core::MeleeIntent& intent,
                                       Engine::Core::CombatAttackFamily family,
                                       bool mounted,
                                       bool finisher) noexcept -> CombatActionId;

struct MeleeControlTick {

  float aim_delta_x{0.0F};
  float aim_delta_y{0.0F};

  float view_pitch_degrees{0.0F};

  int move_right_axis{0};
  int move_forward_axis{0};

  float held_duration{0.0F};
  bool primary_held{false};

  float delta_time{0.0F};
};

void advance_melee_control(Engine::Core::Entity& fighter,
                           const MeleeControlTick& tick) noexcept;

} // namespace Game::Systems::CombatActions
