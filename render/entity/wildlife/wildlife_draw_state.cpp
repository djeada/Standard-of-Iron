#include "wildlife_draw_state.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "../../../game/core/component.h"
#include "../../../game/core/entity.h"

namespace Render::GL::Wildlife {

namespace {

// Where each animal is in its gait cycle. Held here rather than on the entity because
// the stride that converts ground into cycles is a property of the model, and the
// simulation has no business knowing it.
struct GaitCursor {
  float distance{0.0F};
  float cycles{0.0F};
  float stamp{0.0F};
};

constexpr std::size_t k_cursor_prune_threshold = 512U;
constexpr float k_cursor_max_age = 30.0F;

auto gait_cursors() -> std::unordered_map<std::uint32_t, GaitCursor>& {
  static std::unordered_map<std::uint32_t, GaitCursor> cursors;
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

  if (const auto* renderable =
          ctx.entity->get_component<Engine::Core::RenderableComponent>()) {
    state.coat =
        QVector3D(renderable->color[0], renderable->color[1], renderable->color[2]);
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

auto gait_phase(const DrawState& state, float advance) -> float {
  float const offset = hash_unit_float(state.seed, 11U);

  auto& cursors = gait_cursors();
  auto [entry, inserted] = cursors.try_emplace(state.seed);
  GaitCursor& cursor = entry->second;
  if (inserted) {
    cursor.distance = state.distance;
    cursor.cycles = offset;
    prune_gait_cursors(state.time);
  }
  cursor.stamp = state.time;

  // Zero when the same animal is drawn twice in one frame, so extra passes cannot
  // wind the cycle on.
  float const stepped = state.distance - cursor.distance;
  cursor.distance = state.distance;
  if (advance > 1.0e-4F && stepped > 0.0F) {
    cursor.cycles += stepped / advance;
  }

  return cursor.cycles - std::floor(cursor.cycles);
}

} // namespace Render::GL::Wildlife
