#pragma once

#include "../horse/rig.h"

namespace Render::GL {

class HorseRenderer : public HorseRendererBase {
public:
  using HorseRendererBase::render;
};

} // namespace Render::GL
