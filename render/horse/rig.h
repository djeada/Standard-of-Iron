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

inline auto calculateHorseLOD(float distance) -> HorseLOD;

struct HorseRenderStats {
  uint32_t horsesTotal{0};
  uint32_t horsesRendered{0};
  uint32_t horsesSkippedLOD{0};
  uint32_t lodFull{0};
  uint32_t lodReduced{0};
  uint32_t lodMinimal{0};

  void reset() {
    horsesTotal = 0;
    horsesRendered = 0;
    horsesSkippedLOD = 0;
    lodFull = 0;
    lodReduced = 0;
    lodMinimal = 0;
  }
};

auto getHorseRenderStats() -> const HorseRenderStats &;

void resetHorseRenderStats();

struct HorseDimensions {
  float bodyLength{};
  float bodyWidth{};
  float bodyHeight{};
  float barrel_centerY{};

  float neckLength{};
  float neckRise{};

  float headLength{};
  float headWidth{};
  float headHeight{};
  float muzzleLength{};

  float legLength{};
  float hoofHeight{};

  float tailLength{};

  float saddle_height{};
  float saddleThickness{};
  float seatForwardOffset{};

  float stirrupDrop{};
  float stirrupOut{};

  float idleBobAmplitude{};
  float moveBobAmplitude{};
};

struct HorseVariant {
  QVector3D coatColor;
  QVector3D mane_color;
  QVector3D tail_color;
  QVector3D muzzleColor;
  QVector3D hoof_color;
  QVector3D saddleColor;
  QVector3D blanketColor;
  QVector3D tack_color;
};

struct HorseGait {
  float cycleTime{};
  float frontLegPhase{};
  float rearLegPhase{};
  float strideSwing{};
  float strideLift{};
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

auto makeHorseDimensions(uint32_t seed) -> HorseDimensions;
auto makeHorseVariant(uint32_t seed, const QVector3D &leatherBase,
                      const QVector3D &clothBase) -> HorseVariant;
auto makeHorseProfile(uint32_t seed, const QVector3D &leatherBase,
                      const QVector3D &clothBase) -> HorseProfile;
auto compute_mount_frame(const HorseProfile &profile) -> MountedAttachmentFrame;
auto compute_rein_state(uint32_t horse_seed,
                        const HumanoidAnimationContext &rider_ctx) -> ReinState;
auto compute_rein_handle(const MountedAttachmentFrame &mount, bool is_left,
                         float slack, float tension) -> QVector3D;
auto evaluate_horse_motion(HorseProfile &profile, const AnimationInputs &anim,
                           const HumanoidAnimationContext &rider_ctx)
    -> HorseMotionSample;
void apply_mount_vertical_offset(MountedAttachmentFrame &frame, float bob);

inline void scaleHorseDimensions(HorseDimensions &dims, float scale) {
  dims.bodyLength *= scale;
  dims.bodyWidth *= scale;
  dims.bodyHeight *= scale;
  dims.neckLength *= scale;
  dims.neckRise *= scale;
  dims.headLength *= scale;
  dims.headWidth *= scale;
  dims.headHeight *= scale;
  dims.muzzleLength *= scale;
  dims.legLength *= scale;
  dims.hoofHeight *= scale;
  dims.tailLength *= scale;
  dims.saddleThickness *= scale;
  dims.seatForwardOffset *= scale;
  dims.stirrupOut *= scale;
  dims.stirrupDrop *= scale;
  dims.barrel_centerY *= scale;
  dims.saddle_height *= scale;
  dims.idleBobAmplitude *= scale;
  dims.moveBobAmplitude *= scale;
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

  void renderSimplified(const DrawContext &ctx, const AnimationInputs &anim,
                        const HumanoidAnimationContext &rider_ctx,
                        HorseProfile &profile,
                        const MountedAttachmentFrame *shared_mount,
                        const HorseMotionSample *shared_motion,
                        ISubmitter &out) const;

  void renderMinimal(const DrawContext &ctx, HorseProfile &profile,
                     const HorseMotionSample *shared_motion,
                     ISubmitter &out) const;

protected:
  virtual void drawAttachments(const DrawContext &, const AnimationInputs &,
                               const HumanoidAnimationContext &, HorseProfile &,
                               const MountedAttachmentFrame &, float, float,
                               float, const HorseBodyFrames &,
                               ISubmitter &) const {}

private:
  void renderFull(const DrawContext &ctx, const AnimationInputs &anim,
                  const HumanoidAnimationContext &rider_ctx,
                  HorseProfile &profile,
                  const MountedAttachmentFrame *shared_mount,
                  const ReinState *shared_reins,
                  const HorseMotionSample *shared_motion,
                  ISubmitter &out) const;
};

inline auto calculateHorseLOD(float distance) -> HorseLOD {
  const auto &settings = Render::GraphicsSettings::instance();
  if (distance < settings.horseFullDetailDistance()) {
    return HorseLOD::Full;
  }
  if (distance < settings.horseReducedDetailDistance()) {
    return HorseLOD::Reduced;
  }
  if (distance < settings.horseMinimalDetailDistance()) {
    return HorseLOD::Minimal;
  }
  return HorseLOD::Billboard;
}

} // namespace Render::GL
