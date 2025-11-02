#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

// Roman Heavy Armor - Lorica segmentata style with horizontal bands
class RomanHeavyArmorRenderer : public IEquipmentRenderer {
public:
  RomanHeavyArmorRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

// Roman Light Armor - Lighter version with fewer bands
class RomanLightArmorRenderer : public IEquipmentRenderer {
public:
  RomanLightArmorRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
