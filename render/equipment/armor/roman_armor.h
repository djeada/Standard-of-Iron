#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct RomanHeavyArmorConfig {};
struct RomanLightArmorConfig {};

class RomanHeavyArmorRenderer : public IEquipmentRenderer {
public:
  RomanHeavyArmorRenderer() = default;

  static void submit(const RomanHeavyArmorConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

class RomanLightArmorRenderer : public IEquipmentRenderer {
public:
  RomanLightArmorRenderer() = default;

  static void submit(const RomanLightArmorConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

} // namespace Render::GL
