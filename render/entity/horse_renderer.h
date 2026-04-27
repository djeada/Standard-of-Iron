#pragma once

#include "../horse/horse_renderer_base.h"

namespace Render::GL {

class HorseRenderer : public HorseRendererBase {
public:
  using HorseRendererBase::render;

  HorseRenderer();

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override;
};

} // namespace Render::GL
