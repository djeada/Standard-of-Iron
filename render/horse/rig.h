#pragma once

#include <QVector3D>
#include <cstdint>

namespace Render::GL {

struct DrawContext;
struct AnimationInputs;
struct HumanoidAnimationContext;
class ISubmitter;

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

struct HorseMountFrame {
  QVector3D saddle_center;
  QVector3D seat_position;
  QVector3D seat_forward;
  QVector3D seat_right;
  QVector3D seat_up;

  QVector3D stirrup_attach_left;
  QVector3D stirrup_attach_right;
  QVector3D stirrup_bottom_left;
  QVector3D stirrup_bottom_right;

  QVector3D rein_attach_left;
  QVector3D rein_attach_right;
  QVector3D bridle_base;
};

auto makeHorseDimensions(uint32_t seed) -> HorseDimensions;
auto makeHorseVariant(uint32_t seed, const QVector3D &leatherBase,
                      const QVector3D &clothBase) -> HorseVariant;
auto makeHorseProfile(uint32_t seed, const QVector3D &leatherBase,
                      const QVector3D &clothBase) -> HorseProfile;
auto compute_mount_frame(const HorseProfile &profile) -> HorseMountFrame;

class HorseRendererBase {
public:
  virtual ~HorseRendererBase() = default;

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              const HumanoidAnimationContext &rider_ctx,
              HorseProfile &profile, ISubmitter &out) const;

protected:
  virtual void drawAttachments(const DrawContext &, const AnimationInputs &,
                               const HumanoidAnimationContext &,
                               HorseProfile &, const HorseMountFrame &,
                               float, float, float, ISubmitter &) const {}
};

} // namespace Render::GL
