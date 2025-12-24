#pragma once

#include "../entity/registry.h"
#include "../graphics_settings.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>

namespace Render::GL {

struct AnimationInputs;
struct HumanoidAnimationContext;
class ISubmitter;

inline auto calculate_elephant_lod(float distance) -> HorseLOD;

struct ElephantRenderStats {
  uint32_t elephants_total{0};
  uint32_t elephants_rendered{0};
  uint32_t elephants_skipped_lod{0};
  uint32_t lod_full{0};
  uint32_t lod_reduced{0};
  uint32_t lod_minimal{0};

  void reset() {
    elephants_total = 0;
    elephants_rendered = 0;
    elephants_skipped_lod = 0;
    lod_full = 0;
    lod_reduced = 0;
    lod_minimal = 0;
  }
};

auto get_elephant_render_stats() -> const ElephantRenderStats &;

void reset_elephant_render_stats();

struct ElephantDimensions {
  float body_length{};
  float body_width{};
  float body_height{};
  float barrel_center_y{};

  float neck_length{};
  float neck_width{};

  float head_length{};
  float head_width{};
  float head_height{};

  float trunk_length{};
  float trunk_base_radius{};
  float trunk_tip_radius{};

  float ear_width{};
  float ear_height{};
  float ear_thickness{};

  float leg_length{};
  float leg_radius{};
  float foot_radius{};

  float tail_length{};

  float tusk_length{};
  float tusk_radius{};

  float howdah_width{};
  float howdah_length{};
  float howdah_height{};

  float idle_bob_amplitude{};
  float move_bob_amplitude{};
};

struct ElephantVariant {
  QVector3D skin_color;
  QVector3D skin_highlight;
  QVector3D skin_shadow;
  QVector3D ear_inner_color;
  QVector3D tusk_color;
  QVector3D toenail_color;
  QVector3D howdah_wood_color;
  QVector3D howdah_fabric_color;
  QVector3D howdah_metal_color;
};

struct ElephantGait {
  float cycle_time{};
  float front_leg_phase{};
  float rear_leg_phase{};
  float stride_swing{};
  float stride_lift{};
};

struct ElephantProfile {
  ElephantDimensions dims{};
  ElephantVariant variant;
  ElephantGait gait{};
};

struct ElephantAttachmentFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};

  auto make_local_transform(const QMatrix4x4 &parent,
                            const QVector3D &local_offset,
                            float uniform_scale) const -> QMatrix4x4 {
    QMatrix4x4 m = parent;
    QVector3D const world_pos = origin + right * local_offset.x() +
                                up * local_offset.y() +
                                forward * local_offset.z();
    m.translate(world_pos);
    QMatrix4x4 basis;
    basis.setColumn(0, QVector4D(right * uniform_scale, 0.0F));
    basis.setColumn(1, QVector4D(up * uniform_scale, 0.0F));
    basis.setColumn(2, QVector4D(forward * uniform_scale, 0.0F));
    basis.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
    return m * basis;
  }
};

struct ElephantBodyFrames {
  ElephantAttachmentFrame head{};
  ElephantAttachmentFrame neck_base{};
  ElephantAttachmentFrame back_center{};
  ElephantAttachmentFrame howdah{};
  ElephantAttachmentFrame rump{};
  ElephantAttachmentFrame tail_base{};
  ElephantAttachmentFrame trunk_base{};
  ElephantAttachmentFrame trunk_tip{};
  ElephantAttachmentFrame ear_left{};
  ElephantAttachmentFrame ear_right{};
};

struct HowdahAttachmentFrame {
  QVector3D howdah_center;
  QVector3D seat_position;
  QVector3D seat_forward;
  QVector3D seat_right;
  QVector3D seat_up;
  QVector3D ground_offset;
};

struct ElephantMotionSample {
  float phase = 0.0F;
  float bob = 0.0F;
  bool is_moving = false;
  float trunk_swing = 0.0F;
  float ear_flap = 0.0F;
};

// Leg indices for the 4-leg gait system
enum class LegIndex : int { FrontLeft = 0, FrontRight = 1, RearLeft = 2, RearRight = 3 };

// Per-leg state for stance/swing animation
struct ElephantLegState {
  QVector3D planted_foot{0.0F, 0.0F, 0.0F};  // World-space foot plant position
  QVector3D swing_start{0.0F, 0.0F, 0.0F};   // Where swing began
  QVector3D swing_target{0.0F, 0.0F, 0.0F};  // Where swing will land
  float swing_progress = 0.0F;                // 0 = just lifted, 1 = just planted
  bool in_swing = false;                      // true = swinging, false = stance
};

// Complete gait state for all 4 legs
struct ElephantGaitState {
  ElephantLegState legs[4]{};
  float cycle_phase = 0.0F;         // Overall gait cycle [0,1)
  float weight_shift_x = 0.0F;      // Lateral weight shift for balance
  float weight_shift_z = 0.0F;      // Fore-aft weight shift
  float shoulder_lag = 0.0F;        // Secondary motion: shoulder rotation lag
  float hip_lag = 0.0F;             // Secondary motion: hip rotation lag
  bool initialized = false;
};

// Solved leg pose from IK
struct ElephantLegPose {
  QVector3D hip;
  QVector3D knee;
  QVector3D ankle;
  QVector3D foot;
  float upper_radius;
  float lower_radius;
};

auto make_elephant_dimensions(uint32_t seed) -> ElephantDimensions;
auto make_elephant_variant(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantVariant;
auto make_elephant_profile(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantProfile;
auto get_or_create_cached_elephant_profile(
    uint32_t seed, const QVector3D &fabric_base,
    const QVector3D &metal_base) -> ElephantProfile;
void advance_elephant_profile_cache_frame();
auto compute_howdah_frame(const ElephantProfile &profile)
    -> HowdahAttachmentFrame;
auto evaluate_elephant_motion(ElephantProfile &profile,
                              const AnimationInputs &anim)
    -> ElephantMotionSample;
void apply_howdah_vertical_offset(HowdahAttachmentFrame &frame, float bob);

// Gait system functions
void update_elephant_gait(ElephantGaitState &state,
                          const ElephantProfile &profile,
                          const AnimationInputs &anim,
                          const QVector3D &body_world_pos,
                          float body_forward_z);

auto solve_elephant_leg_ik(const QVector3D &hip, const QVector3D &foot_target,
                           float upper_len, float lower_len,
                           float lateral_sign) -> ElephantLegPose;

auto evaluate_swing_position(const ElephantLegState &leg,
                             float lift_height) -> QVector3D;

inline void scale_elephant_dimensions(ElephantDimensions &dims, float scale) {
  dims.body_length *= scale;
  dims.body_width *= scale;
  dims.body_height *= scale;
  dims.neck_length *= scale;
  dims.neck_width *= scale;
  dims.head_length *= scale;
  dims.head_width *= scale;
  dims.head_height *= scale;
  dims.trunk_length *= scale;
  dims.trunk_base_radius *= scale;
  dims.trunk_tip_radius *= scale;
  dims.ear_width *= scale;
  dims.ear_height *= scale;
  dims.ear_thickness *= scale;
  dims.leg_length *= scale;
  dims.leg_radius *= scale;
  dims.foot_radius *= scale;
  dims.tail_length *= scale;
  dims.tusk_length *= scale;
  dims.tusk_radius *= scale;
  dims.howdah_width *= scale;
  dims.howdah_length *= scale;
  dims.howdah_height *= scale;
  dims.barrel_center_y *= scale;
  dims.idle_bob_amplitude *= scale;
  dims.move_bob_amplitude *= scale;
}

class ElephantRendererBase {
public:
  virtual ~ElephantRendererBase() = default;

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              ElephantProfile &profile,
              const HowdahAttachmentFrame *shared_howdah,
              const ElephantMotionSample *shared_motion, ISubmitter &out,
              HorseLOD lod) const;

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              ElephantProfile &profile,
              const HowdahAttachmentFrame *shared_howdah,
              const ElephantMotionSample *shared_motion, ISubmitter &out) const;

  void render_simplified(const DrawContext &ctx, const AnimationInputs &anim,
                         ElephantProfile &profile,
                         const HowdahAttachmentFrame *shared_howdah,
                         const ElephantMotionSample *shared_motion,
                         ISubmitter &out) const;

  void render_minimal(const DrawContext &ctx, ElephantProfile &profile,
                      const ElephantMotionSample *shared_motion,
                      ISubmitter &out) const;

protected:
  virtual void draw_howdah(const DrawContext &, const AnimationInputs &,
                           ElephantProfile &, const HowdahAttachmentFrame &,
                           float, float, const ElephantBodyFrames &,
                           ISubmitter &) const {}

private:
  void render_full(const DrawContext &ctx, const AnimationInputs &anim,
                   ElephantProfile &profile,
                   const HowdahAttachmentFrame *shared_howdah,
                   const ElephantMotionSample *shared_motion,
                   ISubmitter &out) const;
};

inline auto calculate_elephant_lod(float distance) -> HorseLOD {
  const auto &settings = Render::GraphicsSettings::instance();
  if (distance < settings.horse_full_detail_distance()) {
    return HorseLOD::Full;
  }
  if (distance < settings.horse_reduced_detail_distance()) {
    return HorseLOD::Reduced;
  }
  if (distance < settings.horse_minimal_detail_distance()) {
    return HorseLOD::Minimal;
  }
  return HorseLOD::Billboard;
}

} // namespace Render::GL
