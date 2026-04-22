#pragma once

#include "creature_render_state.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>

namespace Engine::Core {
class UnitComponent;
}

namespace Render::GL {
struct DrawContext;
}

namespace Render::Creature::Pipeline {

[[nodiscard]] auto pass_intent_from_ctx(
    const Render::GL::DrawContext &ctx) noexcept -> RenderPassIntent;

[[nodiscard]] auto derive_unit_seed(
    const Render::GL::DrawContext &ctx,
    const Engine::Core::UnitComponent *unit) noexcept -> std::uint32_t;

[[nodiscard]] auto
sample_terrain_height_or_fallback(float world_x, float world_z,
                                  float fallback_y) noexcept -> float;

[[nodiscard]] auto
model_world_origin(const QMatrix4x4 &model) noexcept -> QVector3D;

void set_model_world_y(QMatrix4x4 &model, float world_y) noexcept;

[[nodiscard]] inline auto fast_random(std::uint32_t &state) noexcept -> float {
  state = state * 1664525U + 1013904223U;
  return static_cast<float>(state & 0x7FFFFFU) / static_cast<float>(0x7FFFFFU);
}

[[nodiscard]] inline auto instance_seed(std::uint32_t base_seed,
                                        int index) noexcept -> std::uint32_t {
  return base_seed ^ static_cast<std::uint32_t>(index * 9176);
}

} // namespace Render::Creature::Pipeline
