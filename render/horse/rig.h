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

inline auto calculate_horse_lod(float distance) -> HorseLOD;

struct HorseRenderStats {
  uint32_t horses_total{0};
  uint32_t horses_rendered{0};
  uint32_t horses_skipped_lod{0};
  uint32_t profiles_computed{0};
  uint32_t profiles_cached{0};
  uint32_t lod_full{0};
  uint32_t lod_reduced{0};
  uint32_t lod_minimal{0};

  void reset() {
    horses_total = 0;
    horses_rendered = 0;
    horses_skipped_lod = 0;
    profiles_computed = 0;
    profiles_cached = 0;
    lod_full = 0;
    lod_reduced = 0;
    lod_minimal = 0;
  }
};

auto get_horse_render_stats() -> const HorseRenderStats &;

void reset_horse_render_stats();

struct HorseDimensions {
  float body_length{};
  float body_width{};
  float body_height{};
  float barrel_center_y{};

  float neck_length{};
  float neck_rise{};

  float head_length{};
  float head_width{};
  float head_height{};
  float muzzle_length{};

  float leg_length{};
  float hoof_height{};

  float tail_length{};

  float saddle_height{};
  float saddle_thickness{};
  float seat_forward_offset{};

  float stirrup_drop{};
  float stirrup_out{};

  float idle_bob_amplitude{};
  float move_bob_amplitude{};
};

struct HorseVariant {
  QVector3D coat_color;
  QVector3D mane_color;
  QVector3D tail_color;
  QVector3D muzzle_color;
  QVector3D hoof_color;
  QVector3D saddle_color;
  QVector3D blanket_color;
  QVector3D tack_color;
};

struct HorseGait {
  float cycle_time{};
  float front_leg_phase{};
  float rear_leg_phase{};
  float stride_swing{};
  float stride_lift{};
};

struct HorseProfile {
  HorseDimensions dims{};
  HorseVariant variant;
  HorseGait gait{};
};

struct HorseAttachmentFrame {
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

struct HorseBodyFrames {
  HorseAttachmentFrame head{};
  HorseAttachmentFrame neck_base{};
  HorseAttachmentFrame withers{};
  HorseAttachmentFrame back_center{};
  HorseAttachmentFrame croup{};
  HorseAttachmentFrame chest{};
  HorseAttachmentFrame barrel{};
  HorseAttachmentFrame rump{};
  HorseAttachmentFrame tail_base{};
  HorseAttachmentFrame muzzle{};
};

struct MountedAttachmentFrame {
  QVector3D saddle_center;
  QVector3D seat_position;
  QVector3D seat_forward;
  QVector3D seat_right;
  QVector3D seat_up;

  QVector3D stirrup_attach_left;
  QVector3D stirrup_attach_right;
  QVector3D stirrup_bottom_left;
  QVector3D stirrup_bottom_right;

  QVector3D rein_bit_left;
  QVector3D rein_bit_right;
  QVector3D bridle_base;
  QVector3D ground_offset;
  auto stirrup_attach(bool is_left) const -> const QVector3D &;
  auto stirrup_bottom(bool is_left) const -> const QVector3D &;
};

struct ReinState {
  float slack = 0.0F;
  float tension = 0.0F;
};

struct HorseMotionSample {
  float phase = 0.0F;
  float bob = 0.0F;
  bool is_moving = false;
  float rider_intensity = 0.0F;
};

auto make_horse_dimensions(uint32_t seed) -> HorseDimensions;
auto make_horse_variant(uint32_t seed, const QVector3D &leather_base,
                        const QVector3D &cloth_base) -> HorseVariant;
auto make_horse_profile(uint32_t seed, const QVector3D &leather_base,
                        const QVector3D &cloth_base) -> HorseProfile;
auto get_or_create_cached_horse_profile(
    uint32_t seed, const QVector3D &leather_base,
    const QVector3D &cloth_base) -> HorseProfile;
void advance_horse_profile_cache_frame();
auto compute_mount_frame(const HorseProfile &profile) -> MountedAttachmentFrame;
auto compute_rein_state(uint32_t horse_seed,
                        const HumanoidAnimationContext &rider_ctx) -> ReinState;
auto compute_rein_handle(const MountedAttachmentFrame &mount, bool is_left,
                         float slack, float tension) -> QVector3D;
auto evaluate_horse_motion(HorseProfile &profile, const AnimationInputs &anim,
                           const HumanoidAnimationContext &rider_ctx)
    -> HorseMotionSample;
void apply_mount_vertical_offset(MountedAttachmentFrame &frame, float bob);

inline void scale_horse_dimensions(HorseDimensions &dims, float scale) {
  dims.body_length *= scale;
  dims.body_width *= scale;
  dims.body_height *= scale;
  dims.neck_length *= scale;
  dims.neck_rise *= scale;
  dims.head_length *= scale;
  dims.head_width *= scale;
  dims.head_height *= scale;
  dims.muzzle_length *= scale;
  dims.leg_length *= scale;
  dims.hoof_height *= scale;
  dims.tail_length *= scale;
  dims.saddle_thickness *= scale;
  dims.seat_forward_offset *= scale;
  dims.stirrup_out *= scale;
  dims.stirrup_drop *= scale;
  dims.barrel_center_y *= scale;
  dims.saddle_height *= scale;
  dims.idle_bob_amplitude *= scale;
  dims.move_bob_amplitude *= scale;
}

class HorseRendererBase {
public:
  virtual ~HorseRendererBase() = default;

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
              const MountedAttachmentFrame *shared_mount,
              const ReinState *shared_reins,
              const HorseMotionSample *shared_motion, ISubmitter &out,
              HorseLOD lod) const;

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
              const MountedAttachmentFrame *shared_mount,
              const ReinState *shared_reins,
              const HorseMotionSample *shared_motion, ISubmitter &out) const;

  void render_simplified(const DrawContext &ctx, const AnimationInputs &anim,
                         const HumanoidAnimationContext &rider_ctx,
                         HorseProfile &profile,
                         const MountedAttachmentFrame *shared_mount,
                         const HorseMotionSample *shared_motion,
                         ISubmitter &out) const;

  void render_minimal(const DrawContext &ctx, HorseProfile &profile,
                      const HorseMotionSample *shared_motion,
                      ISubmitter &out) const;

protected:
  virtual void draw_attachments(const DrawContext &, const AnimationInputs &,
                                const HumanoidAnimationContext &,
                                HorseProfile &, const MountedAttachmentFrame &,
                                float, float, float, const HorseBodyFrames &,
                                ISubmitter &) const {}

private:
  void render_full(const DrawContext &ctx, const AnimationInputs &anim,
                   const HumanoidAnimationContext &rider_ctx,
                   HorseProfile &profile,
                   const MountedAttachmentFrame *shared_mount,
                   const ReinState *shared_reins,
                   const HorseMotionSample *shared_motion,
                   ISubmitter &out) const;
};

inline auto calculate_horse_lod(float distance) -> HorseLOD {
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
