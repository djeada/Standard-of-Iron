#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "wildlife_draw_state.h"

namespace Render::GL {
class ISubmitter;
}

namespace Render::GL::Wildlife {

// A butchered sheep is a cartoon: it sways and sees stars under the mallet,
// then pops a puff of wool and keels over legs-up with the stars still going.
inline constexpr float k_sheep_star_linger_seconds = 4.0F;
inline constexpr float k_sheep_star_fade_seconds = 1.0F;
inline constexpr float k_sheep_wool_poof_seconds = 2.4F;
inline constexpr int k_sheep_star_count = 5;
inline constexpr int k_sheep_wool_tuft_count = 11;

struct SheepSlapstick {
  // 0..1: how much of the halo of stars is showing.
  float stars{0.0F};
  // Centre of the halo in the sheep's own space.
  QVector3D star_centre{0.0F, 0.80F, 0.34F};
  float star_radius{0.17F};
  // Dizzy sway, in degrees, about the sheep's feet.
  float sway_roll{0.0F};
  float sway_pitch{0.0F};
  float sway_yaw{0.0F};
  // Seconds since the wool burst; negative when there is none.
  float poof_time{-1.0F};
};

[[nodiscard]] auto plan_sheep_slapstick(const DrawState& state) -> SheepSlapstick;

[[nodiscard]] auto sheep_sway_matrix(const SheepSlapstick& gag) -> QMatrix4x4;

struct WoolTuft {
  QVector3D position;
  float radius{0.0F};
};

[[nodiscard]] auto
sheep_wool_tuft(std::uint32_t seed, int index, float poof_time) -> WoolTuft;

void submit_sheep_slapstick(const QMatrix4x4& model,
                            const SheepSlapstick& gag,
                            float time,
                            std::uint32_t seed,
                            const QVector3D& wool,
                            ISubmitter& out);

} // namespace Render::GL::Wildlife
