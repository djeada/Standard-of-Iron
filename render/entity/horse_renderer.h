#pragma once

#include "render/horse/horse_renderer_base.h"

namespace Render::GL {

class HorseRenderer : public HorseRendererBase {
public:
  using HorseRendererBase::render;

  HorseRenderer();
};

} // namespace Render::GL
