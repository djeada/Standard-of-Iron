#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "game/wildlife/wildlife_species.h"
#include "render/creature/animation_state_components.h"
#include "render/entity/registry.h"

namespace Render::GL::Wildlife {

struct DrawState {
  float speed_ratio{0.0F};
  float distance{0.0F};
  float time{0.0F};
  float alarm{0.0F};
  bool grazing{false};
  bool alert{false};
  QVector3D coat{0.8F, 0.8F, 0.8F};
  std::uint32_t seed{1U};
  Game::Wildlife::Behavior behavior{Game::Wildlife::Behavior::Graze};

  float bite_progress{-1.0F};
  float flinch_progress{-1.0F};
  float death_progress{-1.0F};
  bool dead{false};
};

enum class GaitTier : std::uint8_t {
  Stand = 0,
  Walk = 1,
  Run = 2
};

struct ClipTransition {
  Render::Creature::AnimationStateId outgoing{Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};
  float weight{0.0F};
};

[[nodiscard]] auto resolve_draw_state(const DrawContext& ctx,
                                      float top_speed) -> DrawState;

[[nodiscard]] auto gait_speed(const DrawState& state) -> float;

[[nodiscard]] auto resolve_gait_tier(const DrawState& state,
                                     float ratio,
                                     float walk_threshold,
                                     float run_threshold) -> GaitTier;

[[nodiscard]] auto grazing_latched(const DrawState& state, bool moving) -> bool;

[[nodiscard]] auto gait_phase(const DrawState& state, float advance) -> float;

[[nodiscard]] auto ambient_phase(const DrawState& state, float period_seconds) -> float;

[[nodiscard]] auto action_phase(const DrawState& state, float raw) -> float;

[[nodiscard]] auto resolve_clip_transition(const DrawState& state,
                                           Render::Creature::AnimationStateId incoming,
                                           float incoming_phase) -> ClipTransition;

[[nodiscard]] auto tinted(const QVector3D& color, float factor) -> QVector3D;

[[nodiscard]] auto mixed(const QVector3D& a, const QVector3D& b, float t) -> QVector3D;

[[nodiscard]] auto hash_unit_float(std::uint32_t seed, std::uint32_t salt) -> float;

} // namespace Render::GL::Wildlife
