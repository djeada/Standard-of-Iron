#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

// Kingdom Heavy Armor - Full plate with prominent pauldrons and gorget
class KingdomHeavyArmorRenderer : public IEquipmentRenderer {
public:
  KingdomHeavyArmorRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

// Kingdom Light Armor - Lighter breastplate, no pauldrons
class KingdomLightArmorRenderer : public IEquipmentRenderer {
public:
  KingdomLightArmorRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
