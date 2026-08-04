#include "wildlife_draw_state.h"

#include <algorithm>
#include <cmath>

#include "../../../game/core/component.h"
#include "../../../game/core/entity.h"

namespace Render::GL::Wildlife {

namespace {

constexpr float k_reference_speed = 3.2F;

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

auto ellipsoid_at(const QMatrix4x4& parent,
                  const QVector3D& center,
                  const QVector3D& radii) -> QMatrix4x4 {
  QMatrix4x4 model = parent;
  model.translate(center);
  model.scale(radii);
  return model;
}

auto resolve_draw_state(const DrawContext& ctx, float stride_rate) -> DrawState {
  DrawState state;
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
  }
  state.speed_ratio = std::clamp(speed / k_reference_speed, 0.0F, 1.0F);
  if (state.speed_ratio > 0.02F) {
    state.grazing = false;
  }

  float const offset = hash_unit_float(state.seed, 11U);
  float const rate = stride_rate * (0.35F + (state.speed_ratio * 1.35F));
  state.phase = std::fmod((ctx.animation_time * rate) + offset, 1.0F);
  return state;
}

} // namespace Render::GL::Wildlife
