#pragma once

#include <QVector3D>
#include <cstdint>

namespace Render::GL {

struct DrawContext;
struct AnimationInputs;
class ISubmitter;

struct HorseDimensions {
  float bodyLength;
  float bodyWidth;
  float bodyHeight;
  float barrel_centerY;

  float neckLength;
  float neckRise;

  float headLength;
  float headWidth;
  float headHeight;
  float muzzleLength;

  float legLength;
  float hoofHeight;

  float tailLength;

  float saddle_height;
  float saddleThickness;
  float seatForwardOffset;

  float stirrupDrop;
  float stirrupOut;

  float idleBobAmplitude;
  float moveBobAmplitude;
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
  float cycleTime;
  float frontLegPhase;
  float rearLegPhase;
  float strideSwing;
  float strideLift;
};

struct HorseProfile {
  HorseDimensions dims{};
  HorseVariant variant;
  HorseGait gait{};
};

auto makeHorseDimensions(uint32_t seed) -> HorseDimensions;
auto makeHorseVariant(uint32_t seed, const QVector3D &leatherBase,
                      const QVector3D &clothBase) -> HorseVariant;
auto makeHorseProfile(uint32_t seed, const QVector3D &leatherBase,
                      const QVector3D &clothBase) -> HorseProfile;

class HorseRenderer {
public:
  static void render(const DrawContext &ctx, const AnimationInputs &anim,
                     const HorseProfile &profile, ISubmitter &out);
};

} // namespace Render::GL
