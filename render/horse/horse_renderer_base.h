#pragma once

#include <QVector3D>

#include <utility>

#include "animation/rig/horse_attachment_frames.h"
#include "dimensions.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/registry.h"

namespace Render::Creature::Pipeline {
struct CreaturePreparationResult;
}

namespace Render::GL {
struct AnimationInputs;
struct HumanoidAnimationContext;
class ISubmitter;
} // namespace Render::GL

namespace Render::GL {

class HorseRendererBase {
public:
  HorseRendererBase();

  explicit HorseRendererBase(Render::Creature::Pipeline::UnitVisualSpec spec)
      : m_visual_spec(std::move(spec)) {}

  virtual ~HorseRendererBase() = default;

  [[nodiscard]] auto
  visual_spec() const noexcept -> const Render::Creature::Pipeline::UnitVisualSpec& {
    return m_visual_spec;
  }

  [[nodiscard]] auto get_proportion_scaling() const noexcept -> QVector3D {
    return m_visual_spec.scaling.as_vector();
  }

  void render(const DrawContext& ctx,
              const AnimationInputs& anim,
              const HumanoidAnimationContext& rider_ctx,
              HorseProfile& profile,
              const HorseMotionSample* shared_motion,
              ISubmitter& out,
              Render::Creature::CreatureLOD lod) const;

  void render(const DrawContext& ctx,
              const AnimationInputs& anim,
              const HumanoidAnimationContext& rider_ctx,
              HorseProfile& profile,
              const HorseMotionSample* shared_motion,
              ISubmitter& out) const;

protected:
  void set_visual_spec(Render::Creature::Pipeline::UnitVisualSpec spec) {
    m_visual_spec = std::move(spec);
  }

private:
  Render::Creature::Pipeline::UnitVisualSpec m_visual_spec{};
};

} // namespace Render::GL
