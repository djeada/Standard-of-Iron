#pragma once

#include <QVector3D>

#include <utility>

#include "dimensions.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/elephant/runtime/motion_sample.h"
#include "render/entity/registry.h"

namespace Render::GL {

struct AnimationInputs;
class ISubmitter;

class ElephantRendererBase {
public:
  ElephantRendererBase();

  explicit ElephantRendererBase(Render::Creature::Pipeline::UnitVisualSpec spec)
      : m_visual_spec(std::move(spec)) {}

  virtual ~ElephantRendererBase() = default;

  [[nodiscard]] auto
  visual_spec() const noexcept -> const Render::Creature::Pipeline::UnitVisualSpec& {
    return m_visual_spec;
  }

  [[nodiscard]] auto get_proportion_scaling() const noexcept -> QVector3D {
    return m_visual_spec.scaling.as_vector();
  }

  void render(const DrawContext& ctx,
              const AnimationInputs& anim,
              ElephantProfile& profile,
              const HowdahAttachmentFrame* shared_howdah,
              const ElephantMotionSample* shared_motion,
              ISubmitter& out,
              Render::Creature::CreatureLOD lod) const;

  void render(const DrawContext& ctx,
              const AnimationInputs& anim,
              ElephantProfile& profile,
              const HowdahAttachmentFrame* shared_howdah,
              const ElephantMotionSample* shared_motion,
              ISubmitter& out) const;

protected:
  void set_visual_spec(Render::Creature::Pipeline::UnitVisualSpec spec) {
    m_visual_spec = std::move(spec);
  }

private:
  Render::Creature::Pipeline::UnitVisualSpec m_visual_spec{};
};

} // namespace Render::GL
