#pragma once

#include "../humanoid/rig.h"
#include "../palette.h"
#include "../submitter.h"

namespace Render::GL {

struct DrawContext;

/**
 * @brief Interface for equipment renderers (helmets, armor, weapons)
 *
 * Equipment pieces are independent renderers that attach to body frames.
 * They are designed for composition: unit renderers look up equipment by ID
 * and render them at appropriate attachment points.
 */
class IEquipmentRenderer {
public:
  virtual ~IEquipmentRenderer() = default;

  /**
   * @brief Render equipment at the specified attachment frame
   *
   * @param ctx Draw context containing entity, world, and rendering state
   * @param frames Body frames providing attachment points (head, torso, etc.)
   * @param palette Color palette for the equipment
   * @param anim Animation context for dynamic equipment (e.g., cloth physics)
   * @param submitter Output submitter for drawing commands
   */
  virtual void render(const DrawContext &ctx, const BodyFrames &frames,
                      const HumanoidPalette &palette,
                      const HumanoidAnimationContext &anim,
                      ISubmitter &submitter) = 0;
};

} // namespace Render::GL
