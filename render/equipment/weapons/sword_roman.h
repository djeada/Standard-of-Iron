#pragma once

#include "render/equipment/i_equipment_renderer.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/palette.h"
#include "sword_renderer.h"

namespace Render::GL {

class RomanSwordRenderer : public SwordRenderer {
public:
  RomanSwordRenderer();
};

} // namespace Render::GL
