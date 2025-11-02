#pragma once

#include "../i_equipment_renderer.h"
#include "../../humanoid/rig.h"
#include "../../palette.h"

namespace Render::GL {

// Roman heavy helmet - Imperial Gallic style galea for legionaries
// Features rounded dome, reinforced brow ridge, cheek guards, neck guard,
// visor cross, breathing holes, brass decorations, and officer's plume/crest
// Based on historical Roman legionary helmets
class RomanHeavyHelmetRenderer : public IEquipmentRenderer {
public:
  RomanHeavyHelmetRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
