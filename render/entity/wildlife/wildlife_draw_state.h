#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "../../../game/wildlife/wildlife_species.h"
#include "../registry.h"

namespace Render::GL::Wildlife {

struct DrawState {
  float phase{0.0F};
  float speed_ratio{0.0F};
  float alarm{0.0F};
  bool grazing{false};
  bool alert{false};
  QVector3D coat{0.8F, 0.8F, 0.8F};
  std::uint32_t seed{1U};
  Game::Wildlife::Behavior behavior{Game::Wildlife::Behavior::Graze};
};

[[nodiscard]] auto resolve_draw_state(const DrawContext& ctx,
                                      float stride_rate) -> DrawState;

[[nodiscard]] auto tinted(const QVector3D& color, float factor) -> QVector3D;

[[nodiscard]] auto mixed(const QVector3D& a, const QVector3D& b, float t) -> QVector3D;

[[nodiscard]] auto hash_unit_float(std::uint32_t seed, std::uint32_t salt) -> float;

} // namespace Render::GL::Wildlife
