#include "wildlife_draw_state.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "../../entity_appearance.h"
#include "game/core/component.h"
#include "game/core/entity.h"

namespace Render::GL::Wildlife {

namespace {

struct GaitCursor {
  float distance{0.0F};
  float cycles{0.0F};
  float stamp{0.0F};
  float speed{0.0F};
  float elapsed{0.0F};
  float pending_distance{0.0F};
  float ambient{0.0F};
  float action{0.0F};
  float graze_hold{0.0F};
  std::uint8_t tier{0U};
  bool sampled{false};
};

constexpr float k_action_restart_drop = 0.25F;

constexpr float k_gait_speed_smoothing = 0.18F;
constexpr float k_gait_max_step_seconds = 0.25F;

constexpr float k_gait_exit_fraction = 0.55F;

constexpr float k_graze_release_seconds = 1.2F;

constexpr std::size_t k_cursor_prune_threshold = 512U;
constexpr float k_cursor_max_age = 30.0F;

auto gait_cursors() -> std::unordered_map<std::uint32_t, GaitCursor>& {
  static std::unordered_map<std::uint32_t, GaitCursor> cursors;
  return cursors;
}

constexpr float k_clip_blend_seconds = 0.22F;

struct ClipCursor {
  Render::Creature::AnimationStateId state{Render::Creature::AnimationStateId::Idle};
  Render::Creature::AnimationStateId outgoing{Render::Creature::AnimationStateId::Idle};
  float last_phase{0.0F};
  float outgoing_phase{0.0F};
  float blend_remaining{0.0F};
  float stamp{0.0F};
};

auto clip_cursors() -> std::unordered_map<std::uint32_t, ClipCursor>& {
  static std::unordered_map<std::uint32_t, ClipCursor> cursors;
  return cursors;
}

void prune_gait_cursors(float now) {
  auto& cursors = gait_cursors();
  if (cursors.size() < k_cursor_prune_threshold) {
    return;
  }
  std::erase_if(cursors, [now](const auto& entry) {
    return now - entry.second.stamp > k_cursor_max_age;
  });
}

} // namespace

auto hash_unit_float(std::uint32_t seed, std::uint32_t salt) -> float {
  std::uint32_t value = seed ^ (salt * 2654435761U);
  value ^= value >> 15U;
  value *= 2246822519U;
  value ^= value >> 13U;
  value *= 3266489917U;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xFFFFFFU) / 16777216.0F;
}

auto tinted(const QVector3D& color, float factor) -> QVector3D {
  return {std::clamp(color.x() * factor, 0.0F, 1.0F),
          std::clamp(color.y() * factor, 0.0F, 1.0F),
          std::clamp(color.z() * factor, 0.0F, 1.0F)};
}

auto mixed(const QVector3D& a, const QVector3D& b, float t) -> QVector3D {
  return a + ((b - a) * std::clamp(t, 0.0F, 1.0F));
}

auto resolve_draw_state(const DrawContext& ctx, float top_speed) -> DrawState {
  DrawState state;
  state.time = ctx.animation_time;
  if (ctx.entity == nullptr) {
    return state;
  }

  state.seed = static_cast<std::uint32_t>(ctx.entity->get_id() * 2654435761ULL) | 1U;

  if (ctx.entity->get_component<Engine::Core::RenderableComponent>() != nullptr) {
    state.coat = Render::entity_color(*ctx.entity);
  }
  float const variation = 0.9F + (hash_unit_float(state.seed, 7U) * 0.18F);
  state.coat = tinted(state.coat, variation);

  if (const auto* wildlife =
          ctx.entity->get_component<Engine::Core::WildlifeComponent>()) {
    state.behavior = wildlife->behavior;
    state.alarm = wildlife->alarm_timer;
    state.grazing = wildlife->behavior == Game::Wildlife::Behavior::Graze;
    state.alert = wildlife->behavior == Game::Wildlife::Behavior::Flee ||
                  wildlife->behavior == Game::Wildlife::Behavior::Stalk ||
                  wildlife->alarm_timer > 0.0F;
    if (wildlife->bite_timer > 0.0F) {
      constexpr float k_bite_seconds =
          Engine::Core::WildlifeComponent::k_bite_animation_seconds;
      state.bite_progress =
          std::clamp(1.0F - (wildlife->bite_timer / k_bite_seconds), 0.0F, 1.0F);
    }
    if (wildlife->flinch_timer > 0.0F) {
      constexpr float k_flinch_seconds =
          Engine::Core::WildlifeComponent::k_flinch_animation_seconds;
      state.flinch_progress =
          std::clamp(1.0F - (wildlife->flinch_timer / k_flinch_seconds), 0.0F, 1.0F);
    }
  }

  if (const auto* death =
          ctx.entity->get_component<Engine::Core::DeathAnimationComponent>()) {
    if (death->state == Engine::Core::DeathSequenceState::DeadHold) {
      state.dead = true;
      state.death_progress = 1.0F;
    } else {
      state.death_progress = std::clamp(
          death->state_time / std::max(death->state_duration, 0.001F), 0.0F, 1.0F);
    }
  }

  float speed = 0.0F;
  if (const auto* movement =
          ctx.entity->get_component<Engine::Core::MovementComponent>()) {
    float const vx = movement->get_vx();
    float const vz = movement->get_vz();
    speed = std::sqrt((vx * vx) + (vz * vz));
    state.distance = movement->get_travelled();
  }
  state.speed_ratio = std::clamp(speed / std::max(top_speed, 0.01F), 0.0F, 1.0F);
  if (state.speed_ratio > 0.02F) {
    state.grazing = false;
  }

  return state;
}

auto gait_speed(const DrawState& state) -> float {
  auto& cursors = gait_cursors();
  auto [entry, inserted] = cursors.try_emplace(state.seed);
  GaitCursor& cursor = entry->second;
  if (inserted) {
    cursor.cycles = hash_unit_float(state.seed, 11U);
    cursor.ambient = hash_unit_float(state.seed, 23U);
    prune_gait_cursors(state.time);
  }

  bool const restarted =
      inserted || state.distance < cursor.distance || state.time < cursor.stamp;
  if (restarted) {
    cursor.distance = state.distance;
    cursor.stamp = state.time;
    cursor.speed = 0.0F;
    cursor.pending_distance = 0.0F;
  }

  float const elapsed = std::max(state.time - cursor.stamp, 0.0F);

  if (!restarted && elapsed <= 1.0e-5F) {
    cursor.elapsed = 0.0F;
    return cursor.speed;
  }
  cursor.elapsed = std::min(elapsed, k_gait_max_step_seconds);
  cursor.stamp = state.time;

  float const stepped = std::max(0.0F, state.distance - cursor.distance);
  cursor.distance = state.distance;
  cursor.pending_distance += stepped;

  if (elapsed > 1.0e-5F) {
    float const measured = stepped / elapsed;
    float const blend = std::clamp(
        cursor.elapsed / (k_gait_speed_smoothing + cursor.elapsed), 0.0F, 1.0F);
    cursor.speed += (measured - cursor.speed) * blend;
    if (stepped <= 1.0e-6F &&
        (state.bite_progress >= 0.0F || state.flinch_progress >= 0.0F)) {

      cursor.speed = 0.0F;
    }
  }
  cursor.sampled = true;
  return cursor.speed;
}

auto resolve_gait_tier(const DrawState& state,
                       float ratio,
                       float walk_threshold,
                       float run_threshold) -> GaitTier {
  auto const stateless = [&]() {
    if (ratio > run_threshold) {
      return GaitTier::Run;
    }
    return ratio > walk_threshold ? GaitTier::Walk : GaitTier::Stand;
  };

  auto& cursors = gait_cursors();
  auto const entry = cursors.find(state.seed);
  if (entry == cursors.end()) {
    return stateless();
  }
  GaitCursor& cursor = entry->second;

  GaitTier tier = static_cast<GaitTier>(cursor.tier);
  switch (tier) {
  case GaitTier::Run:
    if (ratio <= run_threshold * k_gait_exit_fraction) {
      tier = ratio > walk_threshold ? GaitTier::Walk : GaitTier::Stand;
    }
    break;
  case GaitTier::Walk:
    if (ratio > run_threshold) {
      tier = GaitTier::Run;
    } else if (ratio <= walk_threshold * k_gait_exit_fraction) {
      tier = GaitTier::Stand;
    }
    break;
  case GaitTier::Stand:
    tier = stateless();
    break;
  }

  cursor.tier = static_cast<std::uint8_t>(tier);
  return tier;
}

auto grazing_latched(const DrawState& state, bool moving) -> bool {
  auto& cursors = gait_cursors();
  auto const entry = cursors.find(state.seed);
  if (entry == cursors.end()) {
    return state.grazing && !moving;
  }
  GaitCursor& cursor = entry->second;

  if (moving || state.alert || state.dead || state.death_progress >= 0.0F ||
      state.flinch_progress >= 0.0F) {
    cursor.graze_hold = 0.0F;
    return false;
  }
  if (state.grazing) {
    cursor.graze_hold = k_graze_release_seconds;
    return true;
  }
  cursor.graze_hold = std::max(0.0F, cursor.graze_hold - cursor.elapsed);
  return cursor.graze_hold > 0.0F;
}

auto gait_phase(const DrawState& state, float advance) -> float {
  auto& cursors = gait_cursors();
  auto const entry = cursors.find(state.seed);
  if (entry == cursors.end()) {
    return hash_unit_float(state.seed, 11U);
  }
  GaitCursor& cursor = entry->second;

  if (advance > 1.0e-4F) {

    cursor.cycles += cursor.pending_distance / advance;
    cursor.cycles -= std::floor(cursor.cycles);
    cursor.pending_distance = 0.0F;
  }
  return cursor.cycles - std::floor(cursor.cycles);
}

auto ambient_phase(const DrawState& state, float period_seconds) -> float {
  auto& cursors = gait_cursors();
  auto const entry = cursors.find(state.seed);
  if (entry == cursors.end() || period_seconds <= 1.0e-3F) {
    return 0.0F;
  }
  GaitCursor& cursor = entry->second;
  cursor.ambient += cursor.elapsed / period_seconds;
  return cursor.ambient - std::floor(cursor.ambient);
}

auto action_phase(const DrawState& state, float raw) -> float {
  auto& cursors = gait_cursors();
  auto const entry = cursors.find(state.seed);
  if (entry == cursors.end()) {
    return raw;
  }
  GaitCursor& cursor = entry->second;
  if (raw + k_action_restart_drop < cursor.action) {
    cursor.action = raw;
  } else {
    cursor.action = std::max(cursor.action, raw);
  }
  return cursor.action;
}

auto resolve_clip_transition(const DrawState& state,
                             Render::Creature::AnimationStateId incoming,
                             float incoming_phase) -> ClipTransition {
  auto& cursors = clip_cursors();
  auto [entry, inserted] = cursors.try_emplace(state.seed);
  ClipCursor& cursor = entry->second;
  if (inserted) {
    cursor.state = incoming;
    cursor.outgoing = incoming;
    cursor.last_phase = incoming_phase;
    cursor.stamp = state.time;
    if (cursors.size() >= k_cursor_prune_threshold) {
      std::erase_if(cursors, [now = state.time](const auto& item) {
        return now - item.second.stamp > k_cursor_max_age;
      });
    }
    return {};
  }

  float const elapsed = std::clamp(state.time - cursor.stamp, 0.0F, k_cursor_max_age);
  cursor.stamp = state.time;

  if (incoming != cursor.state) {
    cursor.outgoing = cursor.state;
    cursor.outgoing_phase = cursor.last_phase;
    cursor.blend_remaining = k_clip_blend_seconds;
    cursor.state = incoming;
  } else {
    cursor.blend_remaining = std::max(0.0F, cursor.blend_remaining - elapsed);
  }
  cursor.last_phase = incoming_phase;

  if (cursor.blend_remaining <= 0.0F) {
    return {};
  }

  ClipTransition transition;
  transition.outgoing = cursor.outgoing;
  transition.phase = cursor.outgoing_phase;
  transition.weight = cursor.blend_remaining / k_clip_blend_seconds;
  return transition;
}

} // namespace Render::GL::Wildlife
