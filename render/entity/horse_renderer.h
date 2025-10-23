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
  float barrelCenterY;

  float neckLength;
  float neckRise;

  float headLength;
  float headWidth;
  float headHeight;
  float muzzleLength;

  float legLength;
  float hoofHeight;

  float tailLength;

  float saddleHeight;
  float saddleThickness;
  float seatForwardOffset;

  float stirrupDrop;
  float stirrupOut;

  float idleBobAmplitude;
  float moveBobAmplitude;
};

struct HorseVariant {
  QVector3D coatColor;
  QVector3D maneColor;
  QVector3D tailColor;
  QVector3D muzzleColor;
  QVector3D hoofColor;
  QVector3D saddleColor;
  QVector3D blanketColor;
  QVector3D tackColor;
};

struct HorseGait {
  float cycleTime;
  float frontLegPhase;
  float rearLegPhase;
  float strideSwing;
  float strideLift;
};

struct HorseProfile {
  HorseDimensions dims;
  HorseVariant variant;
  HorseGait gait;
};

HorseDimensions makeHorseDimensions(uint32_t seed);
HorseVariant makeHorseVariant(uint32_t seed, const QVector3D &leatherBase,
                              const QVector3D &clothBase);
HorseProfile makeHorseProfile(uint32_t seed, const QVector3D &leatherBase,
                              const QVector3D &clothBase);

class HorseRenderer {
public:
  void render(const DrawContext &ctx, const AnimationInputs &anim,
              const HorseProfile &profile, ISubmitter &out) const;
};

} // namespace Render::GL
