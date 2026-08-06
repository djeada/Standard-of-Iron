#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "../../../game/wildlife/wildlife_species.h"
#include "../registry.h"

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
};

// `top_speed` is the species' own catalogue speed, so speed_ratio spans its whole
// range. Sharing one reference across species left a sheep, which tops out at 1.5,
// unable to reach a ratio its own run threshold would ever accept.
[[nodiscard]] auto resolve_draw_state(const DrawContext& ctx,
                                      float top_speed) -> DrawState;

// One gait cycle per `advance` of ground covered, integrated per animal.
//
// Both halves matter. Driving off ground distance instead of `animation_time * rate`
// removes the original teleport, where the rate multiplied the entire elapsed time
// and every change of speed shifted the legs by however many cycles had accumulated
// since the scenario began. Integrating rather than dividing removes the second one:
// a walk and a run cover different ground per cycle, so switching between them would
// otherwise reinterpret the whole distance travelled so far.
[[nodiscard]] auto gait_phase(const DrawState& state, float advance) -> float;

[[nodiscard]] auto tinted(const QVector3D& color, float factor) -> QVector3D;

[[nodiscard]] auto mixed(const QVector3D& a, const QVector3D& b, float t) -> QVector3D;

[[nodiscard]] auto hash_unit_float(std::uint32_t seed, std::uint32_t salt) -> float;

} // namespace Render::GL::Wildlife
