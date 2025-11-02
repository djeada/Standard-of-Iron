#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

// Carthage Heavy Armor - Mixed metal and leather with distinctive styling
class CarthageHeavyArmorRenderer : public IEquipmentRenderer {
public:
  CarthageHeavyArmorRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

// Carthage Light Armor - Minimal protection, mostly leather
class CarthageLightArmorRenderer : public IEquipmentRenderer {
public:
  CarthageLightArmorRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
